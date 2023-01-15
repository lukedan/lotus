#ifndef LOTUS_RENDERER_UTILS_COLOR_SPACES_HLSLI
#define LOTUS_RENDERER_UTILS_COLOR_SPACES_HLSLI

namespace ycocg {
	static const float3x3 matrix_from_rgb = float3x3(
		 0.25f, 0.5f,  0.25f,
		 0.5f,  0.0f, -0.5f,
		-0.25f, 0.5f, -0.25f
	);
	static const float3x3 matrix_to_rgb = float3x3(
		1.0f,  1.0f, -1.0f,
		1.0f,  0.0f,  1.0f,
		1.0f, -1.0f, -1.0f
	);

	float3 from_rgb(float3 rgb) {
		return mul(matrix_from_rgb, rgb);
	}

	float3 to_rgb(float3 ycc) {
		float tmp = ycc.r - ycc.b;
		return float3(tmp + ycc.g, ycc.r + ycc.b, tmp - ycc.g);
	}
}

#endif
