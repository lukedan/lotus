#pragma once

/// \file
/// Scene-related classes.

#include <vector>
#include <unordered_map>

#include "lotus/math/matrix.h"
#include "lotus/system/window.h"
#include "lotus/gpu/context.h"
#include "caching.h"
#include "resources.h"
#include "resource_bindings.h"
#include "assets.h"
#include "execution.h"
#include "commands.h"

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


	namespace _details {
		/// Data about a command queue.
		struct queue_data {
		public:
			/// Initializes all fields of this struct.
			queue_data(context &c, gpu::command_queue q, gpu::timeline_semaphore sem) :
				queue(std::move(q)), batch_semaphore(std::move(sem)), ctx(c), uploads(nullptr) {
			}

			gpu::command_queue queue; ///< The queue.
			gpu::timeline_semaphore batch_semaphore; ///< A semaphore that is signaled after each batch.
			context &ctx; ///< The context that owns this queue.

			// per-batch data
			std::vector<command> batch_commands; ///< Recorded commands.
			std::uint32_t num_timers = 0; ///< Number of registered timers so far.
			bool within_pass = false; ///< Whether this queue is currently recording pass commands.
			// TODO: get rid of this
			upload_buffers uploads; ///< Upload buffers.


			/// Resets batch-specific data.
			void reset_batch() {
				batch_commands.clear();
				num_timers = 0;
				within_pass = false;
				uploads = nullptr;
			}

			/// Adds a command. Does not care if we're currently within a pass.
			template <typename Cmd, typename ...Args> void add_command(
				static_optional<std::u8string, should_register_debug_names> description, Args &&...args
			) {
				_check_command<Cmd>();
				batch_commands.emplace_back(std::move(description)).value.emplace<Cmd>(std::forward<Args>(args)...);
			}
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
		friend execution::context;
	public:
		/// Helper class used to retrieve the device associated with a \ref context.
		struct device_access {
			friend assets::manager;
			friend upload_buffers;
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

			/// Uploads image data to the GPU. This function immediately creates and fills the staging buffer, but actual
			/// image uploading is deferred. The pixels format of the input data is assumed to be the same as the
			/// image (i.e. direct memcpy can be used), and the rows are assumed to be tightly packed.
			void upload_image(const image2d_view &target, const std::byte *data, std::u8string_view description);
			/// Uploads buffer data to the GPU.
			void upload_buffer(
				const buffer &target, std::span<const std::byte> data, std::uint32_t offset,
				std::u8string_view descriptorion
			);
			/// \overload
			template <typename T> void upload_buffer(
				const buffer &target, std::span<const T> data, std::uint32_t byte_offset, std::u8string_view description
			) {
				auto *d = reinterpret_cast<const std::byte*>(data.data());
				upload_buffer(target, { d, d + data.size_bytes() }, byte_offset, description);
			}

			/// Copies data from the first buffer to the second.
			void copy_buffer(
				const buffer &source, const buffer &target,
				std::uint32_t src_offset, std::uint32_t dst_offset, std::uint32_t sz,
				std::u8string_view description
			);

			/// Builds the given \ref blas.
			void build_blas(blas&, std::u8string_view description);
			/// Builds the given \ref tlas.
			void build_tlas(tlas&, std::u8string_view description);

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
			std::u8string_view name, std::span<const geometry_buffers_view>, const pool&
		);
		/// \overload
		[[nodiscard]] blas request_blas(
			std::u8string_view name, std::initializer_list<const geometry_buffers_view> geometries, const pool &p
		) {
			return request_blas(name, { geometries.begin(), geometries.end() }, p);
		}
		/// Creates a top-level acceleration structure for the given input instances.
		[[nodiscard]] tlas request_tlas(std::u8string_view name, std::span<const blas_reference>, const pool&);
		/// Creates a cached descriptor set.
		[[nodiscard]] cached_descriptor_set create_cached_descriptor_set(
			std::u8string_view name, std::span<const all_resource_bindings::numbered_binding>
		);
		/// \overload
		[[nodiscard]] cached_descriptor_set create_cached_descriptor_set(
			std::u8string_view name, std::initializer_list<all_resource_bindings::numbered_binding> bindings
		) {
			return create_cached_descriptor_set(name, { bindings.begin(), bindings.end() });
		}


		/// Analyzes and executes all recorded commands.
		std::vector<batch_statistics_early> execute_all();
		/// Waits until all previous batches have finished executing.
		void wait_idle();


		/// Writes the given images into the given descriptor array.
		void write_image_descriptors(
			image_descriptor_array&, std::uint32_t first_index, std::span<const image2d_view>
		);
		/// \override
		void write_image_descriptors(
			image_descriptor_array &arr, std::uint32_t first_index, std::initializer_list<image2d_view> imgs
		) {
			write_image_descriptors(arr, first_index, { imgs.begin(), imgs.end() });
		}
		/// Writes the given buffers into the given descriptor array.
		void write_buffer_descriptors(
			buffer_descriptor_array&, std::uint32_t first_index, std::span<const structured_buffer_view>
		);
		/// \override
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

		/// Callback function for when statistics for a new batch is available.
		static_function<void(std::uint32_t, batch_statistics_late)> on_batch_statistics_available = nullptr;
	private:
		/// Indicates a descriptor set bind point.
		enum class _bind_point {
			graphics,   ///< The descriptor sets are used for graphics operations.
			compute,    ///< The descriptor sets are used for compute operations.
			raytracing, ///< The descriptor sets are used for ray tracing operations.
		};

		/// Holds data about a timer.
		struct _timer_data {
			/// Initializes this structure to empty.
			_timer_data(std::nullptr_t) {
			}
			/// Initializes this timer with the index of the first command and the timer's name.
			_timer_data(std::uint32_t start, std::u8string_view n) : first_command(start), name(n) {
			}

			std::uint32_t first_command = 0; ///< Index of the first command to time.
			/// Index past the last command to time.
			std::uint32_t last_command = std::numeric_limits<std::uint32_t>::max();
			std::u8string_view name; ///< The name of this timer.
		};


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

		context_cache _cache; ///< Cached objects.

		std::thread::id _thread; ///< \ref execute_all() can only be called from the thread that created this object.

		std::deque<execution::batch_resources> _all_resources; ///< Resources that are in use by previous operations.
		execution::batch_resources _deferred_delete_resources; ///< Resources that are marked for deferred deletion.
		std::uint32_t _batch_index = 0; ///< Index of the last batch that has been submitted.

		/// Counter used to uniquely identify resources.
		unique_resource_id _resource_index = unique_resource_id::invalid;


		/// Initializes all fields of the context.
		context(
			gpu::context&, const gpu::adapter_properties&, gpu::device&, std::span<const gpu::command_queue>
		);

		/// Allocates a unique resource index.
		[[nodiscard]] unique_resource_id _allocate_resource_id() {
			using _int_type = std::underlying_type_t<unique_resource_id>;
			_resource_index = static_cast<unique_resource_id>(static_cast<_int_type>(_resource_index) + 1);
			return _resource_index;
		}

		/// Creates the backing image for the given \ref _details::image2d if it hasn't been created.
		void _maybe_create_image(_details::image2d&);
		/// Creates the backing image for the given \ref _details::image3d if it hasn't been created.
		void _maybe_create_image(_details::image3d&);
		/// Creates the backing buffer for the given \ref _details::buffer if it hasn't been created.
		void _maybe_create_buffer(_details::buffer&);

		/// Creates an \ref gpu::image2d_view, without recording it anywhere, and returns the object itself.
		/// This function is used when we need to keep the view between render commands and flushes.
		[[nodiscard]] gpu::image2d_view _create_image_view(const recorded_resources::image2d_view&);
		/// Creates an \ref gpu::image3d_view, without recording it anywhere, and returns the object itself.
		/// This function is used when we need to keep the view between render commands and flushes.
		[[nodiscard]] gpu::image3d_view _create_image_view(const recorded_resources::image3d_view&);
		/// Creates or finds a \ref gpu::image2d_view from the given \ref renderer::image2d_view, allocating
		/// storage for then image if necessary.
		[[nodiscard]] gpu::image2d_view &_request_image_view(
			execution::context&, const recorded_resources::image2d_view&
		);
		/// Creates or finds a \ref gpu::image3d_view from the given \ref renderer::image3d_view, allocating
		/// storage for then image if necessary.
		[[nodiscard]] gpu::image3d_view &_request_image_view(
			execution::context&, const recorded_resources::image3d_view&
		);
		/// Creates or finds a \ref gpu::image2d_view from the next image in the given \ref swap_chain.
		[[nodiscard]] gpu::image2d_view &_request_image_view(
			execution::context&, const recorded_resources::swap_chain&
		);

		/// Initializes the given \ref _details::descriptor_array if necessary.
		template <typename RecordedResource, typename View> void _maybe_initialize_descriptor_array(
			_details::descriptor_array<RecordedResource, View> &arr
		) {
			if (!arr.set) {
				const auto &layout = _cache.get_descriptor_set_layout(
					cache_keys::descriptor_set_layout::for_descriptor_array(arr.type)
				);
				arr.set = _device.create_descriptor_set(_descriptor_pool, layout, arr.capacity);
				arr.resources.reserve(arr.capacity);
				while (arr.resources.size() < arr.capacity) {
					arr.resources.emplace_back(nullptr);
				}
			}
		}

		/// Initializes the given \ref _details::blas if necessary.
		void _maybe_initialize_blas(_details::blas&);
		/// Initializes the given \ref _details::tlas if necessary.
		void _maybe_initialize_tlas(_details::tlas&);
		/// Copies build input for the given \ref _details::tlas if necessary.
		void _maybe_copy_tlas_build_input(execution::context&, _details::tlas&);

		/// Writes one descriptor array element into the given array.
		template <typename RecordedResource, typename View> void _write_one_descriptor_array_element(
			_details::descriptor_array<RecordedResource, View> &arr, RecordedResource rsrc, std::uint32_t index
		) {
			auto &cur_ref = arr.resources[index];
			if (auto *surf = cur_ref.resource._ptr) {
				auto old_index = cur_ref.reference_index;
				surf->array_references[old_index] = surf->array_references.back();
				auto new_ref = surf->array_references[old_index];
				new_ref.array->resources[new_ref.index].reference_index = old_index;
				surf->array_references.pop_back();
				if constexpr (!std::is_same_v<View, std::nullopt_t>) {
					if (cur_ref.view.value && !_all_resources.empty()) {
						_all_resources.back().record(std::move(cur_ref.view.value));
					}
				}
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

		/// Returns the descriptor type of an image binding.
		template <gpu::image_type Type> [[nodiscard]] gpu::descriptor_type _get_descriptor_type(
			const descriptor_resource::basic_image<Type> &img
		) const {
			switch (img.binding_type) {
			case image_binding_type::read_only:
				return gpu::descriptor_type::read_only_image;
			case image_binding_type::read_write:
				return gpu::descriptor_type::read_write_image;
			}
			return gpu::descriptor_type::num_enumerators;
		}
		/// Returns the descriptor type of a swap chain.
		[[nodiscard]] gpu::descriptor_type _get_descriptor_type(
			const recorded_resources::swap_chain&
		) const {
			return gpu::descriptor_type::read_write_image;
		}
		/// Returns the descriptor type of a constant buffer.
		[[nodiscard]] gpu::descriptor_type _get_descriptor_type(const descriptor_resource::constant_buffer&) const {
			return gpu::descriptor_type::constant_buffer;
		}
		/// Returns the descriptor type of a buffer binding.
		[[nodiscard]] gpu::descriptor_type _get_descriptor_type(
			const descriptor_resource::structured_buffer &buf
		) const {
			switch (buf.binding_type) {
			case buffer_binding_type::read_only:
				return gpu::descriptor_type::read_only_buffer;
			case buffer_binding_type::read_write:
				return gpu::descriptor_type::read_write_buffer;
			}
			return gpu::descriptor_type::num_enumerators;
		}
		/// Returns \ref gpu::descriptor_type::constant_buffer.
		[[nodiscard]] gpu::descriptor_type _get_descriptor_type(
			const descriptor_resource::immediate_constant_buffer&
		) const {
			return gpu::descriptor_type::constant_buffer;
		}
		/// Returns \ref gpu::descriptor_type::acceleration_structure.
		[[nodiscard]] gpu::descriptor_type _get_descriptor_type(const recorded_resources::tlas&) const {
			return gpu::descriptor_type::acceleration_structure;
		}
		/// Returns \ref gpu::descriptor_type::sampler.
		[[nodiscard]] gpu::descriptor_type _get_descriptor_type(const sampler_state&) const {
			return gpu::descriptor_type::sampler;
		}

		/// Creates a descriptor binding for an 2D image.
		void _create_descriptor_binding_impl(
			execution::transition_buffer&, gpu::descriptor_set&,
			const gpu::descriptor_set_layout&, std::uint32_t reg,
			const descriptor_resource::image2d&, const gpu::image2d_view&
		);
		/// Creates a descriptor binding from a 3D image.
		void _create_descriptor_binding_impl(
			execution::transition_buffer&, gpu::descriptor_set&,
			const gpu::descriptor_set_layout&, std::uint32_t reg,
			const descriptor_resource::image3d&, const gpu::image3d_view&
		);
		/// Creates a descriptor binding for a swap chain image.
		void _create_descriptor_binding_impl(
			execution::transition_buffer&, gpu::descriptor_set&,
			const gpu::descriptor_set_layout&, std::uint32_t reg,
			const recorded_resources::swap_chain&, const gpu::image2d_view&
		);
		/// Creates a descriptor binding for a constant buffer.
		void _create_descriptor_binding_impl(
			execution::transition_buffer&, gpu::descriptor_set&,
			const gpu::descriptor_set_layout&, std::uint32_t reg,
			const descriptor_resource::constant_buffer&
		);
		/// Creates a descriptor binding for a structured buffer.
		void _create_descriptor_binding_impl(
			execution::transition_buffer&, gpu::descriptor_set&,
			const gpu::descriptor_set_layout&, std::uint32_t reg,
			const descriptor_resource::structured_buffer&
		);
		/// Creates a descriptor binding for an acceleration structure.
		void _create_descriptor_binding_impl(
			execution::transition_buffer&, gpu::descriptor_set&,
			const gpu::descriptor_set_layout&, std::uint32_t reg,
			const recorded_resources::tlas&
		);
		/// Creates a descriptor binding for a sampler.
		void _create_descriptor_binding_impl(
			gpu::descriptor_set&,
			const gpu::descriptor_set_layout&, std::uint32_t reg,
			const sampler_state&
		);

		/// \overload
		void _create_descriptor_binding(
			execution::context &ectx, gpu::descriptor_set &set,
			const gpu::descriptor_set_layout &layout, std::uint32_t reg,
			const descriptor_resource::image2d &img
		) {
			_create_descriptor_binding_impl(
				ectx.transitions, set, layout, reg, img, _request_image_view(ectx, img.view)
			);
		}
		/// \overload
		void _create_descriptor_binding(
			execution::context &ectx, gpu::descriptor_set &set,
			const gpu::descriptor_set_layout &layout, std::uint32_t reg,
			const descriptor_resource::image3d &img
		) {
			_create_descriptor_binding_impl(
				ectx.transitions, set, layout, reg, img, _request_image_view(ectx, img.view)
			);
		}
		/// \overload
		void _create_descriptor_binding(
			execution::context &ectx, gpu::descriptor_set &set,
			const gpu::descriptor_set_layout &layout, std::uint32_t reg,
			const recorded_resources::swap_chain &img
		) {
			_create_descriptor_binding_impl(
				ectx.transitions, set, layout, reg, img, _request_image_view(ectx, img)
			);
		}
		/// \overload
		void _create_descriptor_binding(
			execution::context &ectx, gpu::descriptor_set &set,
			const gpu::descriptor_set_layout &layout, std::uint32_t reg,
			const descriptor_resource::constant_buffer &buf
		) {
			_create_descriptor_binding_impl(ectx.transitions, set, layout, reg, buf);
		}
		/// \overload
		void _create_descriptor_binding(
			execution::context &ectx, gpu::descriptor_set &set,
			const gpu::descriptor_set_layout &layout, std::uint32_t reg,
			const descriptor_resource::structured_buffer &buf
		) {
			_create_descriptor_binding_impl(ectx.transitions, set, layout, reg, buf);
		}
		/// Creates a descriptor binding for an immediate constant buffer.
		void _create_descriptor_binding(
			execution::context&, gpu::descriptor_set&,
			const gpu::descriptor_set_layout&, std::uint32_t reg,
			const descriptor_resource::immediate_constant_buffer&
		);
		/// \overload
		void _create_descriptor_binding(
			execution::context &ectx, gpu::descriptor_set &set,
			const gpu::descriptor_set_layout &layout, std::uint32_t reg,
			const recorded_resources::tlas &as
		) {
			_create_descriptor_binding_impl(ectx.transitions, set, layout, reg, as);
		}
		/// \overload
		void _create_descriptor_binding(
			execution::context&, gpu::descriptor_set &set,
			const gpu::descriptor_set_layout &layout, std::uint32_t reg,
			const sampler_state &s
		) {
			_create_descriptor_binding_impl(set, layout, reg, s);
		}

		/// \overload
		void _create_descriptor_binding_cached(
			_details::cached_descriptor_set &set, std::uint32_t reg,
			const descriptor_resource::image2d &img
		) {
			auto &view = set.image2d_views.emplace_back(_create_image_view(img.view));
			set.resource_references.emplace_back(img.view._ptr->shared_from_this());
			execution::transition_buffer dummy = nullptr; // TODO
			_create_descriptor_binding_impl(
				dummy, set.set, *set.layout, reg, img, view
			);
		}
		/// \overload
		void _create_descriptor_binding_cached(
			_details::cached_descriptor_set &set, std::uint32_t reg,
			const descriptor_resource::image3d &img
		) {
			auto &view = set.image3d_views.emplace_back(_create_image_view(img.view));
			set.resource_references.emplace_back(img.view._ptr->shared_from_this());
			execution::transition_buffer dummy = nullptr; // TODO
			_create_descriptor_binding_impl(
				dummy, set.set, *set.layout, reg, img, view
			);
		}
		/// \overload
		void _create_descriptor_binding_cached(
			_details::cached_descriptor_set&, std::uint32_t,
			const recorded_resources::swap_chain&
		) {
			std::abort(); // swap chain images are not allowed
		}
		/// \overload
		void _create_descriptor_binding_cached(
			_details::cached_descriptor_set &set, std::uint32_t reg,
			const descriptor_resource::constant_buffer &buf
		) {
			set.resource_references.emplace_back(buf.data._ptr->shared_from_this());
			execution::transition_buffer dummy = nullptr; // TODO
			_create_descriptor_binding_impl(dummy, set.set, *set.layout, reg, buf);
		}
		/// \overload
		void _create_descriptor_binding_cached(
			_details::cached_descriptor_set &set, std::uint32_t reg,
			const descriptor_resource::structured_buffer &buf
		) {
			set.resource_references.emplace_back(buf.data._ptr->shared_from_this());
			execution::transition_buffer dummy = nullptr; // TODO
			_create_descriptor_binding_impl(dummy, set.set, *set.layout, reg, buf);
		}
		/// Creates a descriptor binding for an immediate constant buffer.
		void _create_descriptor_binding_cached(
			_details::cached_descriptor_set&, std::uint32_t,
			const descriptor_resource::immediate_constant_buffer&
		) {
			std::abort(); // not implemented
		}
		/// \overload
		void _create_descriptor_binding_cached(
			_details::cached_descriptor_set &set, std::uint32_t reg,
			const recorded_resources::tlas &as
		) {
			set.resource_references.emplace_back(as._ptr->shared_from_this());
			execution::transition_buffer dummy = nullptr; // TODO
			_create_descriptor_binding_impl(dummy, set.set, *set.layout, reg, as);
		}
		/// \overload
		void _create_descriptor_binding_cached(
			_details::cached_descriptor_set &set, std::uint32_t reg,
			const sampler_state &s
		) {
			_create_descriptor_binding_impl(set.set, *set.layout, reg, s);
		}

		/// Collects all descriptor ranges and returns a key for a descriptor set layout.
		[[nodiscard]] cache_keys::descriptor_set_layout _get_descriptor_set_layout_key(
			std::span<const all_resource_bindings::numbered_binding>
		);

		/// Creates a new descriptor set from the given bindings.
		[[nodiscard]] std::tuple<
			cache_keys::descriptor_set_layout,
			const gpu::descriptor_set_layout&,
			gpu::descriptor_set&
		> _use_descriptor_set(execution::context&, std::span<const all_resource_bindings::numbered_binding>);
		/// Returns the descriptor set of the given bindless descriptor array, and flushes all pending operations.
		template <typename RecordedResource, typename View> [[nodiscard]] std::tuple<
			cache_keys::descriptor_set_layout, const gpu::descriptor_set_layout&, gpu::descriptor_set&
		> _use_descriptor_set(
			execution::context &ectx, const recorded_resources::descriptor_array<RecordedResource, View> &arr
		) {
			auto key = cache_keys::descriptor_set_layout::for_descriptor_array(arr._ptr->type);
			auto &layout = _cache.get_descriptor_set_layout(key);

			constexpr bool _validate_descriptor_array = false;
			if constexpr (_validate_descriptor_array) {
				for (std::size_t i = 0; i < arr._ptr->resources.size(); ++i) {
					const auto &rsrc = arr._ptr->resources[i];
					if (rsrc.resource) {
						crash_if(rsrc.resource._ptr->array_references.size() <= rsrc.reference_index);
						crash_if(rsrc.resource._ptr->array_references[rsrc.reference_index].array != arr._ptr);
						crash_if(rsrc.resource._ptr->array_references[rsrc.reference_index].index != i);
					}
				}
			}

			_maybe_initialize_descriptor_array(*arr._ptr);
			ectx.transitions.stage_all_transitions_for(*arr._ptr);
			ectx.flush_descriptor_array_writes(*arr._ptr, layout);

			return { std::move(key), layout, arr._ptr->set };
		}
		/// Returns the descriptor set of the given cached descriptor set.
		[[nodiscard]] std::tuple<
			cache_keys::descriptor_set_layout,
			const gpu::descriptor_set_layout&,
			gpu::descriptor_set&
		> _use_descriptor_set(execution::context&, const recorded_resources::cached_descriptor_set&);

		/// Checks and creates a descriptor set for the given resources.
		[[nodiscard]] std::tuple<
			cache_keys::pipeline_resources,
			const gpu::pipeline_resources&,
			std::vector<execution::descriptor_set_info>
		> _check_and_create_descriptor_set_bindings(
			execution::context&, _details::numbered_bindings_view
		);
		/// Binds the given descriptor sets.
		void _bind_descriptor_sets(
			execution::context&, const gpu::pipeline_resources&,
			std::vector<execution::descriptor_set_info>, _bind_point
		);

		/// Handles non-pass commands by crashing.
		template <typename T> [[nodiscard]] execution::pass_command_data _preprocess_pass_command(
			execution::context&, const gpu::frame_buffer_layout&, const T&
		) {
			crash_if(true);
			return nullptr;
		}
		/// Preprocesses the given instanced draw command.
		[[nodiscard]] execution::pass_command_data _preprocess_pass_command(
			execution::context&, const gpu::frame_buffer_layout&, const commands::draw_instanced&
		);

		/// Wrapper around command handlers that performs generic operations, such as: invalidating timers.
		template <typename Cmd> void _handle_generic_command(execution::context &ectx, Cmd &&cmd) {
			if constexpr (bit_mask::contains<commands::flags::advances_timer>(std::decay_t<Cmd>::get_flags())) {
				ectx.invalidate_timer();
			}
			_handle_command(ectx, std::forward<Cmd>(cmd));
		}

		/// Handles an invalid command by asserting.
		void _handle_command(execution::context&, const commands::invalid&) {
			std::abort();
		}
		/// Handles an image upload command.
		void _handle_command(execution::context&, commands::upload_image&);
		/// Handles a buffer upload command.
		void _handle_command(execution::context&, commands::upload_buffer&);
		/// Handles a buffer copy command.
		void _handle_command(execution::context&, commands::copy_buffer&);
		/// Handles a BLAS build command.
		void _handle_command(execution::context&, const commands::build_blas&);
		/// Handles a TLAS build command.
		void _handle_command(execution::context&, const commands::build_tlas&);

		/// Handles a begin pass command.
		void _handle_command(execution::context&, const commands::begin_pass&);
		/// Handles a instanced draw command.
		void _handle_command(execution::context&, const commands::draw_instanced&);
		/// Handles an end pass command.
		void _handle_command(execution::context&, const commands::end_pass&);

		/// Handles a dispatch compute command.
		void _handle_command(execution::context&, const commands::dispatch_compute&);
		/// Handles a ray trace command.
		void _handle_command(execution::context&, const commands::trace_rays&);
		/// Handles a present command.
		void _handle_command(execution::context&, const commands::present&);

		/// Handles a start timer event.
		void _handle_command(execution::context&, const commands::start_timer&);
		/// Handles an end timer event.
		void _handle_command(execution::context&, const commands::end_timer&);

		/// Handles a pause command.
		void _handle_command(execution::context&, const commands::pause_for_debugging&);

		/// Cleans up all unused resources, and updates timestamp information to latest.
		void _cleanup();

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


	template <typename Type> void _details::context_managed_deleter::operator()(Type *ptr) const {
		_ctx->_deferred_delete(ptr);
	}
}
