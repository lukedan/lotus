#include "lotus/profiler.h"

/// \file
/// CPU profiler implementation.

namespace lotus::profiler {
	std::vector<thread_samples> thread_manager::flush() {
		std::scoped_lock lock(_lock);
		std::vector<thread_samples> result;
		for (auto it = _thread_mapping.begin(); it != _thread_mapping.end(); ) {
			thread_samples &s = result.emplace_back();
			s.thread_id = it->first;
			s.samples   = std::move(it->second.samples);

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
