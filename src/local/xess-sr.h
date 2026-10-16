#ifndef EMULATOR_SRC_LOCAL_XESS_SR_H_
#define EMULATOR_SRC_LOCAL_XESS_SR_H_
// Intel XeSS super resolution (KYTY_UPSCALE=xess), through the XeSS SDK's libxess.dll (next to the emulator) on its
// Vulkan backend: any GPU with DP4a. FrameGen::UpscaleComposite records it right before the game's composite reads
// its 3D image, as for DLSS and FSR 3.1.
#include "graphics/host_gpu/graphicContext.h"

#include <cstdint>
#include <vector>

namespace Libs::Graphics {
namespace FrameGen {
struct Camera;
}
namespace Xess {

[[nodiscard]] bool Wanted();
// Device creation: the extensions XeSS asks for (VK_EXT_mutable_descriptor_type), when the device has them.
void AddDeviceExtensions(vk::Instance instance, vk::PhysicalDevice physical_device,
                         const std::vector<vk::ExtensionProperties>& available, std::vector<const char*>& extensions);
// Device creation: its features (mutableDescriptorType) chained before `next` when its extension was added; returns
// the new head of the chain (`next` when none).
void* ChainDeviceFeatures(const std::vector<const char*>& extensions, void* next);
// After the device exists.
void OnDevice(vk::Instance instance, vk::PhysicalDevice physical_device, vk::Device device);

struct UpscaleDesc {
	VkCommandBuffer    command = VK_NULL_HANDLE;
	const VulkanImage* color   = nullptr;
	vk::ImageView      color_view = nullptr;
	const VulkanImage* depth   = nullptr;
	vk::ImageView      depth_view = nullptr;
	const VulkanImage* motion  = nullptr;
	vk::ImageView      motion_view = nullptr;
	const VulkanImage* output  = nullptr;
	vk::ImageView      output_view = nullptr;
	vk::Extent2D       render {};
	vk::Extent2D       upscale {};
	bool               reset = false;
};
// Color, depth and motion in SHADER_READ_ONLY_OPTIMAL, the output in GENERAL.
bool Upscale(const UpscaleDesc& desc);

} // namespace Xess
} // namespace Libs::Graphics

#endif
