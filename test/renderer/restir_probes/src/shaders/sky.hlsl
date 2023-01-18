#include "misc/fullscreen_quad_vs.hlsl"

#include "shader_types.hlsli"
#include "shading.hlsli"

Texture2D<float3>             sky_latlong : register(t0, space0);
ConstantBuffer<sky_constants> constants   : register(b1, space0);

LOTUS_DECLARE_BASIC_SAMPLER_BINDINGS(space1);

struct ps_output {
	float3 color : SV_Target0;
	float2 motion_vector : SV_Target1;
};

ps_output main_ps(ps_input input) {
	float2 uv_cs = input.uv * 2.0f - 1.0f;
	float4 after_xform = float4(uv_cs.x, -uv_cs.y, 1.0f, 1.0f);
	float3 dir = normalize(mul(constants.inverse_projection_view_no_translation, after_xform * constants.znear).xyz);
	float3 irr = fetch_sky_latlong(sky_latlong, linear_sampler, dir) * constants.sky_scale;

	float4 prev_pos = mul(constants.prev_projection_view_no_translation, float4(dir, 0.0f));
	prev_pos.xyz /= prev_pos.w;
	prev_pos.xy = prev_pos.xy * float2(0.5f, -0.5f) + 0.5f;
	float2 uv_diff = prev_pos.xy - input.uv;

	ps_output result;
	result.color         = irr;
	result.motion_vector = uv_diff;
	return result;
}
