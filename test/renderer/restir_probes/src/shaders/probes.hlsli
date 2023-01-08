#ifndef PROBES_HLSLI
#define PROBES_HLSLI

#include "common.hlsli"

#include "shader_types.hlsli"

namespace probes {
	uint coord_to_index(uint3 coord, probe_constants data) {
		return coord.x + data.grid_size.x * (coord.y + data.grid_size.y * coord.z);
	}

	float3 uv_from_position(float3 position, probe_constants data) {
		return mul(data.world_to_grid, float4(position, 1.0f)).xyz;
	}

	float3 coord_from_position(float3 position, probe_constants data) {
		float3 pos = uv_from_position(position, data);
		return pos * (data.grid_size - 1);
	}

	float3 coord_to_position(int3 coord, probe_constants data) {
		float3 local_pos = coord / float3(data.grid_size - 1);
		return mul(data.grid_to_world, float4(local_pos, 1.0f)).xyz;
	}


	struct weights {
		float w[2][2][2];
		float sum;
	};


	weights get_weights(float3 pos, float3 normal, uint3 cell, probe_constants data) {
		weights result = (weights)0.0f;

		float3 offset = saturate(coord_from_position(pos, data) - cell);
		[unroll]
		for (uint dx = 0; dx <= 1; ++dx) {
			float wx = dx == 0 ? 1.0f - offset.x : offset.x;
			[unroll]
			for (uint dy = 0; dy <= 1; ++dy) {
				float wy = dy == 0 ? 1.0f - offset.y : offset.y;
				[unroll]
				for (uint dz = 0; dz <= 1; ++dz) {
					float wz = dz == 0 ? 1.0f - offset.z : offset.z;

					uint3 cur_cell = cell + uint3(dx, dy, dz);
					float3 probe_pos = coord_to_position(cur_cell, data);
					float3 offset = probe_pos - pos;
					if (dot(normal, offset) > 0.0f) {
						float w = wx * wy * wz;
						result.sum += w;
						result.w[dx][dy][dz] = w;
					}
				}
			}
		}

		return result;
	}

	uint3 get_random_coord(float3 pos, float3 normal, uint3 cell, probe_constants data, float xi) {
		weights w = get_weights(pos, normal, cell, data);
		xi *= w.sum;
		[unroll]
		for (uint dx = 0; dx <= 1; ++dx) {
			[unroll]
			for (uint dy = 0; dy <= 1; ++dy) {
				[unroll]
				for (uint dz = 0; dz <= 1; ++dz) {
					if (xi < w.w[dx][dy][dz]) {
						return cell + uint3(dx, dy, dz);
					}
					xi -= w.w[dx][dy][dz];
				}
			}
		}
		return cell + 1;
	}

	/*uint3 get_nearest_coord(float3 pos, float3 normal, uint3 cell, probe_constants data) {
		float min_dist = max_float_v;
		uint3 min_dist_coord = cell;
		for (uint dx = 0; dx <= 1; ++dx) {
			for (uint dy = 0; dy <= 1; ++dy) {
				for (uint dz = 0; dz <= 1; ++dz) {
					uint3 cur_cell = cell + uint3(dx, dy, dz);
					float3 probe_pos = coord_to_position(cur_cell, data);
					float3 offset = probe_pos - pos;
					if (dot(normal, offset) > 0.0f) {
						float dist = dot(offset, offset);
						if (dist < min_dist) {
							min_dist = dist;
							min_dist_coord = cur_cell;
						}
					}
				}
			}
		}
		return min_dist_coord;
	}*/
}

#endif
