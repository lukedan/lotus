#ifndef RT_HLSLI
#define RT_HLSLI

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

#endif
