#pragma once

#include <vector>

#include "lotus/system/window.h"
#include "lotus/graphics/resources.h"
#include "lotus/graphics/frame_buffer.h"
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
				std::uint64_t i,
				std::u8string n
			) :
				image(nullptr), current_usages(mips, graphics::image_usage::initial),
				size(sz), num_mips(mips), format(fmt), tiling(tiling), usages(usages),
				id(i), name(std::move(n)) {
			}

			graphics::image2d image; ///< Image for the surface.
			
			std::vector<graphics::image_usage> current_usages; ///< Current usage of each mip of the surface.

			cvec2s size; ///< The size of this surface.
			std::uint32_t num_mips = 0; ///< Number of mips.
			graphics::format format = graphics::format::none; ///< Original pixel format.
			// TODO are these necessary?
			graphics::image_tiling tiling = graphics::image_tiling::optimal; ///< Tiling of this image.
			graphics::image_usage::mask usages = graphics::image_usage::mask::none; ///< Possible usages.

			std::uint64_t id = 0; ///< Used to uniquely identify this surface.
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
				chain(nullptr), current_size(zero), desired_size(zero), current_format(graphics::format::none),
				next_image_index(invalid_image_index), window(wnd), num_images(imgs),
				expected_formats(std::move(fmts)), name(std::move(n)) {
			}

			graphics::swap_chain chain; ///< The swap chain.
			std::vector<graphics::fence> fences; ///< Synchronization primitives for each back buffer.
			std::vector<graphics::image2d> images; ///< Images in this swap chain.
			std::vector<graphics::image_usage> current_usages; ///< Current usages of all back buffers.

			cvec2s current_size; ///< Current size of swap chain images.
			cvec2s desired_size; ///< Desired size of swap chain images.
			graphics::format current_format; ///< Format of the swap chain images.

			std::uint32_t next_image_index = 0; ///< Index of the next image.

			system::window &window; ///< The window that owns this swap chain.
			std::uint32_t num_images; ///< Number of images in the swap chain.
			std::vector<graphics::format> expected_formats; ///< Expected swap chain formats.

			// TODO make this debug only?
			std::u8string name; ///< Name of this swap chain.
		};

		// TODO?
		struct fence {
			// TODO?
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
			/// Default copy construction.
			context_managed_deleter(const context_managed_deleter&) = default;
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
	}


	namespace recorded_resources {
		class image2d_view;
		class swap_chain;
	}

	/// A reference of a view into a 2D image.
	class image2d_view {
		friend context;
		friend recorded_resources::image2d_view;
	public:
		/// Initializes this view to empty.
		image2d_view(std::nullptr_t) : _mip_levels(graphics::mip_levels::all()) {
		}

		/// Creates another view of the image in another format.
		[[nodiscard]] image2d_view view_as(graphics::format fmt) const {
			return image2d_view(_surface, fmt, _mip_levels);
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
		image2d_view(std::shared_ptr<_details::surface2d> surf, graphics::format fmt, graphics::mip_levels mips) :
			_surface(std::move(surf)), _view_format(fmt), _mip_levels(mips) {
		}

		std::shared_ptr<_details::surface2d> _surface; ///< The surface that this is a view of.
		/// The format to view as; may be different from the original format of the surface.
		graphics::format _view_format = graphics::format::none;
		graphics::mip_levels _mip_levels; ///< Mip levels that are included in this view.
	};

	/// A reference of a swap chain.
	class swap_chain {
		friend context;
		friend recorded_resources::swap_chain;
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


	/// Recorded resources. These objects don't hold ownership of the underlying objects, but otherwise they're
	/// exactly the same.
	namespace recorded_resources {
		/// \ref renderer::image2d_view.
		class image2d_view {
			friend context;
		public:
			/// Conversion from a non-recorded \ref renderer::image2d_view.
			explicit image2d_view(const renderer::image2d_view &view) :
				_surface(view._surface.get()), _view_format(view._view_format), _mip_levels(view._mip_levels) {
			}

			/// Returns whether this object holds a valid image.
			[[nodiscard]] bool is_valid() const {
				return _surface;
			}
			/// \overload
			[[nodiscard]] explicit operator bool() const {
				return is_valid();
			}
		private:
			_details::surface2d *_surface = nullptr; ///< The surface.
			graphics::format _view_format = graphics::format::none; ///< The format of this surface.
			graphics::mip_levels _mip_levels; ///< Mip levels.
		};

		/// \ref renderer::swap_chain.
		class swap_chain {
			friend context;
		public:
			/// Conversion from a non-recorded \ref renderer::swap_chain.
			explicit swap_chain(const renderer::swap_chain &c) : _swap_chain(c._swap_chain.get()) {
			}

			/// Returns whether this object holds a valid image.
			[[nodiscard]] bool is_valid() const {
				return _swap_chain;
			}
			/// \overload
			[[nodiscard]] explicit operator bool() const {
				return is_valid();
			}
		private:
			_details::swap_chain *_swap_chain = nullptr; ///< The swap chain.
		};
	}


	/// Reference to a 2D color image that can be rendered to.
	struct surface2d_color {
		/// Initializes the surface to empty.
		surface2d_color(std::nullptr_t) :
			view(std::in_place_type<recorded_resources::image2d_view>, nullptr), access(nullptr) {
		}
		/// Initializes all fields of this struct.
		surface2d_color(const image2d_view &v, graphics::color_render_target_access acc) :
			view(std::in_place_type<recorded_resources::image2d_view>, v), access(acc) {
		}
		/// Initializes all fields of this struct.
		surface2d_color(const swap_chain &v, graphics::color_render_target_access acc) :
			view(std::in_place_type<recorded_resources::swap_chain>, v), access(acc) {
		}

		/// The underlying image.
		std::variant<recorded_resources::image2d_view, recorded_resources::swap_chain> view;
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

		recorded_resources::image2d_view view; ///< The underlying image.
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
			/// Initializes all fields of this struct.
			image2d(const image2d_view &v, image_binding_type type) : view(v), binding_type(type) {
			}
			/// Creates a read-only image binding.
			[[nodiscard]] inline static image2d create_read_only(const image2d_view &img) {
				return image2d(img, image_binding_type::read_only);
			}
			/// Creates a read-write image binding.
			[[nodiscard]] inline static image2d create_read_write(const image2d_view &img) {
				return image2d(img, image_binding_type::read_write);
			}

			recorded_resources::image2d_view view; ///< A view of the image.
			/// Whether this resource is bound as writable.
			image_binding_type binding_type = image_binding_type::read_only;
		};
		/// The next image in a swap chain.
		struct swap_chain_image {
			/// Initializes all fields of this struct.
			explicit swap_chain_image(const swap_chain &chain) : image(chain) {
			}

			recorded_resources::swap_chain image; ///< The swap chain.
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
		resource_set_binding(std::vector<resource_binding> b, std::uint32_t space) :
			bindings(std::move(b)), space(space) {

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
}
