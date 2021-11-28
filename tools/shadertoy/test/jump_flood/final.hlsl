Texture2D<float2> seed;

float3 main(ps_input input) : SV_Target0 {
	float2 seed_pos = seed.Load(int3(input.position.xy, 0));
	float distance = length(input.position.xy - seed_pos);
	distance = clamp(distance / BAND_WIDTH, 0.0f, 7.999);
	float phase = distance + globals.time;
	float brightness = 1.0f - (phase - floor(phase));
	float brightness2 = 1.0f - distance / 8;
	brightness *= pow(brightness2, 0.1f);
	brightness = pow(brightness, 20);
	int color_code = (int)phase % 6 + 1;
	return brightness * float3(color_code & 1, (color_code & 2) >> 1, (color_code & 4) >> 2);
}
