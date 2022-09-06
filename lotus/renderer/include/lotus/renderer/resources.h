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
	class blas;
	class tlas;
	namespace _details {
		struct surface2d;
		struct buffer;
		struct swap_chain;
		struct descriptor_array;
		struct blas;
		struct tlas;
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
	/// Buffer binding types.
	enum class buffer_binding_type {
		read_only,  ///< Read-only buffer.
		read_write, ///< Read-write buffer.

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

		/// \ref renderer::blas.
		class blas {
			friend context;
		public:
			/// Conversion from a non-recorded \ref renderer::blas.
			blas(const renderer::blas&);

			/// Returns whether this object holds a valid BLAS.
			[[nodiscard]] bool is_valid() const {
				return _blas;
			}
			/// \overload
			[[nodiscard]] explicit operator bool() const {
				return is_valid();
			}
		private:
			_details::blas *_blas = nullptr; ///< The BLAS.
		};

		/// \ref renderer::tlas.
		class tlas {
			friend context;
		public:
			/// Conversion from a non-recorded \ref renderer::tlas.
			tlas(const renderer::tlas&);

			/// Returns whether this object holds a valid TLAS.
			[[nodiscard]] bool is_valid() const {
				return _tlas;
			}
			/// \overload
			[[nodiscard]] explicit operator bool() const {
				return is_valid();
			}
		private:
			_details::tlas *_tlas = nullptr; ///< The TLAS.
		};
	}


	/// An input buffer binding. Largely similar to \ref gpu::input_buffer_layout.
	struct input_buffer_binding {
		/// Initializes this buffer to empty.
		input_buffer_binding(std::nullptr_t) : data(nullptr) {
		}
		/// Initializes all fields of this struct.
		input_buffer_binding(
			std::uint32_t index,
			recorded_resources::buffer d, std::uint32_t off, std::uint32_t str,
			gpu::input_buffer_rate rate, std::vector<gpu::input_buffer_element> elems
		) :
			elements(std::move(elems)), data(d), stride(str), offset(off),
			buffer_index(index), input_rate(rate) {
		}
		/// Creates a buffer corresponding to the given input.
		[[nodiscard]] inline static input_buffer_binding create(
			recorded_resources::buffer buf, std::uint32_t off, gpu::input_buffer_layout layout
		) {
			return input_buffer_binding(
				static_cast<std::uint32_t>(layout.buffer_index),
				buf, off, static_cast<std::uint32_t>(layout.stride),
				layout.input_rate, { layout.elements.begin(), layout.elements.end() }
			);
		}

		std::vector<gpu::input_buffer_element> elements; ///< Elements in this vertex buffer.
		recorded_resources::buffer data; ///< The buffer.
		std::uint32_t stride = 0; ///< The size of one vertex.
		std::uint32_t offset = 0; ///< Offset from the beginning of the buffer.
		std::uint32_t buffer_index = 0; ///< Binding index for this input buffer.
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
			recorded_resources::buffer buf, std::uint32_t off, gpu::index_format fmt
		) : data(buf), offset(off), format(fmt) {
		}

		recorded_resources::buffer data; ///< The index buffer.
		std::uint32_t offset = 0; ///< Offset from the beginning of the buffer where indices start.
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
			std::uint32_t vert_off,
			std::uint32_t vert_stride,
			std::uint32_t vert_count,
			recorded_resources::buffer indices,
			gpu::index_format idx_fmt,
			std::uint32_t idx_off,
			std::uint32_t idx_count
		) :
			vertex_data(verts),
			vertex_format(vert_fmt),
			vertex_offset(vert_off),
			vertex_stride(vert_stride),
			vertex_count(vert_count),
			index_data(indices),
			index_format(idx_fmt),
			index_offset(idx_off),
			index_count(idx_count) {
		}

		recorded_resources::buffer vertex_data; ///< Vertex position buffer.
		gpu::format vertex_format = gpu::format::none; ///< Vertex format.
		std::uint32_t vertex_offset = 0; ///< Offset to the first vertex in bytes.
		std::uint32_t vertex_stride = 0; ///< Stride of a vertex in bytes.
		std::uint32_t vertex_count  = 0; ///< Number of vertices.

		recorded_resources::buffer index_data; ///< Index buffer.
		gpu::index_format index_format = gpu::index_format::uint16; ///< Index format.
		std::uint32_t index_offset = 0; ///< Offset to the first index in bytes.
		std::uint32_t index_count  = 0; ///< Number of indices in the buffer.
	};


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
			buffer(std::uint32_t sz, gpu::buffer_usage::mask usg, std::uint64_t i, std::u8string_view n) :
				data(nullptr), size(sz), usages(usg), id(i), name(n) {
			}

			gpu::buffer data; ///< The buffer.

			/// Current usage of this buffer.
			gpu::buffer_usage current_usage = gpu::buffer_usage::read_only_buffer;

			std::uint32_t size; ///< The size of this buffer.
			gpu::buffer_usage::mask usages = gpu::buffer_usage::mask::none; ///< Possible usages.

			std::uint64_t id = 0; ///< Used to uniquely identify this buffer.
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

		/// A bottom-level acceleration structure.
		struct blas {
		public:
			/// Initializes this structure.
			blas(std::vector<geometry_buffers_view> in, std::u8string_view n) :
				handle(nullptr), memory(nullptr), geometry(nullptr), build_sizes(uninitialized),
				input(std::move(in)), name(n) {
			}

			// these are populated when we actually build the BVH
			gpu::bottom_level_acceleration_structure handle; ///< The acceleration structure.
			gpu::buffer memory; ///< Memory for this acceleration structure.
			/// Geometry for this acceleration structure.
			gpu::bottom_level_acceleration_structure_geometry geometry;
			/// Memory requirements for the acceleration structure.
			gpu::acceleration_structure_build_sizes build_sizes;

			std::vector<geometry_buffers_view> input; ///< Build input.

			// TODO buffer references

			std::u8string name; ///< Name of this object.
		};

		/// A top-level acceleration structure.
		struct tlas {
		public:
			gpu::top_level_acceleration_structure handle; ///< The acceleration structure.

			std::vector<std::shared_ptr<blas>> references; ///< References to bottom level acceleration structures.

			std::u8string name; ///< Name of this object.
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
		/// Creates another view of the given mip levels of this image.
		[[nodiscard]] image2d_view view_mips(gpu::mip_levels mips) const {
			return image2d_view(_surface, _view_format, mips);
		}
		/// Creates another view of the given mip levels of this image in another format.
		[[nodiscard]] image2d_view view_mips_as(gpu::format fmt, gpu::mip_levels mips) const {
			return image2d_view(_surface, fmt, mips);
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

		/// Returns the number of mip levels allocated for this texture.
		[[nodiscard]] std::uint32_t get_num_mip_levels() const {
			return _surface->num_mips;
		}
		/// Returns the mip levels that are visible for this image view.
		[[nodiscard]] const gpu::mip_levels &get_viewed_mip_levels() const {
			return _mip_levels;
		}

		/// Returns whether this object holds a valid image view.
		[[nodiscard]] bool is_valid() const {
			return _surface != nullptr;
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
			return _buffer != nullptr;
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
			return _swap_chain != nullptr;
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
			return _array != nullptr;
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

	/// A bottom level acceleration structure.
	class blas {
		friend context;
		friend recorded_resources::blas;
	public:
		/// Initializes this object to empty.
		blas(std::nullptr_t) {
		}

		/// Returns whether this object holds a valid acceleration structure.
		[[nodiscard]] bool is_valid() const {
			return _blas != nullptr;
		}
		/// \overload
		[[nodiscard]] explicit operator bool() const {
			return is_valid();
		}
	private:
		/// Initializes this acceleration structure.
		explicit blas(std::shared_ptr<_details::blas> b) : _blas(std::move(b)) {
		}

		std::shared_ptr<_details::blas> _blas; ///< The acceleration structure.
	};

	/// A top level acceleration structure.
	class tlas {
		friend context;
		friend recorded_resources::tlas;
	public:
		/// Initializes this object to empty.
		tlas(std::nullptr_t) {
		}

		/// Returns whether this object holds a valid acceleration structure.
		[[nodiscard]] bool is_valid() const {
			return _tlas != nullptr;
		}
		/// \overload
		[[nodiscard]] explicit operator bool() const {
			return is_valid();
		}
	private:
		std::shared_ptr<_details::tlas> _tlas; ///< The acceleration structure.
	};
}
