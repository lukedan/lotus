void fill_pixel(Texture2D<float2> tex, int2 center, int2 coord, int2 resolution, inout float2 value) {
	if (any(coord < 0) || any(coord >= resolution)) {
		return;
	}
	float2 seed = tex.Load(int3(coord, 0));
	float2 off_cur = value - center;
	float2 off_new = seed - center;
	if (dot(off_new, off_new) < dot(off_cur, off_cur)) {
		value = seed;
	}
}

float2 fill(Texture2D<float2> tex, int2 coord, int2 resolution, uint offset) {
	float2 seed = tex.Load(int3(coord, 0));

	fill_pixel(tex, coord, coord + int2(-offset, -offset), resolution, seed);
	fill_pixel(tex, coord, coord + int2(      0, -offset), resolution, seed);
	fill_pixel(tex, coord, coord + int2( offset, -offset), resolution, seed);

	fill_pixel(tex, coord, coord + int2(-offset, 0), resolution, seed);
	fill_pixel(tex, coord, coord + int2( offset, 0), resolution, seed);

	fill_pixel(tex, coord, coord + int2(-offset, offset), resolution, seed);
	fill_pixel(tex, coord, coord + int2(      0, offset), resolution, seed);
	fill_pixel(tex, coord, coord + int2( offset, offset), resolution, seed);

	return seed;
}

Texture2D<float2> seed_tex;

float2 main(ps_input input) : SV_Target0{
	float2 new_seed = fill(seed_tex, input.position.xy, globals.resolution, FILL_OFFSET);
#ifdef DEBUG
	return new_seed / globals.resolution;
#endif
	return new_seed;
}
