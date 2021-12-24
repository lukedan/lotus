#include "pcg32.hlsl"

Texture2D<float4> textures[] : register(t0);

struct material_data {
	float4 base_color;

	float normal_scale;
	float metalness;
	float roughness;
	float alpha_cutoff;

	uint base_color_index;
	uint normal_index;
	uint metallic_roughness_index;
	uint _padding;

	uint _paddint2[52];
};
struct global_data {
	float3 camera_position;
	float t_min;
	float3 top_left;
	float t_max;
	float4 right;
	float3 down;
	uint frame_index;
};
RaytracingAccelerationStructure rtas      : register(t0, space1);
ConstantBuffer<global_data> globals       : register(b1, space1);
RWTexture2D<float4> output                : register(u2, space1);

struct vertex {
	float3 position;
	float3 normal;
	float2 uv;
	float4 tangent;
};
struct instance_data {
	uint first_index;
	uint first_vertex;
	uint material_index;
};
StructuredBuffer<material_data> materials : register(t3, space1);
StructuredBuffer<vertex> vertices         : register(t4, space1);
StructuredBuffer<uint> indices            : register(t5, space1);
StructuredBuffer<instance_data> instances : register(t6, space1);
SamplerState image_sampler                : register(s7, space1);

struct ray_payload {
	float3 light;
	uint bounces;
	pcg32 rand;
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
	uint index_offset = 3 * PrimitiveIndex();
	hit_triangle result;
	[unroll]
	for (uint i = 0; i < 3; ++i) {
		result.verts[i] = vertices[instance.first_vertex + indices[instance.first_index + index_offset + i]];
	}
	return result;
}
hit_triangle get_hit_triangle_unindexed_object_space() {
	instance_data instance = instances[InstanceID()];
	uint vertex_offset = 3 * PrimitiveIndex();
	hit_triangle result;
	[unroll]
	for (uint i = 0; i < 3; ++i) {
		result.verts[i] = vertices[instance.first_vertex + vertex_offset + i];
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
	float3 normal;
	float4 tangent;
	float2 uv;
};
hit_point interpolate_hit_point(hit_triangle tri, float2 barycentrics) {
	hit_point result;
	result.normal =
		tri.verts[0].normal * (1.0f - barycentrics.x - barycentrics.y) +
		tri.verts[1].normal * barycentrics.x +
		tri.verts[2].normal * barycentrics.y;
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
	pt.normal = mul(ObjectToWorld3x4(), float4(pt.normal, 0.0f));
	pt.tangent = float4(mul(ObjectToWorld3x4(), float4(pt.tangent.xyz, 0.0f)), pt.tangent.w);
	return pt;
}

float3 get_hit_position() {
	return WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
}

void handle_anyhit(inout ray_payload payload, float2 barycentrics, hit_triangle tri) {
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
	float angle = xi.x * 2.0f * 3.14159265f;
	float2 xz = sqrt(xi.y) * float2(cos(angle), sin(angle));
	return float3(xz.x, sqrt(1.0f - xi.y), xz.y);
}

void handle_closest_hit(inout ray_payload payload, float2 barycentrics, hit_triangle tri) {
	++payload.bounces;
	if (payload.bounces >= 19) {
		payload.light = float3(0.0f, 0.0f, 0.0f);
		return;
	}

	hit_point hit = transform_hit_point_to_world_space(interpolate_hit_point(tri, barycentrics));
	instance_data instance = instances[InstanceID()];
	material_data mat = materials[instance.material_index];
	float3 base_color = textures[mat.base_color_index].SampleLevel(image_sampler, hit.uv, 0).rgb;

	float xi1 = pcg32_random_01(payload.rand);
	float xi2 = pcg32_random_01(payload.rand);
	float3 dir = square_to_unit_hemisphere_y_cosine(float2(xi1, xi2));
	payload.light *= base_color;
	if (dot(WorldRayDirection(), hit.normal) > 0.0f) {
		dir.y = -dir.y;
	}

	float3 bitangent = hit.tangent.w * cross(hit.normal, hit.tangent.xyz);
	float3 shading_normal_sample = textures[mat.normal_index].SampleLevel(image_sampler, hit.uv, 0).xyz;
	shading_normal_sample = shading_normal_sample * 2.0f - 1.0f;
	float3 shading_normal = normalize(
		hit.tangent.xyz * shading_normal_sample.x +
		bitangent * shading_normal_sample.y +
		hit.normal * shading_normal_sample.z
	);

	float3 normal = shading_normal; // TODO
	float3 basis = abs(normal.x) > 0.3f ? float3(0.0f, 1.0f, 0.0f) : float3(1.0f, 0.0f, 0.0f);
	float3 tangent = normalize(cross(basis, normal));
	float3 bbitangent = cross(normal, tangent);
	float3 new_dir = dir.x * tangent + dir.y * normal + dir.z * bbitangent;

	RayDesc ray;
	ray.Origin    = get_hit_position();
	ray.TMin      = globals.t_min;
	ray.Direction = new_dir;
	ray.TMax      = globals.t_max;
	TraceRay(rtas, 0, 0xFF, 0, 1, 0, ray, payload);
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
	payload.light = float3(1.0f, 1.0f, 1.0f);
	payload.rand = pcg32_seed(globals.frame_index, DispatchRaysIndex().x * 10007 + DispatchRaysIndex().y);

	float2 jitter;
	jitter.x = pcg32_random_01(payload.rand);
	jitter.y = pcg32_random_01(payload.rand);

	RayDesc ray;
	ray.Origin    = globals.camera_position;
	ray.TMin      = globals.t_min;
	ray.Direction =
		globals.top_left +
		(DispatchRaysIndex().x + jitter.x) * globals.right.xyz +
		(DispatchRaysIndex().y + jitter.y) * globals.down.xyz;
	ray.TMax      = globals.t_max;

	TraceRay(rtas, 0, 0xFF, 0, 1, 0, ray, payload);

	if (globals.frame_index == 0) {
		output[DispatchRaysIndex().xy].rgb = payload.light;
	} else {
		output[DispatchRaysIndex().xy].rgb += payload.light;
	}
}
