#ifndef TYPES_HLSLI
#define TYPES_HLSLI

struct lighting_constants {
	float4x4 inverse_projection_view;
	float2   depth_linearization_constants;
	uint2    screen_size;
	uint     num_lights;
	bool     trace_shadow_rays_for_naive;
	bool     trace_shadow_rays_for_reservoir;
	uint     diffuse_mode;
	uint     specular_mode;
	bool     use_indirect;
};

struct lighting_combine_constants {
	float lighting_scale;
};


struct reservoir_common {
	float sum_weights;
	float contribution_weight;
	uint num_samples;
};

struct direct_lighting_reservoir {
	uint light_index; // starts from 1, 0 stands for "no light"
	reservoir_common data;
};

struct indirect_lighting_reservoir {
	float3 irradiance;
	float distance;
	float2 direction_octahedral;
	reservoir_common data;
};

struct probe_data {
	float4 irradiance_sh2_r;
	float4 irradiance_sh2_g;
	float4 irradiance_sh2_b;
};

struct probe_constants {
	float4x4 world_to_grid;
	float4x4 grid_to_world;
	uint3 grid_size;
	uint direct_reservoirs_per_probe;
	uint indirect_reservoirs_per_probe;
};

struct direct_reservoir_update_constants {
	uint num_lights;
	uint frame_index;
	uint sample_count_cap;
};

struct indirect_reservoir_update_constants {
	uint frame_index;
	uint sample_count_cap;
};


struct fill_buffer_constants {
	uint value;
	uint size;
};

struct gbuffer_visualization_constants {
	uint mode;
};

struct visualize_probes_constants {
	float4x4 projection_view;
	float3 unit_right;
	float size;
	float3 unit_down;
	uint mode;
	float3 unit_forward;
	float lighting_scale;
};

struct shade_point_debug_constants {
	float4 camera;
	float4 top_left;
	float4 x;
	float4 y;
	uint2 window_size;
	uint num_lights;
	uint mode;
	uint num_frames;
};

#endif
