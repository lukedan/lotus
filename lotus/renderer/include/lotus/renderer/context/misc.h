#pragma once

/// \file
/// Miscellaneous types used during context execution.

#include "lotus/utils/static_function.h"
#include "lotus/gpu/resources.h"

namespace lotus::renderer {
	class context;
	class constant_uploader;
	namespace _details {
		struct queue_data;
	}
	namespace execution {
		struct pipeline_resources_info;
	}


	namespace statistics {
		/// Result of a single timer.
		struct timer_result {
			/// Initializes this object to empty.
			timer_result(std::nullptr_t) {
			}

			std::u8string name; ///< The name of this timer.
			float duration_ms = 0.0f; ///< Duration of the timer in milliseconds.
		};

		/// Statistics about transitions.
		struct transitions {
			/// Initializes all statistics to zero.
			transitions(zero_t) {
			}

			u32 requested_image2d_transitions    = 0; ///< Number of 2D image transitions requested.
			u32 requested_image3d_transitions    = 0; ///< Number of 3D image transitions requested.
			u32 requested_buffer_transitions     = 0; ///< Number of buffer transitions requested.
			u32 requested_raw_buffer_transitions = 0; ///< Number of raw buffer transitions requested.

			u32 submitted_image2d_transitions    = 0; ///< Number of 2D image transitions submitted.
			u32 submitted_image3d_transitions    = 0; ///< Number of 3D image transitions submitted.
			u32 submitted_buffer_transitions     = 0; ///< Number of buffer transitions submitted.
			u32 submitted_raw_buffer_transitions = 0; ///< Number of raw buffer transitions submitted.
		};
	}

	/// Batch statistics that are available as soon as a batch has been submitted.
	struct batch_statistics_early {
		/// Initializes all statistics to zero.
		batch_statistics_early(zero_t) {
		}

		std::vector<statistics::transitions> transitions; ///< Transition statistics.
	};
	/// Batch statistics that are only available once a batch has finished execution.
	struct batch_statistics_late {
		/// Initializes all statistics to zero.
		batch_statistics_late(zero_t) {
		}

		std::vector<std::vector<statistics::timer_result>> timer_results; ///< Timer results for each queue.
	};
}
