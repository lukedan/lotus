#include "common.hlsli"
#include "material_common.hlsli"
#include "utils/pcg32.hlsli"

#include "types.hlsli"
#include "brdf.hlsl"

#define RT_USE_PCG32
//#define RT_SKYVIS_DEBUG
//#define RT_ALBEDO_DEBUG
//#define RT_NORMAL_DEBUG

RaytracingAccelerationStructure rtas      : register(t0, space0);
ConstantBuffer<global_data> globals       : register(b1, space0);
RWTexture2D<float4> output                : register(u2, space0);
SamplerState image_sampler                : register(s3, space0);

Texture2D<float4>        textures[]  : register(t0, space1);
StructuredBuffer<float3> positions[] : register(t0, space2);
StructuredBuffer<float3> normals[]   : register(t0, space3);
StructuredBuffer<float4> tangents[]  : register(t0, space4);
StructuredBuffer<float2> uvs[]       : register(t0, space5);
StructuredBuffer<uint>   indices[]   : register(t0, space6);

StructuredBuffer<rt_instance_data> instances               : register(t0, space7);
StructuredBuffer<geometry_data> geometries                 : register(t1, space7);
StructuredBuffer<generic_pbr_material::material> materials : register(t2, space7);

struct vertex {
	float3 position;
	float3 normal;
	float2 uv;
	float4 tangent;
};

float rd(uint sequence, uint index) {
	return frac(pow(0.898654f, sequence) * index);
}


struct ray_payload {
	float3 light;
	uint bounces;
#ifdef RT_USE_PCG32
	pcg32::state rand;
#endif
};

[shader("miss")]
void main_miss(inout ray_payload payload) {
	// do nothing
}

struct hit_triangle {
	vertex verts[3];
};
void compute_tangent(inout hit_triangle tri) {
	// u0.u * U + u0.v * V = x0
	// u1.u * U + u1.v * V = x1
	// u0.u * u1.v * U - u1.u * u0.v * U = x0 * u1.v - x1 * u0.v
	float2 u0 = tri.verts[1].uv - tri.verts[0].uv;
	float2 u1 = tri.verts[2].uv - tri.verts[0].uv;
	float3 x0 = tri.verts[1].position - tri.verts[0].position;
	float3 x1 = tri.verts[2].position - tri.verts[0].position;
	float3 tangent = (x0 * u1.y - x1 * u0.y) / (u0.x * u1.y - u1.x * u0.y);

	float len2 = dot(tangent, tangent);
	if (len2 < 0.0001f || !isfinite(len2)) { // also handles NaN
		float3 normal = normalize(cross(
			tri.verts[1].position - tri.verts[0].position,
			tri.verts[2].position - tri.verts[0].position
		));
		tangent = tangent_frame::any(normal).tangent;
	}

	tri.verts[0].tangent = tri.verts[1].tangent = tri.verts[2].tangent = float4(tangent, 1.0f);
}
hit_triangle get_hit_triangle_indexed_object_space() {
	rt_instance_data instance = instances[InstanceID()];
	geometry_data geometry = geometries[instance.geometry_index];
	uint index_offset = 3 * PrimitiveIndex();
	hit_triangle result;
	[unroll]
	for (uint i = 0; i < 3; ++i) {
		uint index = indices[NonUniformResourceIndex(geometry.index_buffer)][index_offset + i];
		result.verts[i].position = positions[NonUniformResourceIndex(geometry.vertex_buffer )][index];
		result.verts[i].normal   = normals  [NonUniformResourceIndex(geometry.normal_buffer )][index];
		result.verts[i].uv       = uvs      [NonUniformResourceIndex(geometry.uv_buffer     )][index];
		if (geometry.tangent_buffer != max_uint_v) {
			result.verts[i].tangent = tangents[NonUniformResourceIndex(geometry.tangent_buffer)][index];
		}
	}
	if (geometry.tangent_buffer == max_uint_v) {
		compute_tangent(result);
	}
	return result;
}
hit_triangle get_hit_triangle_unindexed_object_space() {
	rt_instance_data instance = instances[InstanceID()];
	geometry_data geometry = geometries[instance.geometry_index];
	uint index = 3 * PrimitiveIndex();
	hit_triangle result;
	[unroll]
	for (uint i = 0; i < 3; ++i) {
		result.verts[i].position = positions[NonUniformResourceIndex(geometry.vertex_buffer )][index + i];
		result.verts[i].normal   = normals  [NonUniformResourceIndex(geometry.normal_buffer )][index + i];
		result.verts[i].uv       = uvs      [NonUniformResourceIndex(geometry.uv_buffer     )][index + i];
		if (geometry.tangent_buffer != max_uint_v) {
			result.verts[i].tangent = tangents[NonUniformResourceIndex(geometry.tangent_buffer)][index + i];
		}
	}
	if (geometry.tangent_buffer == max_uint_v) {
		compute_tangent(result);
	}
	return result;
}

struct hit_point {
	float3 position;
	float3 normal;
	float3 geometric_normal;
	float3 tangent;
	float3 bitangent;
	float2 uv;
};
hit_point interpolate_hit_point(hit_triangle tri, float2 barycentrics) {
	hit_point result;
	result.position =
		tri.verts[0].position * (1.0f - barycentrics.x - barycentrics.y) +
		tri.verts[1].position * barycentrics.x +
		tri.verts[2].position * barycentrics.y;
	result.normal =
		tri.verts[0].normal * (1.0f - barycentrics.x - barycentrics.y) +
		tri.verts[1].normal * barycentrics.x +
		tri.verts[2].normal * barycentrics.y;
	result.geometric_normal = normalize(cross(
		tri.verts[1].position - tri.verts[0].position,
		tri.verts[2].position - tri.verts[0].position
	));
	result.uv =
		tri.verts[0].uv * (1.0f - barycentrics.x - barycentrics.y) +
		tri.verts[1].uv * barycentrics.x +
		tri.verts[2].uv * barycentrics.y;
	float4 tangent =
		tri.verts[0].tangent * (1.0f - barycentrics.x - barycentrics.y) +
		tri.verts[1].tangent * barycentrics.x +
		tri.verts[2].tangent * barycentrics.y;
	result.tangent = tangent.xyz;
	result.bitangent = cross(result.normal, result.tangent) * tangent.w;
	return result;
}
hit_point transform_hit_point_to_world_space(hit_point pt, float3x3 normal_transform) {
	pt.position         = mul(ObjectToWorld3x4(), float4(pt.position, 1.0f));
	pt.normal           = mul(normal_transform,   pt.normal);
	pt.geometric_normal = mul(normal_transform,   pt.geometric_normal);
	pt.tangent          = normalize(mul(ObjectToWorld3x4(), float4(pt.tangent.xyz, 0.0f)));
	pt.bitangent        = normalize(mul(ObjectToWorld3x4(), float4(pt.bitangent, 0.0f)));
	return pt;
}

float3 get_hit_position() {
	return WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
}

void handle_anyhit(inout ray_payload payload, float2 barycentrics, hit_triangle tri) {
	hit_point hit = interpolate_hit_point(tri, barycentrics);
	rt_instance_data instance = instances[InstanceID()];
	generic_pbr_material::material mat = materials[instance.material_index];
	float threshold = mat.properties.alpha_cutoff;
	if (threshold > 0.0f) {
		float alpha = textures[mat.assets.albedo_texture].SampleLevel(image_sampler, hit.uv, 0).a;
		if (alpha < threshold) {
			IgnoreHit();
		}
	}
}

[shader("anyhit")]
void main_anyhit_indexed(inout ray_payload payload, BuiltInTriangleIntersectionAttributes attr) {
	handle_anyhit(payload, attr.barycentrics, get_hit_triangle_indexed_object_space());
}

[shader("anyhit")]
void main_anyhit_unindexed(inout ray_payload payload, BuiltInTriangleIntersectionAttributes attr) {
	handle_anyhit(payload, attr.barycentrics, get_hit_triangle_unindexed_object_space());
}

float3 square_to_unit_hemisphere_y_cosine(float2 xi) {
	float angle = xi.x * 2.0f * pi;
	float2 xz = sqrt(xi.y) * float2(cos(angle), sin(angle));
	return float3(xz.x, sqrt(1.0f - xi.y), xz.y);
}

void handle_closest_hit(inout ray_payload payload, float2 barycentrics, hit_triangle tri) {
	++payload.bounces;
	if (payload.bounces >= 19) {
		payload.light = (float3)0.0f;
		return;
	}

	rt_instance_data instance = instances[InstanceID()];
	generic_pbr_material::material mat = materials[instance.material_index];

	hit_point interpolated_hit = interpolate_hit_point(tri, barycentrics);
	hit_point hit = transform_hit_point_to_world_space(interpolated_hit, (float3x3)instance.normal_transform);

	if (dot(WorldRayDirection(), hit.geometric_normal) >= 0.0f) {
		hit.geometric_normal = -hit.geometric_normal;
		hit.normal = -hit.normal;
	}

#ifdef RT_ALBEDO_DEBUG
	{
		payload.light = textures[NonUniformResourceIndex(mat.assets.albedo_texture)].SampleLevel(image_sampler, hit.uv, 0).rgb;
		return;
	}
#endif

#ifdef RT_SKYVIS_DEBUG
#ifdef RT_USE_PCG32
	float xi1 = pcg32::random_01(payload.rand);
	float xi2 = pcg32::random_01(payload.rand) * 2.0f * pi;
#else
	float xi1 = rd(payload.bounces * 2 + 2, globals.frame_index);
	float xi2 = rd(payload.bounces * 2 + 3, globals.frame_index) * 2.0f * pi;
#endif
	float2 xy = sqrt(xi1) * float2(cos(xi2), sin(xi2));
#if 1
	float3 dir = normalize(hit.geometric_normal);
#else
	float3 dir = normalize(hit.normal);
#endif
	if (dot(WorldRayDirection(), dir) > 0.0f) {
		dir = -dir;
	}

	tangent_frame::tbn tbn = tangent_frame::any(dir);
	dir = dir * sqrt(1.0f - dot(xy, xy)) + xy.x * tbn.tangent + xy.y * tbn.bitangent;

	RayDesc ray;
	ray.Origin    = get_hit_position();
	ray.TMin      = globals.t_min;
	ray.Direction = dir;
	ray.TMax      = globals.t_max;
	payload.light *= 0.8f;
	TraceRay(rtas, 0, 0xFF, 0, 0, 0, ray, payload);
	return;
#endif

	float3 base_color = textures[mat.assets.albedo_texture].SampleLevel(image_sampler, hit.uv, 0).rgb;
	base_color *= mat.properties.albedo_multiplier.rgb;
	float4 material_props = textures[mat.assets.properties_texture].SampleLevel(image_sampler, hit.uv, 0);
	float metalness = 1.0f;
	float roughness = 1.0f;
	if (/*mat.is_metallic_roughness*/true) {
		metalness = material_props.z;
		roughness = material_props.y;
	} else {
		// TODO
		roughness = 0.0f;
	}
#if 1
	metalness = 0.5f;
	roughness = 0.5f;
#endif
	metalness = saturate(metalness * mat.properties.metalness_multiplier);
	roughness = saturate(roughness * mat.properties.roughness_multiplier);
	roughness = max(0.01f, roughness);
	float alpha = sqr(roughness);

	float3 shading_normal_sample = textures[mat.assets.normal_texture].SampleLevel(image_sampler, hit.uv, 0).xyz;
	shading_normal_sample = shading_normal_sample * 2.0f - 1.0f;
	shading_normal_sample.z = sqrt(max(0.0001f, 1.0f - dot(shading_normal_sample.xy, shading_normal_sample.xy)));
	float3 shading_normal = normalize(
		hit.tangent * shading_normal_sample.x +
		hit.bitangent * shading_normal_sample.y +
		hit.normal * shading_normal_sample.z
	);

#ifdef RT_NORMAL_DEBUG
	/*payload.light = normalize(hit.normal) * 0.5f + 0.5f;*/
	payload.light = shading_normal * 0.5f + 0.5f;
	/*payload.light = shading_normal_sample * 0.5f + 0.5f;*/
	/*payload.light = hit.tangent.xyz * 0.5f + 0.5f;*/
	//payload.light = float3(length(hit.tangent), length(bitangent), length(hit.normal)) * 0.5f;
	return;
#endif

	if (dot(WorldRayDirection(), shading_normal) >= 0.0f) {
		payload.light = (float3)0.0f;
		return;
	}
	float3 shading_tangent = normalize(cross(hit.bitangent, shading_normal));
	if (any(isnan(shading_tangent))) {
		shading_tangent = normalize(cross(float3(0.3f, 0.4f, 0.5f), shading_normal));
	}
	float3 shading_bitangent = cross(shading_normal, shading_tangent);

	float3 attenuation = payload.light;
	float3 total_light = (float3)0.0f;

	if (payload.bounces == 1) {
		{ // evaluate diffuse lobe
#ifdef RT_USE_PCG32
			float xi1 = pcg32::random_01(payload.rand);
			float xi2 = pcg32::random_01(payload.rand);
#else
			float xi1 = rd(3, globals.frame_index);
			float xi2 = rd(4, globals.frame_index);
#endif

			float3 normal_dir = square_to_unit_hemisphere_y_cosine(float2(xi1, xi2));
			float3 dir = shading_tangent * normal_dir.x + shading_normal * normal_dir.y + shading_bitangent * normal_dir.z;
			float3 brdf = disney_diffuse_brdf_times_pi(dir, -WorldRayDirection(), shading_normal, base_color, metalness, roughness);
			payload.light = attenuation * brdf;

			RayDesc ray;
			ray.Origin = get_hit_position();
			ray.TMin = globals.t_min;
			ray.Direction = dir;
			ray.TMax = globals.t_max;
			TraceRay(rtas, 0, 0xFF, 0, 1, 0, ray, payload);
			total_light += payload.light;
		}

		{ // evaluate specular lobe
#ifdef RT_USE_PCG32
			float xi1 = pcg32::random_01(payload.rand);
			float xi2 = pcg32::random_01(payload.rand);
#else
			float xi1 = rd(5, globals.frame_index);
			float xi2 = rd(6, globals.frame_index);
#endif

#if 1
			float2 sampled = importance_sample_ggx_cos_theta(alpha, xi1);
			float cos_theta = sampled.x;
			float sin_theta = sqrt(1.0f - sqr(cos_theta));
			float cos_phi = cos(xi2 * 2.0f * pi);
			float sin_phi = sin(xi2 * 2.0f * pi);
			float3 half_vec =
				sin_theta * (cos_phi * shading_tangent + sin_phi * shading_bitangent) +
				cos_theta * shading_normal;
			float3 dir = reflect(WorldRayDirection(), half_vec);
			float inv_pdf = 4.0f * dot(dir, half_vec) * sampled.y;
			float cosine = dot(dir, shading_normal);
			float3 brdf = disney_specular_brdf(dir, -WorldRayDirection(), shading_normal, base_color, metalness, roughness);
			payload.light = attenuation * brdf * cosine * inv_pdf;
#else
			float3 view = -WorldRayDirection();
			float3 view_ts = float3(dot(view, shading_bitangent), dot(view, shading_normal), dot(view, shading_tangent));
			float3 sampled = importance_sample_visible_ggx_smith_g(view_ts, roughness, float2(xi1, xi2));
			float3 half_vec = shading_bitangent * sampled.x + shading_normal * sampled.y + shading_tangent * sampled.z;
			float3 dir = reflect(WorldRayDirection(), half_vec);

			float l_h = dot(dir, half_vec);
			float n_l = dot(shading_normal, dir);

			// specular
			float a = max(0.001f, sqr(roughness));
			float fh = schlick_fresnel(l_h);
			float3 specular0 = lerp((float3)0.08f, base_color, metalness);
			float3 f_specular = lerp(specular0, (float3)1.0f, fh);
#	if 1
			float3 refl = smith_g_ggx(n_l, a) * f_specular;
#	else
			float n_v = dot(shading_normal, view);
			float da = n_v * sqrt(a + (1.0f - a) * sqr(n_l));
			float db = n_l * sqrt(a + (1.0f - a) * sqr(n_v));
			float shadowing = 2.0f * da * db / (da + db);
			float3 refl = f_specular * shadowing / smith_g_ggx(n_v, a);
#	endif
			payload.light = attenuation * refl;
#endif

			if (dot(dir, shading_normal) > 0.0f) {
				RayDesc ray;
				ray.Origin = get_hit_position();
				ray.TMin = globals.t_min;
				ray.Direction = dir;
				ray.TMax = globals.t_max;
				TraceRay(rtas, 0, 0xFF, 0, 1, 0, ray, payload);
				total_light += payload.light;
			}
		}
	} else {
#ifdef RT_USE_PCG32
		float xi1 = pcg32::random_01(payload.rand);
		float xi2 = pcg32::random_01(payload.rand);
#else
		float xi1 = rd(payload.bounces * 2 + 3, globals.frame_index);
		float xi2 = rd(payload.bounces * 2 + 4, globals.frame_index);
#endif

		float3 dir;
		float cosine_over_pdf;
		float threshold = 0.5f * (1.0f - metalness);
		if (xi1 < threshold) {
			xi1 /= threshold;
			float3 normal_dir = square_to_unit_hemisphere_y_cosine(float2(xi1, xi2));
			dir = shading_tangent * normal_dir.x + shading_normal * normal_dir.y + shading_bitangent * normal_dir.z;
			cosine_over_pdf = 1.0f;
		} else {
			xi1 = (xi1 - threshold) / (1.0f - threshold);
			float2 sampled = importance_sample_ggx_cos_theta(alpha, xi1);
			float cos_theta = sampled.x;
			float sin_theta = sqrt(1.0f - sqr(cos_theta));
			float cos_phi = cos(xi2 * 2.0f * pi);
			float sin_phi = sin(xi2 * 2.0f * pi);
			float3 half_vec =
				sin_theta * (cos_phi * shading_tangent + sin_phi * shading_bitangent) +
				cos_theta * shading_normal;
			dir = reflect(WorldRayDirection(), half_vec);
			cosine_over_pdf = dot(dir, shading_normal) * 4.0f * dot(dir, half_vec) * sampled.y;
		}
		float3 brdf = disney_brdf(dir, -WorldRayDirection(), shading_normal, base_color, metalness, roughness);
		payload.light = attenuation * brdf * cosine_over_pdf;

		RayDesc ray;
		ray.Origin = get_hit_position();
		ray.TMin = globals.t_min;
		ray.Direction = dir;
		ray.TMax = globals.t_max;
		TraceRay(rtas, 0, 0xFF, 0, 1, 0, ray, payload);
		total_light += payload.light;
	}

	payload.light = total_light;
}

[shader("closesthit")]
void main_closesthit_indexed(inout ray_payload payload, BuiltInTriangleIntersectionAttributes attr) {
	handle_closest_hit(payload, attr.barycentrics, get_hit_triangle_indexed_object_space());
}

[shader("closesthit")]
void main_closesthit_unindexed(inout ray_payload payload, BuiltInTriangleIntersectionAttributes attr) {
	handle_closest_hit(payload, attr.barycentrics, get_hit_triangle_unindexed_object_space());
}

[shader("raygeneration")]
void main_raygen() {
	ray_payload payload = (ray_payload)0;
	payload.light = (float3)1.0f;
#ifdef RT_USE_PCG32
	payload.rand = pcg32::seed(DispatchRaysIndex().y * 10000 + DispatchRaysIndex().x, globals.frame_index);
#endif

	float2 jitter;
	jitter.x = rd(1, globals.frame_index);
	jitter.y = rd(2, globals.frame_index);

	RayDesc ray;
	ray.Origin    = globals.camera_position;
	ray.TMin      = globals.t_min;
	ray.Direction = normalize(
		globals.top_left +
		(DispatchRaysIndex().x + jitter.x) * globals.right.xyz +
		(DispatchRaysIndex().y + jitter.y) * globals.down.xyz
	);
	ray.TMax      = globals.t_max;

	TraceRay(rtas, 0, 0xFF, 0, 0, 0, ray, payload);

	if (globals.frame_index == 0) {
		output[DispatchRaysIndex().xy].rgb = payload.light;
	} else {
		output[DispatchRaysIndex().xy].rgb += payload.light;
	}
}
