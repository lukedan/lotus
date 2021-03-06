#pragma once

/// \file
/// Scene-related classes.

#include <vector>
#include <unordered_map>

#include "lotus/math/matrix.h"
#include "lotus/utils/index.h"
#include "lotus/containers/intrusive_linked_list.h"
#include "lotus/containers/pool.h"
#include "lotus/system/window.h"
#include "lotus/graphics/context.h"
#include "caching.h"
#include "resources.h"

namespace lotus::renderer {
	/// Provides information about a pass's shader and input buffer layout.
	class pass_context {
	public:
		/// Default virtual destructor.
		virtual ~pass_context() = default;

		/// Retrieves a vertex shader and corresponding input buffer bindings appropriate for this pass, and for the
		/// given material and geometry.
		[[nodiscard]] virtual std::pair<
			assets::handle<assets::shader>, std::vector<input_buffer_binding>
		> get_vertex_shader(context&, const assets::material::context_data&, const assets::geometry&) = 0;
		/// Retrieves a pixel shader appropriate for this pass and the given material.
		[[nodiscard]] virtual assets::handle<assets::shader> get_pixel_shader(
			context&, const assets::material::context_data&
		) = 0;
	};


	/// Commands to be executed during a render pass.
	namespace pass_commands {
		/// Draws a number of instances of a given mesh.
		struct draw_instanced {
			/// Initializes all fields of this struct.
			draw_instanced(
				std::uint32_t num_instances,
				std::vector<input_buffer_binding> in, std::uint32_t num_verts,
				index_buffer_binding indices, std::uint32_t num_indices,
				graphics::primitive_topology t,
				all_resource_bindings resources,
				assets::handle<assets::shader> vs,
				assets::handle<assets::shader> ps,
				graphics_pipeline_state s
			) :
				inputs(std::move(in)), index_buffer(std::move(indices)), resource_bindings(std::move(resources)),
				vertex_shader(std::move(vs)), pixel_shader(std::move(ps)), state(std::move(s)),
				instance_count(num_instances), vertex_count(num_verts), index_count(num_indices), topology(t) {
			}

			std::vector<input_buffer_binding> inputs;       ///< Input buffers.
			index_buffer_binding              index_buffer; ///< Index buffer, if applicable.

			all_resource_bindings resource_bindings; ///< Resource bindings.
			// TODO more shaders
			assets::handle<assets::shader> vertex_shader; ///< Vertex shader.
			assets::handle<assets::shader> pixel_shader;  ///< Pixel shader.
			graphics_pipeline_state state; ///< Render pipeline state.

			std::uint32_t instance_count = 0; ///< Number of instances to draw.
			std::uint32_t vertex_count   = 0; ///< Number of vertices.
			std::uint32_t index_count    = 0; ///< Number of indices.
			/// Primitive topology.
			graphics::primitive_topology topology = graphics::primitive_topology::point_list;
		};
	}
	/// A union of all pass command types.
	struct pass_command {
		/// Initializes this command.
		template <typename ...Args> explicit pass_command(std::u8string_view desc, Args &&...args) :
			value(std::forward<Args>(args)...), description(desc) {
		}

		std::variant<pass_commands::draw_instanced> value; ///< The value of this command.
		/// Debug description of this command.
		[[no_unique_address]] static_optional<std::u8string, should_register_debug_names> description;
	};

	/// Commands.
	namespace context_commands {
		/// Placeholder for an invalid command.
		struct invalid {
		};

		/// Uploads contents from the given staging buffer to the highest visible mip of the target image.
		struct upload_image {
			/// Initializes all fields of this struct.
			upload_image(graphics::staging_buffer src, recorded_resources::image2d_view dst) :
				source(std::move(src)), destination(dst) {
			}

			graphics::staging_buffer source; ///< Image data.
			recorded_resources::image2d_view destination; ///< Image to upload to.
		};

		/// Compute shader dispatch.
		struct dispatch_compute {
			/// Initializes all fields of this struct.
			dispatch_compute(
				all_resource_bindings rsrc,
				assets::handle<assets::shader> compute_shader,
				cvec3<std::uint32_t> numgroups
			) : resources(std::move(rsrc)),
				shader(std::move(compute_shader)),
				num_thread_groups(numgroups) {
			}

			all_resource_bindings resources; ///< All resource bindings.
			assets::handle<assets::shader> shader; ///< The shader.
			cvec3<std::uint32_t> num_thread_groups = uninitialized; ///< Number of thread groups.
		};

		/// Executes a render pass.
		struct render_pass {
			/// Initializes the render target.
			render_pass(std::vector<surface2d_color> color_rts, surface2d_depth_stencil ds_rt, cvec2s sz) :
				color_render_targets(std::move(color_rts)), depth_stencil_target(std::move(ds_rt)),
				render_target_size(sz) {
			}

			std::vector<surface2d_color> color_render_targets; ///< Color render targets.
			surface2d_depth_stencil depth_stencil_target; ///< Depth stencil render target.
			cvec2s render_target_size; ///< The size of the render target.
			std::vector<pass_command> commands; ///< Commands within this pass.
		};

		/// Presents the given swap chain.
		struct present {
			/// Initializes the target.
			explicit present(swap_chain t) : target(std::move(t)) {
			}

			swap_chain target; ///< The swap chain to present.
		};
	}
	/// A union of all context command types.
	struct context_command {
		/// Initializes this command.
		template <typename ...Args> explicit context_command(std::u8string_view desc, Args &&...args) :
			value(std::forward<Args>(args)...), description(desc) {
		}
		/// \overload
		template <typename ...Args> explicit context_command(
			static_optional<std::u8string, should_register_debug_names> desc, Args &&...args
		) : value(std::forward<Args>(args)...), description(std::move(desc)) {
		}

		std::variant<
			context_commands::invalid,
			context_commands::upload_image,
			context_commands::dispatch_compute,
			context_commands::render_pass,
			context_commands::present
		> value; ///< The value of this command.
		/// Debug description of this command.
		[[no_unique_address]] static_optional<std::u8string, should_register_debug_names> description;
	};


	/// Keeps track of the rendering of a frame, including resources used for rendering.
	class context {
		friend _details::surface2d;
		friend _details::context_managed_deleter;
	public:
		class command_list;

		/// A pass being rendered.
		class pass {
			friend context;
		public:
			/// No move construction.
			pass(pass&&) = delete;
			/// No copy construction.
			pass(const pass&) = delete;

			/// Draws a number of instances with the given inputs.
			void draw_instanced(
				std::vector<input_buffer_binding>,
				std::uint32_t num_verts,
				index_buffer_binding,
				std::uint32_t num_indices,
				graphics::primitive_topology,
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
				graphics_pipeline_state,
				std::uint32_t num_insts,
				std::u8string_view description
			);

			/// Finishes rendering to the pass and records all commands into the context.
			void end();
		private:
			// TODO allocator?
			context_commands::render_pass _command; ///< The resulting render pass command.
			context *_context = nullptr; ///< The context that created this pass.
			/// The description of this pass.
			[[no_unique_address]] static_optional<std::u8string, should_register_debug_names> _description;

			/// Initializes the pass.
			pass(
				context &ctx, std::vector<surface2d_color> color_rts, surface2d_depth_stencil ds_rt, cvec2s sz,
				std::u8string_view description
			) :
				_context(&ctx),
				_command(std::move(color_rts), std::move(ds_rt), sz),
				_description(description) {
			}
		};

		/// Creates a new context object.
		[[nodiscard]] static context create(
			graphics::context&, const graphics::adapter_properties&, graphics::device&, graphics::command_queue&
		);
		/// Disposes of all resources.
		~context();

		/// Creates a 2D image with the given properties.
		[[nodiscard]] image2d_view request_image2d(
			std::u8string_view name, cvec2s size, std::uint32_t num_mips, graphics::format, graphics::image_usage::mask
		);
		/// Creates a swap chain with the given properties.
		[[nodiscard]] swap_chain request_swap_chain(
			std::u8string_view name, system::window&,
			std::uint32_t num_images, std::span<const graphics::format> formats
		);
		/// \overload
		[[nodiscard]] swap_chain request_swap_chain(
			std::u8string_view name, system::window &wnd,
			std::uint32_t num_images, std::initializer_list<graphics::format> formats
		) {
			return request_swap_chain(name, wnd, num_images, std::span{ formats.begin(), formats.end() });
		}
		/// Creates a descriptor array with the given properties.
		[[nodiscard]] descriptor_array request_descriptor_array(
			std::u8string_view name, graphics::descriptor_type, std::uint32_t capacity
		);


		/// Uploads image data to the GPU. This function immediately creates and fills the staging buffer, but actual
		/// image uploading is deferred. The pixels format of the input data is assumed to be the same as the
		/// image (i.e. direct memcpy can be used), and the rows are assumed to be tightly packed.
		void upload_image(const image2d_view &target, const void *data, std::u8string_view description);

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
			std::vector<surface2d_color>, surface2d_depth_stencil, cvec2s sz, std::u8string_view description
		);

		/// Presents the given swap chain.
		void present(swap_chain, std::u8string_view description);


		/// Analyzes and executes all pending commands. This is the only place where that happens - it doesn't happen
		/// automatically.
		void flush();
		/// Waits until all previous batches have finished executing.
		void wait_idle();


		/// Writes the given images into the given descriptor array.
		void write_image_descriptors(descriptor_array&, std::uint32_t first_index, std::span<const image2d_view>);
		/// \override
		void write_image_descriptors(
			descriptor_array &arr, std::uint32_t first_index, std::initializer_list<image2d_view> imgs
		) {
			write_image_descriptors(arr, first_index, { imgs.begin(), imgs.end() });
		}
		// TODO buffer descriptors


		/// Returns \ref _device_memory_index.
		[[nodiscard]] graphics::memory_type_index HACK_device_memory_type_index() const {
			return _device_memory_index;
		}
		/// Returns \ref _upload_memory_index.
		[[nodiscard]] graphics::memory_type_index HACK_upload_memory_type_index() const {
			return _upload_memory_index;
		}
	private:
		/// Indicates a descriptor set bind point.
		enum class _bind_point {
			graphics, ///< The descriptor sets are used for graphics operations.
			compute,  ///< The descriptor sets are used for compute operations.
		};

		/// A descriptor set and its register space.
		struct _descriptor_set_info {
			/// Initializes this structure to empty.
			_descriptor_set_info(std::nullptr_t) {
			}
			/// Initializes all fields of this struct.
			_descriptor_set_info(graphics::descriptor_set &se, std::uint32_t s) : set(&se), space(s) {
			}

			graphics::descriptor_set *set = nullptr; ///< The descriptor set.
			std::uint32_t space = 0; ///< Register space of this descriptor set.
		};
		/// Cached data used by a single pass command.
		struct _pass_command_data {
			/// Initializes this structure to empty.
			_pass_command_data(std::nullptr_t) {
			}

			const graphics::pipeline_resources *resources = nullptr; ///< Pipeline resources.
			const graphics::graphics_pipeline_state *pipeline_state = nullptr; ///< Pipeline state.
			std::vector<_descriptor_set_info> descriptor_sets; ///< Descriptor sets.
		};
		/// Contains information about a layout transition operation.
		struct _surface2d_transition_info {
			/// Initializes this structure to empty.
			_surface2d_transition_info(std::nullptr_t) : mip_levels(graphics::mip_levels::all()) {
			}
			/// Initializes all fields of this struct.
			_surface2d_transition_info(_details::surface2d &surf, graphics::mip_levels mips, graphics::image_usage u) :
				surface(&surf), mip_levels(mips), usage(u) {
			}

			_details::surface2d *surface = nullptr; ///< The surface to transition.
			graphics::mip_levels mip_levels; ///< Mip levels to transition.
			graphics::image_usage usage = graphics::image_usage::num_enumerators; ///< Usage to transition to.
		};
		// TODO convert this into "generic image transition"
		/// Contains information about a layout transition operation.
		struct _swap_chain_transition_info {
			/// Initializes this structure to empty.
			_swap_chain_transition_info(std::nullptr_t) {
			}
			/// Initializes all fields of this struct.
			_swap_chain_transition_info(_details::swap_chain &c, graphics::image_usage u) : chain(&c), usage(u) {
			}

			_details::swap_chain *chain = nullptr; ///< The swap chain to transition.
			graphics::image_usage usage = graphics::image_usage::num_enumerators; ///< Usage to transition to.
		};

		/// Transient resources used by a batch.
		struct _batch_resources {
			std::deque<graphics::descriptor_set>          descriptor_sets;        ///< Descriptor sets.
			std::deque<graphics::descriptor_set_layout>   descriptor_set_layouts; ///< Descriptor set layouts.
			std::deque<graphics::pipeline_resources>      pipeline_resources;     ///< Pipeline resources.
			std::deque<graphics::compute_pipeline_state>  compute_pipelines;      ///< Compute pipeline states.
			std::deque<graphics::graphics_pipeline_state> graphics_pipelines;     ///< Graphics pipeline states.
			std::deque<graphics::image2d>                 images;                 ///< Images.
			std::deque<graphics::image2d_view>            image_views;            ///< Image views.
			std::deque<graphics::buffer>                  buffers;                ///< Constant buffers.
			std::deque<graphics::command_list>            command_lists;          ///< Command lists.
			std::deque<graphics::frame_buffer>            frame_buffers;          ///< Frame buffers.
			std::deque<graphics::sampler>                 samplers;               ///< Samplers.
			std::deque<graphics::swap_chain>              swap_chains;            ///< Swap chains.
			std::deque<graphics::fence>                   fences;                 ///< Fences.

			std::vector<std::unique_ptr<_details::surface2d>>  surface2d_meta;    ///< Images to be disposed next frame.
			std::vector<std::unique_ptr<_details::swap_chain>> swap_chain_meta; ///< Swap chain to be disposed next frame.

			/// Registers the given object as a resource.
			template <typename T> T &record(T &&obj) {
				if constexpr (std::is_same_v<T, graphics::descriptor_set>) {
					return descriptor_sets.emplace_back(std::move(obj));
				} else if constexpr (std::is_same_v<T, graphics::descriptor_set_layout>) {
					return descriptor_set_layouts.emplace_back(std::move(obj));
				} else if constexpr (std::is_same_v<T, graphics::pipeline_resources>) {
					return pipeline_resources.emplace_back(std::move(obj));
				} else if constexpr (std::is_same_v<T, graphics::compute_pipeline_state>) {
					return compute_pipelines.emplace_back(std::move(obj));
				} else if constexpr (std::is_same_v<T, graphics::graphics_pipeline_state>) {
					return graphics_pipelines.emplace_back(std::move(obj));
				} else if constexpr (std::is_same_v<T, graphics::image2d>) {
					return images.emplace_back(std::move(obj));
				} else if constexpr (std::is_same_v<T, graphics::image2d_view>) {
					return image_views.emplace_back(std::move(obj));
				} else if constexpr (std::is_same_v<T, graphics::buffer>) {
					return buffers.emplace_back(std::move(obj));
				} else if constexpr (std::is_same_v<T, graphics::command_list>) {
					return command_lists.emplace_back(std::move(obj));
				} else if constexpr (std::is_same_v<T, graphics::sampler>) {
					return samplers.emplace_back(std::move(obj));
				} else if constexpr (std::is_same_v<T, graphics::swap_chain>) {
					return swap_chains.emplace_back(std::move(obj));
				} else if constexpr (std::is_same_v<T, graphics::fence>) {
					return fences.emplace_back(std::move(obj));
				} else {
					static_assert(sizeof(T*) == 0, "Unhandled resource type");
				}
			}
		};

		/// Stores temporary objects used when executing commands.
		struct _execution_context {
		public:
			/// 1MB for immediate constant buffers.
			constexpr static std::size_t immediate_constant_buffer_cache_size = 1024 * 1024;

			/// Creates a new execution context for the given context.
			[[nodiscard]] inline static _execution_context create(context&);
			/// No move construction.
			_execution_context(const _execution_context&) = delete;
			/// No move assignment.
			_execution_context &operator=(const _execution_context&) = delete;

			/// Creates the command list if necessary, and returns the current command list.
			[[nodiscard]] graphics::command_list &get_command_list();
			/// Submits the current command list.
			///
			/// \return Whether a command list has been submitted. If not, an empty submission will have been
			///         performed with the given synchronization requirements.
			bool submit(graphics::command_queue &q, graphics::queue_synchronization sync) {
				flush_immediate_constant_buffers();

				if (!_list) {
					q.submit_command_lists({}, std::move(sync));
					return false;
				}

				_list->finish();
				q.submit_command_lists({ _list }, std::move(sync));
				_list = nullptr;
				return true;
			}

			/// Records the given object to be disposed of when this frame finishes.
			template <typename T> T &record(T &&obj) {
				return _resources.record(std::move(obj));
			}
			/// Creates a new buffer with the given parameters.
			[[nodiscard]] graphics::buffer &create_buffer(
				std::size_t size, graphics::memory_type_index mem_type, graphics::buffer_usage::mask usage
			) {
				return _resources.buffers.emplace_back(_ctx._device.create_committed_buffer(size, mem_type, usage));
			}
			/// Creates a frame buffer with the given parameters.
			[[nodiscard]] graphics::frame_buffer &create_frame_buffer(
				std::span<const graphics::image2d_view *const> color_rts,
				const graphics::image2d_view *ds_rt,
				cvec2s size
			) {
				return _resources.frame_buffers.emplace_back(
					_ctx._device.create_frame_buffer(color_rts, ds_rt, size)
				);
			}

			/// Stages a surface transition operation, and notifies any descriptor arrays affected.
			void stage_transition(_details::surface2d&, graphics::mip_levels, graphics::image_usage);
			/// Stages a swap chain transition operation.
			void stage_transition(_details::swap_chain &chain, graphics::image_usage usage) {
				_swap_chain_transitions.emplace_back(chain, usage);
			}
			/// Stages a raw buffer transition operation. No state tracking is performed for such operations; this is
			/// only intended to be used internally when the usage of a buffer is known.
			void stage_transition(graphics::buffer &buf, graphics::buffer_usage from, graphics::buffer_usage to) {
				std::pair<graphics::buffer_usage, graphics::buffer_usage> usage(from, to);
				auto [it, inserted] = _raw_buffer_transitions.emplace(&buf, usage);
				assert(inserted || it->second == usage);
			}
			/// Stages all pending transitions from the given descriptor array.
			void stage_all_transitions(_details::descriptor_array&);
			/// Flushes all staged surface transition operations.
			void flush_transitions();

			/// Stages the given immediate constant buffer.
			[[nodiscard]] graphics::constant_buffer_view stage_immediate_constant_buffer(std::span<const std::byte>);
			/// Flushes all staged immediate constant buffers.
			void flush_immediate_constant_buffers();
		private:
			/// Initializes this context.
			_execution_context(context &ctx, _batch_resources &rsrc) :
				_ctx(ctx), _resources(rsrc),
				_immediate_constant_device_buffer(nullptr),
				_immediate_constant_upload_buffer(nullptr) {
			}

			context &_ctx; ///< The associated context.
			_batch_resources &_resources; ///< Internal resources.
			graphics::command_list *_list = nullptr; ///< Current command list.

			/// Staged image transition operations.
			std::vector<_surface2d_transition_info> _surface_transitions;
			/// Staged swap chain transition operations.
			std::vector<_swap_chain_transition_info> _swap_chain_transitions;
			/// Staged raw buffer transition operations.
			std::unordered_map<
				graphics::buffer*, std::pair<graphics::buffer_usage, graphics::buffer_usage>
			> _raw_buffer_transitions;

			/// Amount used in \ref _immediate_constant_device_buffer.
			std::size_t _immediate_constant_buffer_used = 0;
			/// Buffer containing all immediate constant buffers, located on the device memory.
			graphics::buffer _immediate_constant_device_buffer;
			/// Upload buffer for \ref _immediate_constant_device_buffer.
			graphics::buffer _immediate_constant_upload_buffer;
			/// Mapped pointer for \ref _immediate_constant_upload_buffer.
			std::byte *_immediate_constant_upload_buffer_ptr = nullptr;
		};

		graphics::context &_context;     ///< Associated graphics context.
		graphics::device &_device;       ///< Associated device.
		graphics::command_queue &_queue; ///< Associated command queue.

		graphics::command_allocator _cmd_alloc; ///< Command allocator.
		/// Command allocator for all command lists that are recorded and submitted immediately.
		graphics::command_allocator _transient_cmd_alloc;
		graphics::descriptor_pool _descriptor_pool; ///< Descriptor pool to allocate descriptors out of.
		graphics::descriptor_set_layout _empty_layout; ///< Empty descriptor set layout.
		graphics::timeline_semaphore _batch_semaphore; ///< A semaphore that is signaled after each batch.

		graphics::adapter_properties _adapter_properties; ///< Adapter properties.

		/// Index of a memory type suitable for uploading to the device.
		graphics::memory_type_index _upload_memory_index = graphics::memory_type_index::invalid;
		/// Index of a memory type best for resources that are resident on the device.
		graphics::memory_type_index _device_memory_index = graphics::memory_type_index::invalid;

		context_cache _cache; ///< Cached objects.

		std::thread::id _thread; ///< \ref flush() can only be called from the thread that created this object.

		std::vector<context_command> _commands; ///< Recorded commands.

		std::deque<_batch_resources> _all_resources; ///< Resources that are in use by previous operations.
		_batch_resources _deferred_delete_resources; ///< Resources that are marked for deferred deletion.
		std::uint32_t _batch_index = 0; ///< Index of the last batch that has been submitted.

		std::uint64_t _resource_index = 0; ///< Counter used to uniquely identify resources.

		/// Initializes all fields of the context.
		context(
			graphics::context&, const graphics::adapter_properties&, graphics::device&, graphics::command_queue&
		);

		/// Creates the backing image for the given \ref _details::surface2d if it hasn't been created.
		void _maybe_create_image(_details::surface2d&);
		/// Creates an \ref graphics::image2d_view, without recording it anywhere, and returns the object itself.
		/// This function is used when we need to keep the view between render commands and flushes.
		[[nodiscard]] graphics::image2d_view _create_image_view(const recorded_resources::image2d_view&);

		/// Creates or finds a \ref graphics::image2d_view from the given \ref renderer::image2d_view, allocating
		/// storage for then image if necessary.
		[[nodiscard]] graphics::image2d_view &_request_image_view(
			_execution_context&, const recorded_resources::image2d_view&
		);
		/// Creates or finds a \ref graphics::image2d_view from the next image in the given \ref swap_chain.
		[[nodiscard]] graphics::image2d_view &_request_image_view(
			_execution_context&, const recorded_resources::swap_chain&
		);

		/// Initializes the given \ref _details::descriptor_array if necessary.
		void _maybe_initialize_descriptor_array(_details::descriptor_array&);

		/// Returns the descriptor type of an image binding.
		[[nodiscard]] graphics::descriptor_type _get_descriptor_type(const descriptor_resource::image2d &img) const {
			switch (img.binding_type) {
			case image_binding_type::read_only:
				return graphics::descriptor_type::read_only_image;
			case image_binding_type::read_write:
				return graphics::descriptor_type::read_write_image;
			}
			return graphics::descriptor_type::num_enumerators;
		}
		/// Returns the descriptor type of a swap chain.
		[[nodiscard]] graphics::descriptor_type _get_descriptor_type(
			const descriptor_resource::swap_chain_image&
		) const {
			return graphics::descriptor_type::read_write_image;
		}
		/// Returns \ref graphics::descriptor_type::constant_buffer.
		[[nodiscard]] graphics::descriptor_type _get_descriptor_type(
			const descriptor_resource::immediate_constant_buffer&
		) const {
			return graphics::descriptor_type::constant_buffer;
		}
		/// Returns \ref graphics::descriptor_type::sampler.
		[[nodiscard]] graphics::descriptor_type _get_descriptor_type(const descriptor_resource::sampler&) const {
			return graphics::descriptor_type::sampler;
		}

		/// Creates a descriptor binding for an image.
		void _create_descriptor_binding(
			_execution_context&, graphics::descriptor_set&,
			const graphics::descriptor_set_layout&, std::uint32_t reg,
			const descriptor_resource::image2d&
		);
		/// Creates a descriptor binding for an image.
		void _create_descriptor_binding(
			_execution_context&, graphics::descriptor_set&,
			const graphics::descriptor_set_layout&, std::uint32_t reg,
			const descriptor_resource::swap_chain_image&
		);
		/// Creates a descriptor binding for an immediate constant buffer.
		void _create_descriptor_binding(
			_execution_context&, graphics::descriptor_set&,
			const graphics::descriptor_set_layout&, std::uint32_t reg,
			const descriptor_resource::immediate_constant_buffer&
		);
		/// Creates a descriptor binding for a sampler.
		void _create_descriptor_binding(
			_execution_context&, graphics::descriptor_set&,
			const graphics::descriptor_set_layout&, std::uint32_t reg,
			const descriptor_resource::sampler&
		);

		/// Creates a new descriptor set from the given bindings.
		[[nodiscard]] std::tuple<
			cache_keys::descriptor_set_layout,
			const graphics::descriptor_set_layout&,
			graphics::descriptor_set&
		> _use_descriptor_set(_execution_context&, const resource_set_binding::descriptor_bindings&);
		/// Returns the descriptor set of the given bindless descriptor array, and flushes all pending operations.
		[[nodiscard]] std::tuple<
			cache_keys::descriptor_set_layout,
			const graphics::descriptor_set_layout&,
			graphics::descriptor_set&
		> _use_descriptor_set(_execution_context&, const recorded_resources::descriptor_array&);

		/// Checks and creates a descriptor set for the given resources.
		[[nodiscard]] std::tuple<
			cache_keys::pipeline_resources,
			const graphics::pipeline_resources&,
			std::vector<context::_descriptor_set_info>
		> _check_and_create_descriptor_set_bindings(
			_execution_context&, const all_resource_bindings&
		);
		/// Binds the given descriptor sets.
		void _bind_descriptor_sets(
			_execution_context&, const graphics::pipeline_resources&,
			std::vector<_descriptor_set_info>, _bind_point
		);

		/// Preprocesses the given instanced draw command.
		[[nodiscard]] _pass_command_data _preprocess_command(
			_execution_context&, const graphics::frame_buffer_layout&, const pass_commands::draw_instanced&
		);

		/// Handles a instanced draw command.
		void _handle_pass_command(
			_execution_context&, _pass_command_data, const pass_commands::draw_instanced&
		);

		/// Handles an invalid command by asserting.
		void _handle_command(_execution_context&, const context_commands::invalid&) {
			assert(false);
		}
		/// Handles an image upload command.
		void _handle_command(_execution_context&, const context_commands::upload_image&);
		/// Handles a dispatch compute command.
		void _handle_command(_execution_context&, const context_commands::dispatch_compute&);
		/// Handles a begin pass command.
		void _handle_command(_execution_context&, const context_commands::render_pass&);
		/// Handles a present command.
		void _handle_command(_execution_context&, const context_commands::present&);

		/// Cleans up all unused resources.
		void _cleanup();

		/// Interface to \ref _details::context_managed_deleter for deferring deletion of a surface.
		void _deferred_delete(_details::surface2d *surf) {
			if (surf->image) {
				_deferred_delete_resources.images.emplace_back(std::move(surf->image));
			}
			_deferred_delete_resources.surface2d_meta.emplace_back(surf);
		}
		/// Interface to \ref _details::context_managed_deleter for deferring deletion of a swap chain.
		void _deferred_delete(_details::swap_chain *chain) {
			if (chain->chain) {
				_deferred_delete_resources.swap_chains.emplace_back(std::move(chain->chain));
				for (auto &fence : chain->fences) {
					_deferred_delete_resources.fences.emplace_back(std::move(fence));
				}
			}
			_deferred_delete_resources.swap_chain_meta.emplace_back(chain);
		}
		/// Interface to \ref _details::context_managed_deleter for deferring deletion of a descriptor array.
		void _deferred_delete(_details::descriptor_array*) {
			// TODO
		}
	};

	/// An instance in a scene.
	struct instance {
		/// Initializes this instance to empty.
		instance(std::nullptr_t) : material(nullptr), geometry(nullptr), transform(uninitialized) {
		}

		assets::handle<assets::material> material; ///< The material of this instance.
		assets::handle<assets::geometry> geometry; ///< Geometry of this instance.
		mat44f transform; ///< Transform of this instance.
	};


	template <typename Type> void _details::context_managed_deleter::operator()(Type *ptr) const {
		assert(_ctx);
		_ctx->_deferred_delete(ptr);
	}
}
