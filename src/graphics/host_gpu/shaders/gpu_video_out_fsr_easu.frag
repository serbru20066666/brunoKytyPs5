#version 450
#extension GL_GOOGLE_include_directive : require

// AMD FidelityFX Super Resolution 1, the scaling pass (EASU): draws the frame at the size of
// the target (see PresentFsr). The constants are FsrEasuCon's.

layout(location = 0) out vec4 color;
layout(binding = 0) uniform sampler2D source; // linear, clamp to edge
layout(push_constant) uniform Easu {
	uvec4 con0;
	uvec4 con1;
	uvec4 con2;
	uvec4 con3;
};

#define A_GPU 1
#define A_GLSL 1
#include "ffx_a.h"
#define FSR_EASU_F 1
AF4 FsrEasuRF(AF2 p) { return textureGather(source, p, 0); }
AF4 FsrEasuGF(AF2 p) { return textureGather(source, p, 1); }
AF4 FsrEasuBF(AF2 p) { return textureGather(source, p, 2); }
#include "ffx_fsr1.h"

void main() {
	AF3 scaled;
	FsrEasuF(scaled, AU2(gl_FragCoord.xy), con0, con1, con2, con3);
	color = vec4(scaled, 1.0);
}
