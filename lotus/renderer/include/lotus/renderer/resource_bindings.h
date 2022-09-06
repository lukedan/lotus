#pragma once

/// \file
/// Shader resource bindings.

#include "resources.h"

namespace lotus::renderer {
	/// Reference to a 2D color image that can be rendered to.
	struct surface2d_color {
		/// Initializes the surface to empty.
		surface2d_color(std::nullptr_t) :
			view(std::in_place_type<recorded_resources::image2d_view>, nullptr), access(nullptr) {
		}
		/// Initializes all fields of this struct.
		surface2d_color(recorded_resources::image2d_view v, gpu::color_render_target_access acc) :
			view(std::in_place_type<recorded_resources::image2d_view>, v), access(acc) {
		}
		/// Initializes all fields of this struct.
		surface2d_color(recorded_resources::swap_chain v, gpu::color_render_target_access acc) :
			view(std::in_place_type<recorded_resources::swap_chain>, v), access(acc) {
		}

		/// The underlying image.
		std::variant<recorded_resources::image2d_view, recorded_resources::swap_chain> view;
		gpu::color_render_target_access access; ///< Usage of this surface in a render pass.
	};
	/// Reference to a 2D depth-stencil image that can be rendered to.
	struct surface2d_depth_stencil {
		/// Initializes this surface to empty.
		surface2d_depth_stencil(std::nullptr_t) : view(nullptr), depth_access(nullptr), stencil_access(nullptr) {
		}
		/// Initializes all fields of this struct.
		surface2d_depth_stencil(
			recorded_resources::image2d_view v,
			gpu::depth_render_target_access d,
			gpu::stencil_render_target_access s = nullptr
		) : view(std::move(v)), depth_access(d), stencil_access(s) {
		}

		recorded_resources::image2d_view view; ///< The underlying image.
		gpu::depth_render_target_access depth_access; ///< Usage of the depth values in a render pass.
		gpu::stencil_render_target_access stencil_access; ///< Usage of the stencil values in a render pass.
	};

	namespace descriptor_resource {
		/// A 2D image.
		struct image2d {
			/// Initializes all fields of this struct.
			image2d(recorded_resources::image2d_view v, image_binding_type type) : view(v), binding_type(type) {
			}
			/// Creates a read-only image binding.
			[[nodiscard]] inline static image2d create_read_only(recorded_resources::image2d_view img) {
				return image2d(img, image_binding_type::read_only);
			}
			/// Creates a read-write image binding.
			[[nodiscard]] inline static image2d create_read_write(recorded_resources::image2d_view img) {
				return image2d(img, image_binding_type::read_write);
			}

			recorded_resources::image2d_view view; ///< A view of the image.
			image_binding_type binding_type = image_binding_type::read_only; ///< Usage of the bound image.
		};
		/// The next image in a swap chain.
		struct swap_chain_image {
			/// Initializes all fields of this struct.
			explicit swap_chain_image(recorded_resources::swap_chain chain) : image(chain) {
			}

			recorded_resources::swap_chain image; ///< The swap chain.
		};
		/// A buffer.
		struct buffer {
			/// Initializes all fields of this struct.
			buffer(
				recorded_resources::buffer d,
				buffer_binding_type t,
				std::uint32_t first,
				std::uint32_t str,
				std::uint32_t c
			) : data(d), binding_type(t), first_element(first), stride(str), count(c) {
			}
			/// Creates a read-only buffer binding.
			[[nodiscard]] inline static buffer create_read_only(
				recorded_resources::buffer buf, std::uint32_t first, std::uint32_t str, std::uint32_t c
			) {
				return buffer(buf, buffer_binding_type::read_only, first, str, c);
			}
			/// Creates a read-write buffer binding.
			[[nodiscard]] inline static buffer create_read_write(
				recorded_resources::buffer buf, std::uint32_t first, std::uint32_t str, std::uint32_t c
			) {
				return buffer(buf, buffer_binding_type::read_write, first, str, c);
			}

			recorded_resources::buffer data; ///< Buffer data.
			buffer_binding_type binding_type = buffer_binding_type::read_only; ///< Usage of the bound buffer.
			std::uint32_t first_element = 0; ///< Index of the first visible element.
			std::uint32_t stride        = 0; ///< Byte offset between two consecutive elements.
			std::uint32_t count         = 0; ///< Number of visible elements.
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
				gpu::filtering min = gpu::filtering::linear,
				gpu::filtering mag = gpu::filtering::linear,
				gpu::filtering mip = gpu::filtering::linear,
				float lod_bias = 0.0f, float minlod = 0.0f, float maxlod = std::numeric_limits<float>::max(),
				std::optional<float> max_aniso = std::nullopt,
				gpu::sampler_address_mode addr_u = gpu::sampler_address_mode::repeat,
				gpu::sampler_address_mode addr_v = gpu::sampler_address_mode::repeat,
				gpu::sampler_address_mode addr_w = gpu::sampler_address_mode::repeat,
				linear_rgba_f border = zero,
				std::optional<gpu::comparison_function> comp = std::nullopt
			) :
				minification(min), magnification(mag), mipmapping(mip),
				mip_lod_bias(lod_bias), min_lod(minlod), max_lod(maxlod), max_anisotropy(max_aniso),
				addressing_u(addr_u), addressing_v(addr_v), addressing_w(addr_w),
				border_color(border), comparison(comp) {
			}

			gpu::filtering minification  = gpu::filtering::nearest;
			gpu::filtering magnification = gpu::filtering::nearest;
			gpu::filtering mipmapping    = gpu::filtering::nearest;
			float mip_lod_bias = 0.0f;
			float min_lod      = 0.0f;
			float max_lod      = 0.0f;
			std::optional<float> max_anisotropy;
			gpu::sampler_address_mode addressing_u = gpu::sampler_address_mode::repeat;
			gpu::sampler_address_mode addressing_v = gpu::sampler_address_mode::repeat;
			gpu::sampler_address_mode addressing_w = gpu::sampler_address_mode::repeat;
			linear_rgba_f border_color = zero;
			std::optional<gpu::comparison_function> comparison;
		};


		/// A union of all possible resource types.
		using value = std::variant<image2d, swap_chain_image, buffer, immediate_constant_buffer, sampler>;
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
		/// Bindings composed of individual descriptors.
		struct descriptor_bindings {
			/// Sorts all bindings based on register index.
			explicit descriptor_bindings(std::vector<resource_binding> b) : bindings(std::move(b)) {
				// sort based on register index
				std::sort(
					bindings.begin(), bindings.end(),
					[](const resource_binding &lhs, const resource_binding &rhs) {
						return lhs.register_index < rhs.register_index;
					}
				);
			}

			/// Converts this object into a \ref resource_set_binding object at the given register space.
			[[nodiscard]] resource_set_binding at_space(std::uint32_t s) && {
				return resource_set_binding(std::move(*this), s);
			}

			std::vector<resource_binding> bindings; ///< All resource bindings.
		};
		/// Initializes this struct from a \ref descriptor_bindings object.
		resource_set_binding(descriptor_bindings b, std::uint32_t s) :
			bindings(std::in_place_type<descriptor_bindings>, std::move(b)), space(s) {
		}
		/// Initializes this struct from a \ref descriptor_array.
		resource_set_binding(recorded_resources::descriptor_array arr, std::uint32_t s) :
			bindings(std::in_place_type<recorded_resources::descriptor_array>, arr), space(s) {
		}
		/// Shorthand for initializing a \ref descriptor_bindings object and then creating a
		/// \ref resource_set_binding.
		[[nodiscard]] inline static resource_set_binding create(std::vector<resource_binding> b, std::uint32_t s) {
			return resource_set_binding(descriptor_bindings(std::move(b)), s);
		}

		std::variant<descriptor_bindings, recorded_resources::descriptor_array> bindings; ///< Bindings.
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