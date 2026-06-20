#include "shader_types.hlsli"
#include "common.hlsli"

ConstantBuffer<ssao_constants> constants  : register(b0, space0);
Texture2D<float>               depth_tex  : register(t1, space0);
RWTexture2D<float>             output     : register(u2, space0);

LOTUS_DECLARE_BASIC_SAMPLER_BINDINGS(space1);

float3 uv_depth_to_pos_cs(float2 uv, float depth) {
	const float linear_depth = rcp(constants.depth_linearize_mul_add.x * depth + constants.depth_linearize_mul_add.y);
	const float4 pos_projected = float4(uv * 2.0f - 1.0f, depth, 1.0f) * linear_depth;
	return mul(constants.inv_projection, pos_projected).xyz;
}

float interleaved_gradient_noise(int2 coord) {
    return fmod(52.9829189f * fmod(0.06711056f * coord.x + 0.00583715f * coord.y, 1.0f), 1.0f);
}

[numthreads(8, 8, 1)]
void main_cs(uint2 thread_id : SV_DispatchThreadID) {
	const uint2 coord = thread_id;
	if (any(coord >= constants.image_size)) {
		return;
	}

	const float2 center_pos = coord + 0.5f;
	const float2 center_uv = center_pos * constants.rcp_image_size;
	const float center_depth = depth_tex[coord];
	if (center_depth == 0.0f) {
		return;
	}
	const float3 center_pos_cs = uv_depth_to_pos_cs(center_uv, center_depth);
	const float radius_ss = constants.radius_pixels_1m / center_pos_cs.z;
	const float3 view_dir = -normalize(center_pos_cs);

	const float angle_step = 2.0f * pi / constants.angular_samples;

	float total_area = 0.0f;
	float angle = interleaved_gradient_noise(coord) * angle_step;
	for (uint ang_sample = 0; ang_sample < constants.angular_samples; ++ang_sample, angle += angle_step) {
		const float2 direction = float2(cos(angle), sin(angle));
		float max_dot = -1.0f;
		float last_dot = -1.0f;
		for (uint rad_sample = 0; rad_sample < constants.radial_samples; ++rad_sample) {
			const float t = (rad_sample + 0.5f) / constants.radial_samples;
			const float radius = t * t * radius_ss;
			const float2 sample_coord = direction * radius + center_pos;
			const float2 sample_uv = sample_coord * constants.rcp_image_size;
			if (any(saturate(sample_uv) != sample_uv)) {
				break;
			}
			const float sample_depth = depth_tex.SampleLevel(point_sampler, sample_uv, 0);
			const float3 sample_pos_cs = uv_depth_to_pos_cs(sample_uv, sample_depth);
			const float3 sample_dir = normalize(sample_pos_cs - center_pos_cs);
			const float dot_val = dot(sample_dir, view_dir);
			const float smooth_dot = dot_val < last_dot ? dot_val : lerp(dot_val, last_dot, constants.smoothing);
			max_dot = max(max_dot, smooth_dot);
			last_dot = smooth_dot;
		}
		total_area += angle_step * (1.0f - max_dot);
	}
	output[coord] = total_area / (2.0f * pi);
}
