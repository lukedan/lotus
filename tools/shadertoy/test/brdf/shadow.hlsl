#include "scene.hlsl"

Texture2D<float3> position;

float main(ps_input input) : SV_Target0 {
	float3 pos = position.Sample(nearest_sampler, input.uv);
	float3 light = scene_light_position();

	float3 dir = light - pos;
	raymarch_result result = raymarch_scene(pos + 0.01 * dir, dir * 0.99, 100, 1.0f);
	return result.distance < 0.99 ? 0.0f : saturate(result.min_cone_proportion);
}
