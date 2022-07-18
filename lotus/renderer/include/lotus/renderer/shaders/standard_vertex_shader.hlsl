#include "material_common.hlsli"
#include LOTUS_MATERIAL_INCLUDE

vs_output main_vs(vs_input input) {
	return transform_geometry(input);
}
