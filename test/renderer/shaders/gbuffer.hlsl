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
};
ConstantBuffer<material_data> material : register(b0, space1);

struct node_data {
	float4x4 transform;
};
ConstantBuffer<node_data> node : register(b0, space2);

struct global_data {
	float4x4 view;
	float4x4 projection_view;
};
ConstantBuffer<global_data> globals : register(b0, space3);
SamplerState image_sampler          : register(s1, space3);


struct vs_input {
	float3 position : POSITION;
	float3 normal : NORMAL;
	float4 tangent : TANGENT;
	float2 uv : TEXCOORD0;
};

struct ps_input {
	float4 position : SV_Position;
	float3 normal : TEXCOORD1;
	float4 tangent : TANGENT;
	float2 uv : TEXCOORD0;
};

struct ps_output {
	float4 base_color_metalness : SV_Target0;
	float4 normal : SV_Target1;
	float roughness : SV_Target2;
};


ps_input main_vs(vs_input input) {
	ps_input output = (ps_input)0;
	float4x4 transform = mul(globals.view, node.transform);
	float4x4 transform_projection = mul(globals.projection_view, node.transform);
	output.position = mul(transform_projection, float4(input.position, 1.0f));
	output.normal = normalize(mul(node.transform, float4(input.normal, 0.0f)).xyz);
	output.tangent = float4(normalize(mul(node.transform, float4(input.tangent.xyz, 0.0f)).xyz), input.tangent.w);
	output.uv = input.uv;
	return output;
}

ps_output main_ps(ps_input input) {
	float4 base_color_val =
		textures[material.base_color_index].SampleLevel(image_sampler, input.uv, 0) * material.base_color;
	if (base_color_val.a < material.alpha_cutoff) {
		discard;
	}

	float3 normal_val = textures[material.normal_index].SampleLevel(image_sampler, input.uv, 0).xyz * 2.0f - 1.0f;
	normal_val.xy *= material.normal_scale;
	float2 metallic_roughness_val =
		textures[material.metallic_roughness_index].SampleLevel(image_sampler, input.uv, 0).bg *
		float2(material.metalness, material.roughness);

	float3 bitangent = input.tangent.w * cross(input.normal, input.tangent.xyz);
	float3 mapped_normal = input.tangent.xyz * normal_val.x + bitangent * normal_val.y + input.normal * normal_val.z;

	ps_output output;
	output.base_color_metalness = float4(base_color_val.rgb, metallic_roughness_val.r);
	output.normal = float4(normalize(mapped_normal), 0.0f);
	output.roughness = metallic_roughness_val.g;
	return output;
}
