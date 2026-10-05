#include "graphics/presentation/window/frameGeneration.h"

#include "common/assert.h"
#include "common/logging/log.h"
#include "common/threads.h"
#include "graphics/host_gpu/graphicContext.h"

#if defined(_WIN32)

#include <ffx_api/ffx_api.h>
#include <ffx_api/ffx_framegeneration.h>
#include <ffx_api/vk/ffx_api_vk.h>

#include <algorithm>
#include <string>

#define NOMINMAX
#include <windows.h>

namespace Libs::Graphics {

namespace {

constexpr vk::ImageSubresourceRange COLOR_RANGE {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

// The value the constant depth buffer is filled with. The emulator has no depth to give, and a
// flat one tells the interpolation that nothing is hidden behind anything else.
constexpr float FLAT_DEPTH = 0.5f;

// Frames the interpolation takes to start using optical flow after a reset. FSR's shader counts
// ten; a couple more keep clear of the edge. Until then it blends the two frames in place, which
// shows as a translucent ghost of the previous one, so those frames are not shown.
constexpr uint32_t SETTLE_FRAMES = 12;

struct OwnedImage {
	vk::ImageCreateInfo info {};
	VulkanImage         image;
};

void Transition(vk::CommandBuffer command, vk::Image image, vk::ImageLayout from,
                vk::ImageLayout to, uint32_t level = 0) {
	vk::ImageMemoryBarrier2 barrier {};
	barrier.srcStageMask  = vk::PipelineStageFlagBits2::eAllCommands;
	barrier.srcAccessMask = vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite;
	barrier.dstStageMask  = vk::PipelineStageFlagBits2::eAllCommands;
	barrier.dstAccessMask = vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite;
	barrier.oldLayout     = from;
	barrier.newLayout     = to;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image               = image;
	barrier.subresourceRange    = {vk::ImageAspectFlagBits::eColor, level, 1, 0, 1};
	vk::DependencyInfo dependency {};
	dependency.imageMemoryBarrierCount = 1;
	dependency.pImageMemoryBarriers    = &barrier;
	command.pipelineBarrier2(&dependency);
}

FfxApiResource Resource(const OwnedImage& owned, uint32_t state) {
	const auto handle      = static_cast<VkImage>(owned.image.image);
	const auto description =
	    ffxApiGetImageResourceDescriptionVK(handle, static_cast<VkImageCreateInfo>(owned.info), 0);
	return ffxApiGetResourceVK(handle, description, state);
}

// The library asks for some functions by the name of the extension that introduced them. A
// device that has them as core functions without those extensions resolves the suffixed name to
// nothing, so fall back to the core name.
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL GetDeviceProcAddr(VkDevice device, const char* name) {
	const auto lookup = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr;
	if (const auto function = lookup(device, name)) {
		return function;
	}
	std::string core_name {name};
	if (core_name.ends_with("KHR")) {
		core_name.resize(core_name.size() - 3);
		return lookup(device, core_name.c_str());
	}
	return nullptr;
}

void LogMessage(uint32_t type, const wchar_t* message) {
	std::string text;
	for (; message != nullptr && *message != 0; ++message) {
		text.push_back(*message < 0x80 ? static_cast<char>(*message) : '?');
	}
	LOGF("FidelityFX %s: %s\n", type == FFX_API_MESSAGE_TYPE_ERROR ? "error" : "warning",
	     text.c_str());
}

} // namespace

struct FrameGeneration::Impl {
	explicit Impl(GraphicContext& graphics_): graphics(graphics_) {}

	~Impl() {
		if (context != nullptr) {
			WaitForGpu();
			destroy_context(&context, nullptr);
		}
		DestroyImages();
		if (library != nullptr) {
			FreeLibrary(library);
		}
	}

	void Load() {
		vk::PhysicalDeviceFeatures features {};
		graphics.physical_device.getFeatures(&features);
		if (features.shaderStorageImageReadWithoutFormat != VK_TRUE ||
		    features.shaderStorageImageWriteWithoutFormat != VK_TRUE) {
			LOGF("Frame generation needs storage images without format, which this GPU does not "
			     "support\n");
			return;
		}
		library = LoadLibraryW(L"amd_fidelityfx_vk.dll");
		if (library == nullptr) {
			LOGF("Frame generation is enabled but amd_fidelityfx_vk.dll was not found next to "
			     "the executable\n");
			return;
		}
		create_context =
		    reinterpret_cast<PfnFfxCreateContext>(GetProcAddress(library, "ffxCreateContext"));
		destroy_context =
		    reinterpret_cast<PfnFfxDestroyContext>(GetProcAddress(library, "ffxDestroyContext"));
		configure = reinterpret_cast<PfnFfxConfigure>(GetProcAddress(library, "ffxConfigure"));
		dispatch  = reinterpret_cast<PfnFfxDispatch>(GetProcAddress(library, "ffxDispatch"));
		available = create_context != nullptr && destroy_context != nullptr &&
		            configure != nullptr && dispatch != nullptr;
		if (!available) {
			LOGF("amd_fidelityfx_vk.dll does not export the FidelityFX API\n");
		}
	}

	// Nothing recorded earlier may still be running when the context or its images go away.
	void WaitForGpu() {
		Common::LockGuard lock(graphics.queue_mutex);
		static_cast<void>(graphics.device.waitIdle());
	}

	void DestroyImages() {
		for (auto* owned: {&input, &output, &depth, &motion}) {
			if (owned->image.image != nullptr) {
				graphics.DeleteImage(owned->image);
				owned->image.image = nullptr;
			}
		}
	}

	bool CreateOwned(OwnedImage& owned, vk::Format format, vk::Extent2D extent,
	                 vk::ImageUsageFlags usage) {
		owned.info               = vk::ImageCreateInfo {};
		owned.info.imageType     = vk::ImageType::e2D;
		owned.info.format        = format;
		owned.info.extent        = vk::Extent3D {extent.width, extent.height, 1};
		owned.info.mipLevels     = 1;
		owned.info.arrayLayers   = 1;
		owned.info.samples       = vk::SampleCountFlagBits::e1;
		owned.info.tiling        = vk::ImageTiling::eOptimal;
		owned.info.usage         = usage;
		owned.info.sharingMode   = vk::SharingMode::eExclusive;
		owned.info.initialLayout = vk::ImageLayout::eUndefined;
		return graphics.CreateImage(owned.info, owned.image);
	}

	bool Recreate(vk::Extent2D new_size) {
		WaitForGpu();
		if (context != nullptr) {
			destroy_context(&context, nullptr);
			context = nullptr;
		}
		DestroyImages();

		size = new_size;
		// The motion vector and depth inputs carry no information, so they can be small.
		render_size    = vk::Extent2D {(std::max)(size.width / 2, 64u), (std::max)(size.height / 2, 64u)};
		has_history    = false;
		inputs_cleared = false;

		// A format every device can write from a compute shader, whatever the swapchain uses.
		const auto format = vk::Format::eR8G8B8A8Unorm;
		using Usage       = vk::ImageUsageFlagBits;
		if (!CreateOwned(input, format, size,
		                 Usage::eSampled | Usage::eTransferDst | Usage::eTransferSrc) ||
		    !CreateOwned(output, format, size,
		                 Usage::eSampled | Usage::eStorage | Usage::eTransferDst |
		                     Usage::eTransferSrc) ||
		    !CreateOwned(depth, vk::Format::eR32Sfloat, render_size,
		                 Usage::eSampled | Usage::eTransferDst) ||
		    !CreateOwned(motion, vk::Format::eR16G16Sfloat, render_size,
		                 Usage::eSampled | Usage::eTransferDst)) {
			LOGF("Frame generation could not create its images\n");
			return false;
		}

		ffxCreateBackendVKDesc backend {};
		backend.header.type      = FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_VK;
		backend.vkDevice         = static_cast<VkDevice>(graphics.device);
		backend.vkPhysicalDevice = static_cast<VkPhysicalDevice>(graphics.physical_device);
		backend.vkDeviceProcAddr = &GetDeviceProcAddr;

		ffxCreateContextDescFrameGeneration description {};
		description.header.type      = FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATION;
		description.header.pNext     = &backend.header;
		description.displaySize      = {size.width, size.height};
		description.maxRenderSize    = {render_size.width, render_size.height};
		description.backBufferFormat = ffxApiGetSurfaceFormatVK(static_cast<VkFormat>(format));
		if (const auto result = create_context(&context, &description.header, nullptr);
		    result != FFX_API_RETURN_OK) {
			LOGF("Creating the frame generation context failed with %u\n",
			     static_cast<uint32_t>(result));
			context = nullptr;
			return false;
		}

		ffxConfigureDescGlobalDebug1 debug {};
		debug.header.type = FFX_API_CONFIGURE_DESC_TYPE_GLOBALDEBUG1;
		debug.fpMessage   = &LogMessage;
		debug.debugLevel  = FFX_API_CONFIGURE_GLOBALDEBUG_LEVEL_WARNINGS;
		configure(&context, &debug.header);

		LOGF("Frame generation ready at %ux%u\n", size.width, size.height);
		return true;
	}

	bool Record(vk::CommandBuffer command, vk::Image source, uint32_t source_level,
	            vk::Extent2D source_extent, vk::Extent2D frame_size, float frame_time_ms) {
		if (!available) {
			return false;
		}
		if (context == nullptr || frame_size != size) {
			if (!Recreate(frame_size)) {
				available = false;
				return false;
			}
		}

		using Layout = vk::ImageLayout;
		if (!inputs_cleared) {
			// No game hands over motion vectors or depth: motion is all zero and depth is flat,
			// which leaves the interpolation to its optical flow.
			vk::ClearColorValue flat_depth {};
			flat_depth.float32[0] = FLAT_DEPTH;
			const vk::ClearColorValue no_motion {};
			Transition(command, depth.image.image, Layout::eUndefined, Layout::eTransferDstOptimal);
			Transition(command, motion.image.image, Layout::eUndefined, Layout::eTransferDstOptimal);
			command.clearColorImage(depth.image.image, Layout::eTransferDstOptimal, &flat_depth, 1,
			                        &COLOR_RANGE);
			command.clearColorImage(motion.image.image, Layout::eTransferDstOptimal, &no_motion, 1,
			                        &COLOR_RANGE);
			Transition(command, depth.image.image, Layout::eTransferDstOptimal,
			           Layout::eShaderReadOnlyOptimal);
			Transition(command, motion.image.image, Layout::eTransferDstOptimal,
			           Layout::eShaderReadOnlyOptimal);
			inputs_cleared = true;
		}

		// Bring the frame to the size shown, in an image of our own and in our own format.
		vk::ImageBlit blit {};
		blit.srcSubresource = {vk::ImageAspectFlagBits::eColor, source_level, 0, 1};
		blit.srcOffsets[1]  = vk::Offset3D {static_cast<int32_t>(source_extent.width),
		                                    static_cast<int32_t>(source_extent.height), 1};
		blit.dstSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1};
		blit.dstOffsets[1]  =
		    vk::Offset3D {static_cast<int32_t>(size.width), static_cast<int32_t>(size.height), 1};
		Transition(command, input.image.image, Layout::eUndefined, Layout::eTransferDstOptimal);
		command.blitImage(source, Layout::eTransferSrcOptimal, input.image.image,
		                  Layout::eTransferDstOptimal, 1, &blit, vk::Filter::eLinear);
		Transition(command, input.image.image, Layout::eTransferDstOptimal,
		           Layout::eShaderReadOnlyOptimal);
		Transition(command, output.image.image, Layout::eUndefined, Layout::eGeneral);

		const FfxApiRect2D whole_frame {0, 0, static_cast<int32_t>(size.width),
		                                static_cast<int32_t>(size.height)};
		const auto         command_list = static_cast<VkCommandBuffer>(command);

		ffxConfigureDescFrameGeneration config {};
		config.header.type            = FFX_API_CONFIGURE_DESC_TYPE_FRAMEGENERATION;
		config.frameGenerationEnabled = true;
		// The emulator paces and presents the frames itself; only the interpolation is used.
		config.flags          = FFX_FRAMEGENERATION_FLAG_NO_SWAPCHAIN_CONTEXT_NOTIFY;
		config.generationRect = whole_frame;
		config.frameID        = frame_id;

		ffxDispatchDescFrameGenerationPrepareCameraInfo camera {};
		camera.header.type      = FFX_API_DISPATCH_DESC_TYPE_FRAMEGENERATION_PREPARE_CAMERAINFO;
		camera.cameraRight[0]   = 1.0f;
		camera.cameraUp[1]      = 1.0f;
		camera.cameraForward[2] = 1.0f;

		ffxDispatchDescFrameGenerationPrepare prepare {};
		prepare.header.type             = FFX_API_DISPATCH_DESC_TYPE_FRAMEGENERATION_PREPARE;
		prepare.header.pNext            = &camera.header;
		prepare.frameID                 = frame_id;
		prepare.commandList             = command_list;
		prepare.renderSize              = {render_size.width, render_size.height};
		prepare.motionVectorScale       = {1.0f, 1.0f};
		prepare.frameTimeDelta          = frame_time_ms;
		prepare.cameraNear              = 0.1f;
		prepare.cameraFar               = 1000.0f;
		prepare.cameraFovAngleVertical  = 1.0f;
		prepare.viewSpaceToMetersFactor = 1.0f;
		prepare.depth         = Resource(depth, FFX_API_RESOURCE_STATE_COMPUTE_READ);
		prepare.motionVectors = Resource(motion, FFX_API_RESOURCE_STATE_COMPUTE_READ);

		ffxDispatchDescFrameGeneration generate {};
		generate.header.type        = FFX_API_DISPATCH_DESC_TYPE_FRAMEGENERATION;
		generate.commandList        = command_list;
		generate.presentColor       = Resource(input, FFX_API_RESOURCE_STATE_COMPUTE_READ);
		generate.outputs[0]         = Resource(output, FFX_API_RESOURCE_STATE_UNORDERED_ACCESS);
		generate.numGeneratedFrames = 1;
		generate.reset              = !has_history;
		generate.backbufferTransferFunction = FFX_API_BACKBUFFER_TRANSFER_FUNCTION_SRGB;
		generate.minMaxLuminance[0]         = 0.0f;
		generate.minMaxLuminance[1]         = 1000.0f;
		generate.generationRect             = whole_frame;
		generate.frameID                    = frame_id;

		const auto run = [this](const char* step, ffxReturnCode_t result) {
			if (result == FFX_API_RETURN_OK) {
				return true;
			}
			LOGF("Frame generation failed to %s with %u, turning it off\n", step,
			     static_cast<uint32_t>(result));
			available = false;
			return false;
		};
		const bool done = run("configure", configure(&context, &config.header)) &&
		                  run("prepare", dispatch(&context, &prepare.header)) &&
		                  run("generate", dispatch(&context, &generate.header));
		Transition(command, output.image.image, Layout::eGeneral, Layout::eTransferSrcOptimal);
		++frame_id;

		frames_since_reset = generate.reset ? 0 : frames_since_reset + 1;
		has_history        = done;
		return done && frames_since_reset >= SETTLE_FRAMES;
	}

	GraphicContext& graphics;

	HMODULE              library         = nullptr;
	PfnFfxCreateContext  create_context  = nullptr;
	PfnFfxDestroyContext destroy_context = nullptr;
	PfnFfxConfigure      configure       = nullptr;
	PfnFfxDispatch       dispatch        = nullptr;
	bool                 available       = false;

	ffxContext   context = nullptr;
	vk::Extent2D size {};
	vk::Extent2D render_size {};
	bool         has_history        = false;
	bool         inputs_cleared     = false;
	uint32_t     frames_since_reset = 0;
	uint64_t     frame_id           = 0;

	OwnedImage input;
	OwnedImage output;
	OwnedImage depth;
	OwnedImage motion;
};

FrameGeneration::FrameGeneration(GraphicContext& graphics): m_impl(std::make_unique<Impl>(graphics)) {
	m_impl->Load();
}

FrameGeneration::~FrameGeneration() = default;

bool FrameGeneration::Available() const noexcept {
	return m_impl->available;
}

bool FrameGeneration::Record(vk::CommandBuffer command, vk::Image source, uint32_t source_level,
                             vk::Extent2D source_extent, vk::Extent2D size, float frame_time_ms) {
	return m_impl->Record(command, source, source_level, source_extent, size, frame_time_ms);
}

vk::Image FrameGeneration::Output() const noexcept {
	return m_impl->output.image.image;
}

} // namespace Libs::Graphics

#else

namespace Libs::Graphics {

struct FrameGeneration::Impl {};

FrameGeneration::FrameGeneration(GraphicContext& /*graphics*/) {}

FrameGeneration::~FrameGeneration() = default;

bool FrameGeneration::Available() const noexcept {
	return false;
}

bool FrameGeneration::Record(vk::CommandBuffer /*command*/, vk::Image /*source*/,
                             uint32_t /*source_level*/, vk::Extent2D /*source_extent*/,
                             vk::Extent2D /*size*/, float /*frame_time_ms*/) {
	return false;
}

vk::Image FrameGeneration::Output() const noexcept {
	return nullptr;
}

} // namespace Libs::Graphics

#endif
