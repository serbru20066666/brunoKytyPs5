#ifndef EMULATOR_SRC_GRAPHICS_PRESENTATION_WINDOW_FRAMEGENERATION_H_
#define EMULATOR_SRC_GRAPHICS_PRESENTATION_WINDOW_FRAMEGENERATION_H_

#include "common/common.h"
#include "graphics/host_gpu/vulkanCommon.h"

#include <memory>

namespace Libs::Graphics {

struct GraphicContext;

// Frame generation with AMD FSR 3 (FidelityFX SDK): builds the frame halfway between the last
// frame given and the current one, so twice as many frames reach the screen. Games hand the
// emulator no motion vectors, so the interpolation leans on FSR's optical flow. Needs
// amd_fidelityfx_vk.dll next to the executable; without it, or on a device that cannot run it,
// the pass reports itself as unavailable and presentation is unchanged.
class FrameGeneration final {
public:
	explicit FrameGeneration(GraphicContext& graphics);
	~FrameGeneration();
	KYTY_CLASS_NO_COPY(FrameGeneration);

	[[nodiscard]] bool Available() const noexcept;

	// Records the generation of the frame between the last one given and `source`: level
	// `source_level` of an image in the transfer-source layout, `source_extent` in size, which is
	// scaled to `size`. Returns whether there is a generated frame to show: not on the first
	// frame, after a size change, while the interpolation settles, or on failure.
	[[nodiscard]] bool Record(vk::CommandBuffer command, vk::Image source, uint32_t source_level,
	                          vk::Extent2D source_extent, vk::Extent2D size, float frame_time_ms);

	// The generated frame, `size` in size, in the transfer-source layout once the commands of
	// the Record() that returned true have run.
	[[nodiscard]] vk::Image Output() const noexcept;

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
};

} // namespace Libs::Graphics

#endif // EMULATOR_SRC_GRAPHICS_PRESENTATION_WINDOW_FRAMEGENERATION_H_
