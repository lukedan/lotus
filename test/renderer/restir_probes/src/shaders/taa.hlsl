#include "common.hlsli"

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
	// combine lighting
	float3 diffuse       = diffuse_lighting[dispatch_thread_id];
	float3 specular      = specular_lighting[dispatch_thread_id];
	float3 indirect_spec = indirect_specular[dispatch_thread_id];

	float3 result = diffuse + specular;
	if (constants.use_indirect_specular) {
		result += indirect_spec;
	}

	// TAA
	if (constants.enable_taa) {
		float2 old_uv = (dispatch_thread_id + 0.5f) * constants.rcp_viewport_size + motion_vectors[dispatch_thread_id];
		if (all(old_uv > 0.0f) && all(old_uv < 1.0f)) {
			float3 prev_irr = prev_irradiance.SampleLevel(linear_sampler, old_uv, 0);
			result = lerp(prev_irr, result, constants.ra_factor);
		}
	}

	out_irradiance[dispatch_thread_id] = max(0.0f, result);
}
