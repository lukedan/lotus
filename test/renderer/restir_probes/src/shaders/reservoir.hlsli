#ifndef RESERVOIRS_HLSLI
#define RESERVOIRS_HLSLI

namespace reservoirs {
	bool add_sample(inout reservoir_common r, float sample_pdf, float target_pdf, float xi, uint sample_count_cap) {
		float weight = target_pdf / sample_pdf;
		float sum_weights = r.sum_weights * r.num_samples + weight;

		float probability = weight / sum_weights;
		bool replace = xi < probability;

		float old_sum_weights = r.sum_weights;
		r.num_samples += 1;
		r.sum_weights = sum_weights / r.num_samples;
		if (replace) {
			r.contribution_weight = rcp(target_pdf) * r.sum_weights;
		} else {
			r.contribution_weight *= r.sum_weights / max(0.001f, old_sum_weights);
		}

		r.num_samples = min(r.num_samples, sample_count_cap);

		return replace;
	}
}

#endif
