#ifndef SHADING_HLSLI
#define SHADING_HLSLI

#include "math/sh.hlsli"
#include "brdf/fresnel.hlsli"
#include "brdf/trowbridge_reitz.hlsli"
#include "material_common.hlsli"
#include "lights.hlsli"
#include "types.hlsli"
#include "utils/pcg32.hlsli"

#include "common_shaders/types.hlsli"

#include "probes.hlsli"
#include "reservoir.hlsli"

template <typename T> T interpolate(StructuredBuffer<T> data, uint indices[3], float2 uv) {
	return
		data[indices[0]] * (1.0f - uv.x - uv.y) +
		data[indices[1]] * uv.x +
		data[indices[2]] * uv.y;
}

float3 fetch_sky_latlong(Texture2D<float3> tex, SamplerState linear_sampler, float3 dir) {
	float x = atan2(dir.z, dir.x) / (2.0f * pi);
	float y = -dir.y * 0.5f + 0.5f;
	return tex.SampleLevel(linear_sampler, float2(x, y), 0);
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
	float4 albedo_smp = textures[NonUniformResourceIndex(mat.assets.albedo_texture    )].SampleLevel(smp, uv, 0.0f);
	float3 normal_smp = textures[NonUniformResourceIndex(mat.assets.normal_texture    )].SampleLevel(smp, uv, 0.0f).rgb;
	float4 prop_smp   = textures[NonUniformResourceIndex(mat.assets.properties_texture)].SampleLevel(smp, uv, 0.0f);
	normal_smp = normal_smp * 2.0f - 1.0f;
	normal_smp.z = sqrt(1.0f - dot(normal_smp.xy, normal_smp.xy));

	material::shading_properties result = (material::shading_properties)0;
	// TODO multipliers
	result.albedo      = albedo_smp.rgb;
	result.position_ws = position;
	result.normal_ws   = material::evaluate_normal_mikkt(normal_smp, geom_normal, geom_tangent.xyz, geom_tangent.w);
	result.glossiness  = 1.0f - prop_smp.g;
	result.metalness   = prop_smp.b;
	return result;
}

void brdf(material::shading_properties frag, float3 light, float3 view, out float3 diffuse, out float3 specular) {
	float3 h = normalize(view + light);
	float n_l = saturate(dot(frag.normal_ws, light));
	float n_h = saturate(dot(frag.normal_ws, h));
	float n_v = saturate(dot(frag.normal_ws, view));
	float h_l = saturate(dot(h, light));

	diffuse = (float3)0.0f;
	if (n_l > 0) {
		float fr_d90 = 0.5f + 2.0f * (1.0f - frag.glossiness) * squared(h_l);
		float n_l_5 = squared(squared(n_l)) * n_l;
		float n_v_5 = squared(squared(n_v)) * n_v;
		float fr_d = (1.0f - (1.0f - fr_d90) * n_l_5) * (1.0f - (1.0f - fr_d90) * n_v_5);

		diffuse = (1.0f - frag.metalness) * fr_d * frag.albedo / pi;
	}

	specular = (float3)0.0f;
	if (n_l > 0.0f && n_v > 0.0f) {
		float alpha = max(0.01f, squared(1.0f - frag.glossiness));
		float3 fr_r0 = lerp(0.04f, frag.albedo, frag.metalness);
		float3 fr = fresnel::schlick(fr_r0, h_l);
		float d = trowbridge_reitz::d(n_h, alpha);
		float g = trowbridge_reitz::g2_smith_approximated(n_l, n_v, alpha);

		specular = (fr * d * g) / (4.0f * n_l * n_v);
	}
}

void shade_direct_light(
	material::shading_properties frag, light li, lights::derived_data li_data, float3 view,
	out float3 diffuse, out float3 specular
) {
	float n_l = saturate(-dot(frag.normal_ws, li_data.direction));

	float3 brdf_d, brdf_s;
	brdf(frag, -li_data.direction, view, brdf_d, brdf_s);
	diffuse = li.irradiance * li_data.attenuation * n_l * brdf_d;
	specular = li.irradiance * li_data.attenuation * n_l * brdf_s;
}

void fetch_indirect_sh(
	material::shading_properties frag,
	Texture3D<float4> tex_sh0,
	Texture3D<float4> tex_sh1,
	Texture3D<float4> tex_sh2,
	Texture3D<float4> tex_sh3,
	SamplerState linear_clamp_sampler,
	probe_constants probe_consts,
	out sh::sh2 shr,
	out sh::sh2 shg,
	out sh::sh2 shb
) {
	float3 pos = frag.position_ws + frag.normal_ws; // TODO better normal offset
	float3 cell_f = probes::uv_from_position(pos, probe_consts);

	float4 sh0 = tex_sh0.SampleLevel(linear_clamp_sampler, cell_f, 0.0f);
	float4 sh1 = tex_sh1.SampleLevel(linear_clamp_sampler, cell_f, 0.0f);
	float4 sh2 = tex_sh2.SampleLevel(linear_clamp_sampler, cell_f, 0.0f);
	float4 sh3 = tex_sh3.SampleLevel(linear_clamp_sampler, cell_f, 0.0f);

	shr = (sh::sh2)float4(sh0.r, sh1.r, sh2.r, sh3.r);
	shg = (sh::sh2)float4(sh0.g, sh1.g, sh2.g, sh3.g);
	shb = (sh::sh2)float4(sh0.b, sh1.b, sh2.b, sh3.b);
}

float3 evaluate_indirect_diffuse(
	material::shading_properties frag,
	sh::sh2 shr,
	sh::sh2 shg,
	sh::sh2 shb
) {
	sh::sh2 cosine_lobe = sh::clamped_cosine::eval_sh2(frag.normal_ws);
	float3 color = float3(
		sh::integrate(shr, cosine_lobe),
		sh::integrate(shg, cosine_lobe),
		sh::integrate(shb, cosine_lobe)
	);
	return max(0.0f, color) * frag.albedo * ((1.0f - frag.metalness) / pi);
}

float3 approximate_indirect_specular(
	material::shading_properties frag, float3 view,
	sh::sh2 shr,
	sh::sh2 shg,
	sh::sh2 shb,
	Texture2D<float2> envmap_lut,
	SamplerState linear_sampler,
	float2 envlut_uvscale, float2 envlut_uvbias
) {
	float roughness = 1.0f - frag.glossiness;

	float n_dot_v = saturate(dot(view, frag.normal_ws));
	float3 f0 = lerp(0.04f, frag.albedo, frag.metalness);
	float2 lut_uv = float2(roughness, n_dot_v);
	float2 lut_value = envmap_lut.SampleLevel(linear_sampler, lut_uv * envlut_uvscale + envlut_uvbias, 0);
	float3 term2 = f0 * lut_value.x + lut_value.y;

	// https://github.com/TheRealMJP/BakingLab/blob/master/BakingLab/Mesh.hlsl, PrefilteredSHSpecular()
	float l1scale = 1.66711256633276f / (1.65715038133932f + roughness);
	shr.b1 = shr.b1 * l1scale;
	shg.b1 = shg.b1 * l1scale;
	shb.b1 = shb.b1 * l1scale;

	float3 light = -reflect(view, frag.normal_ws);
	float3 lookup_dir = normalize(lerp(light, frag.normal_ws, saturate(roughness - 0.25f)));
	sh::sh2 coefficients = sh::impulse::eval_sh2(lookup_dir);
	float3 term1 = float3(
		sh::integrate(shr, coefficients),
		sh::integrate(shg, coefficients),
		sh::integrate(shb, coefficients)
	);

	return term1 * term2;
}

void evaluate_reservoir_direct_lighting(
	material::shading_properties frag, float3 view,
	StructuredBuffer<direct_lighting_reservoir> direct_probes,
	StructuredBuffer<light> all_lights,
	probe_constants probe_consts,
	inout pcg32::state rng,
	out float3 diffuse,
	out float3 specular
) {
	// probe information
	float3 cell_f = probes::coord_from_position(frag.position_ws, probe_consts);
	uint3 cell_index = (uint3)clamp((int3)cell_f, 0, (int3)probe_consts.grid_size - 2);
	uint3 use_probe = probes::get_random_coord(frag.position_ws, frag.normal_ws, cell_index, probe_consts, pcg32::random_01(rng));
	uint use_probe_index = probes::coord_to_index(use_probe, probe_consts);
	uint direct_reservoir_index = use_probe_index * probe_consts.direct_reservoirs_per_probe;

	diffuse = (float3)0.0f;
	specular = (float3)0.0f;
	for (uint i = 0; i < probe_consts.direct_reservoirs_per_probe; ++i) {
		direct_lighting_reservoir cur_res = direct_probes[direct_reservoir_index + i];
		if (cur_res.light_index == 0) {
			continue;
		}

		light cur_li = all_lights[cur_res.light_index - 1];
		lights::derived_data light_data = lights::compute_derived_data(cur_li, frag.position_ws);
		float3 d, s;
		shade_direct_light(frag, cur_li, light_data, view, d, s);
		diffuse += cur_res.data.contribution_weight * d;
		specular += cur_res.data.contribution_weight * s;
	}
	diffuse /= probe_consts.direct_reservoirs_per_probe;
	specular /= probe_consts.direct_reservoirs_per_probe;
}

float3 shade_point(
	material::shading_properties frag, float3 view,
	StructuredBuffer<direct_lighting_reservoir> direct_probes,
	Texture3D<float4> indirect_sh0,
	Texture3D<float4> indirect_sh1,
	Texture3D<float4> indirect_sh2,
	Texture3D<float4> indirect_sh3,
	SamplerState linear_clamp_sampler,
	StructuredBuffer<light> all_lights,
	probe_constants probe_consts,
	inout pcg32::state rng
) {
	// direct
	float3 direct_diffuse;
	float3 direct_specular;
	evaluate_reservoir_direct_lighting(
		frag, view, direct_probes, all_lights, probe_consts, rng,
		direct_diffuse, direct_specular
	);

	// indirect diffuse
	sh::sh2 shr, shg, shb;
	fetch_indirect_sh(
		frag, indirect_sh0, indirect_sh1, indirect_sh2, indirect_sh3,
		linear_clamp_sampler, probe_consts, shr, shg, shb
	);
	float3 indirect_diffuse = evaluate_indirect_diffuse(frag, shr, shg, shb);

	return direct_diffuse + direct_specular + indirect_diffuse;
}

#endif
