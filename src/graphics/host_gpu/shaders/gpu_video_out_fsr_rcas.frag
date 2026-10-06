#version 450
#extension GL_GOOGLE_include_directive : require

// AMD FidelityFX Super Resolution 1, the sharpening pass (RCAS), over the scaled frame (see
// PresentFsr). The constant is FsrRcasCon's.

layout(location = 0) out vec4 color;
layout(binding = 0) uniform sampler2D source;
layout(push_constant) uniform Rcas {
	uvec4 con;
};

#define A_GPU 1
#define A_GLSL 1
#include "ffx_a.h"
#define FSR_RCAS_F 1
AF4  FsrRcasLoadF(ASU2 p) { return texelFetch(source, p, 0); }
void FsrRcasInputF(inout AF1 r, inout AF1 g, inout AF1 b) {}
#include "ffx_fsr1.h"

void main() {
	AF3 sharp;
	FsrRcasF(sharp.r, sharp.g, sharp.b, AU2(gl_FragCoord.xy), con);
	color = vec4(sharp, 1.0);
}
