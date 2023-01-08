#include "shader_types.hlsli"

ConstantBuffer<fill_texture3d_constants> constants : register(b0, space0);
RWTexture3D<float4> target : register(u1, space0);

[numthreads(4, 4, 4)]
void main_cs(uint3 dispatch_thread_id : SV_DispatchThreadID) {
	if (any(dispatch_thread_id >= constants.size)) {
		return;
	}
	target[dispatch_thread_id] = constants.value;
}
