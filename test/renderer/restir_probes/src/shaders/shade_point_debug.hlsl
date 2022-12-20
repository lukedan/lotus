#include "common.hlsli"
#include "types.hlsli"
#include "material_common.hlsli"
#include "math/distribution.hlsli"
#include "math/sh.hlsli"
#include "lights.hlsli"

#include "common_shaders/types.hlsli"

#include "shader_types.hlsli"
#include "probes.hlsli"
#include "reservoir.hlsli"
#include "shading.hlsli"

ConstantBuffer<probe_constants>             probe_consts   : register(b0, space0);
ConstantBuffer<shade_point_debug_constants> constants      : register(b1, space0);
StructuredBuffer<direct_lighting_reservoir> direct_probes  : register(t2, space0);
StructuredBuffer<probe_data>                indirect_sh    : register(t3, space0);
RWTexture2D<float4>                         out_irradiance : register(u4, space0);
RaytracingAccelerationStructure             rtas           : register(t5, space0);

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

[numthreads(8, 8, 1)]
void main_cs(uint2 dispatch_thread_id : SV_DispatchThreadID) {
	if (any(dispatch_thread_id >= constants.window_size)) {
		return;
	}

	float3 direction = (constants.top_left + dispatch_thread_id.x * constants.x + dispatch_thread_id.y * constants.y).xyz;

	float3 irradiance = (float3)0.0f;
	material::shading_properties fragment = (material::shading_properties)0;

	RayDesc ray;
	ray.Origin    = constants.camera.xyz;
	ray.TMin      = 0.0f;
	ray.Direction = direction;
	ray.TMax      = max_float_v;

	RayQuery<RAY_FLAG_CULL_NON_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> ray_query;
	ray_query.TraceRayInline(rtas, 0, 0xFF, ray);
	ray_query.Proceed();

	if (ray_query.CommittedStatus() == COMMITTED_TRIANGLE_HIT) { // shade hit point
		// fetch geometry data
		uint   instance_index = ray_query.CommittedInstanceID();
		float3 hit_position   = ray_query.WorldRayOrigin() + ray_query.CommittedRayT() * ray_query.WorldRayDirection();

		rt_instance_data               inst = instances[instance_index];
		geometry_data                  geom = geometries[inst.geometry_index];
		generic_pbr_material::material mat  = materials[inst.material_index];

		fragment = fetch_material_properties(
			hit_position,
			ray_query.CommittedPrimitiveIndex(),
			ray_query.CommittedTriangleBarycentrics(),
			ray_query.CommittedObjectToWorld3x4(),
			inst, geom, mat,
			textures, positions, normals, tangents, uvs, indices,
			linear_sampler
		);

		irradiance = shade_point(fragment, direct_probes, indirect_sh, all_lights, probe_consts);
	}

	float4 result = (float4)1.0f;
	if (constants.mode == 1) {
		result.rgb = irradiance;
	} else if (constants.mode == 2) {
		result.rgb = fragment.albedo;
	} else if (constants.mode == 3) {
		result.rgb = fragment.normal_ws * 0.5f + 0.5f;
	}
	out_irradiance[dispatch_thread_id] = result;
}
