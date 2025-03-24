#ifndef LOTUS_RENDERER_UTILS_PCG32_HLSLI
#define LOTUS_RENDERER_UTILS_PCG32_HLSLI

struct pcg32 {
	uint64_t st;
	uint64_t inc;

	uint next() {
		uint64_t oldstate = st;
		// advance internal state
		st = oldstate * 6364136223846793005ULL + (inc | 1);
		// calculate output function (XSH RR), uses old state for max ILP
		uint xorshifted = (uint)(((oldstate >> 18u) ^ oldstate) >> 27u);
		uint rot = (uint)(oldstate >> 59u);
		return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
	}

	static pcg32 seed(uint64_t st, uint64_t seq) {
		pcg32 result = { 0u, (seq << 1u) | 1u };
		result.next();
		result.st += st;
		result.next();
		return result;
	}


	float next_01() { // [0, 1)
		return (next() & ((1 << 24) - 1)) / (float)(1 << 24);
	}

	uint next_below_nonuniform(uint value) {
		return next() % value;
	}

	// there may be unreachable values for large ranges
	uint next_below_fast(uint value) {
		return min((uint)floor(next_01() * value), value - 1);
	}
};

#endif
