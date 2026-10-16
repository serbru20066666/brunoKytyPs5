#pragma once

#include <cmath>
#include <cstdint>
#include <string_view>

namespace Loader {
struct Program;
// KYTY_RENDER_HEIGHT=<720..1440> (run-windows.ps1 -RenderHeight, the launcher's "Render resolution"): Demon's Souls'
// performance mode - the PS5's default, its 3D rendered at 2560x1440 and upscaled by the game to a 3840x2160 output -
// renders its 3D at that height (16:9) instead. The output, the UI and what the game draws at output size stay 4K;
// GPU-bound PCs (a laptop's RTX 3080: 1-1 standing 20.7 -> 24.5 fps at 1080) trade sharpness for frame rate.
// Unset or 1440: the game as it is. The game's own 1080p output modes (r_Wants4K 1 and 2) are no option: their UI
// keeps the 4K layout (off screen).
// KYTY_RENDER_ASPECT=<w>:<h> (run-windows.ps1 -Aspect, the launcher's "Aspect ratio"; 21:9 is taken as 64:27): the 3D
// rendered wider than 16:9, up to 32:9, at the render height - 3416x1440 for 21:9 - and its projection's aspect (the
// render setup's 16:9, AspectStore) to match: more of the world to the sides, not a stretched 16:9 view. The game's
// output stays 3840x2160: the wider picture is squeezed into it, the HUD drawn over it as at 16:9, and the presenter
// widens the frames showing the 3D back to the aspect (movies, menus and loading screens stay 16:9: WideAspect()). The
// HUD's elements keep their shape at their sides of the screen (src/local/hud-anchor.h); the in-game menus and the
// markers over the world (lock-on, enemy health, on their targets) are as much wider. (The game's output made as wide,
// 5120x2160, laid its HUD and menus out at 16:9 against its left edge.)
namespace DemonsSoulsRenderSize {
void Install(Program* program);
// The aspect the 3D is rendered at when KYTY_RENDER_ASPECT took effect (Install), else 0 (the game's 16:9).
[[nodiscard]] double WideAspect();

// The video settings' mode 3 case (the setter's and the constructor's): the output size, then the render size, as
// RIP-relative pairs of 32-bit width and height, then a short jump; every build seen (01.005.000, 01.007.000) has it.
inline constexpr std::string_view ModeCase = "c5 fb 10 0d ?? ?? ?? ?? c5 fb 10 05 ?? ?? ?? ?? eb";
inline constexpr uint32_t         OutputWidth = 3840, OutputHeight = 2160, RenderWidth = 2560, RenderHeight = 1440;
// The render size query's per-mode tables (modes 1-5): widths, heights.
inline constexpr uint32_t ModeWidths[5] {1600, 1920, 2560, 3840, 2688};
inline constexpr uint32_t ModeHeights[5] {900, 1080, 1440, 2160, 1512};
inline constexpr int      PerformanceMode = 2; // (index in the tables)
// The render setup's constructor storing the projection's aspect, 16/9 as a float: mov dword [rbx+4], 0x3fe38e39
// (01.007.000 0x90090ae7f, 01.005.000 0x9008ef287; the only such store either build has).
inline constexpr std::string_view AspectStore = "c7 43 04 39 8e e3 3f";
inline constexpr double           GameAspect  = 16.0 / 9.0;
inline constexpr double           WidestAspect = 32.0 / 9.0;

// The render size for a height and an aspect, its width a multiple of 8; {0, 0} for a height out of range.
struct Size {
	uint32_t width  = 0;
	uint32_t height = 0;
};
inline Size RenderSizeFor(uint32_t height, double aspect = GameAspect) {
	if (height < 720 || height > RenderHeight) return {};
	const auto width = static_cast<uint32_t>(std::lround(height * aspect / 8.0)) * 8u;
	return {width, height};
}

// KYTY_RENDER_ASPECT's value: "<w>:<h>" or a number; 21:9 is the displays' 64:27. 0 when unset, at most 16:9 or wider
// than 32:9 (wider is untested: the game's targets were made for 3840 at most).
inline double ParseAspect(std::string_view text) {
	if (text == "21:9") return 64.0 / 27.0;
	double     w = 0.0, h = 1.0;
	const auto colon = text.find(':');
	const auto number = [](std::string_view part) {
		double value = 0.0, scale = 0.0;
		for (const char c: part) {
			if (c == '.' && scale == 0.0) {
				scale = 1.0;
			} else if (c >= '0' && c <= '9') {
				if (scale == 0.0) {
					value = value * 10.0 + (c - '0');
				} else {
					scale /= 10.0;
					value += (c - '0') * scale;
				}
			} else {
				return -1.0;
			}
		}
		return part.empty() ? -1.0 : value;
	};
	if (colon == std::string_view::npos) {
		w = number(text);
	} else {
		w = number(text.substr(0, colon));
		h = number(text.substr(colon + 1));
	}
	if (w <= 0.0 || h <= 0.0) return 0.0;
	const double aspect = w / h;
	return aspect > GameAspect + 0.01 && aspect <= WidestAspect + 0.01 ? aspect : 0.0;
}
} // namespace DemonsSoulsRenderSize
} // namespace Loader
