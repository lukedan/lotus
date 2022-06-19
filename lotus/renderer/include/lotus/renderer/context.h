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

namespace lotus::renderer {
	class context;

	/// Image binding types.
	enum class image_binding_type {
		read_only,  ///< Read-only surface.
		read_write, ///< Read-write surface.

		num_enumerators ///< Number of enumerators.
	};

	/// Internal data structures used by the rendering context.
	namespace _details {
		/// Returns the descriptor type that corresponds to the image binding.
		[[nodiscard]] graphics::descriptor_type to_descriptor_type(image_binding_type);


		/// A 2D surface managed by a context.
		struct surface2d {
		public:
			/// Initializes this surface to empty.
			surface2d(
				cvec2s sz,
				std::uint32_t mips,
				graphics::format fmt,
				graphics::image_tiling tiling,
				graphics::image_usage::mask usages,
				std::u8string n
			) :
				image(nullptr), size(sz), current_usage(graphics::image_usage::initial),
				num_mips(mips), original_format(fmt), tiling(tiling), usages(usages), name(std::move(n)) {
			}

			graphics::image2d image; ///< Image for the surface.
			cvec2s size; ///< The size of this surface.
			graphics::image_usage current_usage; ///< Current usage of this surface.

			std::uint32_t num_mips; ///< Number of mips.
			graphics::format original_format; ///< Original pixel format.
			graphics::image_tiling tiling; ///< Tiling of this image. (Not sure if this necessary)
			graphics::image_usage::mask usages; ///< Possible usages. (Not sure if this necessary)

			// TODO make this debug only?
			std::u8string name; ///< Name of this image.
		};

		/// A swap chain associated with a window, managed by a context.
		struct swap_chain {
		public:
			/// Image index indicating that a next image has not been acquired.
			constexpr static std::uint32_t invalid_image_index = std::numeric_limits<std::uint32_t>::max();

			/// Initializes all fields of this structure.
			swap_chain(
				system::window &wnd, std::uint32_t imgs, std::vector<graphics::format> fmts, std::u8string n
			) :
				chain(nullptr), current_size(zero), current_format(graphics::format::none),
				next_image_index(invalid_image_index), window(wnd), num_images(imgs),
				expected_formats(std::move(fmts)), name(std::move(n)) {
			}

			graphics::swap_chain chain; ///< The swap chain.
			std::vector<graphics::fence> fences; ///< Synchronization primitives for each back buffer.

			cvec2s current_size; ///< Current size of swap chain images.
			graphics::format current_format; ///< Format of the swap chain images.

			std::uint32_t next_image_index = 0; ///< Index of the next image.

			system::window &window; ///< The window that owns this swap chain.
			std::uint32_t num_images; ///< Number of images in the swap chain.
			std::vector<graphics::format> expected_formats; ///< Expected swap chain formats.

			// TODO make this debug only?
			std::u8string name; ///< Name of this swap chain.
		};

		struct fence {

		};


		/// Deleter used with \p std::shared_ptr to defer all delete operations to a context.
		struct context_managed_deleter {
		public:
			/// Initializes this deleter to empty.
			context_managed_deleter(std::nullptr_t) {
			}
			/// Initializes \ref _ctx.
			explicit context_managed_deleter(context &ctx) : _ctx(&ctx) {
			}
			/// Default move construction.
			context_managed_deleter(context_managed_deleter&&) = default;
			/// Default copy construction.
			context_managed_deleter(const context_managed_deleter&) = default;
			/// Default move assignment.
			context_managed_deleter &operator=(context_managed_deleter&&) = default;
			/// Default copy assignment.
			context_managed_deleter &operator=(const context_managed_deleter&) = default;

			/// Hands the pointer to the context for deferred disposal.
			template <typename Type> void operator()(Type*) const;

			/// Returns the context currently associated with this deleter.
			[[nodiscard]] context *get_context() const {
				return _ctx;
			}

			/// Assumes that the given shared pointer uses a \ref context_managed_deleter, and returns the associated
			/// context. Asserts if the pointer does not use a \ref context_managed_deleter.
			template <typename Type> [[nodiscard]] inline static context *get_context_from(
				std::shared_ptr<Type> ptr
			) {
				if (ptr) {
					auto *deleter = std::get_deleter<context_managed_deleter>(ptr);
					assert(deleter);
					return deleter->get_context();
				}
				return nullptr;
			}
		private:
			context *_ctx = nullptr; ///< The context.
		};


		/// A descriptor set and its register space.
		struct descriptor_set_info {
			/// Initializes this structure to empty.
			descriptor_set_info(std::nullptr_t) {
			}
			/// Initializes all fields of this struct.
			descriptor_set_info(graphics::descriptor_set &se, std::uint32_t s) : set(&se), space(s) {
			}

			graphics::descriptor_set *set = nullptr; ///< The descriptor set.
			std::uint32_t space = 0; ///< Register space of this descriptor set.
		};
	}


	/// A reference of a view into a 2D image.
	class image2d_view {
		friend context;
	public:
		/// Initializes this view to empty.
		image2d_view(std::nullptr_t) {
		}

		/// Creates another view of the image in another format.
		[[nodiscard]] image2d_view view_as(graphics::format fmt) const {
			return image2d_view(_surface, fmt);
		}

		/// Returns whether this object holds a valid image view.
		[[nodiscard]] bool is_valid() const {
			return _surface.get();
		}
		/// \overload
		[[nodiscard]] explicit operator bool() const {
			return is_valid();
		}
	private:
		/// Initializes all fields of this struct.
		image2d_view(std::shared_ptr<_details::surface2d> surf, graphics::format fmt) :
			_surface(std::move(surf)), _view_format(fmt) {
		}

		std::shared_ptr<_details::surface2d> _surface; ///< The surface that this is a view of.
		/// The format to view as; may be different from the original format of the surface.
		graphics::format _view_format = graphics::format::none;
	};

	/// A reference of a swap chain.
	class swap_chain {
		friend context;
	public:
		/// Initializes this object to empty.
		swap_chain(std::nullptr_t) {
		}

		/// Resizes this swap chain.
		void resize(cvec2s);

		/// Returns whether this object holds a valid image view.
		[[nodiscard]] bool is_valid() const {
			return _swap_chain.get();
		}
		/// \overload
		[[nodiscard]] explicit operator bool() const {
			return is_valid();
		}
	private:
		/// Initializes this swap chain.
		explicit swap_chain(std::shared_ptr<_details::swap_chain> chain) : _swap_chain(std::move(chain)) {
		}

		std::shared_ptr<_details::swap_chain> _swap_chain; ///< The swap chain.
	};

	/// Reference to a 2D color image that can be rendered to.
	struct surface2d_color {
		/// Initializes the surface to empty.
		surface2d_color(std::nullptr_t) : view(std::in_place_type<image2d_view>, nullptr), access(nullptr) {
		}
		/// Initializes all fields of this struct.
		surface2d_color(image2d_view v, graphics::color_render_target_access acc) :
			view(std::in_place_type<image2d_view>, std::move(v)), access(acc) {
		}
		/// Initializes all fields of this struct.
		surface2d_color(swap_chain v, graphics::color_render_target_access acc) :
			view(std::in_place_type<swap_chain>, std::move(v)), access(acc) {
		}

		std::variant<image2d_view, swap_chain> view; ///< The underlying image.
		graphics::color_render_target_access access; ///< Usage of this surface in a render pass.
	};
	/// Reference to a 2D depth-stencil image that can be rendered to.
	struct surface2d_depth_stencil {
		/// Initializes this surface to empty.
		surface2d_depth_stencil(std::nullptr_t) : view(nullptr), depth_access(nullptr), stencil_access(nullptr) {
		}
		/// Initializes all fields of this struct.
		surface2d_depth_stencil(
			image2d_view v,
			graphics::depth_render_target_access d,
			graphics::stencil_render_target_access s = nullptr
		) : view(std::move(v)), depth_access(d), stencil_access(s) {
		}

		image2d_view view; ///< The underlying image.
		graphics::depth_render_target_access depth_access; ///< Usage of the depth values in a render pass.
		graphics::stencil_render_target_access stencil_access; ///< Usage of the stencil values in a render pass.
	};

	/// An input buffer binding. Largely similar to \ref graphics::input_buffer_layout.
	struct input_buffer_binding {
		/// Initializes this buffer to empty.
		input_buffer_binding(std::nullptr_t) : buffer(nullptr) {
		}
		/// Initializes all fields of this struct.
		input_buffer_binding(
			std::uint32_t index,
			assets::owning_handle<assets::buffer> buf, std::uint32_t off, std::uint32_t str,
			graphics::input_buffer_rate rate, std::vector<graphics::input_buffer_element> elems
		) :
			elements(std::move(elems)), buffer(std::move(buf)), stride(str), offset(off),
			buffer_index(index), input_rate(rate) {
		}
		/// Creates a buffer corresponding to the given input.
		[[nodiscard]] inline static input_buffer_binding create(
			assets::owning_handle<assets::buffer> buf, std::uint32_t off, graphics::input_buffer_layout layout
		) {
			return input_buffer_binding(
				static_cast<std::uint32_t>(layout.buffer_index), std::move(buf),
				off, static_cast<std::uint32_t>(layout.stride),
				layout.input_rate, { layout.elements.begin(), layout.elements.end() }
			);
		}

		std::vector<graphics::input_buffer_element> elements; ///< Elements in this vertex buffer.
		assets::owning_handle<assets::buffer> buffer; ///< The buffer.
		std::uint32_t stride = 0; ///< The size of one vertex.
		std::uint32_t offset = 0; ///< Offset from the beginning of the buffer.
		std::uint32_t buffer_index = 0; ///< Index of the vertex buffer.
		/// Specifies how the buffer data is used.
		graphics::input_buffer_rate input_rate = graphics::input_buffer_rate::per_vertex;
	};
	/// An index buffer binding.
	struct index_buffer_binding {
		/// Initializes this binding to empty.
		index_buffer_binding(std::nullptr_t) : buffer(nullptr) {
		}
		/// Initializes all fields of this struct.
		index_buffer_binding(
			assets::owning_handle<assets::buffer> buf, std::uint32_t off, graphics::index_format fmt
		) : buffer(std::move(buf)), offset(off), format(fmt) {
		}

		assets::owning_handle<assets::buffer> buffer; ///< The index buffer.
		std::uint32_t offset = 0; ///< Offset from the beginning of the buffer where indices start.
		graphics::index_format format = graphics::index_format::uint32; ///< Format of indices.
	};

	namespace descriptor_resource {
		/// A 2D image.
		struct image2d {
		public:
			/// Creates a read-only image binding.
			[[nodiscard]] inline static image2d create_read_only(image2d_view img) {
				return image2d(std::move(img), false);
			}
			/// Creates a read-write image binding.
			[[nodiscard]] inline static image2d create_read_write(image2d_view img) {
				return image2d(std::move(img), true);
			}

			image2d_view view; ///< A view of the image.
			bool writable; ///< Whether this resource is bound as writable.
		private:
			/// Initializes all fields of this struct.
			image2d(image2d_view v, bool write) : view(std::move(v)), writable(write) {
			}
		};
		/// The next image in a swap chain.
		struct swap_chain_image {
			/// Initializes all fields of this struct.
			explicit swap_chain_image(swap_chain chain) : image(std::move(chain)) {
			}

			swap_chain image; ///< The swap chain.
		};
		/// Constant buffer with data that will be copied to VRAM when a command list is executed.
		struct immediate_constant_buffer {
			/// Initializes all fields of this struct.
			explicit immediate_constant_buffer(std::vector<std::byte> d) : data(std::move(d)) {
			}
			/// Creates a buffer with data from the given object.
			template <typename T> [[nodiscard]] inline static std::enable_if_t<
				std::is_trivially_copyable_v<std::decay_t<T>>, immediate_constant_buffer
			> create_for(const T &obj) {
				constexpr std::size_t size = sizeof(std::decay_t<T>);
				std::vector<std::byte> data(size);
				std::memcpy(data.data(), &obj, size);
				return immediate_constant_buffer(std::move(data));
			}

			std::vector<std::byte> data; ///< Constant buffer data.
		};
		/// A sampler.
		struct sampler {
			/// Initializes the sampler value to a default point sampler.
			sampler(std::nullptr_t) {
			}
			/// Initializes all fields of this struct.
			sampler(
				graphics::filtering min = graphics::filtering::linear,
				graphics::filtering mag = graphics::filtering::linear,
				graphics::filtering mip = graphics::filtering::linear,
				float lod_bias = 0.0f, float minlod = 0.0f, float maxlod = std::numeric_limits<float>::max(),
				std::optional<float> max_aniso = std::nullopt,
				graphics::sampler_address_mode addr_u = graphics::sampler_address_mode::repeat,
				graphics::sampler_address_mode addr_v = graphics::sampler_address_mode::repeat,
				graphics::sampler_address_mode addr_w = graphics::sampler_address_mode::repeat,
				linear_rgba_f border = zero,
				std::optional<graphics::comparison_function> comp = std::nullopt
			) :
				minification(min), magnification(mag), mipmapping(mip),
				mip_lod_bias(lod_bias), min_lod(minlod), max_lod(maxlod), max_anisotropy(max_aniso),
				addressing_u(addr_u), addressing_v(addr_v), addressing_w(addr_w),
				border_color(border), comparison(comp) {
			}

			graphics::filtering minification  = graphics::filtering::nearest;
			graphics::filtering magnification = graphics::filtering::nearest;
			graphics::filtering mipmapping    = graphics::filtering::nearest;
			float mip_lod_bias = 0.0f;
			float min_lod      = 0.0f;
			float max_lod      = 0.0f;
			std::optional<float> max_anisotropy;
			graphics::sampler_address_mode addressing_u = graphics::sampler_address_mode::repeat;
			graphics::sampler_address_mode addressing_v = graphics::sampler_address_mode::repeat;
			graphics::sampler_address_mode addressing_w = graphics::sampler_address_mode::repeat;
			linear_rgba_f border_color = zero;
			std::optional<graphics::comparison_function> comparison;

		};


		/// A union of all possible resource types.
		using value = std::variant<image2d, swap_chain_image, immediate_constant_buffer, sampler>;
	}

	/// The binding of a single resource.
	struct resource_binding {
		/// Initializes all fields of this struct.
		resource_binding(descriptor_resource::value rsrc, std::uint32_t reg) :
			resource(std::move(rsrc)), register_index(reg) {
		}

		/// The resource to bind to this register.
		descriptor_resource::value resource;
		std::uint32_t register_index = 0; ///< Register index to bind to.
	};
	/// The binding of a set of resources corresponding to a descriptor set or register space.
	struct resource_set_binding {
		/// Initializes all fields of this struct, and sorts all bindings based on register index.
		resource_set_binding(std::vector<resource_binding> bindings, std::uint32_t space) :
			bindings(std::move(bindings)), space(space) {

			// sort based on register index
			std::sort(
				bindings.begin(), bindings.end(),
				[](const resource_binding &lhs, const resource_binding &rhs) {
					return lhs.register_index < rhs.register_index;
				}
			);
		}

		std::vector<resource_binding> bindings; ///< All resource bindings.
		std::uint32_t space; ///< Register space to bind to.
	};
	/// Contains information about all resource bindings used during a compute shader dispatch or a pass.
	struct all_resource_bindings {
		/// Initializes the struct to empty.
		all_resource_bindings(std::nullptr_t) {
		}
		/// Initializes the resource sets and sorts them based on their register space.
		[[nodiscard]] static all_resource_bindings from_unsorted(std::vector<resource_set_binding>);

		/// Removes duplicate sets and sorts all sets and bindings.
		void consolidate();

		std::vector<resource_set_binding> sets; ///< Sets of resource bindings.
	};


	/// Commands to be executed during a render pass.
	namespace pass_commands {
		/// Cached data used by a single command.
		struct command_data {
			/// Initializes this structure to empty.
			command_data(std::nullptr_t) {
			}

			graphics::pipeline_resources *resources = nullptr; ///< Pipeline resources.
			graphics::graphics_pipeline_state *pipeline_state = nullptr; ///< Pipeline state.
			std::vector<_details::descriptor_set_info> descriptor_sets; ///< Descriptor sets.
		};


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

			/// Registers the given object as a resource.
			template <typename T> T &record(T obj) {
				if constexpr (std::is_same_v<T, graphics::descriptor_set>) {
					return _resources.descriptor_sets.emplace_back(std::move(obj));
				} else if constexpr (std::is_same_v<T, graphics::pipeline_resources>) {
					return _resources.pipeline_resources.emplace_back(std::move(obj));
				} else if constexpr (std::is_same_v<T, graphics::compute_pipeline_state>) {
					return _resources.compute_pipelines.emplace_back(std::move(obj));
				} else if constexpr (std::is_same_v<T, graphics::graphics_pipeline_state>) {
					return _resources.graphics_pipelines.emplace_back(std::move(obj));
				} else if constexpr (std::is_same_v<T, graphics::image2d>) {
					return _resources.images.emplace_back(std::move(obj));
				} else if constexpr (std::is_same_v<T, graphics::sampler>) {
					return _resources.samplers.emplace_back(std::move(obj));
				} else if constexpr (std::is_same_v<T, graphics::command_list>) {
					return _resources.command_lists.emplace_back(std::move(obj));
				} else {
					static_assert(sizeof(T*) == 0, "Unhandled resource type");
				}
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
		private:
			/// Initializes this context.
			_execution_context(context &ctx, _batch_resources &rsrc) : _ctx(ctx), _resources(rsrc) {
			}

			context &_ctx; ///< The associated context.
			_batch_resources &_resources; ///< Internal resources.
			graphics::command_list *_list = nullptr; ///< Current command list.
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

		/// Initializes all fields of the context.
		context(assets::manager&, graphics::context&, graphics::command_queue&);

		/// Creates or finds a \ref graphics::image2d_view from the given \ref renderer::image2d_view, allocating
		/// storage for then image if necessary.
		[[nodiscard]] graphics::image2d_view &_request_image_view(_execution_context&, const image2d_view&);
		/// Creates or finds a \ref graphics::image2d_view from the next image in the given \ref swap_chain.
		[[nodiscard]] graphics::image2d_view &_request_image_view(_execution_context&, const swap_chain&);

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
		[[nodiscard]] std::vector<_details::descriptor_set_info> _check_and_create_descriptor_set_bindings(
			_execution_context&,
			std::initializer_list<const asset<assets::shader>*>,
			const all_resource_bindings&
		);
		/// Binds the given descriptor sets.
		void _bind_descriptor_sets(
			_execution_context&, const graphics::pipeline_resources&,
			std::vector<_details::descriptor_set_info>, _bind_point
		);

		/// Creates pipeline resources for the given set of shaders.
		[[nodiscard]] graphics::pipeline_resources &_create_pipeline_resources(
			_execution_context&, std::initializer_list<const asset<assets::shader>*>
		);

		/// Preprocesses the given instanced draw command.
		[[nodiscard]] pass_commands::command_data _preprocess_command(
			_execution_context&, const graphics::frame_buffer_layout&, const pass_commands::draw_instanced&
		);

		/// Handles a instanced draw command.
		void _handle_pass_command(
			_execution_context&, pass_commands::command_data, const pass_commands::draw_instanced&
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
			delete surf;
		}
		/// Interface to \ref _details::context_managed_deleter for deferring deletion of a swap chain.
		void _deferred_delete(_details::swap_chain *chain) {
			if (chain->chain) {
				_deferred_delete_resources.swap_chains.emplace_back(std::move(chain->chain));
				for (auto &fence : chain->fences) {
					_deferred_delete_resources.fences.emplace_back(std::move(fence));
				}
			}
			delete chain;
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
