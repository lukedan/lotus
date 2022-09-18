#include "types.hlsli"
#include "pcg32.hlsl"
#include "brdf.hlsl"

/*#define RT_SKYVIS_DEBUG*/

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

StructuredBuffer<instance_data> instances  : register(t0, space7);
StructuredBuffer<geometry_data> geometries : register(t1, space7);
StructuredBuffer<material_data> materials  : register(t2, space7);

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
	pcg32 rand;
#endif
};

[shader("miss")]
void main_miss(inout ray_payload payload) {
	// do nothing
}

struct hit_triangle {
	vertex verts[3];
};
hit_triangle get_hit_triangle_indexed_object_space() {
	instance_data instance = instances[InstanceID()];
	geometry_data geometry = geometries[instance.geometry_index];
	uint index_offset = 3 * PrimitiveIndex();
	hit_triangle result;
	[unroll]
	for (uint i = 0; i < 3; ++i) {
		uint index = indices[NonUniformResourceIndex(geometry.index_buffer)][index_offset + i];
		result.verts[i].position = positions[NonUniformResourceIndex(geometry.vertex_buffer )][index];
		result.verts[i].normal   = normals  [NonUniformResourceIndex(geometry.normal_buffer )][index];
		result.verts[i].tangent  = tangents [NonUniformResourceIndex(geometry.tangent_buffer)][index];
		result.verts[i].uv       = uvs      [NonUniformResourceIndex(geometry.uv_buffer     )][index];
	}
	return result;
}
hit_triangle get_hit_triangle_unindexed_object_space() {
	instance_data instance = instances[InstanceID()];
	geometry_data geometry = geometries[instance.geometry_index];
	uint index = 3 * PrimitiveIndex();
	hit_triangle result;
	[unroll]
	for (uint i = 0; i < 3; ++i) {
		result.verts[i].position = positions[NonUniformResourceIndex(geometry.vertex_buffer )][index + i];
		result.verts[i].normal   = normals  [NonUniformResourceIndex(geometry.normal_buffer )][index + i];
		result.verts[i].tangent  = tangents [NonUniformResourceIndex(geometry.tangent_buffer)][index + i];
		result.verts[i].uv       = uvs      [NonUniformResourceIndex(geometry.uv_buffer     )][index + i];
	}
	return result;
}
hit_triangle transform_hit_triangle_to_world_space(hit_triangle tri) {
	[unroll]
	for (int i = 0; i < 3; ++i) {
		tri.verts[i].position = mul(ObjectToWorld3x4(), float4(tri.verts[i].position, 1.0f));
		tri.verts[i].normal   = mul(ObjectToWorld3x4(), float4(tri.verts[i].normal,   0.0f));
	}
	return tri;
}

struct hit_point {
	float3 position;
	float3 normal;
	float3 geometric_normal;
	float4 tangent;
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
	result.tangent =
		tri.verts[0].tangent * (1.0f - barycentrics.x - barycentrics.y) +
		tri.verts[1].tangent * barycentrics.x +
		tri.verts[2].tangent * barycentrics.y;
	return result;
}
hit_point transform_hit_point_to_world_space(hit_point pt) {
	pt.position = mul(ObjectToWorld3x4(), float4(pt.position, 1.0f));
	pt.normal = mul(ObjectToWorld3x4(), float4(pt.normal, 0.0f));
	pt.geometric_normal = mul(ObjectToWorld3x4(), float4(pt.geometric_normal, 0.0f));
	pt.tangent = float4(mul(ObjectToWorld3x4(), float4(pt.tangent.xyz, 0.0f)), pt.tangent.w);
	return pt;
}

float3 get_hit_position() {
	return WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
}

/*void handle_anyhit(inout ray_payload payload, float2 barycentrics, hit_triangle tri) {
	hit_point hit = interpolate_hit_point(tri, barycentrics);
	instance_data instance = instances[InstanceID()];
	material_data mat = materials[instance.material_index];
	float threshold = mat.alpha_cutoff;
	if (threshold > 0.0f) {
		float alpha = textures[mat.base_color_index].SampleLevel(image_sampler, hit.uv, 0).a;
		if (alpha < threshold) {
			IgnoreHit();
		}
	}
}*/

[shader("anyhit")]
void main_anyhit_indexed(inout ray_payload payload, BuiltInTriangleIntersectionAttributes attr) {
	/*handle_anyhit(payload, attr.barycentrics, get_hit_triangle_indexed_object_space());*/
}

[shader("anyhit")]
void main_anyhit_unindexed(inout ray_payload payload, BuiltInTriangleIntersectionAttributes attr) {
	/*handle_anyhit(payload, attr.barycentrics, get_hit_triangle_unindexed_object_space());*/
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

	hit_point hit = transform_hit_point_to_world_space(interpolate_hit_point(tri, barycentrics));

#ifdef RT_ALBEDO_DEBUG
	instance_data inst = instances[InstanceID()];
	material_data mat = materials[inst.material_index];
	payload.light = textures[NonUniformResourceIndex(mat.base_color_index)].SampleLevel(image_sampler, hit.uv, 0);
	return;
#endif

#ifdef RT_SKYVIS_DEBUG
#ifdef RT_USE_PCG32
	float xi1 = pcg32_random_01(payload.rand);
	float xi2 = pcg32_random_01(payload.rand) * 2.0f * pi;
#else
	float xi1 = rd(payload.bounces * 2 + 2, globals.frame_index);
	float xi2 = rd(payload.bounces * 2 + 3, globals.frame_index) * 2.0f * pi;
#endif
	float2 xy = sqrt(xi1) * float2(cos(xi2), sin(xi2));
	float3 dir = normalize(hit.normal);
	if (dot(WorldRayDirection(), dir) > 0.0f) {
		dir = -dir;
	}

	float3 t, b;
	get_any_tangent_space(dir, t, b);
	dir = dir * sqrt(1.0f - dot(xy, xy)) + xy.x * t + xy.y * b;

	RayDesc ray;
	ray.Origin = get_hit_position();
	ray.TMin = globals.t_min;
	ray.Direction = dir;
	ray.TMax = globals.t_max;
	TraceRay(rtas, 0, 0xFF, 0, 0, 0, ray, payload);
	return;
#endif

	instance_data instance = instances[InstanceID()];

	material_data mat = materials[instance.material_index];
	float3 base_color = textures[mat.base_color_index].SampleLevel(image_sampler, hit.uv, 0).rgb;
	base_color *= mat.base_color.rgb;
	float4 material_props = textures[mat.metallic_roughness_index].SampleLevel(image_sampler, hit.uv, 0);
	float metalness = 1.0f;
	float roughness = 1.0f;
	if (mat.is_metallic_roughness) {
		metalness = material_props.z;
		roughness = material_props.y;
	} else {
		// TODO
		roughness = 0.0f;
	}
	metalness = saturate(metalness * mat.metalness);
	roughness = saturate(roughness * mat.roughness);
	roughness = max(0.01f, roughness);
	float alpha = sqr(roughness);

	float3 bitangent = hit.tangent.w * cross(hit.normal, hit.tangent.xyz);
	float3 shading_normal_sample = textures[mat.normal_index].SampleLevel(image_sampler, hit.uv, 0).xyz;
	shading_normal_sample = shading_normal_sample * 2.0f - 1.0f;
	float3 shading_normal = normalize(
		hit.tangent.xyz * shading_normal_sample.x +
		bitangent * shading_normal_sample.y +
		hit.normal * shading_normal_sample.z
	);
	if (dot(WorldRayDirection(), shading_normal) >= 0.0f) {
		payload.light = (float3)0.0f;
		return;
	}
	float3 shading_tangent = normalize(cross(bitangent, shading_normal));
	if (any(isnan(shading_tangent))) {
		shading_tangent = normalize(cross(float3(0.3f, 0.4f, 0.5f), shading_normal));
	}
	float3 shading_bitangent = cross(shading_normal, shading_tangent);

	float3 attenuation = payload.light;
	float3 total_light = (float3)0.0f;

	if (payload.bounces == 1) {
		{ // evaluate diffuse lobe
#ifdef RT_USE_PCG32
			float xi1 = pcg32_random_01(payload.rand);
			float xi2 = pcg32_random_01(payload.rand);
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
			float xi1 = pcg32_random_01(payload.rand);
			float xi2 = pcg32_random_01(payload.rand);
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
		float xi1 = pcg32_random_01(payload.rand);
		float xi2 = pcg32_random_01(payload.rand);
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
	payload.rand = pcg32_seed(DispatchRaysIndex().y * 10000 + DispatchRaysIndex().x, globals.frame_index);
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
