#pragma once

/// \file
/// Job system.

#include <vector>
#include <queue>
#include <any>
#include <memory>
#include <thread>
#include <atomic>
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
			bool has_producer = false; ///< Whether there is a producer task for this resource.
			bool value_ready = false;

			/// Returns the value cast to the given type. Crashes if it doesn't match.
			template <typename T> T &get() {
				crash_if(type != typeid(T));
				T *ptr = std::any_cast<T>(&value);
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
			/// The job function. Returns whether all outputs are ready.
			static_function<bool(job_data&)> job = nullptr;
			std::vector<resource_ptr> inputs; ///< Job inputs.
			std::vector<resource_ptr> outputs; ///< Job outputs.

			std::optional<u32> count; ///< The number of parallel duplicate instances of this job to run.
			std::atomic<u32> start_count = 0; ///< The number of instances of this job that has started running.
			std::atomic<u32> finish_count = 0; ///< The number of instances of this job that has finished running.

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

		/// Applies the given operation to types in the given tuple.
		template <typename, template <typename> typename> struct apply_to_tuple;
		/// Specialization for functionality.
		template <
			template <typename> typename Func, typename ...Args
		> struct apply_to_tuple<std::tuple<Args...>, Func> {
			using type = std::tuple<Func<Args>...>; ///< Type.
		};
		/// Shorthand for \ref apply_to_tuple::type.
		template <
			typename Tuple, template <typename> typename Func
		> using apply_to_tuple_t = apply_to_tuple<Tuple, Func>::type;

		/// Returns the type \p std::span<const T>.
		template <typename T> struct const_span {
			using type = std::span<const T>; ///< Type.
		};
		/// Shorthand for \ref const_span::type.
		template <typename T> using const_span_t = typename const_span<T>::type;

		/// Helpers for mono jobs.
		namespace mono {
			/// Job function traits.
			template <typename> struct func_traits;
			/// Specialization for function pointers.
			template <typename Output, typename ...Inputs> struct func_traits<Output(*)(Inputs...)> {
				using input_type = std::tuple<Inputs...>; ///< All input types.
				using output_type = Output; ///< Output type.
			};
			/// Shorthand for \ref func_traits::input_type.
			template <typename T> using func_input_type_t = func_traits<T>::input_type;
			/// Shorthand for \ref func_traits::output_type.
			template <typename T> using func_output_type_t = func_traits<T>::output_type;
			/// Job function input count.
			template <typename T> constexpr u32 func_input_count_v = std::tuple_size_v<func_input_type_t<T>>;
			/// Job function output count.
			template <typename T> constexpr u32 func_output_count_v = std::tuple_size_v<func_output_type_t<T>>;

			/// Handler for job arguments.
			template <typename> struct args_handler;
			/// End of recursion.
			template <> struct args_handler<std::tuple<>> {
				/// Resolves the given arguments.
				[[nodiscard]] static std::tuple<> resolve_inputs(std::span<const resource_ptr>) {
					return {};
				}
				/// Does nothing.
				template <typename Tuple> static void apply_outputs(Tuple&&, std::span<const resource_ptr>) {
				}
			};
			/// Specialization for tuples.
			template <typename First, typename ...Rest> struct args_handler<std::tuple<First, Rest...>> {
				/// Resolves the given arguments.
				[[nodiscard]] static std::tuple<First, Rest...> resolve_inputs(std::span<const resource_ptr> args) {
					// TODO use template for
					using type = std::remove_cvref_t<First>;
					return std::tuple_cat(
						std::tuple<First>(args[0]->get<type>()),
						args_handler<std::tuple<Rest...>>::resolve_inputs(args.subspan<1>())
					);
				}
				/// Applies the given outputs.
				template <typename Tuple> static void apply_outputs(Tuple &&out, std::span<const resource_ptr> args) {
					// TODO use template for
					constexpr usize tuple_index = std::tuple_size_v<Tuple> - 1 - sizeof...(Rest);
					args[0]->set<First>(std::get<tuple_index>(std::move(out)));
					args_handler<std::tuple<Rest...>>::apply_outputs(std::move(out), args.subspan<1>());
				}
			};
		}
		/// Helpers for multi jobs.
		namespace multi {
			/// Job function traits.
			template <typename> struct func_traits;
			/// Specialization for function pointers.
			template <typename Output, typename ...Inputs> struct func_traits<Output(*)(u32, Inputs...)> {
				using input_type = std::tuple<Inputs...>; ///< All input types.
				using output_type = Output; ///< Output type.
			};
			/// Shorthand for \ref func_traits::input_type.
			template <typename T> using func_input_type_t = func_traits<T>::input_type;
			/// Shorthand for \ref func_traits::output_type.
			template <typename T> using func_output_type_t = func_traits<T>::output_type;
			/// Job function input count.
			template <typename T> constexpr u32 func_input_count_v = std::tuple_size_v<func_input_type_t<T>>;
			/// Job function output count.
			template <typename T> constexpr u32 func_output_count_v = std::tuple_size_v<func_output_type_t<T>>;

			/// Handler for job arguments.
			template <typename> struct args_handler;
			/// End of recursion.
			template <> struct args_handler<std::tuple<>> {
				/// Does nothing.
				static void prepare_inputs(std::span<const resource_ptr>, u32) {
				}
				/// Returns empty.
				[[nodiscard]] static std::tuple<> get_spans(std::span<const resource_ptr>) {
					return {};
				}
				/// Does nothing.
				template <typename SpanTuple, typename OutputTuple> static void set(
					SpanTuple, OutputTuple&&, u32
				) {
				}
				/// Returns empty.
				template <typename SpanTuple> static std::tuple<> get(SpanTuple, u32) {
					return {};
				}
			};
			/// Specialization for tuples.
			template <typename First, typename ...Rest> struct args_handler<std::tuple<First, Rest...>> {
				/// Prepares inputs.
				static void prepare_inputs(std::span<const resource_ptr> outputs, u32 count) {
					outputs[0]->value.emplace<std::vector<First>>(count);
					args_handler<std::tuple<Rest...>>::prepare_inputs(outputs.subspan<1>(), count);
				}
				/// Resolves the given arguments.
				[[nodiscard]] static std::tuple<std::span<First>, std::span<Rest>...> get_spans(
					std::span<const resource_ptr> args
				) {
					// TODO use template for
					return std::tuple_cat(
						std::tuple<std::span<First>>(args[0]->get<std::vector<First>>()),
						args_handler<std::tuple<Rest...>>::get_spans(args.subspan<1>())
					);
				}
				/// Sets the element at \p index in each span to the corresponding element in \p output.
				template <typename SpanTuple, typename OutputTuple> static void set(
					SpanTuple spans, OutputTuple &&output, u32 index
				) {
					// TODO use template for
					constexpr usize tuple_index = std::tuple_size_v<SpanTuple> - 1 - sizeof...(Rest);
					std::get<tuple_index>(spans)[index] = std::move(std::get<tuple_index>(output));
					args_handler<std::tuple<Rest...>>::set(std::move(output), spans, index);
				}
				/// Returns the element at \p index in each span.
				template <typename SpanTuple> static std::tuple<First, Rest...> get(SpanTuple spans, u32 index) {
					constexpr usize tuple_index = std::tuple_size_v<SpanTuple> - 1 - sizeof...(Rest);
					return std::tuple_cat(
						std::tuple<First>(std::get<tuple_index>(spans)[index]),
						args_handler<std::tuple<Rest...>>::get(spans, index)
					);
				}
			};
		}
	}

	/// Handle used to reference a resource used as input/output of jobs.
	struct resource_handle {
		friend manager;
	public:
		/// Returns whether this handle is valid.
		[[nodiscard]] bool is_valid() const {
			return _resource.get();
		}
		/// \overload
		[[nodiscard]] explicit operator bool() const {
			return is_valid();
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
		constexpr static u32 default_batch_size = 8; ///< Default batch size for multi jobs.

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
			result._resource->has_producer = true;
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
		template <typename JobFunc> void schedule_mono_job(
			JobFunc job_func,
			_details::array_like_tuple_t<_details::mono::func_input_count_v<JobFunc>, resource_handle> inputs,
			_details::array_like_tuple_t<_details::mono::func_output_count_v<JobFunc>, resource_handle> outputs
		) {
			_details::job_ptr job = std::make_unique<_details::job_data>();
			_move_into_vector(inputs, job->inputs);
			_move_into_vector(outputs, job->outputs);
			job->job = [job_func](_details::job_data &job_data) {
				return _mono_job_wrapper(job_func, job_data);
			};
			_control->schedule_job(std::move(job));
		}
		/// Schedules a number of identical independent jobs that can run in parallel. The inputs and outputs must be
		/// \p std::vector types.
		template <u32 BatchSize, typename JobFunc> void schedule_multi_job(
			JobFunc job_func,
			_details::array_like_tuple_t<_details::multi::func_input_count_v<JobFunc>, resource_handle> inputs,
			_details::array_like_tuple_t<_details::multi::func_output_count_v<JobFunc>, resource_handle> outputs,
			u32 count
		) {
			_details::job_ptr job = std::make_unique<_details::job_data>();
			_move_into_vector(inputs, job->inputs);
			_move_into_vector(outputs, job->outputs);
			job->count = count;
			job->job = [job_func](_details::job_data &job_data) {
				return _multi_job_wrapper<BatchSize>(job_func, job_data);
			};
			// TODO do this before running the job to save memory?
			using output_handler = _details::multi::args_handler<_details::multi::func_input_type_t<JobFunc>>;
			output_handler::prepare_inputs(job->outputs, count);
			_control->schedule_job(std::move(job));
		}
		/// Overload of \ref schedule_multi_job() with a default batch size.
		template <typename JobFunc> void schedule_multi_job(
			JobFunc job_func,
			_details::array_like_tuple_t<_details::multi::func_input_count_v<JobFunc>, resource_handle> inputs,
			_details::array_like_tuple_t<_details::multi::func_output_count_v<JobFunc>, resource_handle> outputs,
			u32 count
		) {
			schedule_multi_job<default_batch_size>(job_func, std::move(inputs), std::move(outputs), count);
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

			/// Marks the given resource as ready without notifying \ref _signal. The caller should be holding the
			/// lock.
			void _mark_resource_ready_locked(_details::resource_data*);
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

		/// Wrapper for a mono job function. Handles input and output parameters.
		template <typename JobFunc> static bool _mono_job_wrapper(JobFunc job, _details::job_data &job_data) {
			using input_handler = _details::mono::args_handler<_details::mono::func_input_type_t<JobFunc>>;
			using output_handler = _details::mono::args_handler<_details::mono::func_output_type_t<JobFunc>>;
			output_handler::apply_outputs(
				std::apply(job, input_handler::resolve_inputs(job_data.inputs)),
				job_data.outputs
			);
			return true;
		}

		/// Wrapper for a multi job function. Handles input and output parameters.
		template <u32 BatchSize, typename JobFunc> static bool _multi_job_wrapper(
			JobFunc job, _details::job_data &job_data
		) {
			using func_input_t = _details::multi::func_input_type_t<JobFunc>;
			using func_output_t = _details::multi::func_output_type_t<JobFunc>;
			using input_spans_t = _details::apply_to_tuple_t<func_input_t, _details::const_span_t>;
			using output_spans_t = _details::apply_to_tuple_t<func_output_t, std::span>;
			input_spans_t input_spans = _details::multi::args_handler<func_input_t>::get_spans(job_data.inputs);
			output_spans_t output_spans = _details::multi::args_handler<func_input_t>::get_spans(job_data.outputs);
			constexpr u32 batch_size = BatchSize;

			const u32 count = job_data.count.value();
			bool outputs_ready = false;
			while (true) {
				const u32 start_index = job_data.start_count.fetch_add(batch_size);
				if (start_index >= count) {
					break;
				}
				const u32 cur_batch_size = std::min(batch_size, count - start_index);
				for (u32 i = 0; i < cur_batch_size; ++i) {
					const u32 cur_index = start_index + i;
					_details::multi::args_handler<func_output_t>::set(
						output_spans,
						std::apply(
							[job, cur_index]<typename ...Args>(Args &&...args) {
								return job(cur_index, std::forward<Args>(args)...);
							},
							_details::multi::args_handler<func_input_t>::get(input_spans, cur_index)
						),
						cur_index
					);
				}
				const u32 finish_index = job_data.finish_count.fetch_add(cur_batch_size);
				if (finish_index + cur_batch_size == count) {
					outputs_ready = true;
					break;
				}
			}
			return outputs_ready;
		}
	};
}
