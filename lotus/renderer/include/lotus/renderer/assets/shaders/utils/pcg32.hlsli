#ifndef LOTUS_RENDERER_UTILS_PCG32_HLSLI
#define LOTUS_RENDERER_UTILS_PCG32_HLSLI

namespace pcg32 {
	struct state {
		uint64_t st;
		uint64_t inc;
	};

	uint random(inout state rng) {
		uint64_t oldstate = rng.st;
		// advance internal state
		rng.st = oldstate * 6364136223846793005ULL + (rng.inc | 1);
		// calculate output function (XSH RR), uses old state for max ILP
		uint xorshifted = (uint)(((oldstate >> 18u) ^ oldstate) >> 27u);
		uint rot = (uint)(oldstate >> 59u);
		return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
	}

	state seed(uint64_t st, uint64_t seq) {
		state result = { 0u, (seq << 1u) | 1u };
		random(result);
		result.st += st;
		random(result);
		return result;
	}


	float random_01(inout state rng) { // [0, 1)
		return (random(rng) & ((1 << 24) - 1)) / (float)(1 << 24);
	}

	uint random_below_nonuniform(uint value, inout state rng) {
		return random(rng) % value;
	}

	// there may be unreachable values for large ranges
	uint random_below_fast(uint value, inout state rng) {
		return min((uint)floor(random_01(rng) * value), value - 1);
	}
}

#endif
