#pragma once

/// \file
/// Job system.

#include <vector>
#include <queue>
#include <any>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "lotus/types.h"
#include "lotus/common.h"
#include "lotus/utils/static_function.h"

namespace lotus::job_system {
	class manager;

	namespace _details {
		struct job_data;

		/// Resource used as input/output for jobs.
		struct resource_data {
			/// Initializes the type of this resource.
			explicit resource_data(const std::type_info &ty) : type(ty) {
			}

			const std::type_info &type; ///< The type of this resource.
			std::any value; ///< The value, or empty if the job hasn't finished.

			// these are protected by the lock
			std::vector<job_data*> consumers; ///< All jobs that depend on this resource.
			bool value_ready = false;

			/// Returns the value cast to the given type. Crashes if it doesn't match.
			template <typename T> const T &get() const {
				crash_if(type != typeid(T));
				const T *ptr = std::any_cast<T>(&value);
				crash_if(!ptr);
				return *ptr;
			}
			/// Sets the value. The \ref value_ready flag is not updated.
			template <typename T> void set(T &&obj) {
				crash_if(type != typeid(T));
				crash_if(value.has_value());
				value.emplace<T>(std::move(obj));
			}
		};
		using resource_ptr = std::shared_ptr<resource_data>; ///< Shared pointer to resource data.

		/// Job data containing the job itself, references to inputs/outputs, and synchronization info.
		struct job_data {
			static_function<void(const job_data&)> job = nullptr; ///< The job function.
			std::vector<resource_ptr> inputs; ///< Job inputs.
			std::vector<resource_ptr> outputs; ///< Job outputs.

			// protected by the lock
			u32 num_pending_inputs = 0; ///< Number of inputs that are not ready yet.
		};
		using job_ptr = std::shared_ptr<job_data>; ///< Shared pointer to job data.

		/// Worker data.
		struct worker_data {
			/// Initializes the thread.
			explicit worker_data(std::thread t) : thread(std::move(t)) {
			}

			std::thread thread; ///< The thread.
		};

		template <typename T, typename Tuple> struct append_tuple;
		/// Utility for appending a type to a tuple.
		template <typename T, typename ...Args> struct append_tuple<T, std::tuple<Args...>> {
			using type = std::tuple<Args..., T>; ///< Type.
		};

		template <usize, typename> struct array_like_tuple;
		/// Termination when \p Count is 0.
		template <typename T> struct array_like_tuple<0, T> {
			using type = std::tuple<>; ///< Type.
		};
		/// Termination when \p Count is 1.
		template <typename T> struct array_like_tuple<1, T> {
			using type = std::tuple<T>; ///< Type.
		};
		/// Constructs a tuple containing \p Count objects of type \p T.
		template <usize Count, typename T> struct array_like_tuple {
			using type = append_tuple<T, typename array_like_tuple<Count - 1, T>::type>::type; ///< Type.
		};
		/// Shorthand for \ref array_like_tuple::type.
		template <usize Count, typename T> using array_like_tuple_t = array_like_tuple<Count, T>::type;
	}

	/// Handle used to reference a resource used as input/output of jobs.
	struct resource_handle {
		friend manager;
	public:
		/// Returns whether this handle is valid.
		[[nodiscard]] bool is_valid() const {
			return _resource.get();
		}
	private:
		_details::resource_ptr _resource; ///< The resource.

		/// Initializes \ref _resource.
		explicit resource_handle(_details::resource_ptr rsrc) : _resource(std::move(rsrc)) {
		}
	};

	/// Manager for a number of job threads.
	class manager {
	public:
		/// Default move constructor.
		manager(manager&&) = default;
		/// Creates a new manager object and spawns the given number of worker threads.
		[[nodiscard]] static manager spawn_workers(u32 count);
		/// Stops all workers.
		~manager();

		/// Creates a resource with the given type.
		[[nodiscard]] resource_handle create_resource(const std::type_info&);
		/// \overload
		template <typename T> [[nodiscard]] resource_handle create_resource() {
			return create_resource(typeid(T));
		}
		/// Creates a new resource and assigns the given value to it.
		template <typename T> [[nodiscard]] resource_handle create_resource_with_value(T obj) {
			resource_handle result = create_resource<T>();
			result._resource->value.emplace<T>(std::move(obj));
			// no need to lock since this resource is just being created
			result._resource->value_ready = true;
			return result;
		}

		/// Retrieves the value of the given resource, blocking if it's not ready yet.
		template <typename T> [[nodiscard]] const T &get_resource_value_blocking(resource_handle h) {
			_control->wait_for_resource(h._resource.get());
			const T *ptr = std::any_cast<T>(&h._resource->value);
			crash_if(!ptr);
			return *ptr;
		}

		/// Schedules a new job. The job will run as soon as all inputs are ready.
		template <typename ...Outputs, typename ...Inputs> void schedule_job(
			std::tuple<Outputs...> (*job_func)(Inputs...),
			_details::array_like_tuple_t<sizeof...(Inputs), resource_handle> inputs,
			_details::array_like_tuple_t<sizeof...(Outputs), resource_handle> outputs
		) {
			_details::job_ptr job = std::make_unique<_details::job_data>();
			_move_into_vector(inputs, job->inputs);
			_move_into_vector(outputs, job->outputs);
			job->job = [job_func](const _details::job_data &job_data) {
				_job_wrapper(job_func, job_data);
			};
			_control->schedule_job(std::move(job));
		}
	private:
		/// Pinned data shared between all threads.
		struct _control_block {
		public:
			/// Worker function.
			void worker_function(std::string thread_name);
			/// Spawns a new worker.
			[[nodiscard]] _details::worker_data spawn_worker(std::string thread_name);

			/// Schedules the given job.
			void schedule_job(_details::job_ptr);

			/// Waits for the give resource to be computed.
			void wait_for_resource(const _details::resource_data*);

			/// Signals all threads to terminate.
			void terminate();
		private:
			std::mutex _job_lock; ///< Lock for modifying jobs.
			std::condition_variable _signal; ///< Condition variable used to signal the worker threads.
			// protected by the lock
			std::vector<_details::job_ptr> _waiting_jobs; ///< Jobs that have inputs that are not ready.
			std::queue<_details::job_ptr> _pending_jobs; ///< Jobs that can run next.
			bool _terminate = false; ///< Whether or not to terminate.
		};

		std::vector<_details::worker_data> _workers; ///< Worker threads.
		std::unique_ptr<_control_block> _control; ///< Control block.

		/// Initializes the control block.
		explicit manager(std::unique_ptr<_control_block> ctrl) : _control(std::move(ctrl)) {
		}


		/// Moves resource handles from a \p std::tuple into the given \p std::vector.
		template <typename Tuple, usize Index = 0> static void _move_into_vector(
			Tuple &tuple, std::vector<_details::resource_ptr> &vec
		) {
			if constexpr (Index == 0) {
				vec.reserve(std::tuple_size_v<Tuple>);
			}
			// TODO use template for
			if constexpr (Index < std::tuple_size_v<Tuple>) {
				vec.emplace_back(std::move(std::get<Index>(tuple)._resource));
				_move_into_vector<Tuple, Index + 1>(tuple, vec);
			}
		}
		/// Retrieves a tuple of all job inputs.
		template <typename First, typename ...Rest> [[nodiscard]] static std::tuple<First, Rest...> _resolve_inputs(
			std::span<const _details::resource_ptr> args
		) {
			// TODO use template for
			if constexpr (sizeof...(Rest) > 0) {
				return std::tuple_cat(
					std::tuple<First>(args[0]->get<First>()),
					_resolve_inputs<Rest...>(args.subspan<1>())
				);
			} else {
				return std::tuple<First>(args[0]->get<First>());
			}
		}
		/// Copies outputs into all resources.
		template <typename First, typename ...Rest, typename Tuple> static void _apply_outputs(
			Tuple &&out, std::span<const _details::resource_ptr> args
		) {
			// TODO use template for
			constexpr usize tuple_index = std::tuple_size_v<Tuple> - 1 - sizeof...(Rest);
			args[0]->set<First>(std::get<tuple_index>(std::move(out)));
			if constexpr (sizeof...(Rest) > 0) {
				_apply_outputs<Tuple, Rest...>(std::move(out), args.subspan<1>());
			}
		}
		/// Wrapper for a job function. Handles input and output parameters.
		template <typename ...Outputs, typename ...Inputs> static void _job_wrapper(
			std::tuple<Outputs...> (*job)(Inputs...), const _details::job_data &job_data
		) {
			_apply_outputs<Outputs...>(
				std::apply(job, _resolve_inputs<Inputs...>(job_data.inputs)),
				job_data.outputs
			);
		}
	};
}
