float metaball(float2 pos, float2 center, float radius, float ramp = 0.01f) {
	return rcp(max((length(pos - center) - radius) * ramp + 1.0f, 0.01f));
}

float2 metaball_center(float2 period, float2 resolution, float time) {
	return (float2(cos(time * period.x), sin(time * period.y)) * 0.3f + 0.5f) * resolution;
}

float2 main(ps_input input) : SV_Target0 {
	float2 pos = input.uv * globals.resolution;
	float x =
		metaball(pos, metaball_center(float2(1.3f, 1.2f), globals.resolution, globals.time), 0.04f * globals.resolution.y) +
		metaball(pos, metaball_center(float2(1.0f, 0.6f), globals.resolution, globals.time), 0.05f * globals.resolution.y) +
		metaball(pos, metaball_center(float2(0.3f, -1.4f), globals.resolution, globals.time), 0.02f * globals.resolution.y);
	return x > 1.0f ? input.position.xy : float2(-1000000, -1000000);
}
