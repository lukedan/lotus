struct vs_input {
	float4 position : POSITION;
	float4 normal : NORMAL;
	float2 uv : TEXCOORD0;
	uint instance_id : SV_InstanceID;
};

struct ps_input {
	float4 position : SV_Position;
	float2 uv : TEXCOORD0;
	float3 normal : TEXCOORD1;
};

struct node_data {
	float4x4 transform;
};

struct global_data {
	float4x4 view;
	float4x4 projection_view;
};

uniform Texture2D<float4> albedo : register(t0);
uniform SamplerState image_sampler : register(s1);
uniform StructuredBuffer<node_data> nodes : register(t0, space1);
uniform StructuredBuffer<global_data> global_buffer : register(t1, space1);
uniform SamplerState unused : register(s2, space1);

ps_input main_vs(vs_input input) {
	ps_input output = (ps_input)0;
	node_data node = nodes[input.instance_id];
	global_data globals = global_buffer[0];
	float4x4 transform = mul(globals.view, node.transform);
	float4x4 transform_projection = mul(globals.projection_view, node.transform);
	output.position = mul(transform_projection, float4(input.position.xyz, 1.0f));
	output.normal = mul(transform, float4(input.normal.xyz, 0.0f)).xyz;
	output.uv = input.uv;
	return output;
}

float4 main_ps(ps_input input) : SV_Target0 {
	return albedo.SampleLevel(image_sampler, input.uv, 0);
}
