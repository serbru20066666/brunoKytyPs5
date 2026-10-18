#ifndef EMULATOR_SRC_GRAPHICS_PRESENTATION_SYSTEMOVERLAY_H_
#define EMULATOR_SRC_GRAPHICS_PRESENTATION_SYSTEMOVERLAY_H_

#include "common/common.h"
#include "graphics/host_gpu/vulkanCommon.h"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <span>
#include <string_view>

union SDL_Event;
struct SDL_Window;

namespace Libs::Graphics {

struct GraphicContext;

struct SystemOverlayVisualState {
	bool     active;
	uint64_t revision;
};

void                     InitializeSplashScreen(const std::filesystem::path& sce_sys);
void                     HideSplashScreen();
// The startup precompile's progress screen: phase 0 shaders, 1 graphics pipelines.
void SetStartupPrecompileProgress(bool active, uint32_t phase, uint32_t done, uint32_t total);
// Defined with the pipeline cache: returns once the startup precompile has finished.
void WaitForStartupPrecompile();
void                     InitializeSystemOverlayInput(SDL_Window* window);
void                     ShutdownSystemOverlayInput();
bool                     ProcessSystemOverlayInput(const SDL_Event& event);
SystemOverlayVisualState GetSystemOverlayVisualState() noexcept;
// KYTY_FPS_HUD=1: the frame rate in one small line at the top right of the picture, drawn with each present (not a
// reason to present: it does not keep the overlay active). With frame generation, the frames shown too.
bool                     FpsOverlayEnabled() noexcept;
void                     SetFpsOverlayRates(double game_fps, double present_fps) noexcept;
void                     NotifyTrophyUnlocked(std::string_view name, int32_t grade,
                                              std::span<const std::byte> icon_png);

class SystemOverlay final {
public:
	explicit SystemOverlay(GraphicContext& graphics);
	~SystemOverlay();
	KYTY_CLASS_NO_COPY(SystemOverlay);

	[[nodiscard]] bool PrepareFrame(vk::Extent2D extent, vk::Format format, uint32_t image_count);
	void               Record(vk::CommandBuffer command, vk::ImageView target);
	void               ReleaseVulkan();

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
};

} // namespace Libs::Graphics

#endif // EMULATOR_SRC_GRAPHICS_PRESENTATION_SYSTEMOVERLAY_H_
