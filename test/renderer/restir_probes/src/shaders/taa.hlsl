#include "common.hlsli"
#include "utils/color_spaces.hlsli"

#include "shader_types.hlsli"

Texture2D<float3>   diffuse_lighting  : register(t0, space0);
Texture2D<float3>   specular_lighting : register(t1, space0);
Texture2D<float3>   indirect_specular : register(t2, space0);
Texture2D<float3>   prev_irradiance   : register(t3, space0);
Texture2D<float2>   motion_vectors    : register(t4, space0);
RWTexture2D<float3> out_irradiance    : register(u5, space0);

ConstantBuffer<taa_constants> constants : register(b6, space0);

LOTUS_DECLARE_BASIC_SAMPLER_BINDINGS(space1);

[numthreads(8, 8, 1)]
void main_cs(uint2 dispatch_thread_id : SV_DispatchThreadID) {
	if (any(dispatch_thread_id >= constants.viewport_size)) {
		return;
	}

	// combine lighting
	float3 diffuse       = diffuse_lighting[dispatch_thread_id];
	float3 specular      = specular_lighting[dispatch_thread_id];
	float3 indirect_spec = indirect_specular[dispatch_thread_id];

	float3 irr = diffuse + specular;
	if (constants.use_indirect_specular) {
		irr += indirect_spec;
	}

	// TAA
	if (constants.enable_taa) {
		float2 old_uv = (dispatch_thread_id + 0.5f) * constants.rcp_viewport_size + motion_vectors[dispatch_thread_id];
		if (all(old_uv > 0.0f) && all(old_uv < 1.0f)) {
			// color bounding box
			float3 irr_ycocg = ycocg::from_rgb(irr);
			float3 ycc_min = irr_ycocg;
			float3 ycc_max = irr_ycocg;
			[unroll]
			for (int x = -1; x <= 1; ++x) {
				[unroll]
				for (int y = -1; y <= 1; ++y) {
					if (x == 0 && y == 0) {
						continue;
					}

					uint2 coord = (uint2)clamp((int2)dispatch_thread_id + int2(x, y), 0, (int2)constants.viewport_size - 1);
					float3 dif_off      = diffuse_lighting[coord];
					float3 spec_off     = specular_lighting[coord];
					float3 indirect_off = indirect_specular[coord];

					float3 irr_off = dif_off + spec_off;
					if (constants.use_indirect_specular) {
						irr_off += indirect_off;
					}

					float3 ycc = ycocg::from_rgb(irr_off);
					ycc_min = min(ycc, ycc_min);
					ycc_max = max(ycc, ycc_max);
				}
			}

			float3 prev_irr = prev_irradiance.SampleLevel(linear_sampler, old_uv, 0);

			// intersect with bounding box
			float3 prev_irr_ycocg = ycocg::from_rgb(prev_irr);
			float3 color_offset = prev_irr_ycocg - irr_ycocg;
			float3 target = select(color_offset > 0, ycc_max, ycc_min);
			float3 ratio3 = abs(target - irr_ycocg) / max(0.01f, abs(color_offset));
			float ratio = min(1.0f, min(ratio3.x, min(ratio3.y, ratio3.z)));
			prev_irr = ycocg::to_rgb(irr_ycocg + color_offset * ratio);

			// blend
			irr = lerp(prev_irr, irr, constants.ra_factor);
		}

		irr = max(0.0f, irr); // filter out nan
	}

	out_irradiance[dispatch_thread_id] = irr;
}
