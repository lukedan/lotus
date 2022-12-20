#include "shader_types.hlsli"

RWStructuredBuffer<uint>              target : register(u0, space0);
ConstantBuffer<fill_buffer_constants> data   : register(b1, space0);

[numthreads(64, 1, 1)]
void main_cs(uint dispatch_thread_id : SV_DispatchThreadID) {
	if (dispatch_thread_id < data.size) {
		target[dispatch_thread_id] = data.value;
	}
}
