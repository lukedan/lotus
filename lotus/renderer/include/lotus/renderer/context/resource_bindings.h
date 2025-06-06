#pragma once

/// \file
/// Shader resource bindings.

#include "lotus/containers/short_vector.h"
#include "lotus/renderer/common.h"

namespace lotus::renderer {
	struct resource_binding;


	/// Recorded resources. These objects don't hold ownership of the underlying objects, but otherwise they're
	/// exactly the same.
	namespace recorded_resources {
		/// Template for resources that requires only one pointer to a type.
		template <typename Resource> struct basic_handle {
			friend context;
			friend execution::descriptor_set_builder;
			friend execution::queue_context;
			friend execution::queue_pseudo_context;
			friend execution::batch_context;
		public:
			/// Initializes this resource handle to empty.
			basic_handle(std::nullptr_t) {
			}
			/// Conversion from a non-recorded resource.
			basic_handle(const basic_resource_handle<Resource>&);

			/// Returns whether this handle points to a valid object.
			[[nodiscard]] bool is_valid() const {
				return _ptr != nullptr;
			}
			/// \overload
			[[nodiscard]] explicit operator bool() const {
				return is_valid();
			}
		protected:
			Resource *_ptr = nullptr; ///< Pointer to the resource.
		};


		/// \ref renderer::basic_image_view.
		template <gpu::image_type Type> struct basic_image_view :
			public basic_handle<_details::image_data_t<Type>> {

			friend context;
			friend execution::batch_context;
			friend execution::queue_context;
			friend execution::queue_pseudo_context;
		public:
			/// Initializes this struct to empty.
			basic_image_view(std::nullptr_t) : _base(nullptr), _mip_levels(gpu::mip_levels::all()) {
			}
			/// Conversion from a non-recorded \ref renderer::basic_image_view.
			basic_image_view(const renderer::image_view_base<Type>&);

			/// Returns a copy of this structure that ensures only the first specified mip is used, and logs a
			/// warning if that's not the case.
			[[nodiscard]] basic_image_view highest_mip_with_warning() const;
		private:
			using _base = basic_handle<_details::image_data_t<Type>>; ///< Base type.

			gpu::format _view_format = gpu::format::none; ///< The format of this image.
			gpu::mip_levels _mip_levels; ///< Mip levels.
		};

		/// \ref renderer::buffer.
		using buffer = basic_handle<_details::buffer>;

		/// \ref renderer::structured_buffer_view.
		struct structured_buffer_view : public basic_handle<_details::buffer> {
			friend context;
			friend execution::batch_context;
			friend execution::queue_context;
			friend execution::queue_pseudo_context;
			friend execution::descriptor_set_builder;
		public:
			/// Initializes this struct to empty.
			structured_buffer_view(std::nullptr_t) : basic_handle(nullptr) {
			}
			/// Conversion from a non-recorded \ref renderer::structured_buffer_view.
			structured_buffer_view(const renderer::structured_buffer_view&);
		private:
			u32 _stride = 0; ///< Byte stride between elements.
			u32 _first = 0; ///< The first buffer element.
			u32 _count = 0; ///< Number of visible buffer elements.
		};

		/// \ref renderer::swap_chain.
		using swap_chain = basic_handle<_details::swap_chain>;

		/// \ref renderer::blas.
		using blas = basic_handle<_details::blas>;

		/// \ref renderer::tlas.
		using tlas = basic_handle<_details::tlas>;

		/// \ref renderer::cached_descriptor_set.
		using cached_descriptor_set = basic_handle<_details::cached_descriptor_set>;

		/// \ref renderer::dependency.
		using dependency = basic_handle<_details::dependency>;


		/// Type mapping from non-recorded resources.
		template <typename T> struct mapped_type {
			using type = T; ///< Mapped type.
		};
		/// Type mapping for \ref renderer::swap_chain.
		template <> struct mapped_type<renderer::swap_chain> {
			using type = recorded_resources::swap_chain; ///< Maps to \ref recorded_resources::swap_chain.
		};
		/// Type mapping for \ref renderer::tlas.
		template <> struct mapped_type<renderer::tlas> {
			using type = recorded_resources::tlas; ///< Maps to \ref recorded_resources::tlas.
		};
		/// Type mapping for \ref renderer::image_descriptor_array.
		template <> struct mapped_type<renderer::image_descriptor_array> {
			/// Maps to \ref recorded_resources::image_descriptor_array.
			using type = recorded_resources::image_descriptor_array;
		};
		/// Type mapping for \ref renderer::buffer_descriptor_array.
		template <> struct mapped_type<renderer::buffer_descriptor_array> {
			/// Maps to \ref recorded_resources::buffer_descriptor_array.
			using type = recorded_resources::buffer_descriptor_array;
		};

		/// Shorthand for \ref mapped_type::type.
		template <typename T> using mapped_type_t = mapped_type<T>::type;
	}


	/// An input buffer binding. Largely similar to \ref gpu::input_buffer_layout.
	struct input_buffer_binding {
		/// Initializes this buffer to empty.
		input_buffer_binding(std::nullptr_t) : data(nullptr) {
		}
		/// Initializes all fields of this struct.
		input_buffer_binding(
			u32 index,
			recorded_resources::buffer d, u32 off, u32 str,
			gpu::input_buffer_rate rate, short_vector<gpu::input_buffer_element, 4> elems
		) :
			elements(std::move(elems)), data(d), stride(str), offset(off),
			buffer_index(index), input_rate(rate) {
		}
		/// Creates a buffer corresponding to the given input.
		[[nodiscard]] inline static input_buffer_binding create(
			recorded_resources::buffer buf, u32 off, gpu::input_buffer_layout layout
		) {
			return input_buffer_binding(
				static_cast<u32>(layout.buffer_index),
				buf, off, static_cast<u32>(layout.stride),
				layout.input_rate, { layout.elements.begin(), layout.elements.end() }
			);
		}

		short_vector<gpu::input_buffer_element, 4> elements; ///< Elements in this vertex buffer.
		recorded_resources::buffer data; ///< The buffer.
		u32 stride = 0; ///< The size of one vertex.
		u32 offset = 0; ///< Offset from the beginning of the buffer in bytes.
		u32 buffer_index = 0; ///< Binding index for this input buffer.
		/// Specifies how the buffer data is used.
		gpu::input_buffer_rate input_rate = gpu::input_buffer_rate::per_vertex;
	};
	/// An index buffer binding.
	struct index_buffer_binding {
		/// Initializes this binding to empty.
		index_buffer_binding(std::nullptr_t) : data(nullptr) {
		}
		/// Initializes all fields of this struct.
		index_buffer_binding(
			recorded_resources::buffer buf, u32 off, gpu::index_format fmt
		) : data(buf), offset(off), format(fmt) {
		}

		recorded_resources::buffer data; ///< The index buffer.
		u32 offset = 0; ///< Offset in bytes from the beginning of the buffer where indices start in bytes.
		gpu::index_format format = gpu::index_format::uint32; ///< Format of indices.
	};
	/// A view into buffers related to a geometry used for ray tracing.
	struct geometry_buffers_view {
		/// Initializes this structure to empty.
		geometry_buffers_view(std::nullptr_t) : vertex_data(nullptr), index_data(nullptr) {
		}
		/// Initializes all fields of this struct.
		geometry_buffers_view(
			recorded_resources::buffer verts,
			gpu::format vert_fmt,
			u32 vert_off,
			u32 vert_stride,
			u32 vert_count,
			recorded_resources::buffer indices,
			gpu::index_format idx_fmt,
			u32 idx_off,
			u32 idx_count,
			gpu::raytracing_geometry_flags f
		) :
			vertex_data(verts),
			vertex_format(vert_fmt),
			vertex_offset(vert_off),
			vertex_stride(vert_stride),
			vertex_count(vert_count),
			index_data(indices),
			index_format(idx_fmt),
			index_offset(idx_off),
			index_count(idx_count),
			flags(f) {
		}

		recorded_resources::buffer vertex_data; ///< Vertex position buffer.
		gpu::format vertex_format = gpu::format::none; ///< Vertex format.
		u32 vertex_offset = 0; ///< Offset to the first vertex in bytes.
		u32 vertex_stride = 0; ///< Stride of a vertex in bytes.
		u32 vertex_count  = 0; ///< Number of vertices.

		recorded_resources::buffer index_data; ///< Index buffer.
		gpu::index_format index_format = gpu::index_format::uint16; ///< Index format.
		u32 index_offset = 0; ///< Offset to the first index in bytes.
		u32 index_count  = 0; ///< Number of indices in the buffer.

		gpu::raytracing_geometry_flags flags = gpu::raytracing_geometry_flags::none; ///< Flags.
	};


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
		/// An image.
		template <gpu::image_type Type> struct basic_image {
			/// Initializes all fields of this struct.
			basic_image(recorded_resources::basic_image_view<Type> v, image_binding_type type) :
				view(v), binding_type(type) {
			}

			recorded_resources::basic_image_view<Type> view; ///< A view of the image.
			image_binding_type binding_type = image_binding_type::num_enumerators; ///< Usage of the bound image.
		};
		using image2d = basic_image<gpu::image_type::type_2d>; ///< A 2D image.
		using image3d = basic_image<gpu::image_type::type_3d>; ///< A 2D image.
		/// A swap chain.
		struct swap_chain {
			/// Initializes all fields of this struct.
			swap_chain(recorded_resources::swap_chain c, image_binding_type type) : chain(c), binding_type(type) {
			}

			recorded_resources::swap_chain chain; ///< The swap chain.
			image_binding_type binding_type = image_binding_type::num_enumerators; ///< Usage of the bound image.
		};
		/// A constant buffer.
		struct constant_buffer {
			/// Initializes all fields of this struct.
			constant_buffer(recorded_resources::buffer d, usize off, usize sz) :
				data(d), offset(off), size(sz) {
			}

			recorded_resources::buffer data; ///< Buffer data.
			usize offset = 0; ///< Byte offset of the constant buffer.
			usize size = 0; ///< Size of the constant buffer in bytes.
		};
		/// A structured buffer.
		struct structured_buffer {
			/// Initializes all fields of this struct.
			structured_buffer(
				recorded_resources::structured_buffer_view d, buffer_binding_type t
			) : data(d), binding_type(t) {
			}

			recorded_resources::structured_buffer_view data; ///< Buffer data.
			buffer_binding_type binding_type = buffer_binding_type::read_only; ///< Usage of the bound buffer.
		};
	}

	/// All resource bindings.
	struct all_resource_bindings {
		/// A numbered descriptor binding.
		struct numbered_binding {
			using value_type = std::variant<
				descriptor_resource::image2d,
				descriptor_resource::image3d,
				descriptor_resource::swap_chain,
				descriptor_resource::constant_buffer,
				descriptor_resource::structured_buffer,
				recorded_resources::tlas,
				sampler_state
			>; ///< Contents of the binding.

			/// Initializes this binding.
			template <typename T> numbered_binding(u32 v, T &&val) :
				value(std::in_place_type<recorded_resources::mapped_type_t<std::decay_t<T>>>, std::forward<T>(val)),
				register_index(v) {
			}

			value_type value; ///< The binding.
			u32 register_index = 0; ///< Register index of this binding.
		};
		/// An array of numbered descriptor bindings that belong to the same register space.
		using numbered_descriptor_bindings = std::vector<numbered_binding>;
		/// Numbered descriptor bindings for the same set.
		struct numbered_set_binding {
			using value_type = std::variant<
				numbered_descriptor_bindings,
				recorded_resources::image_descriptor_array,
				recorded_resources::buffer_descriptor_array,
				recorded_resources::cached_descriptor_set
			>; ///< Contents of the binding.

			/// Initializes this set binding with the given list of descriptor bindings.
			numbered_set_binding(u32 s, numbered_descriptor_bindings bindings) :
				value(std::move(bindings)), register_space(s) {
			}
			/// Initializes this set binding.
			template <typename T> numbered_set_binding(u32 s, T &&val) :
				value(std::in_place_type<recorded_resources::mapped_type_t<std::decay_t<T>>>, std::forward<T>(val)),
				register_space(s) {
			}

			value_type value; ///< Bindings.
			u32 register_space = 0; ///< Register space of all the bindings.
		};

		/// A named descriptor binding.
		struct named_binding {
			using value_type = std::variant<
				descriptor_resource::image2d,
				descriptor_resource::image3d,
				descriptor_resource::swap_chain,
				descriptor_resource::constant_buffer,
				descriptor_resource::structured_buffer,
				recorded_resources::tlas,
				sampler_state,
				recorded_resources::image_descriptor_array,
				recorded_resources::buffer_descriptor_array
			>; ///< Contents of the binding.

			/// Initializes this binding.
			template <typename T> named_binding(std::u8string_view n, T &&val) :
				value(std::in_place_type<recorded_resources::mapped_type_t<std::decay_t<T>>>, std::forward<T>(val)),
				name(n) {
			}

			value_type value; ///< The binding.
			std::u8string_view name; ///< The name of this binding. Note that this struct does not own this string.
		};

		/// Initializes this structure to empty.
		all_resource_bindings(std::nullptr_t) {
		}
		/// Initializes all fields of this struct.
		all_resource_bindings(std::vector<numbered_set_binding> numbered, std::vector<named_binding> named) :
			numbered_sets(std::move(numbered)), named_bindings(std::move(named)) {
		}

		std::vector<numbered_set_binding> numbered_sets; ///< Numbered descriptor set bindings.
		std::vector<named_binding> named_bindings; ///< Named bindings.
	};

	namespace _details {
		/// A list of numbered set bindings.
		using numbered_bindings = std::vector<all_resource_bindings::numbered_set_binding>;
		/// A view of numbered set bindings.
		using numbered_bindings_view = std::span<const all_resource_bindings::numbered_set_binding>;

		/// Used to collect and sort descriptor bindings.
		class bindings_builder {
		public:
			/// Adds the given sets to this builder.
			void add(numbered_bindings sets) {
				if (_sets.empty()) {
					_sets = std::move(sets);
				} else {
					for (auto &s : sets) {
						_sets.emplace_back(std::move(s));
					}
				}
			}
			/// Adds the given named bindings to this builder, using the provided reflection data to infer bindings.
			void add(
				std::vector<all_resource_bindings::named_binding>,
				std::span<const gpu::shader_reflection *const>
			);

			/// \overload
			void add(
				all_resource_bindings resources,
				std::span<const gpu::shader_reflection *const> shaders
			) {
				add(std::move(resources.numbered_sets));
				add(std::move(resources.named_bindings), shaders);
			}
			/// \overload
			void add(
				all_resource_bindings resources,
				std::initializer_list<const gpu::shader_reflection*> shaders
			) {
				add(std::move(resources), { shaders.begin(), shaders.end() });
			}

			/// Sorts all bindings and returns them, leaving this object empty.
			[[nodiscard]] numbered_bindings sort_and_take();
		private:
			numbered_bindings _sets; ///< All collected bindings so far.
		};
	}
}
