#ifndef SHADING_HLSLI
#define SHADING_HLSLI

#include "math/sh.hlsli"

#include "probes.hlsli"
#include "reservoir.hlsli"

template <typename T> T interpolate(StructuredBuffer<T> data, uint indices[3], float2 uv) {
	return
		data[indices[0]] * (1.0f - uv.x - uv.y) +
		data[indices[1]] * uv.x +
		data[indices[2]] * uv.y;
}

material::shading_properties fetch_material_properties(
	float3 position, uint triangle_index, float2 triangle_uv, float3x4 object_to_world,
	rt_instance_data inst,
	geometry_data geom,
	generic_pbr_material::material mat,
	Texture2D<float4>        textures[],
	StructuredBuffer<float3> positions[],
	StructuredBuffer<float3> normals[],
	StructuredBuffer<float4> tangents[],
	StructuredBuffer<float2> uvs[],
	StructuredBuffer<uint>   indices[],
	SamplerState smp
) {
	uint tri_indices[3];
	[unroll]
	for (uint i = 0; i < 3; ++i) {
		tri_indices[i] =
			geom.index_buffer == max_uint_v ?
			triangle_index * 3 + i :
			indices[NonUniformResourceIndex(geom.index_buffer)][triangle_index * 3 + i];
	}

	float3 geom_normal  = interpolate(normals [NonUniformResourceIndex(geom.normal_buffer)],  tri_indices, triangle_uv);
	float4 geom_tangent = interpolate(tangents[NonUniformResourceIndex(geom.tangent_buffer)], tri_indices, triangle_uv);
	float2 uv           = interpolate(uvs     [NonUniformResourceIndex(geom.uv_buffer)],      tri_indices, triangle_uv);

	geom_normal = mul((float3x3)inst.normal_transform, geom_normal);
	geom_tangent.xyz = mul(object_to_world, float4(geom_tangent.xyz, 0.0f));


	// fetch texture data
	float4 albedo_smp = textures[NonUniformResourceIndex(mat.assets.albedo_texture)].SampleLevel(smp, uv, 0.0f);
	float3 normal_smp = textures[NonUniformResourceIndex(mat.assets.normal_texture)].SampleLevel(smp, uv, 0.0f).rgb;
	normal_smp = normal_smp * 2.0f - 1.0f;
	normal_smp.z = sqrt(1.0f - dot(normal_smp.xy, normal_smp.xy));

	material::shading_properties result = (material::shading_properties)0;
	result.albedo      = albedo_smp.rgb;
	result.position_ws = position;
	result.normal_ws   = material::evaluate_normal_mikkt(normal_smp, geom_normal, geom_tangent.xyz, geom_tangent.w);
	// TODO gloss and metalness
	return result;
}

float3 shade_point(
	material::shading_properties frag,
	StructuredBuffer<direct_lighting_reservoir> direct_probes,
	StructuredBuffer<probe_data> indirect_sh,
	StructuredBuffer<light> all_lights,
	probe_constants probe_consts
) {
	// probe information
	float3 cell_f = probes::coord_from_position(frag.position_ws, probe_consts);
	uint3 cell_index = (uint3)clamp((int3)cell_f, 0, (int3)probe_consts.grid_size - 2);
	uint3 use_probe = probes::get_nearest_coord(frag.position_ws, frag.normal_ws, cell_index, probe_consts);
	uint use_probe_index = probes::coord_to_index(use_probe, probe_consts);
	uint direct_reservoir_index = use_probe_index * probe_consts.direct_reservoirs_per_probe;


	// shade direct lights
	float3 diffuse_irradiance = (float3)0.0f;
	for (uint i = 0; i < probe_consts.direct_reservoirs_per_probe; ++i) {
		direct_lighting_reservoir cur_res = direct_probes[direct_reservoir_index + i];
		if (cur_res.light_index == 0) {
			continue;
		}

		light cur_li = all_lights[cur_res.light_index - 1];
		lights::derived_data light_data = lights::compute_derived_data(cur_li, frag.position_ws);
		float n_l = -dot(frag.normal_ws, light_data.direction);
		if (n_l > 0) {
			diffuse_irradiance += cur_res.data.contribution_weight * light_data.attenuation * n_l * cur_li.irradiance / pi;
		}
	}
	diffuse_irradiance /= probe_consts.direct_reservoirs_per_probe;

	// shade indirect lights
	probe_data probe_sh = indirect_sh[use_probe_index];
	sh::sh2 cosine_lobe = sh::clamped_cosine::eval_sh2(frag.normal_ws);
	float3 color = float3(
		sh::integrate((sh::sh2)probe_sh.irradiance_sh2_r, cosine_lobe),
		sh::integrate((sh::sh2)probe_sh.irradiance_sh2_g, cosine_lobe),
		sh::integrate((sh::sh2)probe_sh.irradiance_sh2_b, cosine_lobe)
	) / pi;
	diffuse_irradiance += max(0.0f, color);

	return diffuse_irradiance * frag.albedo;
}

#endif
