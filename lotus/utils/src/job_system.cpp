#include "lotus/utils/job_system.h"

/// \file
/// Implementation of the job system.

#include "lotus/logging.h"
#include "lotus/utils/strings.h"
#include "lotus/utils/profiler.h"
#include "lotus/system/thread_handle.h"

namespace lotus::job_system {
	void manager::_control_block::worker_function(std::string thread_name) {
		system::thread_handle::current().set_name(string::assume_utf8(thread_name));
		thread_name = {};

		std::unique_lock lock(_job_lock);
		while (true) {
			profiler::scope p1(u8"Wake Up");

			if (_terminate) {
				break;
			}
			// wait for a new job
			if (_pending_jobs.empty()) {
				p1.end(); // don't profile wait times
				_signal.wait(lock);
				continue;
			}
			_details::job_ptr job = std::move(_pending_jobs.front());
			_pending_jobs.pop();

			// run job with lock released
			lock.unlock();
			job->job(*job);
			lock.lock();

			// notify other jobs
			for (const _details::resource_ptr &output : job->outputs) {
				output->value_ready = true;
				for (_details::job_data *consumer : output->consumers) {
					--consumer->num_pending_inputs;
					if (consumer->num_pending_inputs == 0) {
						// move job to pending queue
						bool found_job = false;
						for (auto it = _waiting_jobs.begin(); it != _waiting_jobs.end(); ++it) {
							if (it->get() == consumer) {
								found_job = true;
								_pending_jobs.emplace(std::move(*it));
								std::swap(*it, _waiting_jobs.back());
								_waiting_jobs.pop_back();
								break;
							}
						}
						crash_if(!found_job);
					}
				}
			}
			_signal.notify_all();
		}
	}

	_details::worker_data manager::_control_block::spawn_worker(std::string thread_name) {
		return _details::worker_data(std::thread(&_control_block::worker_function, this, std::move(thread_name)));
	}

	void manager::_control_block::schedule_job(_details::job_ptr job) {
		std::scoped_lock lock(_job_lock);

		// register input dependencies
		job->num_pending_inputs = 0;
		for (const _details::resource_ptr &input : job->inputs) {
			input->consumers.emplace_back(job.get());
			if (!input->value_ready) {
				++job->num_pending_inputs;
			}
		}

		// add to pending jobs
		if (job->num_pending_inputs == 0) {
			_pending_jobs.emplace(std::move(job));
			_signal.notify_one();
		} else {
			_waiting_jobs.emplace_back(std::move(job));
		}
	}

	void manager::_control_block::wait_for_resource(const _details::resource_data *rsrc) {
		std::unique_lock lock(_job_lock);
		while (!rsrc->value_ready) {
			_signal.wait(lock);
		}
	}

	void manager::_control_block::terminate() {
		std::unique_lock lock(_job_lock);
		_terminate = true;

		usize num_jobs_discarded = _waiting_jobs.size();
		_waiting_jobs.clear();
		while (!_pending_jobs.empty()) {
			++num_jobs_discarded;
			_pending_jobs.pop();
		}
		if (num_jobs_discarded) {
			log().warn("Job system shutdown: {} job(s) discarded", num_jobs_discarded);
		}

		_signal.notify_all();
	}


	manager manager::spawn_workers(u32 count) {
		manager result(std::make_unique<_control_block>());
		result._workers.reserve(count);
		for (u32 i = 0; i < count; ++i) {
			result._workers.emplace_back(result._control->spawn_worker(std::format("Worker Thread {}", i)));
		}
		return result;
	}

	manager::~manager() {
		_control->terminate();
		for (_details::worker_data &worker : _workers) {
			worker.thread.join();
		}
	}

	resource_handle manager::create_resource(const std::type_info &type) {
		return resource_handle(std::make_shared<_details::resource_data>(type));
	}
}
