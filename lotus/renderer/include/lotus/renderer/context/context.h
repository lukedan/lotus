#pragma once

/// \file
/// Scene-related classes.

#include <vector>
#include <unordered_map>

#include "lotus/math/matrix.h"
#include "lotus/system/window.h"
#include "lotus/gpu/context.h"
#include "resources.h"
#include "resource_bindings.h"
#include "assets.h"
#include "commands.h"
#include "execution/caching.h"
#include "execution/batch_context.h"

namespace lotus::renderer {
	/// Cached material and pass related instance data.
	struct instance_render_details {
		/// Initializes this object to empty.
		instance_render_details(std::nullptr_t) : vertex_shader(nullptr), pixel_shader(nullptr), pipeline(nullptr) {
		}

		std::vector<input_buffer_binding> input_buffers; ///< Input buffer bindings.
		assets::handle<assets::shader> vertex_shader; ///< Vertex shader.
		assets::handle<assets::shader> pixel_shader; ///< Pixel shader.
		graphics_pipeline_state pipeline; ///< Pipeline state.
	};

	/// Provides information about a pass's shader and input buffer layout.
	class pass_context {
	public:
		/// Default virtual destructor.
		virtual ~pass_context() = default;

		/// Computes derived render data for an instance.
		[[nodiscard]] virtual instance_render_details get_render_details(
			context&, const assets::material::context_data&, const assets::geometry&
		) = 0;
	};

	/// Contains data about a staging buffer.
	struct staging_buffer {
		/// Initializes all fields of this struct.
		staging_buffer(buffer d, gpu::staging_buffer::metadata m, std::size_t s) :
			data(std::move(d)), meta(m), total_size(s) {
		}

		buffer data; ///< The buffer.
		gpu::staging_buffer::metadata meta; ///< Metadata.
		std::size_t total_size; ///< Total size of \ref data.
	};


	namespace _details {
		/// Data about a command queue.
		struct queue_data {
		public:
			/// Initializes all fields of this struct.
			queue_data(context &c, gpu::command_queue q, gpu::timeline_semaphore sem) :
				queue(std::move(q)), semaphore(std::move(sem)), ctx(c) {
			}

			gpu::command_queue queue; ///< The queue.
			gpu::timeline_semaphore semaphore; ///< A semaphore used for synchronization.
			gpu::timeline_semaphore::value_type semaphore_value = 0; ///< Current value of the timeline semaphore.
			context &ctx; ///< The context that owns this queue.

			// per-batch data
			std::vector<command> batch_commands; ///< Recorded commands.
			std::uint32_t num_timers = 0; ///< Number of registered timers so far.
			bool within_pass = false; ///< Whether this queue is currently recording pass commands.


			/// Resets batch-specific data.
			void reset_batch() {
				batch_commands.clear();
				num_timers = 0;
				within_pass = false;
			}

			/// Adds a command. Checks if we're currently within a pass.
			template <typename Cmd, typename ...Args> void add_command(std::u8string_view, Args&&...);
		private:
			/// Performs checks before a command is added to this queue.
			template <typename Cmd> void _check_command() {
				if constexpr (bit_mask::contains<commands::flags::pass_command>(Cmd::get_flags())) {
					crash_if(!within_pass);
				}
				if constexpr (bit_mask::contains<commands::flags::non_pass_command>(Cmd::get_flags())) {
					crash_if(within_pass);
				}
			}
		};
	}


	/// Keeps track of the rendering of a frame, including resources used for rendering.
	class context {
		friend _details::image2d;
		friend _details::context_managed_deleter;
		friend _details::queue_data;
		friend execution::queue_context;
		friend execution::queue_pseudo_context;
		friend execution::batch_context;
		friend execution::descriptor_set_builder;
	public:
		/// Helper class used to retrieve the device associated with a \ref context.
		struct device_access {
			friend assets::manager;
		protected:
			/// Retrieves the device associated with the given \ref context.
			[[nodiscard]] static gpu::device &get(context&);
		};

		/// Object whose lifetime marks the duration of a timer.
		class timer {
			friend context;
		public:
			/// Initializes this object to empty.
			timer(std::nullptr_t) {
			}
			/// Move construction.
			timer(timer &&src) noexcept : _q(std::exchange(src._q, nullptr)), _index(src._index) {
			}
			/// No copy construction.
			timer(const timer&) = delete;
			/// Move assignment.
			timer &operator=(timer &&src) noexcept {
				if (&src != this) {
					end();
					_q = std::exchange(src._q, nullptr);
					_index = src._index;
				}
				return *this;
			}
			/// No copy assignment.
			timer &operator=(const timer&) = delete;
			/// Ends the timer.
			~timer() {
				end();
			}

			/// Ends this timer if it's ongoing.
			void end() {
				if (_q) {
					_q->add_command<commands::end_timer>(u8"End Timer", _index);
					_q = nullptr;
				}
			}

			/// Returns whether this object is valid.
			[[nodiscard]] bool is_valid() const {
				return _q != nullptr;
			}
		private:
			/// Initializes all fields of this struct.
			timer(_details::queue_data &q, commands::timer_index i) : _q(&q), _index(i) {
			}

			_details::queue_data *_q = nullptr; ///< Associated command queue.
			commands::timer_index _index = commands::timer_index::invalid; ///< Index of the timer.
		};
		/// A pass being rendered.
		class pass {
			friend context;
		public:
			/// No move construction.
			pass(pass&&) = delete;
			/// No copy construction.
			pass(const pass&) = delete;
			/// Automatically ends the pass.
			~pass() {
				end();
			}

			/// Draws a number of instances with the given inputs.
			void draw_instanced(
				std::vector<input_buffer_binding>,
				std::uint32_t num_verts,
				index_buffer_binding,
				std::uint32_t num_indices,
				gpu::primitive_topology,
				all_resource_bindings,
				assets::handle<assets::shader> vs,
				assets::handle<assets::shader> ps,
				graphics_pipeline_state,
				std::uint32_t num_insts,
				std::u8string_view description
			);
			/// \overload
			void draw_instanced(
				assets::handle<assets::geometry>,
				assets::handle<assets::material>,
				pass_context&,
				std::span<const input_buffer_binding> additional_inputs,
				all_resource_bindings additional_resources,
				constant_uploader&,
				std::uint32_t num_insts,
				std::u8string_view description
			);
			/// \overload
			void draw_instanced(
				assets::handle<assets::geometry>,
				assets::handle<assets::material>,
				const instance_render_details&,
				std::span<const input_buffer_binding> additional_inputs,
				all_resource_bindings additional_resources,
				constant_uploader&,
				std::uint32_t num_insts,
				std::u8string_view description
			);

			/// Finishes rendering to the pass and records all commands into the context.
			void end();
		private:
			_details::queue_data *_q = nullptr; ///< The queue.

			/// Initializes the pass.
			explicit pass(_details::queue_data &q) : _q(&q) {
			}
		};
		/// A handle of a command queue.
		class queue {
			friend context;
		public:
			/// Initializes this handle to empty.
			queue(std::nullptr_t) {
			}

			/// Copies data from the first buffer to the second.
			void copy_buffer(
				const buffer &source, const buffer &target,
				std::uint32_t src_offset, std::uint32_t dst_offset, std::uint32_t sz,
				std::u8string_view description
			);
			/// Copies data from the buffer to the image.
			void copy_buffer_to_image(
				const buffer &source, const image2d_view &target,
				gpu::staging_buffer::metadata, std::uint32_t src_offset, cvec2u32 dst_offset,
				std::u8string_view description
			);
			/// \overload
			void copy_buffer_to_image(
				const staging_buffer &source, const image2d_view &target,
				std::uint32_t src_offset, cvec2u32 dst_offset,
				std::u8string_view description
			);

			/// Builds the given \ref blas.
			void build_blas(blas&, std::span<const geometry_buffers_view>, std::u8string_view description);
			/// \overload
			void build_blas(
				blas &b, std::initializer_list<const geometry_buffers_view> geoms, std::u8string_view description
			) {
				build_blas(b, { geoms.begin(), geoms.end() }, description);
			}
			/// Builds the given \ref tlas.
			void build_tlas(tlas&, std::span<const blas_instance>, std::u8string_view description);
			/// \overload
			void build_tlas(
				tlas &t, std::initializer_list<const blas_instance> instances, std::u8string_view description
			) {
				build_tlas(t, { instances.begin(), instances.end() }, description);
			}

			/// Generates and traces rays.
			void trace_rays(
				std::span<const shader_function> hit_group_shaders,
				std::span<const gpu::hit_shader_group>,
				std::span<const shader_function> general_shaders,
				std::uint32_t raygen_shader_index,
				std::span<const std::uint32_t> miss_shader_indices,
				std::span<const std::uint32_t> shader_groups,
				std::uint32_t max_recursion_depth,
				std::uint32_t max_payload_size,
				std::uint32_t max_attribute_size,
				cvec3u32 num_threads,
				all_resource_bindings,
				std::u8string_view description
			);
			/// \overload
			void trace_rays(
				std::initializer_list<const shader_function> hit_group_shaders,
				std::initializer_list<const gpu::hit_shader_group> hit_groups,
				std::initializer_list<const shader_function> general_shaders,
				std::uint32_t raygen_shader_index,
				std::initializer_list<const std::uint32_t> miss_shader_indices,
				std::initializer_list<const std::uint32_t> shader_groups,
				std::uint32_t max_recursion_depth,
				std::uint32_t max_payload_size,
				std::uint32_t max_attribute_size,
				cvec3u32 num_threads,
				all_resource_bindings resources,
				std::u8string_view description
			) {
				trace_rays(
					{ hit_group_shaders.begin(), hit_group_shaders.end() },
					{ hit_groups.begin(), hit_groups.end() },
					{ general_shaders.begin(), general_shaders.end() },
					raygen_shader_index,
					{ miss_shader_indices.begin(), miss_shader_indices.end() },
					{ shader_groups.begin(), shader_groups.end() },
					max_recursion_depth,
					max_payload_size,
					max_attribute_size,
					num_threads,
					std::move(resources),
					description
				);
			}

			/// Releases a dependency.
			void release_dependency(dependency, std::u8string_view description);
			/// Acquires a dependency.
			void acquire_dependency(dependency, std::u8string_view description);

			/// Runs a compute shader.
			void run_compute_shader(
				assets::handle<assets::shader>, cvec3<std::uint32_t> num_thread_groups, all_resource_bindings,
				std::u8string_view description
			);
			/// Runs a compute shader with the given number of threads. Asserts if the number of threads is not
			/// divisible by the shader's thread group size.
			void run_compute_shader_with_thread_dimensions(
				assets::handle<assets::shader>, cvec3<std::uint32_t> num_threads, all_resource_bindings,
				std::u8string_view description
			);

			/// Starts rendering to the given surfaces. No other operations can be performed until the pass finishes.
			[[nodiscard]] pass begin_pass(
				std::vector<image2d_color>, image2d_depth_stencil, cvec2u32 sz, std::u8string_view description
			);

			/// Presents the given swap chain.
			void present(swap_chain, std::u8string_view description);


			/// Starts a new timer.
			[[nodiscard]] timer start_timer(std::u8string);

			/// Pauses command processing.
			void pause_for_debugging(std::u8string_view description);


			/// Returns whether this a valid handle.
			[[nodiscard]] bool is_valid() const {
				return _q;
			}
			/// \overload
			[[nodiscard]] explicit operator bool() const {
				return is_valid();
			}
		private:
			/// Initializes this handle.
			explicit queue(_details::queue_data &q) : _q(&q) {
			}

			_details::queue_data *_q = nullptr; ///< The queue.
		};

		/// Creates a new context object.
		[[nodiscard]] static context create(
			gpu::context&, const gpu::adapter_properties&, gpu::device&, std::span<const gpu::command_queue>
		);
		/// No move constructor.
		context(context&&) = delete;
		/// Disposes of all resources.
		~context();

		/// Returns the number of queues.
		[[nodiscard]] std::uint32_t get_num_queues() const {
			return static_cast<std::uint32_t>(_queues.size());
		}
		/// Returns the command queue at the given index.
		[[nodiscard]] queue get_queue(std::uint32_t index) {
			return queue(_queues[index]);
		}

		/// Creates a new memory pool. If no valid memory type index is specified, the pool is created for device
		/// memory by default.
		[[nodiscard]] pool request_pool(
			std::u8string_view name,
			gpu::memory_type_index = gpu::memory_type_index::invalid,
			std::uint32_t chunk_size = pool::default_chunk_size
		);
		/// Creates a 2D image with the given properties.
		[[nodiscard]] image2d_view request_image2d(
			std::u8string_view name, cvec2u32 size, std::uint32_t num_mips, gpu::format, gpu::image_usage_mask,
			const pool&
		);
		/// Creates a 3D image with the given properties.
		[[nodiscard]] image3d_view request_image3d(
			std::u8string_view name, cvec3u32 size, std::uint32_t num_mips, gpu::format, gpu::image_usage_mask,
			const pool&
		);
		/// Creates a buffer with the given size.
		[[nodiscard]] buffer request_buffer(
			std::u8string_view name, std::uint32_t size_bytes, gpu::buffer_usage_mask, const pool&
		);
		/// Shorthand for \ref request_buffer() and then viewing it as a structured buffer of the given type.
		template <typename T> [[nodiscard]] structured_buffer_view request_structured_buffer(
			std::u8string_view name, std::uint32_t num_elements, gpu::buffer_usage_mask usages, const pool &p
		) {
			return request_buffer(name, num_elements * sizeof(T), usages, p).get_view<T>(0, num_elements);
		}
		/// Requests a staging buffer for an image with the given size and format.
		[[nodiscard]] staging_buffer request_staging_buffer(std::u8string_view name, cvec2u32 size, gpu::format);
		/// Requests a staging buffer for the entire given subresource of the given image.
		[[nodiscard]] staging_buffer request_staging_buffer_for(std::u8string_view name, const image2d_view&);
		/// Creates a swap chain with the given properties.
		[[nodiscard]] swap_chain request_swap_chain(
			std::u8string_view name, system::window&, queue&,
			std::uint32_t num_images, std::span<const gpu::format> formats
		);
		/// \overload
		[[nodiscard]] swap_chain request_swap_chain(
			std::u8string_view name, system::window &wnd, queue &q,
			std::uint32_t num_images, std::initializer_list<gpu::format> formats
		) {
			return request_swap_chain(name, wnd, q, num_images, std::span{ formats.begin(), formats.end() });
		}
		/// Creates a descriptor array with the given properties.
		[[nodiscard]] image_descriptor_array request_image_descriptor_array(
			std::u8string_view name, gpu::descriptor_type, std::uint32_t capacity
		);
		/// Creates a descriptor array with the given properties.
		[[nodiscard]] buffer_descriptor_array request_buffer_descriptor_array(
			std::u8string_view name, gpu::descriptor_type, std::uint32_t capacity
		);
		/// Creates a bottom-level acceleration structure for the given input geometry.
		[[nodiscard]] blas request_blas(
			std::u8string_view name, const pool&
		);
		/// Creates a top-level acceleration structure for the given input instances.
		[[nodiscard]] tlas request_tlas(std::u8string_view name, const pool&);
		/// Creates a cached descriptor set.
		[[nodiscard]] cached_descriptor_set request_cached_descriptor_set(
			std::u8string_view name, std::span<const all_resource_bindings::numbered_binding>
		);
		/// \overload
		[[nodiscard]] cached_descriptor_set request_cached_descriptor_set(
			std::u8string_view name, std::initializer_list<all_resource_bindings::numbered_binding> bindings
		) {
			return request_cached_descriptor_set(name, { bindings.begin(), bindings.end() });
		}
		/// Creates a dependency object.
		[[nodiscard]] dependency request_dependency(std::u8string_view name);


		/// Analyzes and executes all recorded commands.
		std::vector<batch_statistics_early> execute_all();
		/// Waits until all previous batches have finished executing.
		void wait_idle();


		/// Maps the given buffer for reading and/or writing. Nested \ref map_buffer() and \ref unmap_buffer() calls
		/// are supported.
		[[nodiscard]] std::byte *map_buffer(buffer&);
		/// Unmaps the given buffer. Nested \ref map_buffer() and \ref unmap_buffer() calls are supported.
		void unmap_buffer(buffer&);
		/// Flushes the given memory range that has been written to on the host so that it is visible to the device.
		void flush_mapped_buffer_to_device(buffer&, std::size_t begin, std::size_t length);
		/// Flushes the given memory range that has been written to on the device so that it is visible on the host.
		void flush_mapped_buffer_to_host(buffer&, std::size_t beg, std::size_t length);
		/// Convenience function for mapping the buffer, writing to the buffer, flushing the buffer, and unmapping
		/// it.
		template <typename Cb> void write_data_to_buffer_custom(buffer &buf, Cb &&write_data) {
			auto *ptr = map_buffer(buf);
			write_data(ptr);
			flush_mapped_buffer_to_device(buf, 0, buf.get_size_in_bytes());
			unmap_buffer(buf);
		}
		/// Copies data into the given buffer by calling \ref write_data_to_buffer_custom().
		void write_data_to_buffer(buffer&, std::span<const std::byte>);
		/// Pads and copies tightly-packed pixel data into the given buffer by calling
		/// \ref write_data_to_buffer_custom().
		void write_image_data_to_buffer_tight(
			buffer&, const gpu::staging_buffer::metadata&, std::span<const std::byte> data
		);


		/// Writes the given images into the given descriptor array.
		void write_image_descriptors(
			image_descriptor_array&, std::uint32_t first_index, std::span<const image2d_view>
		);
		/// \overload
		void write_image_descriptors(
			image_descriptor_array &arr, std::uint32_t first_index, std::initializer_list<image2d_view> imgs
		) {
			write_image_descriptors(arr, first_index, { imgs.begin(), imgs.end() });
		}
		/// Writes the given buffers into the given descriptor array.
		void write_buffer_descriptors(
			buffer_descriptor_array&, std::uint32_t first_index, std::span<const structured_buffer_view>
		);
		/// \overload
		void write_buffer_descriptors(
			buffer_descriptor_array &arr, std::uint32_t first_index,
			std::initializer_list<structured_buffer_view> bufs
		) {
			write_buffer_descriptors(arr, first_index, { bufs.begin(), bufs.end() });
		}


		/// Returns the memory type index for memory used for uploading data to the GPU.
		[[nodiscard]] gpu::memory_type_index get_upload_memory_type_index() const {
			return _upload_memory_index;
		}
		/// Returns the memory type index for memory located on the GPU.
		[[nodiscard]] gpu::memory_type_index get_device_memory_type_index() const {
			return _device_memory_index;
		}
		/// Returns the memory type index for reading data back from the GPU.
		[[nodiscard]] gpu::memory_type_index get_readback_memory_type_index() const {
			return _readback_memory_index;
		}
		/// Returns the properties of the current adapter.
		[[nodiscard]] const gpu::adapter_properties &get_adapter_properties() const {
			return _adapter_properties;
		}

		/// Callback function for when statistics for a new batch is available.
		static_function<void(batch_index, batch_statistics_late)> on_batch_statistics_available = nullptr;

		/// Convenience function for printing execution log.
		template <typename ...Args> void execution_log(std::format_string<Args...> str, Args &&...args) {
			if (on_execution_log) {
				const std::string formatted = std::vformat(str.get(), std::make_format_args(args...));
				on_execution_log(string::assume_utf8(formatted));
			}
		}
		/// Callback function for logging execution debugging information.
		static_function<void(std::u8string_view)> on_execution_log = nullptr;
	private:
		gpu::context &_context; ///< Associated graphics context.
		gpu::device &_device;   ///< Associated device.
		std::vector<_details::queue_data> _queues; ///< Command queues.

		gpu::descriptor_pool _descriptor_pool; ///< Descriptor pool to allocate descriptors out of.

		gpu::adapter_properties _adapter_properties; ///< Adapter properties.

		/// Index of a memory type suitable for uploading to the device.
		gpu::memory_type_index _upload_memory_index = gpu::memory_type_index::invalid;
		/// Index of the memory type best for resources that are resident on the device.
		gpu::memory_type_index _device_memory_index = gpu::memory_type_index::invalid;
		/// Index of a memory type suitable for reading data back from the device.
		gpu::memory_type_index _readback_memory_index = gpu::memory_type_index::invalid;

		execution::context_cache _cache; ///< Cached objects.

		std::thread::id _thread; ///< \ref execute_all() can only be called from the thread that created this object.

		/// Data associated with all previous batches that have not finished execution.
		std::deque<execution::batch_data> _batch_data;
		execution::batch_resources _deferred_delete_resources; ///< Resources that are marked for deferred deletion.
		/// Index of the first command of this batch.
		global_submission_index _first_batch_command_index = global_submission_index::zero;

		/// Counter used to uniquely identify resources.
		unique_resource_id _resource_index = unique_resource_id::invalid;
		/// Submission order of all commands.
		global_submission_index _sub_index = static_cast<global_submission_index>(1);
		batch_index _batch_index = static_cast<batch_index>(1); ///< Index of the last batch that has been executed.


		/// Initializes all fields of the context.
		context(
			gpu::context&, const gpu::adapter_properties&, gpu::device&, std::span<const gpu::command_queue>
		);

		/// Allocates a unique resource index.
		[[nodiscard]] unique_resource_id _allocate_resource_id() {
			_resource_index = index::next(_resource_index);
			return _resource_index;
		}
		/// Increments submission index by 1 and returns its value.
		[[nodiscard]] global_submission_index _take_submission_index() {
			const auto result = _sub_index;
			_sub_index = index::next(_sub_index);
			return result;
		}


		/// Requests a buffer.
		[[nodiscard]] std::shared_ptr<_details::buffer> _request_buffer_raw(
			std::u8string_view name,
			std::uint32_t size_bytes,
			gpu::buffer_usage_mask,
			const std::shared_ptr<_details::pool>&
		);


		/// Allocates and creates the backing image for the image resource.
		void _maybe_initialize_image(_details::image2d&);
		/// Allocates and creates the backing image for the image resource.
		void _maybe_initialize_image(_details::image3d&);
		/// Allocates and creates the backing buffer for the buffer resource.
		void _maybe_initialize_buffer(_details::buffer&);
		/// Initializes the given \ref _details::descriptor_array if necessary.
		template <typename RecordedResource, typename View> void _maybe_initialize_descriptor_array(
			_details::descriptor_array<RecordedResource, View> &arr
		) {
			if (!arr.set) {
				arr.layout = &_cache.get_descriptor_set_layout(
					execution::cache_keys::descriptor_set_layout::for_descriptor_array(arr.type)
				);
				arr.set = _device.create_descriptor_set(_descriptor_pool, *arr.layout, arr.capacity);
			}
		}
		/// Initializes the given \ref _details::cached_descriptor_set if necessary.
		void _maybe_initialize_cached_descriptor_set(_details::cached_descriptor_set&);

		/// Adds a 2D image to the given cached descriptor binding.
		void _add_cached_descriptor_binding(
			_details::cached_descriptor_set&, const descriptor_resource::image2d&, std::uint32_t idx
		);
		/// Adds a 3D image to the given cached descriptor binding.
		void _add_cached_descriptor_binding(
			_details::cached_descriptor_set&, const descriptor_resource::image3d&, std::uint32_t idx
		);
		/// Adds a swap chain to the given cached descriptor binding.
		void _add_cached_descriptor_binding(
			_details::cached_descriptor_set&, const descriptor_resource::swap_chain&, std::uint32_t idx
		);
		/// Adds a constant buffer to the given cached descriptor binding.
		void _add_cached_descriptor_binding(
			_details::cached_descriptor_set&, const descriptor_resource::constant_buffer&, std::uint32_t idx
		);
		/// Adds a structured buffer to the given cached descriptor binding.
		void _add_cached_descriptor_binding(
			_details::cached_descriptor_set&, const descriptor_resource::structured_buffer&, std::uint32_t idx
		);
		/// Adds a TLAS to the given cached descriptor binding.
		void _add_cached_descriptor_binding(
			_details::cached_descriptor_set&, const recorded_resources::tlas&, std::uint32_t idx
		);
		/// Adds a sampler to the given cached descriptor binding.
		void _add_cached_descriptor_binding(
			_details::cached_descriptor_set&, const sampler_state&, std::uint32_t idx
		);

		/// Flushes all writes to the given image descriptor array.
		void _flush_descriptor_array_writes(_details::image_descriptor_array&);
		/// Flushes all writes to the given buffer descriptor array.
		void _flush_descriptor_array_writes(_details::buffer_descriptor_array&);

		/// Creates an \ref gpu::image2d_view, without recording it anywhere, and returns the object itself.
		/// This function is used when we need to keep the view between render commands and flushes. This function
		/// assumes that the image has been fully initialized.
		[[nodiscard]] gpu::image2d_view _create_image_view(const _details::image2d&, gpu::format, gpu::mip_levels);
		/// \overload
		[[nodiscard]] gpu::image2d_view _create_image_view(const recorded_resources::image2d_view&);
		/// Creates an \ref gpu::image3d_view, without recording it anywhere, and returns the object itself.
		/// This function is used when we need to keep the view between render commands and flushes. This function
		/// assumes that the image has been fully initialized.
		[[nodiscard]] gpu::image3d_view _create_image_view(const _details::image3d&, gpu::format, gpu::mip_levels);
		/// \overload
		[[nodiscard]] gpu::image3d_view _create_image_view(const recorded_resources::image3d_view&);
		/// Creates or finds a \ref gpu::image2d_view from the given \ref renderer::image2d_view, and records it in
		/// the current \ref execution::batch_resources. This function assumes that the image has been fully
		/// initialized.
		[[nodiscard]] gpu::image2d_view &_request_image_view(const recorded_resources::image2d_view&);
		/// Creates or finds a \ref gpu::image3d_view from the given \ref renderer::image3d_view, and records it in
		/// the current \ref execution::batch_resources. This function assumes that the image has been fully
		/// initialized.
		[[nodiscard]] gpu::image3d_view &_request_image_view(const recorded_resources::image3d_view&);
		/// Creates or finds a \ref gpu::image2d_view from the next image in the given \ref swap_chain, and records
		/// it in the current \ref execution::batch_resources. This function assumes that the image has been fully
		/// initialized.
		[[nodiscard]] gpu::image2d_view &_request_swap_chain_view(const recorded_resources::swap_chain&);

		/// Prepares the given swap chain to be used in a new batch. This function can perform any combination of the
		/// following operations when necessary: Acquiring the next buffer of the swap chain, resizing the swap
		/// chain, and recreating the swap chain. This should only be called during pseudo-execution because it may
		/// wait for all GPU work to be finished.
		void _maybe_update_swap_chain(_details::swap_chain&);

		/// Writes one descriptor array element into the given array.
		template <typename RecordedResource, typename View> void _write_one_descriptor_array_element(
			_details::descriptor_array<RecordedResource, View> &arr, RecordedResource rsrc, std::uint32_t index
		) {
			auto &cur_ref = arr.resources[index];
			// unlink current reference
			if (auto *surf = cur_ref.resource._ptr) {
				// remove reference from image
				auto old_index = cur_ref.reference_index;
				surf->array_references[old_index] = surf->array_references.back();
				surf->array_references.pop_back();
				// update the affected reference - only needs to be done if there is one
				if (old_index < surf->array_references.size()) {
					auto new_ref = surf->array_references[old_index];
					new_ref.array->resources[new_ref.index].reference_index = old_index;
				}
				// record the view for disposal
				if constexpr (!std::is_same_v<View, std::nullopt_t>) {
					if (cur_ref.view.value && !_batch_data.empty()) {
						// this actually belongs to the previous batch
						_batch_data.back().resources.record(std::move(cur_ref.view.value));
					}
				}
				// remove reference from descriptor array
				cur_ref = nullptr;
				arr.has_descriptor_overwrites = true;
			}
			// update recorded image
			cur_ref.resource = rsrc;
			if (cur_ref.resource._ptr) {
				auto &new_surf = *cur_ref.resource._ptr;
				cur_ref.reference_index = static_cast<std::uint32_t>(new_surf.array_references.size());
				auto &new_ref = new_surf.array_references.emplace_back(nullptr);
				new_ref.array = &arr;
				new_ref.index = index;
			}
			// stage the write
			arr.staged_transitions.emplace_back(index);
			arr.staged_writes.emplace_back(index);
		}


		/// Cleans up all unused resources, and updates timestamp information to latest.
		void _cleanup(std::size_t keep_batches);


		// resource deletion handlers
		/// Interface to \ref _details::context_managed_deleter for deferring deletion of a pool.
		void _deferred_delete(_details::pool*) {
			// TODO
		}
		/// Interface to \ref _details::context_managed_deleter for deferring deletion of a 2D image.
		void _deferred_delete(_details::image2d *surf) {
			_deferred_delete_resources.image2d_meta.emplace_back(surf);
		}
		/// Interface to \ref _details::context_managed_deleter for deferring deletion of a 3D image.
		void _deferred_delete(_details::image3d *surf) {
			_deferred_delete_resources.image3d_meta.emplace_back(surf);
		}
		/// Interface to \ref _details::context_managed_deleter for deferring deletion of a buffer.
		void _deferred_delete(_details::buffer *buf) {
			_deferred_delete_resources.buffer_meta.emplace_back(buf);
		}
		/// Interface to \ref _details::context_managed_deleter for deferring deletion of a swap chain.
		void _deferred_delete(_details::swap_chain *chain) {
			_deferred_delete_resources.swap_chain_meta.emplace_back(chain);
		}
		/// Interface to \ref _details::context_managed_deleter for deferring deletion of a descriptor array.
		void _deferred_delete(_details::image_descriptor_array*) {
			// TODO
		}
		/// Interface to \ref _details::context_managed_deleter for deferring deletion of a descriptor array.
		void _deferred_delete(_details::buffer_descriptor_array*) {
			// TODO
		}
		/// Interface to \ref _details::context_managed_deleter for deferring deletion of a BLAS.
		void _deferred_delete(_details::blas*) {
			// TODO
		}
		/// Interface to \ref _details::context_managed_deleter for deferring deletion of a TLAS.
		void _deferred_delete(_details::tlas*) {
			// TODO
		}
		/// Interface to \ref _details::context_managed_deleter for deferring deletion of a dependency.
		void _deferred_delete(_details::dependency*) {
			// TODO
		}
		/// Interface to \ref _details::context_managed_deleter for deferring deletion of a cached descriptor set.
		void _deferred_delete(_details::cached_descriptor_set*) {
			// TODO
		}
	};

	/// An instance in a scene.
	struct instance {
		/// Initializes this instance to empty.
		instance(std::nullptr_t) :
			material(nullptr), geometry(nullptr), transform(uninitialized), prev_transform(uninitialized) {
		}

		assets::handle<assets::material> material; ///< The material of this instance.
		assets::handle<assets::geometry> geometry; ///< Geometry of this instance.
		mat44f transform; ///< Transform of this instance.
		mat44f prev_transform; ///< Transform of this instance for the previous frame.
	};


	namespace _details {
		template <typename Cmd, typename ...Args> void queue_data::add_command(
			std::u8string_view description, Args &&...args
		) {
			_check_command<Cmd>();
			batch_commands.emplace_back(
				description, ctx._take_submission_index()
			).value.emplace<Cmd>(std::forward<Args>(args)...);
		}
	}


	namespace execution {
		template <typename T> T &batch_context::record_batch_resource(T rsrc) {
			return _rctx._batch_data.back().resources.record<T>(std::move(rsrc));
		}
	}


	template <typename Type> void _details::context_managed_deleter::operator()(Type *ptr) const {
		_ctx->_deferred_delete(ptr);
	}
}
