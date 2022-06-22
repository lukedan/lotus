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
#include "asset_manager.h"
#include "resources.h"

namespace lotus::renderer {
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
				assets::owning_handle<assets::shader> vs,
				assets::owning_handle<assets::shader> ps,
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
			assets::owning_handle<assets::shader> vertex_shader; ///< Vertex shader.
			assets::owning_handle<assets::shader> pixel_shader;  ///< Pixel shader.
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
		[[no_unique_address]] static_optional<std::u8string, register_debug_names> description;
	};

	/// Commands.
	namespace context_commands {
		/// Placeholder for an invalid command.
		struct invalid {
		};

		/// Compute shader dispatch.
		struct dispatch_compute {
			/// Initializes all fields of this struct.
			dispatch_compute(
				all_resource_bindings rsrc,
				assets::owning_handle<assets::shader> compute_shader,
				cvec3<std::uint32_t> numgroups
			) : resources(std::move(rsrc)),
				shader(std::move(compute_shader)),
				num_thread_groups(numgroups) {
			}

			all_resource_bindings resources; ///< All resource bindings.
			assets::owning_handle<assets::shader> shader; ///< The shader.
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
			static_optional<std::u8string, register_debug_names> desc, Args &&...args
		) : value(std::forward<Args>(args)...), description(std::move(desc)) {
		}

		std::variant<
			context_commands::invalid,
			context_commands::dispatch_compute,
			context_commands::render_pass,
			context_commands::present
		> value; ///< The value of this command.
		/// Debug description of this command.
		[[no_unique_address]] static_optional<std::u8string, register_debug_names> description;
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
				assets::owning_handle<assets::shader> vs,
				assets::owning_handle<assets::shader> ps,
				graphics_pipeline_state,
				std::uint32_t num_insts,
				std::u8string_view description
			);
			/// \overload
			void draw_instanced(
				assets::owning_handle<assets::geometry>,
				assets::owning_handle<assets::material>,
				std::span<const input_buffer_binding> additional_inputs,
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
			[[no_unique_address]] static_optional<std::u8string, register_debug_names> _description;

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
		[[nodiscard]] static context create(assets::manager&, graphics::context&, graphics::command_queue&);

		/// Creates a 2D surface with the given properties.
		[[nodiscard]] image2d_view request_surface2d(std::u8string_view name, cvec2s size, graphics::format);
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


		/// Runs a compute shader.
		void run_compute_shader(
			assets::owning_handle<assets::shader>, cvec3<std::uint32_t> num_thread_groups, all_resource_bindings,
			std::u8string_view description
		);
		/// Runs a compute shader with the given number of threads. Asserts if the number of threads is not
		/// divisible by the shader's thread group size.
		void run_compute_shader_with_thread_dimensions(
			assets::owning_handle<assets::shader>, cvec3<std::uint32_t> num_threads, all_resource_bindings,
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

			graphics::pipeline_resources *resources = nullptr; ///< Pipeline resources.
			graphics::graphics_pipeline_state *pipeline_state = nullptr; ///< Pipeline state.
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
			graphics::image_usage usage = graphics::image_usage::initial; ///< Usage to transition to.
		};
		/// Contains information about a layout transition operation.
		struct _swap_chain_transition_info {
			/// Initializes this structure to empty.
			_swap_chain_transition_info(std::nullptr_t) {
			}
			/// Initializes all fields of this struct.
			_swap_chain_transition_info(_details::swap_chain &c, graphics::image_usage u) : chain(&c), usage(u) {
			}

			_details::swap_chain *chain = nullptr; ///< The swap chain to transition.
			graphics::image_usage usage = graphics::image_usage::initial; ///< Usage to transition to.
		};

		/// Transient resources used by a batch.
		struct _batch_resources {
			std::deque<graphics::descriptor_set>          descriptor_sets;    ///< Descriptor sets.
			std::deque<graphics::pipeline_resources>      pipeline_resources; ///< Pipeline resources.
			std::deque<graphics::compute_pipeline_state>  compute_pipelines;  ///< Compute pipeline states.
			std::deque<graphics::graphics_pipeline_state> graphics_pipelines; ///< Graphics pipeline states.
			std::deque<graphics::image2d>                 images;             ///< Images.
			std::deque<graphics::image2d_view>            image_views;        ///< Image views.
			std::deque<graphics::buffer>                  buffers;            ///< Constant buffers.
			std::deque<graphics::command_list>            command_lists;      ///< Command lists.
			std::deque<graphics::frame_buffer>            frame_buffers;      ///< Frame buffers.
			std::deque<graphics::sampler>                 samplers;           ///< Samplers.
			std::deque<graphics::swap_chain>              swap_chains;        ///< Swap chains.
			std::deque<graphics::fence>                   fences;             ///< Fences.

			std::vector<std::unique_ptr<_details::surface2d>>  surface2d_meta;    ///< Images to be disposed next frame.
			std::vector<std::unique_ptr<_details::swap_chain>> swap_chain_meta; ///< Swap chain to be disposed next frame.

			/// Registers the given object as a resource.
			template <typename T> T &record(T &&obj) {
				if constexpr (std::is_same_v<T, graphics::descriptor_set>) {
					return descriptor_sets.emplace_back(std::move(obj));
				} else if constexpr (std::is_same_v<T, graphics::pipeline_resources>) {
					return pipeline_resources.emplace_back(std::move(obj));
				} else if constexpr (std::is_same_v<T, graphics::compute_pipeline_state>) {
					return compute_pipelines.emplace_back(std::move(obj));
				} else if constexpr (std::is_same_v<T, graphics::graphics_pipeline_state>) {
					return graphics_pipelines.emplace_back(std::move(obj));
				} else if constexpr (std::is_same_v<T, graphics::image2d>) {
					return images.emplace_back(std::move(obj));
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
			/// Creates a new image view with the given parameters.
			[[nodiscard]] graphics::image2d_view &create_image2d_view(
				graphics::image2d &img, graphics::format fmt, graphics::mip_levels mips
			) {
				return _resources.image_views.emplace_back(
					_ctx._device.create_image2d_view_from(img, fmt, mips)
				);
			}
			/// Creates a new buffer with the given parameters.
			[[nodiscard]] graphics::buffer &create_buffer(
				std::size_t size, graphics::heap_type heap, graphics::buffer_usage::mask usage
			) {
				return _resources.buffers.emplace_back(_ctx._device.create_committed_buffer(size, heap, usage));
			}
			/// Creates a frame buffer with the given parameters.
			[[nodiscard]] graphics::frame_buffer &create_frame_buffer(
				std::span<const graphics::image2d_view *const> color_rts, const graphics::image2d_view *ds_rt, cvec2s size
			) {
				return _resources.frame_buffers.emplace_back(
					_ctx._device.create_frame_buffer(color_rts, ds_rt, size)
				);
			}

			/// Stages a surface transition operation.
			void stage_transition(_details::surface2d &surf, graphics::mip_levels mips, graphics::image_usage usage) {
				_surface_transitions.emplace_back(surf, mips, usage);
			}
			/// Stages a swap chain transition operation.
			void stage_transition(_details::swap_chain &chain, graphics::image_usage usage) {
				_swap_chain_transitions.emplace_back(chain, usage);
			}
			/// Flushes all staged surface transition operations.
			void flush_transitions();
		private:
			/// Initializes this context.
			_execution_context(context &ctx, _batch_resources &rsrc) : _ctx(ctx), _resources(rsrc) {
			}

			context &_ctx; ///< The associated context.
			_batch_resources &_resources; ///< Internal resources.
			graphics::command_list *_list = nullptr; ///< Current command list.

			/// Staged image transition operations.
			std::vector<_surface2d_transition_info> _surface_transitions;
			/// Staged swap chain transition operations.
			std::vector<_swap_chain_transition_info> _swap_chain_transitions;
		};

		assets::manager &_assets;        ///< Associated asset manager.
		graphics::context &_context;     ///< Associated graphics context.
		graphics::device &_device;       ///< Associated device.
		graphics::command_queue &_queue; ///< Associated command queue.

		graphics::command_allocator _cmd_alloc;     ///< Command allocator.
		graphics::descriptor_pool _descriptor_pool; ///< Descriptor pool to allocate descriptors out of.
		graphics::descriptor_set_layout _empty_layout; ///< Empty descriptor set layout.
		graphics::timeline_semaphore _batch_semaphore; ///< A semaphore that is signaled after each batch.

		std::thread::id _thread; ///< \ref flush() can only be called from the thread that created this object.

		std::vector<context_command> _commands; ///< Recorded commands.

		std::deque<_batch_resources> _all_resources; ///< Resources that are in use by previous operations.
		_batch_resources _deferred_delete_resources; ///< Resources that are marked for deferred deletion.
		std::uint32_t _batch_index = 0; ///< Index of the last batch that has been submitted.

		std::uint64_t _resource_index = 0; ///< Counter used to uniquely identify resources.

		/// Initializes all fields of the context.
		context(assets::manager&, graphics::context&, graphics::command_queue&);

		/// Creates or finds a \ref graphics::image2d_view from the given \ref renderer::image2d_view, allocating
		/// storage for then image if necessary.
		[[nodiscard]] graphics::image2d_view &_request_image_view(
			_execution_context&, const recorded_resources::image2d_view&
		);
		/// Creates or finds a \ref graphics::image2d_view from the next image in the given \ref swap_chain.
		[[nodiscard]] graphics::image2d_view &_request_image_view(
			_execution_context&, const recorded_resources::swap_chain&
		);

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

		/// Checks and creates a descriptor set for the given resources.
		[[nodiscard]] std::vector<_descriptor_set_info> _check_and_create_descriptor_set_bindings(
			_execution_context&,
			std::initializer_list<const asset<assets::shader>*>,
			const all_resource_bindings&
		);
		/// Binds the given descriptor sets.
		void _bind_descriptor_sets(
			_execution_context&, const graphics::pipeline_resources&,
			std::vector<_descriptor_set_info>, _bind_point
		);

		/// Creates pipeline resources for the given set of shaders.
		[[nodiscard]] graphics::pipeline_resources &_create_pipeline_resources(
			_execution_context&, std::initializer_list<const asset<assets::shader>*>
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
		/// Handles a dispatch compute command.
		void _handle_command(_execution_context&, const context_commands::dispatch_compute&);
		/// Handles a begin pass command.
		void _handle_command(_execution_context&, const context_commands::render_pass&);
		/// Handles a present command.
		void _handle_command(_execution_context&, const context_commands::present&);

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
	};

	/// An instance in a scene.
	struct instance {
		/// Initializes this instance to empty.
		instance(std::nullptr_t) : material(nullptr), geometry(nullptr), transform(uninitialized) {
		}

		assets::owning_handle<assets::material> material; ///< The material of this instance.
		assets::owning_handle<assets::geometry> geometry; ///< Geometry of this instance.
		mat44f transform; ///< Transform of this instance.
	};


	template <typename Type> void _details::context_managed_deleter::operator()(Type *ptr) const {
		assert(_ctx);
		_ctx->_deferred_delete(ptr);
	}
}
