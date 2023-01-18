#include "common.hlsli"
#include "types.hlsli"
#include "material_common.hlsli"
#include "utils/pcg32.hlsli"
#include "math/distribution.hlsli"
#include "math/sh.hlsli"
#include "lights.hlsli"

#include "common_shaders/types.hlsli"

#include "shader_types.hlsli"
#include "probes.hlsli"
#include "reservoir.hlsli"
#include "shading.hlsli"

ConstantBuffer<probe_constants>                     probe_consts    : register(b0, space0);
ConstantBuffer<indirect_reservoir_update_constants> constants       : register(b1, space0);
ConstantBuffer<lighting_constants>                  lighting_consts : register(b2, space0);
StructuredBuffer<direct_lighting_reservoir>         direct_probes   : register(t3, space0);
Texture3D<float4>                                   indirect_sh0    : register(t4, space0);
Texture3D<float4>                                   indirect_sh1    : register(t5, space0);
Texture3D<float4>                                   indirect_sh2    : register(t6, space0);
Texture3D<float4>                                   indirect_sh3    : register(t7, space0);
RWStructuredBuffer<indirect_lighting_reservoir>     indirect_probes : register(u8, space0);
RaytracingAccelerationStructure                     rtas            : register(t9, space0);
Texture2D<float3>                                   sky_latlong     : register(t10, space0);
Texture2D<float2>                                   envmap_lut      : register(t11, space0);

Texture2D<float4>        textures[]  : register(t0, space1);
StructuredBuffer<float3> positions[] : register(t0, space2);
StructuredBuffer<float3> normals[]   : register(t0, space3);
StructuredBuffer<float4> tangents[]  : register(t0, space4);
StructuredBuffer<float2> uvs[]       : register(t0, space5);
StructuredBuffer<uint>   indices[]   : register(t0, space6);

StructuredBuffer<rt_instance_data>               instances  : register(t0, space7);
StructuredBuffer<geometry_data>                  geometries : register(t1, space7);
StructuredBuffer<generic_pbr_material::material> materials  : register(t2, space7);
StructuredBuffer<light>                          all_lights : register(t3, space7);

LOTUS_DECLARE_BASIC_SAMPLER_BINDINGS(space8);

[numthreads(4, 4, 4)]
void main_cs(uint3 dispatch_thread_id : SV_DispatchThreadID) {
	if (any(dispatch_thread_id >= probe_consts.grid_size)) {
		return;
	}

	uint direct_reservoirs_per_probe = probe_consts.direct_reservoirs_per_probe;
	uint indirect_reservoirs_per_probe = probe_consts.indirect_reservoirs_per_probe;
	uint reservoir_offset = probes::coord_to_index(dispatch_thread_id, probe_consts) * indirect_reservoirs_per_probe;
	float3 probe_position = probes::coord_to_position(dispatch_thread_id, probe_consts);

	pcg32::state rng = pcg32::seed(dispatch_thread_id.z * 70000 + dispatch_thread_id.y * 500 + dispatch_thread_id.x * 3, constants.frame_index);

	for (uint i = 0; i < indirect_reservoirs_per_probe; ++i) {
		indirect_lighting_reservoir reservoir = (indirect_lighting_reservoir)0;
		if (constants.temporal_reuse) {
			reservoir = indirect_probes[reservoir_offset + i];
		}
		reservoir.data.num_samples = min(reservoir.data.num_samples, constants.sample_count_cap);

		float3 direction = distribution::unit_square_to_unit_sphere(float2(pcg32::random_01(rng), pcg32::random_01(rng)));
		float3 irradiance = (float3)0.0f;
		float distance = max_float_v;
		{
			RayDesc ray;
			ray.Origin    = probe_position;
			ray.TMin      = 0.01f;
			ray.Direction = direction;
			ray.TMax      = max_float_v;

			RayQuery<RAY_FLAG_CULL_NON_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> ray_query;
			ray_query.TraceRayInline(rtas, 0, 0xFF, ray);
			ray_query.Proceed();

			if (ray_query.CommittedStatus() == COMMITTED_TRIANGLE_HIT) { // shade hit point
				// fetch geometry data
				uint   instance_index = ray_query.CommittedInstanceID();
				float3 hit_position = ray_query.WorldRayOrigin() + ray_query.CommittedRayT() * ray_query.WorldRayDirection();

				rt_instance_data               inst = instances[instance_index];
				geometry_data                  geom = geometries[inst.geometry_index];
				generic_pbr_material::material mat  = materials[inst.material_index];

				material::shading_properties frag = fetch_material_properties(
					hit_position,
					ray_query.CommittedPrimitiveIndex(),
					ray_query.CommittedTriangleBarycentrics(),
					ray_query.CommittedObjectToWorld3x4(),
					inst, geom, mat,
					textures, positions, normals, tangents, uvs, indices,
					linear_sampler
				);

				irradiance = shade_point(
					frag, -direction, direct_probes, indirect_sh0, indirect_sh1, indirect_sh2, indirect_sh3, linear_clamp_sampler, all_lights,
					envmap_lut, lighting_consts.envmaplut_uvscale, lighting_consts.envmaplut_uvbias, probe_consts, rng
				);
				distance   = ray_query.CommittedRayT();
			} else {
				irradiance = fetch_sky_latlong(sky_latlong, linear_sampler, direction) * constants.sky_scale;
				distance   = max_float_v;
			}
		}

		float probability = max(irradiance.r, max(irradiance.g, irradiance.b));
		if (reservoirs::add_sample(reservoir.data, 0.25f / pi, probability, pcg32::random_01(rng))) {
			reservoir.irradiance           = irradiance;
			reservoir.distance             = distance;
			reservoir.direction_octahedral = octahedral_mapping::from_direction(direction);
		}

		indirect_probes[reservoir_offset + i] = reservoir;
	}
}
