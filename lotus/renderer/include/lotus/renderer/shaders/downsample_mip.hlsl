Texture2D<float4>   input  : register(t0);
RWTexture2D<float4> output : register(u1);

[numthreads(8, 8, 1)]
void main(uint2 thread_id : SV_DispatchThreadID) {
	uint2 top_left = thread_id * 2;
	float4 result = 0.25f * (
		input[top_left] +
		input[top_left + uint2(1, 0)] +
		input[top_left + uint2(0, 1)] +
		input[top_left + uint2(1, 1)]
	);
	output[thread_id] = result;
}
