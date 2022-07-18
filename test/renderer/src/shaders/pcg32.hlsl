#ifndef __PCG32_HLSL__
#define __PCG32_HLSL__

struct pcg32 {
	uint64_t state;
	uint64_t inc;
};

uint pcg32_random(inout pcg32 rng) {
	uint64_t oldstate = rng.state;
	// Advance internal state
	rng.state = oldstate * 6364136223846793005ULL + (rng.inc | 1);
	// Calculate output function (XSH RR), uses old state for max ILP
	uint xorshifted = (uint)(((oldstate >> 18u) ^ oldstate) >> 27u);
	uint rot = (uint)(oldstate >> 59u);
	return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

pcg32 pcg32_seed(uint64_t state, uint64_t seq) {
	pcg32 result = { 0u, (seq << 1u) | 1u };
	pcg32_random(result);
	result.state += state;
	pcg32_random(result);
	return result;
}

float pcg32_random_01(inout pcg32 rng) { // [0, 1)
	return pcg32_random(rng) / 4294967296.0f;
}

#endif
