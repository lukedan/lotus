#ifndef RESERVOIRS_HLSLI
#define RESERVOIRS_HLSLI

namespace reservoirs {
	bool add_sample_weight(inout reservoir_common r, float weight, uint num_samples, float target_pdf, float xi) {
		float sum_weights = r.sum_weights * r.num_samples + weight;

		float probability = weight / sum_weights;
		bool replace = xi < probability;

		r.num_samples += num_samples;
		r.sum_weights = sum_weights / r.num_samples;

		if (replace) {
			r.contribution_weight = rcp(target_pdf) * r.sum_weights;
		}

		return replace;
	}

	bool add_sample(inout reservoir_common r, float sample_pdf, float target_pdf, float xi) {
		return add_sample_weight(r, target_pdf / sample_pdf, 1, target_pdf, xi);
	}

	bool merge(inout reservoir_common r1, reservoir_common r2, float pdf2, float xi) {
		return add_sample_weight(r1, r2.sum_weights * r2.num_samples, r2.num_samples, pdf2, xi);
	}
}

#endif
