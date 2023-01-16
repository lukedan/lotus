#ifndef TYPES_HLSLI
#define TYPES_HLSLI

struct fill_buffer_constants {
	uint value;
	uint size;
};

struct fill_texture3d_constants {
	float4 value;
	uint3 size;
};


struct reservoir_common {
	float sum_weights;
	float rcp_pdf;
	float contribution_weight; // may not need to store this
	uint num_samples;
};

struct direct_lighting_reservoir {
	uint light_index; // starts from 1, 0 stands for "no light"
	reservoir_common data;
};

struct indirect_lighting_reservoir {
	reservoir_common data;
	float3 irradiance;
	float distance;
	float2 direction_octahedral;
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

struct indirect_spatial_reuse_constants {
	int3 offset;
	uint frame_index;
	uint visibility_test_mode;
};

struct summarize_probe_constants {
	float ra_alpha;
};

struct lighting_constants {
	float4x4 jittered_projection_view;
	float4x4 inverse_jittered_projection_view;
	float4   camera;
	float2   depth_linearization_constants;
	float2   envmaplut_uvscale;
	float2   envmaplut_uvbias;
	uint2    screen_size;
	uint     num_lights;
	bool     trace_shadow_rays_for_naive;
	bool     trace_shadow_rays_for_reservoir;
	uint     lighting_mode;
	float    direct_diffuse_multiplier;
	float    direct_specular_multiplier;
	bool     use_indirect;
};

struct indirect_specular_constants {
	bool enable_mis;
	bool use_screenspace_samples;
	bool approx_indirect_indirect_specular;
	bool use_approx_for_everything;
	uint frame_index;
};

struct taa_constants {
	uint2 viewport_size;
	float2 rcp_viewport_size;
	bool use_indirect_specular;
	float ra_factor;
	bool enable_taa;
};

struct lighting_blit_constants {
	float lighting_scale;
};


struct gbuffer_visualization_constants {
	uint mode;
	bool exclude_sky;
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
