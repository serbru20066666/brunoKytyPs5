#ifndef EMULATOR_SRC_LOCAL_FRAME_GEN_H_
#define EMULATOR_SRC_LOCAL_FRAME_GEN_H_
// DLSS Frame Generation through NVIDIA Streamline (KYTY_FRAMEGEN=<generated frames, 1..5>), for
// Demon's Souls PPSA01341 01.007.000 and 01.005.000. The inputs come from the game's own passes, found by frame
// capture (docs/RE-DEMONS-SOULS.md): the G-buffer vertex shaders' constants hold the camera
// (world->clip matrix, the previous frame's, and the TAA jitter), the TAA dispatch reads the depth
// and motion-vector images. The renderer thread gathers them per guest frame; at the guest flip
// they are copied next to the presenter's frame; the present thread hands them to Streamline and
// presents through it (sl.interposer.dll is the process' Vulkan loader, see Initialize()).
#include "graphics/host_gpu/graphicContext.h"
#include "graphics/host_gpu/renderer/image/image.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <span>

namespace Libs::Graphics {
struct RenderState;
struct TextureBinding;
class CommandBuffer;
class TextureCache;
namespace FrameGen {

// The TAA compute shader, and the G-buffer vertex shaders sharing the constant layout below (flattened SRT
// words), of 01.007.000 and of 01.005.000: the same shaders recompiled (KYTY_SHADER_DUMP: the same resources,
// the vertex shaders the same translated program; the TAA images are what is read from its dispatch).
constexpr std::array<uint64_t, 2> TaaShaders {0xa44e580f4f0cfb2bULL, 0x934a6e1197b99148ULL};
constexpr std::array<uint64_t, 6> GbufferVertexShaders {0xf4d2e78cd1769fc7ULL, 0x08e16923875aa8e9ULL, 0x0ffcbf08be26e74eULL,
                                                        0x1d69b5d27198446aULL, 0x5931e753b757f3bcULL, 0xa97f610824942162ULL};
inline bool IsTaaShader(uint64_t hash) {
	return std::find(TaaShaders.begin(), TaaShaders.end(), hash) != TaaShaders.end();
}
inline bool IsGbufferVertexShader(uint64_t hash) {
	return std::find(GbufferVertexShaders.begin(), GbufferVertexShaders.end(), hash) != GbufferVertexShaders.end();
}
constexpr uint32_t SrtViewProj     = 29; // 16 floats, row-major, clip = M * world
constexpr uint32_t SrtPrevViewProj = 45; // 12 floats: the previous frame's rows x, y and w
constexpr uint32_t SrtJitter       = 57; // 2 floats: clip-space offset (NDC units)

struct Camera {
	std::array<float, 16> view_proj {};
	std::array<float, 16> prev_view_proj {};
	std::array<float, 2>  jitter_ndc {};
	bool                  valid = false;
};

// One presented frame's inputs, owned by the presenter's frame.
struct FrameInputs {
	VulkanImage   depth, motion;
	vk::ImageView depth_view = nullptr, motion_view = nullptr;
	vk::Extent2D  render_extent {};
	Camera        camera;
	uint64_t      guest_frame = 0;
	bool          motion_attachment = false; // the motion copy is a color attachment too
	// FSR 3: the game's image before its HUD (the flip image as the first render pass after the 3D scene found it),
	// copied at the guest flip; its pixels as the flip image's (a UNORM format).
	VulkanImage   hudless;
	vk::ImageView hudless_view = nullptr;
	bool          has_hudless  = false;
	bool          hudless_composite = false; // the game's composite drawn without its HUD texture (not a copy with it)
	// ... scaled to the backbuffer (DLSS-G wants it at the backbuffer's size): ScaleHudless.
	VulkanImage   hudless_display;
	vk::ImageView hudless_display_view = nullptr;
	bool          has_hudless_display  = false;
	bool          valid       = false;
};

// Process start, before the window: loads Streamline when KYTY_FRAMEGEN is set (it then becomes
// SDL's Vulkan loader, and so the proxy of instance, device and swapchain calls).
void Initialize();
// After Initialize: Streamline is loaded (device creation then adds what it relies on but does
// not add itself, VK_KHR_present_id for its VK_NV_low_latency2).
[[nodiscard]] bool Requested();
// Frames to generate per rendered frame (KYTY_FRAMEGEN; 0 when off).
[[nodiscard]] int  GeneratedFrames();
// The present thread's stages around a DLSS-G present, timed into the [dlssg] line: the swapchain image acquire, the
// wait for the frame's own GPU work (present_scheduler) and the flip's whole Present (the flip is done when it returns).
void NoteAcquireMs(double ms);
void NoteGpuWaitMs(double ms);
void NoteFlipPresentMs(double ms);
void NoteSubmitMs(double ms);        // present thread: record + submit of the copy into the swapchain
void NoteBeforePresentMs(double ms); // BeforePresent (Streamline tags and options)
// After the device exists: checks that DLSS-G is available on it.
void OnDevice(vk::Instance instance, vk::PhysicalDevice physical_device, vk::Device device);
// Right after the dispatcher is loaded for the device (before the recording layer copies it):
// the command-buffer calls Streamline intercepts only for its command-list state tracking
// (disabled here) go straight to the driver.
void BypassCommandHooks(vk::Device device);
[[nodiscard]] bool Enabled();
// The TAA pass's inputs are gathered at the guest flip: DLSS-G (Enabled) or FSR 3 frame generation (src/local/fsr3-fg.h).
[[nodiscard]] bool Capturing();

// Present thread, around vkQueuePresentKHR of the image showing `inputs` (null: a frame
// without inputs, e.g. a menu). `backbuffer` is the swapchain extent, `region` the part the
// game image covers.
void BeforePresent(const FrameInputs* inputs, vk::Extent2D backbuffer, vk::Rect2D region,
                   vk::Format format, uint32_t image_count);
void AfterPresent();
// Present thread: the frames DLSS-G showed per presented frame (1 without generation), as last
// queried (about once a second), for the KYTY_FPS_HUD panel.
[[nodiscard]] uint32_t PresentedFrames();
// KYTY_FPS_HUD: the panel's rectangle, normalized to the game image. Its motion vectors are
// zeroed so generated frames keep the panel in place.
void SetStaticRect(float x0, float y0, float x1, float y1);

// Renderer thread (Kyty.Gpu).
// The guest frame ran the TAA pass: the game's 3D view (play, or a cutscene in the engine), not a movie, a menu or a
// loading screen; with or without frame generation, by the normal or the table way. The guest flip takes it
// (Presenter::PrepareFrame): above 60 Hz the other frames keep 60 (videoOut.cpp).
void NoteScene();
[[nodiscard]] bool TakeScene();
// The current guest frame ran the TAA pass so far, or the last one flipped did (HudAnchor: the game queues a frame's
// interface before the flip of the one before).
[[nodiscard]] bool SceneNoted();
// The guest flips taken so far (TakeScene).
[[nodiscard]] uint64_t GuestFlips();
// The current guest frame has no camera yet: its next G-buffer draw goes the normal way (the table way's draws have
// no constants on the CPU), whose hook reads it.
[[nodiscard]] bool WantsCamera();
void OnDraw(uint64_t vs_hash, std::span<const uint32_t> flattened_srt);
void OnDispatch(uint64_t cs_hash, ImageId depth, uint64_t depth_address, ImageId motion,
                uint64_t motion_address);
// At the guest flip, in the frame's command buffer: copy the gathered images into `inputs`.
// `flip` (`flip_id`): the image this flip shows (the HUD-less image is taken from the next ones like it).
void PrepareInputs(GraphicContext& graphics, TextureCache& cache, CommandBuffer& command,
                   FrameInputs& inputs, ImageId flip_id, Image& flip);
// Renderer thread, as a render pass begins (CommandBuffer::BeginRendering, after the previous one ended): a pass into
// a recent flip image; the HUD-less copy is taken before the one the HUD starts with.
void OnBeginRendering(CommandBuffer& command, const RenderState& state);
// Renderer thread, after a draw was recorded: in the flip image's render pass, the HUD-less copy is taken after its
// first draw (the scene's full-screen pass; the HUD is drawn after it in the same pass). KYTY_FSR3_HUDLESS_DRAW: after
// which draw.
void AfterDraw(CommandBuffer& command);
// Normal draw path. The game draws its final image (scene and HUD from separate textures) into the flip image with
// one draw. NoteDraw counts the draws into each color target this frame. FlipSlot: the slot of `targets` that is a
// recent flip image (-1: none). BeforeFlipDraw, with everything bound and before the pass begins: that draw once
// more with the HUD's texture (of `images`, the one most draws went to this frame) cleared to transparent, the
// result copied as the HUD-less image, then the flip image and the HUD texture put back as they were.
void NoteDraw(ImageId target);
// The table and native XPR paths: a draw into a recent flip image is left to the normal path (FlipSlot, BeforeFlipDraw).
[[nodiscard]] bool IsFlipImage(ImageId id);
[[nodiscard]] int FlipSlot(std::span<const ImageId> targets);
void BeforeFlipDraw(CommandBuffer& command, const RenderState& rendering, ImageId target, uint32_t slot,
                    std::span<const TextureBinding> images, const std::function<void()>& redraw);
void DestroyInputs(GraphicContext& graphics, FrameInputs& inputs);
// Present thread, in the present's command buffer: the HUD-less image scaled to the backbuffer (DLSS-G).
void ScaleHudless(GraphicContext& graphics, vk::CommandBuffer command, FrameInputs& inputs, vk::Extent2D backbuffer,
                  vk::Format format);

} // namespace FrameGen
} // namespace Libs::Graphics

#endif // EMULATOR_SRC_LOCAL_FRAME_GEN_H_
