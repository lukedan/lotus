#pragma once

/// \file
/// Shader resource bindings.

#include "resources.h"
#include "assets.h"

namespace lotus::renderer {
	struct resource_binding;


	/// Reference to a 2D color image that can be rendered to.
	struct image2d_color {
		/// Initializes the surface to empty.
		image2d_color(std::nullptr_t) :
			view(std::in_place_type<recorded_resources::image2d_view>, nullptr), access(nullptr) {
		}
		/// Initializes all fields of this struct.
		image2d_color(recorded_resources::image2d_view v, gpu::color_render_target_access acc) :
			view(std::in_place_type<recorded_resources::image2d_view>, v), access(acc) {
		}
		/// Initializes all fields of this struct.
		image2d_color(recorded_resources::swap_chain v, gpu::color_render_target_access acc) :
			view(std::in_place_type<recorded_resources::swap_chain>, v), access(acc) {
		}

		/// The underlying image.
		std::variant<recorded_resources::image2d_view, recorded_resources::swap_chain> view;
		gpu::color_render_target_access access; ///< Usage of this surface in a render pass.
	};
	/// Reference to a 2D depth-stencil image that can be rendered to.
	struct image2d_depth_stencil {
		/// Initializes this surface to empty.
		image2d_depth_stencil(std::nullptr_t) : view(nullptr), depth_access(nullptr), stencil_access(nullptr) {
		}
		/// Initializes all fields of this struct.
		image2d_depth_stencil(
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
		namespace _details {
			/// CRTP base class of descriptor resources.
			template <typename Resource> struct resource_base {
				/// Shorthand for \ref resource_binding::resource_binding().
				[[nodiscard]] resource_binding at_register(std::uint32_t) &&;
			};
		}

		/// A 2D image.
		struct image2d : public _details::resource_base<image2d> {
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
		struct swap_chain_image : public _details::resource_base<swap_chain_image> {
			/// Initializes all fields of this struct.
			explicit swap_chain_image(recorded_resources::swap_chain chain) : image(chain) {
			}

			recorded_resources::swap_chain image; ///< The swap chain.
		};
		/// A structured buffer.
		struct structured_buffer : public _details::resource_base<structured_buffer> {
			/// Initializes all fields of this struct.
			structured_buffer(
				recorded_resources::structured_buffer_view d, buffer_binding_type t
			) : data(d), binding_type(t) {
			}
			/// Creates a read-only buffer binding.
			[[nodiscard]] inline static structured_buffer create_read_only(
				recorded_resources::structured_buffer_view buf
			) {
				return structured_buffer(buf, buffer_binding_type::read_only);
			}
			/// Creates a read-write buffer binding.
			[[nodiscard]] inline static structured_buffer create_read_write(
				recorded_resources::structured_buffer_view buf
			) {
				return structured_buffer(buf, buffer_binding_type::read_write);
			}

			recorded_resources::structured_buffer_view data; ///< Buffer data.
			buffer_binding_type binding_type = buffer_binding_type::read_only; ///< Usage of the bound buffer.
		};
		/// Constant buffer with data that will be copied to VRAM when a command list is executed.
		struct immediate_constant_buffer : public _details::resource_base<immediate_constant_buffer> {
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
		/// A top-level acceleration structure.
		struct tlas : public _details::resource_base<tlas> {
			/// Initializes all fields of this structure.
			explicit tlas(const renderer::tlas &as) : acceleration_structure(as) {
			}

			recorded_resources::tlas acceleration_structure; ///< The acceleration structure.
		};
		/// A sampler.
		struct sampler : public sampler_state, public _details::resource_base<sampler> {
			using sampler_state::sampler_state;
		};


		/// A union of all possible resource types.
		using value = std::variant<
			image2d,
			swap_chain_image,
			structured_buffer,
			immediate_constant_buffer,
			tlas,
			sampler
		>;
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
	public:
		/// Bindings composed of individual descriptors.
		struct descriptors {
			/// Sorts all bindings based on register index.
			explicit descriptors(std::vector<resource_binding> b) : bindings(std::move(b)) {
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
		/// Initializes this struct from an \ref image_descriptor_array.
		resource_set_binding(recorded_resources::image_descriptor_array arr, std::uint32_t s) :
			bindings(std::in_place_type<recorded_resources::image_descriptor_array>, arr), space(s) {
		}
		/// Initializes this struct from a \ref buffer_descriptor_array.
		resource_set_binding(recorded_resources::buffer_descriptor_array arr, std::uint32_t s) :
			bindings(std::in_place_type<recorded_resources::buffer_descriptor_array>, arr), space(s) {
		}
		/// Initializes this struct from a \ref cached_descriptor_set.
		resource_set_binding(recorded_resources::cached_descriptor_set set, std::uint32_t s) :
			bindings(std::in_place_type<recorded_resources::cached_descriptor_set>, set), space(s) {
		}

		std::variant<
			descriptors,
			recorded_resources::image_descriptor_array,
			recorded_resources::buffer_descriptor_array,
			recorded_resources::cached_descriptor_set
		> bindings; ///< Bindings.
		std::uint32_t space; ///< Register space to bind to.
	private:
		/// Initializes this struct from a \ref descriptors object.
		resource_set_binding(descriptors b, std::uint32_t s) :
			bindings(std::in_place_type<descriptors>, std::move(b)), space(s) {
		}
	};
	/// Contains information about all resource bindings used during a compute shader dispatch or a pass.
	struct all_resource_bindings {
		/// Initializes the struct to empty.
		all_resource_bindings(std::nullptr_t) {
		}
		/// Initializes the resource sets and sorts them based on their register space.
		[[nodiscard]] static all_resource_bindings from_unsorted(std::vector<resource_set_binding>);
		/// Initializes the resource sets using the given data, assuming that all the registers and register spaces
		/// have been properly sorted.
		[[nodiscard]] static all_resource_bindings assume_sorted(std::vector<resource_set_binding>);

		/// Removes duplicate sets and sorts all sets and bindings.
		void consolidate();

		std::vector<resource_set_binding> sets; ///< Sets of resource bindings.
	};


	/// References a function in a shader library.
	struct shader_function {
		/// Initializes this object to empty.
		shader_function(std::nullptr_t) : shader_library(nullptr) {
		}
		/// Initializes all fields of this struct.
		shader_function(assets::handle<assets::shader_library> lib, const char8_t *entry, gpu::shader_stage s) :
			shader_library(std::move(lib)), entry_point(entry), stage(s) {
		}

		/// Equality and inequality comparisons.
		[[nodiscard]] friend bool operator==(const shader_function &lhs, const shader_function &rhs) {
			return
				lhs.shader_library == rhs.shader_library &&
				std::strcmp(
					reinterpret_cast<const char*>(lhs.entry_point),
					reinterpret_cast<const char*>(rhs.entry_point)
				) == 0 &&
				lhs.stage == rhs.stage;
		}

		assets::handle<assets::shader_library> shader_library; ///< The shader library.
		const char8_t *entry_point = nullptr; ///< Entry point.
		gpu::shader_stage stage = gpu::shader_stage::all; ///< Shader stage of the entry point.
	};
}
namespace std {
	/// Hash function for \ref lotus::renderer::shader_function.
	template <> struct hash<lotus::renderer::shader_function> {
		/// Hashes the given object.
		[[nodiscard]] size_t operator()(const lotus::renderer::shader_function&) const;
	};
}


// implementations
namespace lotus::renderer {
	namespace descriptor_resource::_details {
		template <typename Resource> resource_binding resource_base<Resource>::at_register(std::uint32_t reg) && {
			return resource_binding(std::move(*static_cast<Resource*>(this)), reg);
		}
	}
}
