#include "lotus/profiler.h"

/// \file
/// CPU profiler implementation.

#include "lotus/memory/stack_allocator.h"

namespace lotus::profiler {
	analysis_stack_frame samples::analyze() const {
		analysis_stack_frame result;

		{ // populate the tree
			/// Information about the current stack frame.
			struct _stack_frame {
				/// Initializes all fields of this struct.
				_stack_frame(analysis_stack_frame &f, timestamp beg) : frame(&f), begin(beg) {
				}

				analysis_stack_frame *frame = nullptr; ///< The analysis stack frame.
				timestamp begin = zero; ///< The beginning of this stack frame.
			};

			auto bookmark = get_scratch_bookmark();
			auto stack = bookmark.create_vector_array<_stack_frame>();
			for (const timestamp &ts : timestamps) {
				if (ts.label) {
					analysis_stack_frame *cur_frame = stack.empty() ? &result : stack.back().frame;
					auto [it, inserted] = cur_frame->children.try_emplace(ts.label);
					stack.emplace_back(it->second, ts);
				} else {
					crash_if(stack.empty());
					_stack_frame sf = stack.back();
					stack.pop_back();

					++sf.frame->count;
					sf.frame->total_time_inclusive += ts.time - sf.begin.time;
				}
			}
		}

		{ // compute exclusive time
			auto bookmark = get_scratch_bookmark();
			auto stack = bookmark.create_vector_array<analysis_stack_frame*>();
			// compute top-level inclusive time, and push all children onto the stack for evaluation
			for (auto &[name, child] : result.children) {
				result.total_time_inclusive += child.total_time_inclusive;
				stack.emplace_back(&child);
			}
			while (!stack.empty()) {
				analysis_stack_frame *cur = stack.back();
				stack.pop_back();

				cur->total_time_exclusive = cur->total_time_inclusive;
				for (auto &[name, child] : cur->children) {
					cur->total_time_exclusive -= child.total_time_inclusive;
					stack.emplace_back(&child);
				}
			}
		}

		return result;
	}


	std::vector<thread_samples> thread_manager::flush() {
		std::scoped_lock lock(_lock);
		std::vector<thread_samples> result;
		for (auto it = _thread_mapping.begin(); it != _thread_mapping.end(); ) {
			thread_samples &s = result.emplace_back();
			s.thread_id = it->first;
			s.batches   = std::move(it->second.samples);

			if (!it->second.accumulator) {
				it = _thread_mapping.erase(it);
			} else {
				++it;
			}
		}
		return result;
	}

	thread_manager &thread_manager::instance() {
		static thread_manager _instance;
		return _instance;
	}

	std::unique_ptr<thread_accumulator> thread_manager::_register_thread(std::thread::id id) {
		std::unique_ptr result = std::make_unique<thread_accumulator>(*this);
		{
			std::scoped_lock lock(_lock);
			auto [it, inserted] = _thread_mapping.emplace(id, thread_data());
			// TODO handle when thread IDs repeat
			it->second.accumulator = result.get();
		}
		return result;
	}

	void thread_manager::_upload_samples(std::thread::id id, samples s) {
		std::scoped_lock lock(_lock);
		auto it = _thread_mapping.find(id);
		crash_if(it == _thread_mapping.end());
		crash_if(!it->second.accumulator);
		it->second.samples.emplace_back(std::move(s));
	}

	void thread_manager::_unregister_thread(std::thread::id id, samples s) {
		std::scoped_lock lock(_lock);
		auto it = _thread_mapping.find(id);
		crash_if(it == _thread_mapping.end());
		crash_if(!it->second.accumulator);
		it->second.samples.emplace_back(std::move(s));
		// only difference from _upload_samples
		it->second.accumulator = nullptr;
	}

	thread_local std::unique_ptr<thread_accumulator> thread_manager::_this_thread;
}
