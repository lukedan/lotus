#pragma once

#include <vector>

#include "lotus/logging.h"
#include "lotus/system/window.h"
#include "lotus/gpu/descriptors.h"
#include "lotus/gpu/frame_buffer.h"
#include "lotus/gpu/pipeline.h"
#include "lotus/gpu/resources.h"
#include "common.h"

namespace lotus::renderer {
	class context;
	struct input_buffer_binding;
	struct index_buffer_binding;
	struct all_resource_bindings;
	class image2d_view;
	class buffer;
	class swap_chain;
	class descriptor_array;
	namespace _details {
		struct surface2d;
		struct buffer;
		struct swap_chain;
		struct descriptor_array;
	}
	namespace assets {
		class manager;
		template <typename T> struct handle;
	}
	namespace cache_keys {
		struct descriptor_set_layout;
	}


	/// Image binding types.
	enum class image_binding_type {
		read_only,  ///< Read-only surface.
		read_write, ///< Read-write surface.

		num_enumerators ///< Number of enumerators.
	};

	/// Recorded resources. These objects don't hold ownership of the underlying objects, but otherwise they're
	/// exactly the same.
	namespace recorded_resources {
		/// \ref renderer::image2d_view.
		class image2d_view {
			friend context;
		public:
			/// Initializes this struct to empty.
			image2d_view(std::nullptr_t) : _mip_levels(gpu::mip_levels::all()) {
			}
			/// Conversion from a non-recorded \ref renderer::image2d_view.
			image2d_view(const renderer::image2d_view&);

			/// Returns a copy of this structure that ensures only the first specified mip is used, and logs a
			/// warning if that's not currently the case.
			[[nodiscard]] image2d_view highest_mip_with_warning() const;

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
			gpu::format _view_format = gpu::format::none; ///< The format of this surface.
			gpu::mip_levels _mip_levels; ///< Mip levels.
		};

		/// \ref renderer::buffer.
		class buffer {
			friend context;
		public:
			/// Initializes this struct to empty.
			buffer(std::nullptr_t) {
			}
			/// Conversion from a non-recorded \ref renderer::buffer.
			buffer(const renderer::buffer&);

			/// Returns whether this object holds a valid buffer.
			[[nodiscard]] bool is_valid() const {
				return _buffer;
			}
			/// \overload
			[[nodiscard]] explicit operator bool() const {
				return is_valid();
			}
		private:
			_details::buffer *_buffer = nullptr; ///< The buffer.
		};

		/// \ref renderer::swap_chain.
		class swap_chain {
			friend context;
		public:
			/// Conversion from a non-recorded \ref renderer::swap_chain.
			swap_chain(const renderer::swap_chain&);

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

		/// \ref renderer::descriptor_array.
		class descriptor_array {
			friend context;
		public:
			/// Conversion from a non-recorded \ref renderer::descriptor_array.
			descriptor_array(const renderer::descriptor_array&);

			/// Returns whether this object holds a valid descriptor array.
			[[nodiscard]] bool is_valid() const {
				return _array;
			}
			/// \overload
			[[nodiscard]] explicit operator bool() const {
				return is_valid();
			}
		private:
			_details::descriptor_array *_array = nullptr; ///< The descriptor array.
		};
	}


	/// Internal data structures used by the rendering context.
	namespace _details {
		struct descriptor_array;


		/// Returns the descriptor type that corresponds to the image binding.
		[[nodiscard]] gpu::descriptor_type to_descriptor_type(image_binding_type);


		/// A 2D surface managed by a context.
		struct surface2d {
		public:
			/// A reference to a usage of this surface in a descriptor array.
			struct descriptor_array_reference {
				/// Initializes this reference to empty.
				descriptor_array_reference(std::nullptr_t) {
				}

				descriptor_array *arr = nullptr; ///< The descriptor array.
				std::uint32_t index = 0; ///< The index of this image in the array.
			};

			/// Initializes this surface to empty.
			surface2d(
				cvec2s sz,
				std::uint32_t mips,
				gpu::format fmt,
				gpu::image_tiling tiling,
				gpu::image_usage::mask usages,
				std::uint64_t i,
				std::u8string_view n
			) :
				image(nullptr), current_usages(mips, gpu::image_usage::initial),
				size(sz), num_mips(mips), format(fmt), tiling(tiling), usages(usages),
				id(i), name(n) {
			}

			gpu::image2d image; ///< Image for the surface.
			
			std::vector<gpu::image_usage> current_usages; ///< Current usage of each mip of the surface.

			cvec2s size; ///< The size of this surface.
			std::uint32_t num_mips = 0; ///< Number of mips.
			gpu::format format = gpu::format::none; ///< Original pixel format.
			// TODO are these necessary?
			gpu::image_tiling tiling = gpu::image_tiling::optimal; ///< Tiling of this image.
			gpu::image_usage::mask usages = gpu::image_usage::mask::none; ///< Possible usages.

			std::vector<descriptor_array_reference> array_references; ///< References in descriptor arrays.

			std::uint64_t id = 0; ///< Used to uniquely identify this surface.
			// TODO make this debug only?
			std::u8string name; ///< Name of this image.
		};

		/// A buffer.
		struct buffer {
			/// Initializes this buffer to empty.
			buffer(std::uint32_t sz, gpu::buffer_usage::mask usg, std::u8string_view n) :
				data(nullptr), size(sz), usages(usg), name(n) {
			}

			gpu::buffer data; ///< The buffer.

			/// Current usage of this buffer.
			gpu::buffer_usage current_usage = gpu::buffer_usage::read_only_buffer;

			std::uint32_t size; ///< The size of this buffer.
			gpu::buffer_usage::mask usages = gpu::buffer_usage::mask::none; ///< Possible usages.

			// TODO make this debug only?
			std::u8string name; ///< Name of this buffer.
		};

		/// A swap chain associated with a window, managed by a context.
		struct swap_chain {
		public:
			/// Image index indicating that a next image has not been acquired.
			constexpr static std::uint32_t invalid_image_index = std::numeric_limits<std::uint32_t>::max();

			/// Initializes all fields of this structure without creating a swap chain.
			swap_chain(
				system::window &wnd, std::uint32_t imgs, std::vector<gpu::format> fmts, std::u8string_view n
			) :
				chain(nullptr), current_size(zero), desired_size(zero), current_format(gpu::format::none),
				next_image_index(invalid_image_index), window(wnd), num_images(imgs),
				expected_formats(std::move(fmts)), name(n) {
			}

			gpu::swap_chain chain; ///< The swap chain.
			std::vector<gpu::fence> fences; ///< Synchronization primitives for each back buffer.
			std::vector<gpu::image2d> images; ///< Images in this swap chain.
			std::vector<gpu::image_usage> current_usages; ///< Current usages of all back buffers.

			cvec2s current_size; ///< Current size of swap chain images.
			cvec2s desired_size; ///< Desired size of swap chain images.
			gpu::format current_format; ///< Format of the swap chain images.

			std::uint32_t next_image_index = 0; ///< Index of the next image.

			system::window &window; ///< The window that owns this swap chain.
			std::uint32_t num_images; ///< Number of images in the swap chain.
			std::vector<gpu::format> expected_formats; ///< Expected swap chain formats.

			// TODO make this debug only?
			std::u8string name; ///< Name of this swap chain.
		};

		/// A bindless descriptor array.
		struct descriptor_array {
		public:
			/// A reference to an element in the array.
			struct image_reference {
				/// Initializes this reference to empty.
				image_reference(std::nullptr_t) : image(nullptr) {
				}

				recorded_resources::image2d_view image; ///< The referenced image.
				std::uint32_t reference_index = 0; ///< Index of this reference in \ref surface2d::array_references.
			};

			/// Initializes all fields of this structure without creating a descriptor set.
			descriptor_array(gpu::descriptor_type ty, std::uint32_t cap, std::u8string_view n) :
				set(nullptr), capacity(cap), type(ty), name(n) {
			}

			/// Returns a key for the layout of this descriptor array.
			[[nodiscard]] cache_keys::descriptor_set_layout get_layout_key() const;

			gpu::descriptor_set set; ///< The descriptor set.
			std::uint32_t capacity = 0; ///< The capacity of this array.
			/// The type of this descriptor array.
			gpu::descriptor_type type = gpu::descriptor_type::num_enumerators;

			std::vector<image_reference> images; ///< Contents of this descriptor array.

			/// Indices of all images that have been used externally and may need transitions.
			std::vector<std::uint32_t> staged_transitions;
			/// Indices of all images that have been modified in \ref images but have not been written into \ref set.
			std::vector<std::uint32_t> staged_writes;
			/// Indicates whether there are pending descriptor writes that overwrite an existing descriptor. This
			/// means that we'll need to wait until the previous use of this descriptor array has finished.
			bool has_descriptor_overwrites = false;

			std::u8string name; ///< Name of this descriptor array.
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


	/// A reference of a view into a 2D image.
	class image2d_view {
		friend context;
		friend recorded_resources::image2d_view;
	public:
		/// Initializes this view to empty.
		image2d_view(std::nullptr_t) : _mip_levels(gpu::mip_levels::all()) {
		}

		/// Creates another view of the image in another format.
		[[nodiscard]] image2d_view view_as(gpu::format fmt) const {
			return image2d_view(_surface, fmt, _mip_levels);
		}

		/// Returns the size of the top mip of this image.
		[[nodiscard]] cvec2s get_size() const {
			return _surface->size;
		}
		/// Returns the format that this image is viewed as.
		[[nodiscard]] gpu::format get_viewed_as_format() const {
			return _view_format;
		}
		/// Returns the original format of this image.
		[[nodiscard]] gpu::format get_original_format() const {
			return _surface->format;
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
		image2d_view(std::shared_ptr<_details::surface2d> surf, gpu::format fmt, gpu::mip_levels mips) :
			_surface(std::move(surf)), _view_format(fmt), _mip_levels(mips) {
		}

		std::shared_ptr<_details::surface2d> _surface; ///< The surface that this is a view of.
		/// The format to view as; may be different from the original format of the surface.
		gpu::format _view_format = gpu::format::none;
		gpu::mip_levels _mip_levels; ///< Mip levels that are included in this view.
	};

	/// A reference of a buffer.
	class buffer {
		friend context;
		friend recorded_resources::buffer;
	public:
		/// Initializes the view to empty.
		buffer(std::nullptr_t) {
		}

		/// Returns the size of this buffer.
		[[nodiscard]] std::uint32_t get_size_in_bytes() const {
			return _buffer->size;
		}

		/// Returns whether this object holds a valid buffer.
		[[nodiscard]] bool is_valid() const {
			return _buffer.get();
		}
		/// \overload
		[[nodiscard]] explicit operator bool() const {
			return is_valid();
		}
	private:
		/// Initializes all fields of this struct.
		buffer(std::shared_ptr<_details::buffer> buf) : _buffer(std::move(buf)) {
		}

		std::shared_ptr<_details::buffer> _buffer; ///< The referenced buffer.
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

	/// A bindless descriptor array.
	class descriptor_array {
		friend context;
		friend recorded_resources::descriptor_array;
	public:
		/// Initializes this object to empty.
		descriptor_array(std::nullptr_t) {
		}

		/// Returns whether this object holds a valid descriptor array.
		[[nodiscard]] bool is_valid() const {
			return _array.get();
		}
		/// \overload
		[[nodiscard]] explicit operator bool() const {
			return is_valid();
		}
	private:
		/// Initializes this descriptor array.
		explicit descriptor_array(std::shared_ptr<_details::descriptor_array> arr) : _array(std::move(arr)) {
		}

		std::shared_ptr<_details::descriptor_array> _array; ///< The descriptor array.
	};


	namespace assets {
		/// Unique identifier of an asset.
		struct identifier {
			/// Creates an empty identifier.
			identifier(std::nullptr_t) {
			}
			/// Initializes all fields of this struct.
			explicit identifier(std::filesystem::path p, std::u8string sub = {}) :
				path(std::move(p)), subpath(std::move(sub)) {
			}

			/// Equality comparison.
			[[nodiscard]] friend bool operator==(const identifier&, const identifier&) = default;

			/// Computes a hash for this identifier.
			[[nodiscard]] std::size_t hash() const {
				return hash_combine(std::filesystem::hash_value(path), std::hash<std::u8string>()(subpath));
			}

			std::filesystem::path path; ///< Path to the asset.
			std::u8string subpath; ///< Additional identification of the asset within the file.
		};
	}

	/// An asset.
	template <typename T> class asset {
		friend assets::manager;
	public:
		T value; ///< The asset object.
	private:
		/// Initializes this asset.
		template <typename ...Args> asset(Args &&...args) : value(std::forward<Args>(args)...) {
		}

		/// ID of this asset. This will point to the key stored in the \p std::unordered_map.
		const assets::identifier *_id = nullptr;
	};

	namespace assets {
		/// An owning handle of an asset - \ref asset::ref_count is updated automatically.
		template <typename T> struct handle {
			friend manager;
			friend std::hash<handle<T>>;
		public:
			/// Initializes this handle to empty.
			handle(std::nullptr_t) : _ptr(nullptr) {
			}

			/// Returns the asset.
			[[nodiscard]] const asset<T> &get() const {
				return *_ptr;
			}

			/// Default equality and inequality.
			[[nodiscard]] friend bool operator==(const handle&, const handle&) = default;

			/// Returns whether this handle is valid.
			[[nodiscard]] bool is_valid() const {
				return _ptr != nullptr;
			}
			/// \overload
			[[nodiscard]] explicit operator bool() const {
				return is_valid();
			}
		private:
			/// Initializes this handle to the given asset and increases \ref asset::ref_count.
			explicit handle(std::shared_ptr<asset<T>> ass) : _ptr(std::move(ass)) {
			}

			std::shared_ptr<asset<T>> _ptr; ///< Pointer to the asset.
		};
	}
}
namespace std {
	/// Hash function for \ref lotus::renderer::assets::handle.
	template <typename T> struct hash<lotus::renderer::assets::handle<T>> {
		/// Hashes the given object.
		[[nodiscard]] std::size_t operator()(const lotus::renderer::assets::handle<T> &h) const {
			return lotus::compute_hash(h._ptr);
		}
	};
}


namespace lotus::renderer {
	namespace assets {
		/// The type of an asset.
		enum class type {
			texture,  ///< A texture.
			shader,   ///< A shader.
			material, ///< A material that references a set of shaders and a number of textures.
			geometry, ///< A 3D mesh. Essentially a set of vertices and their attributes.
			blob,     ///< A large binary chunk of data.

			num_enumerators ///< The total number of types.
		};

		/// A loaded 2D texture.
		struct texture2d {
			/// Initializes this texture to empty.
			texture2d(std::nullptr_t) : image(nullptr) {
			}

			image2d_view image; ///< The image.
			std::uint32_t highest_mip_loaded = 0; ///< The highest mip that has been loaded.
			std::uint32_t descriptor_index = 0; ///< Index of this texture in the global bindless descriptor table.
		};
		/// A generic data buffer.
		struct buffer {
			/// Initializes this buffer to empty.
			buffer(std::nullptr_t) : data(nullptr) {
			}

			gpu::buffer data; ///< The buffer.

			std::uint32_t byte_size = 0; ///< The size of this buffer in bytes.
			/// The size of an element in the buffer in bytes. Zero indicates an unstructured buffer.
			std::uint32_t byte_stride = 0;
			gpu::buffer_usage::mask usages = gpu::buffer_usage::mask::none; ///< Allowed usages.
		};
		/// A loaded shader.
		struct shader {
			/// Initializes this object to empty.
			shader(std::nullptr_t) : binary(nullptr), reflection(nullptr) {
			}

			gpu::shader_binary binary; ///< Shader binary.
			gpu::shader_reflection reflection; ///< Reflection data.

			shader_descriptor_bindings descriptor_bindings; ///< Descriptor bindings of this shader.
		};
		/// A material.
		struct material {
		public:
			/// Base class of context-specific material data.
			class context_data {
			public:
				/// Default virtual destructor.
				virtual ~context_data() = default;

				/// Returns the file to include to use this type of material.
				[[nodiscard]] virtual std::u8string_view get_material_include() const = 0;
				/// Creates resource bindings for this material.
				[[nodiscard]] virtual all_resource_bindings create_resource_bindings() const = 0;
			};

			/// Initializes this material to empty.
			material(std::nullptr_t) {
			}
			/// Initializes material data.
			explicit material(std::unique_ptr<context_data> d) : data(std::move(d)) {
			}

			std::unique_ptr<context_data> data; ///< Material data.
		};
		/// A loaded geometry.
		struct geometry {
			/// Information about a buffer used as a rasterization stage input.
			struct input_buffer {
				/// Initializes this buffer to empty.
				input_buffer(std::nullptr_t) : data(nullptr) {
				}

				/// Creates a \ref gpu::vertex_buffer_view from this buffer.
				[[nodiscard]] gpu::vertex_buffer_view into_vertex_buffer_view(std::uint32_t num_verts) const {
					return gpu::vertex_buffer_view(data.get().value.data, format, offset, stride, num_verts);
				}
				/// Creates a \ref input_buffer_binding from this buffer.
				[[nodiscard]] input_buffer_binding into_input_buffer_binding(
					const char8_t *semantic, std::uint32_t semantic_index, std::uint32_t binding_index
				) const;

				handle<buffer> data; ///< Data of this input buffer.
				std::uint32_t offset = 0; ///< Offset of the first element in bytes.
				std::uint32_t stride = 0; ///< Stride between consecutive buffer elements in bytes.
				gpu::format format = gpu::format::none; ///< Format of an element.
			};

			/// Initializes this geometry to empty.
			geometry(std::nullptr_t) :
				vertex_buffer(nullptr), uv_buffer(nullptr), normal_buffer(nullptr), tangent_buffer(nullptr),
				index_buffer(nullptr), acceleration_structure_buffer(nullptr), acceleration_structure(nullptr) {
			}

			/// Returns a \ref gpu::index_buffer_view for the index buffer of this geometry.
			[[nodiscard]] gpu::index_buffer_view get_index_buffer_view() const {
				return gpu::index_buffer_view(
					index_buffer.get().value.data, index_format, 0, num_indices
				);
			}
			/// Returns a \ref index_buffer_binding for the index buffer of this geometry.
			[[nodiscard]] index_buffer_binding get_index_buffer_binding() const;

			input_buffer vertex_buffer;  ///< Vertex buffer.
			input_buffer uv_buffer;      ///< UV buffer.
			input_buffer normal_buffer;  ///< Normal buffer.
			input_buffer tangent_buffer; ///< Tangent buffer.
			std::uint32_t num_vertices = 0; ///< Total number of vertices.

			handle<buffer> index_buffer; ///< The index buffer.
			std::uint32_t num_indices  = 0; ///< Total number of indices.
			gpu::index_format index_format = gpu::index_format::uint32; ///< Format of indices.

			/// Primitive topology.
			gpu::primitive_topology topology = gpu::primitive_topology::point_list;

			gpu::buffer acceleration_structure_buffer; ///< Buffer for acceleration structure.
			gpu::bottom_level_acceleration_structure acceleration_structure; ///< Acceleration structure.
		};
	}


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

	/// An input buffer binding. Largely similar to \ref gpu::input_buffer_layout.
	struct input_buffer_binding {
		/// Initializes this buffer to empty.
		input_buffer_binding(std::nullptr_t) : buffer(nullptr) {
		}
		/// Initializes all fields of this struct.
		input_buffer_binding(
			std::uint32_t index,
			assets::handle<assets::buffer> buf, std::uint32_t off, std::uint32_t str,
			gpu::input_buffer_rate rate, std::vector<gpu::input_buffer_element> elems
		) :
			elements(std::move(elems)), buffer(std::move(buf)), stride(str), offset(off),
			buffer_index(index), input_rate(rate) {
		}
		/// Creates a buffer corresponding to the given input.
		[[nodiscard]] inline static input_buffer_binding create(
			assets::handle<assets::buffer> buf, std::uint32_t off, gpu::input_buffer_layout layout
		) {
			return input_buffer_binding(
				static_cast<std::uint32_t>(layout.buffer_index), std::move(buf),
				off, static_cast<std::uint32_t>(layout.stride),
				layout.input_rate, { layout.elements.begin(), layout.elements.end() }
			);
		}

		std::vector<gpu::input_buffer_element> elements; ///< Elements in this vertex buffer.
		assets::handle<assets::buffer> buffer; ///< The buffer.
		std::uint32_t stride = 0; ///< The size of one vertex.
		std::uint32_t offset = 0; ///< Offset from the beginning of the buffer.
		std::uint32_t buffer_index = 0; ///< Binding index for this input buffer.
		/// Specifies how the buffer data is used.
		gpu::input_buffer_rate input_rate = gpu::input_buffer_rate::per_vertex;
	};
	/// An index buffer binding.
	struct index_buffer_binding {
		/// Initializes this binding to empty.
		index_buffer_binding(std::nullptr_t) : buffer(nullptr) {
		}
		/// Initializes all fields of this struct.
		index_buffer_binding(
			assets::handle<assets::buffer> buf, std::uint32_t off, gpu::index_format fmt
		) : buffer(std::move(buf)), offset(off), format(fmt) {
		}

		assets::handle<assets::buffer> buffer; ///< The index buffer.
		std::uint32_t offset = 0; ///< Offset from the beginning of the buffer where indices start.
		gpu::index_format format = gpu::index_format::uint32; ///< Format of indices.
	};

	namespace descriptor_resource {
		/// A 2D image.
		struct image2d {
		public:
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
			/// Whether this resource is bound as writable.
			image_binding_type binding_type = image_binding_type::read_only;
		};
		/// The next image in a swap chain.
		struct swap_chain_image {
			/// Initializes all fields of this struct.
			explicit swap_chain_image(recorded_resources::swap_chain chain) : image(chain) {
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
