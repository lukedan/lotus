#pragma once

/// \file
/// The CPU profiler.

#include <chrono>
#include <thread>
#include <mutex>
#include <deque>
#include <map>
#include <unordered_map>
#include <vector>

#ifdef __aarch64__
#	include <arm_acle.h>
#endif

#include "lotus/common.h"

namespace lotus::profiler {
	struct thread_accumulator;

	using time_t = u64; ///< Timer value.

	/// Retrieves the timer.
	[[nodiscard]] inline time_t get_timer() {
#ifdef __aarch64__
		return __arm_rsr64("CNTVCT_EL0");
#else
#	error "Not implemented"
#endif
	}
	/// Returns the frequency of \ref get_timer().
	[[nodiscard]] inline u64 get_timer_frequency() {
#ifdef __aarch64__
		return __arm_rsr64("CNTFRQ_EL0");
#else
#	error "Not implemented"
#endif
	}

	/// A single timestamp.
	struct timestamp {
		/// Zero initialization.
		timestamp(zero_t) {
		}
		/// Creates a timestamp representing current time.
		[[nodiscard]] static timestamp now(const char8_t *label) {
			timestamp result = zero;
			result.label = label;
			result.time  = get_timer();
			return result;
		}

		/// The label of this timestamp. \p nullptr indicates the timestamp is being popped, while a valid label
		/// indicates the timestamp is being pushed.
		const char8_t *label = nullptr;
		time_t time; ///< The timestamp.
	};

	/// Analysis result of a stack frame.
	struct analysis_stack_frame {
		std::map<const char8_t*, analysis_stack_frame> children; /// Children stack frames.

		time_t total_time_inclusive = 0; ///< Total time spent in this stack frame, including its children.
		time_t total_time_exclusive = 0; ///< Total time spent in this stack frame, excluding its children.
		u64 count = 0; ///< The number of times this stack frame is called by its parent.
	};

	/// An array of samples.
	struct samples {
		std::deque<timestamp> timestamps; ///< Timestamps.

		/// Analyzes all samples.
		[[nodiscard]] analysis_stack_frame analyze() const;
	};
	/// Samples collected from a thread.
	struct thread_samples {
		std::deque<samples> batches; ///< All batches of samples.
		std::thread::id thread_id; ///< The ID of this thread.
	};

	/// Manages data for all threads.
	struct thread_manager {
		friend thread_accumulator;
	public:
		/// Data associated with a single thread.
		struct thread_data {
			std::deque<samples> samples; ///< Accumulated samples, potentially over multiple frames.
			/// The accumulator for this thread. When the thread terminates, this will be reset to \p nullptr.
			thread_accumulator *accumulator = nullptr;
		};

		/// Returns all accumulated samples and resets accumulated data.
		[[nodiscard]] std::vector<thread_samples> flush();

		/// Returns the global instance.
		static thread_manager &instance();
		/// Returns current thread data. If one doesn't exist, it will be allocated from the global
		/// \ref thread_manager instance.
		static thread_accumulator &get_thread_data();
	private:
		/// Mapping between thread IDs and thread data.
		std::unordered_map<std::thread::id, thread_data> _thread_mapping;
		std::mutex _lock; ///< Lock for accessing \ref _thread_mapping.

		/// Registers a thread and returns the new accumulator.
		[[nodiscard]] std::unique_ptr<thread_accumulator> _register_thread(std::thread::id);
		/// Uploads samples from the given thread.
		void _upload_samples(std::thread::id, samples);
		/// Signals that the given thread has finished execution.
		void _unregister_thread(std::thread::id, samples);

		static thread_local std::unique_ptr<thread_accumulator> _this_thread; ///< Thread local profiler data.
	};

	/// Contains an array of profiler events.
	struct thread_accumulator {
	public:
		/// Initializes \ref _manager.
		explicit thread_accumulator(thread_manager &man) : _manager(man) {
		}
		/// Unregisters this thread.
		~thread_accumulator() {
			_manager._unregister_thread(std::this_thread::get_id(), std::move(_samples));
		}

		/// Pushes a time stamp.
		void push(const char8_t *label) {
			crash_if(!label);
			_samples.timestamps.emplace_back(timestamp::now(label));
		}
		/// Pops a time stamp.
		void pop() {
			_samples.timestamps.emplace_back(timestamp::now(nullptr));
		}

		/// Flushes this accumulator.
		void flush() {
			_manager._upload_samples(std::this_thread::get_id(), std::exchange(_samples, {}));
		}
	private:
		samples _samples; ///< All timestamps.
		thread_manager &_manager; ///< Manager for this object.
	};

	/// A profiler scope.
	struct scope {
		/// Begins a scope with the given name.
		explicit scope(const char8_t *label) {
			thread_manager::get_thread_data().push(label);
		}
		/// Begins a scope with the current function name.
		explicit scope(std::source_location loc = std::source_location::current()) {
			// TODO ensure utf8?
			thread_manager::get_thread_data().push(reinterpret_cast<const char8_t*>(loc.function_name()));
		}
		/// No copy construction.
		scope(const scope&) = delete;
		/// No copy assignment.
		scope &operator=(const scope&) = delete;
		/// Ends the scope.
		~scope() {
			thread_manager::get_thread_data().pop();
		}
	};


	inline thread_accumulator &thread_manager::get_thread_data() {
		if (!_this_thread) {
			_this_thread = instance()._register_thread(std::this_thread::get_id());
		}
		return *_this_thread;
	}
}
