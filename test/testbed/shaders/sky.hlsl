#include "shader_types.hlsli"

ConstantBuffer<sky_constants> constants;

float3 main_ps() : SV_Target0 {
	return constants.color;
}
