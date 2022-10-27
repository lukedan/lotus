#pragma once

#include <vector>

#include "lotus/logging.h"
#include "lotus/system/window.h"
#include "lotus/gpu/descriptors.h"
#include "lotus/gpu/frame_buffer.h"
#include "lotus/gpu/pipeline.h"
#include "lotus/gpu/resources.h"
#include "lotus/renderer/common.h"
#include "execution.h"
#include "pool.h"

namespace lotus::renderer {
	class context;
	struct input_buffer_binding;
	struct index_buffer_binding;
	struct all_resource_bindings;

	template <typename> struct basic_resource_handle;
	struct image2d_view;
	struct buffer;
	struct structured_buffer_view;
	struct swap_chain;
	template <typename, typename> struct descriptor_array;
	struct blas;
	struct tlas;
	struct cached_descriptor_set;

	namespace assets {
		class manager;
		template <typename> struct handle;
	}


	/// Recorded resources. These objects don't hold ownership of the underlying objects, but otherwise they're
	/// exactly the same.
	namespace recorded_resources {
		/// Template for resources that requires only one pointer to a type.
		template <typename Resource> class basic_handle {
			friend context;
			friend execution::context;
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
		private:
			Resource *_ptr = nullptr; ///< Pointer to the resource.
		};


		/// \ref renderer::image2d_view.
		struct image2d_view {
			friend context;
			friend execution::transition_buffer;
			friend execution::context;
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
				return _image;
			}
			/// \overload
			[[nodiscard]] explicit operator bool() const {
				return is_valid();
			}
		private:
			_details::image2d *_image = nullptr; ///< The image.
			gpu::format _view_format = gpu::format::none; ///< The format of this surface.
			gpu::mip_levels _mip_levels; ///< Mip levels.
		};

		/// \ref renderer::buffer.
		using buffer = basic_handle<_details::buffer>;

		/// \ref renderer::structured_buffer_view.
		struct structured_buffer_view {
			friend context;
			friend execution::transition_buffer;
			friend execution::context;
		public:
			/// Initializes this struct to empty.
			structured_buffer_view(std::nullptr_t) {
			}
			/// Conversion from a non-recorded \ref renderer::structured_buffer_view.
			structured_buffer_view(const renderer::structured_buffer_view&);

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
			std::uint32_t _stride = 0; ///< Byte stride between elements.
			std::uint32_t _first = 0; ///< The first buffer element.
			std::uint32_t _count = 0; ///< Number of visible buffer elements.
		};

		/// \ref renderer::swap_chain.
		using swap_chain = basic_handle<_details::swap_chain>;

		/// \ref renderer::descriptor_array.
		template <typename RecordedResource, typename View> using descriptor_array = basic_handle<
			_details::descriptor_array<RecordedResource, View>
		>;

		/// \ref renderer::blas.
		using blas = basic_handle<_details::blas>;

		/// \ref renderer::tlas.
		using tlas = basic_handle<_details::tlas>;

		/// \ref renderer::cached_descriptor_set.
		using cached_descriptor_set = basic_handle<_details::cached_descriptor_set>;
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
		/// Returns the descriptor type that corresponds to the image binding.
		[[nodiscard]] gpu::descriptor_type to_descriptor_type(image_binding_type);

		/// A reference to a usage of this surface in a descriptor array.
		template <typename RecordedResource, typename View> struct descriptor_array_reference {
			/// Initializes this reference to empty.
			descriptor_array_reference(std::nullptr_t) {
			}

			/// The descriptor array.
			descriptor_array<RecordedResource, View> *array = nullptr;
			std::uint32_t index = 0; ///< The index of this image in the array.
		};


		/// Base class of all concrete resource types.
		struct resource : public std::enable_shared_from_this<resource> {
		public:
			/// Initializes common resource properties.
			explicit resource(std::u8string_view n) : name(n) {
			}
			/// Default destructor.
			virtual ~resource() = default;

			/// Returns the type of this resource.
			[[nodiscard]] virtual resource_type get_type() const = 0;

			// TODO make this debug only?
			std::u8string name; ///< The name of this resource.
		};

		/// A 2D image managed by a context.
		struct image2d : public resource {
		public:
			/// Initializes this surface to empty.
			image2d(
				cvec2s sz,
				std::uint32_t mips,
				gpu::format fmt,
				gpu::image_tiling tiling,
				gpu::image_usage_mask usages,
				pool *p,
				std::uint64_t i,
				std::u8string_view n
			) :
				resource(n), image(nullptr), memory_pool(p), memory(nullptr),
				current_usages(mips, image_access::initial()), size(sz), num_mips(mips),
				format(fmt), tiling(tiling), usages(usages), id(i) {
			}

			/// Returns \ref resource_type::image2d.
			[[nodiscard]] resource_type get_type() const override {
				return resource_type::image2d;
			}

			gpu::image2d image; ///< Image for the surface.
			pool *memory_pool = nullptr; ///< Memory pool to allocate this image out of.
			pool::token memory; ///< Allocated memory for this image.
			
			std::vector<image_access> current_usages; ///< Current usage of each mip of the surface.

			cvec2s size; ///< The size of this surface.
			std::uint32_t num_mips = 0; ///< Number of mips.
			gpu::format format = gpu::format::none; ///< Original pixel format.
			// TODO are these necessary?
			gpu::image_tiling tiling = gpu::image_tiling::optimal; ///< Tiling of this image.
			gpu::image_usage_mask usages = gpu::image_usage_mask::none; ///< Possible usages.

			/// References in descriptor arrays.
			std::vector<
				descriptor_array_reference<recorded_resources::image2d_view, gpu::image2d_view>
			> array_references;

			std::uint64_t id = 0; ///< Used to uniquely identify this surface.
		};

		/// A buffer.
		struct buffer : public resource {
			/// Initializes this buffer to empty.
			buffer(std::uint32_t sz, gpu::buffer_usage_mask usg, pool *p, std::uint64_t i, std::u8string_view n) :
				resource(n), data(nullptr), memory_pool(p), memory(nullptr),
				access(buffer_access::initial()), size(sz), usages(usg), id(i) {
			}

			/// Returns \ref resource_type::buffer.
			[[nodiscard]] resource_type get_type() const override {
				return resource_type::buffer;
			}

			gpu::buffer data; ///< The buffer.
			pool *memory_pool = nullptr; ///< Memory pool to allocate this buffer out of.
			pool::token memory; ///< Allocated memory for this image.

			/// Current usage of this buffer.
			buffer_access access;

			std::uint32_t size; ///< The size of this buffer.
			gpu::buffer_usage_mask usages = gpu::buffer_usage_mask::none; ///< Possible usages.

			/// References in descriptor arrays.
			std::vector<
				descriptor_array_reference<recorded_resources::structured_buffer_view, void>
			> array_references;

			std::uint64_t id = 0; ///< Used to uniquely identify this buffer.
		};

		/// A swap chain associated with a window, managed by a context.
		struct swap_chain : public resource {
		public:
			/// Image index indicating that a next image has not been acquired.
			constexpr static std::uint32_t invalid_image_index = std::numeric_limits<std::uint32_t>::max();

			/// Initializes all fields of this structure without creating a swap chain.
			swap_chain(
				system::window &wnd, std::uint32_t imgs, std::vector<gpu::format> fmts, std::u8string_view n
			) :
				resource(n),
				chain(nullptr), current_size(zero), desired_size(zero), current_format(gpu::format::none),
				next_image_index(invalid_image_index), window(wnd), num_images(imgs),
				expected_formats(std::move(fmts)) {
			}

			/// Returns \ref resource_type::swap_chain.
			[[nodiscard]] resource_type get_type() const override {
				return resource_type::swap_chain;
			}

			gpu::swap_chain chain; ///< The swap chain.
			std::vector<gpu::fence> fences; ///< Synchronization primitives for each back buffer.
			std::vector<gpu::image2d> images; ///< Images in this swap chain.
			std::vector<image_access> current_usages; ///< Current usages of all back buffers.

			cvec2s current_size; ///< Current size of swap chain images.
			cvec2s desired_size; ///< Desired size of swap chain images.
			gpu::format current_format; ///< Format of the swap chain images.

			std::uint32_t next_image_index = 0; ///< Index of the next image.

			system::window &window; ///< The window that owns this swap chain.
			std::uint32_t num_images; ///< Number of images in the swap chain.
			std::vector<gpu::format> expected_formats; ///< Expected swap chain formats.
		};

		/// A bindless descriptor array.
		template <typename RecordedResource, typename View> struct descriptor_array : public resource {
		public:
			/// A reference to an element in the array.
			struct resource_reference {
				/// Initializes this reference to empty.
				resource_reference(std::nullptr_t) : resource(nullptr), view(nullptr) {
				}

				RecordedResource resource; ///< The referenced resource.
				static_optional<View, !std::is_same_v<View, void>> view; ///< View object of the resource.
				std::uint32_t reference_index = 0; ///< Index of this reference in \p Descriptor::array_references.
			};

			/// Initializes all fields of this structure without creating a descriptor set.
			descriptor_array(gpu::descriptor_type ty, std::uint32_t cap, std::u8string_view n) :
				resource(n), set(nullptr), capacity(cap), type(ty) {
			}

			/// Returns \ref resource_type::swap_chain.
			[[nodiscard]] resource_type get_type() const override {
				if constexpr (std::is_same_v<RecordedResource, recorded_resources::image2d_view>) {
					return resource_type::image2d_descriptor_array;
				} else if constexpr (std::is_same_v<RecordedResource, recorded_resources::structured_buffer_view>) {
					return resource_type::buffer_descriptor_array;
				} else {
					static_assert(!std::is_same_v<RecordedResource, RecordedResource>, "Not implemented");
					return resource_type::num_enumerators;
				}
			}

			gpu::descriptor_set set; ///< The descriptor set.
			std::uint32_t capacity = 0; ///< The capacity of this array.
			/// The type of this descriptor array.
			gpu::descriptor_type type = gpu::descriptor_type::num_enumerators;

			std::vector<resource_reference> resources; ///< Contents of this descriptor array.

			/// Indices of all resources that have been used externally and may need transitions.
			std::vector<std::uint32_t> staged_transitions;
			/// Indices of all resources that have been modified in \ref resources but have not been written into
			/// \ref set.
			std::vector<std::uint32_t> staged_writes;
			/// Indicates whether there are pending descriptor writes that overwrite an existing descriptor. This
			/// means that we'll need to wait until the previous use of this descriptor array has finished.
			bool has_descriptor_overwrites = false;
		};

		/// A bottom-level acceleration structure.
		struct blas : public resource {
		public:
			/// Initializes this structure.
			blas(std::vector<geometry_buffers_view> in, pool *p, std::u8string_view n) :
				resource(n), handle(nullptr), geometry(nullptr), build_sizes(uninitialized), memory_pool(p),
				input(std::move(in)) {
			}

			/// Returns \ref resource_type::blas.
			[[nodiscard]] resource_type get_type() const override {
				return resource_type::blas;
			}

			// these are populated when we actually build the BVH
			gpu::bottom_level_acceleration_structure handle; ///< The acceleration structure.
			std::shared_ptr<buffer> memory; ///< Memory for this acceleration structure.
			/// Geometry for this acceleration structure.
			gpu::bottom_level_acceleration_structure_geometry geometry;
			/// Memory requirements for the acceleration structure.
			gpu::acceleration_structure_build_sizes build_sizes;
			pool *memory_pool = nullptr; ///< Memory pool to allocate the BLAS out of.

			std::vector<geometry_buffers_view> input; ///< Build input.

			// TODO buffer references
		};

		/// A top-level acceleration structure.
		struct tlas : public resource {
		public:
			/// Initializes this structure.
			tlas(
				std::vector<gpu::instance_description> in,
				std::vector<std::shared_ptr<blas>> refs,
				pool *p,
				std::u8string_view n
			) :
				resource(n), handle(nullptr), input_data(nullptr), build_sizes(uninitialized),
				memory_pool(p), input_data_token(nullptr),
				input(std::move(in)), input_references(std::move(refs)) {
			}

			/// Returns \ref resource_type::tlas.
			[[nodiscard]] resource_type get_type() const override {
				return resource_type::tlas;
			}

			// these are populated when we actually build the BVH
			gpu::top_level_acceleration_structure handle; ///< The acceleration structure.
			std::shared_ptr<buffer> memory; ///< Memory for this acceleration structure.
			/// Input BLAS's uploaded to the GPU. This may be freed manually, after which rebuilding/refitting
			/// requires data to be copied from \ref input again.
			gpu::buffer input_data;
			/// Memory requirements for the acceleration structure.
			gpu::acceleration_structure_build_sizes build_sizes;
			/// Memory pool to allocate this TLAS out of. Input data is also allocated out of this pool.
			pool *memory_pool = nullptr;
			pool::token input_data_token; ///< Token for the input data buffer.

			std::vector<gpu::instance_description> input; ///< Input data.
			bool input_copied = false; ///< Whether \ref input has been copied to \ref input_data.
			std::vector<std::shared_ptr<blas>> input_references; ///< References to all input BLAS's.
		};

		/// A cached descriptor set.
		struct cached_descriptor_set : public resource {
			/// Initializes all fields of this struct.
			cached_descriptor_set(
				gpu::descriptor_set s,
				std::vector<gpu::descriptor_range_binding> rs,
				const gpu::descriptor_set_layout &l,
				std::u8string_view n
			) :
				resource(n),
				set(std::move(s)), ranges(std::move(rs)), layout(&l), transitions(nullptr) {
			}

			/// Returns \ref resource_type::cached_descriptor_set.
			[[nodiscard]] resource_type get_type() const override {
				return resource_type::cached_descriptor_set;
			}

			gpu::descriptor_set set; ///< The descriptor set.

			std::vector<gpu::descriptor_range_binding> ranges; ///< Sorted descriptor ranges.
			const gpu::descriptor_set_layout *layout = nullptr; ///< Layout of this descriptor set.

			/// Records all transitions that are needed when using this descriptor set.
			execution::transition_buffer transitions;
			std::vector<gpu::image2d_view> image_views; ///< Image views used by this descriptor set.
			std::vector<gpu::sampler> samplers; ///< Samplers used by this descriptor set.
			/// All resources referenced by this descriptor set.
			std::vector<std::shared_ptr<resource>> resource_references;
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


	/// Template for all owning resource handles.
	template <typename Resource> struct basic_resource_handle {
		friend context;
		friend recorded_resources::basic_handle<Resource>;
	public:
		/// Initializes this handle to empty.
		basic_resource_handle(std::nullptr_t) {
		}

		/// Returns whether this object holds a valid image view.
		[[nodiscard]] bool is_valid() const {
			return _ptr != nullptr;
		}
		/// \overload
		[[nodiscard]] explicit operator bool() const {
			return is_valid();
		}
	protected:
		/// Initializes this resource handle.
		explicit basic_resource_handle(std::shared_ptr<Resource> p) : _ptr(std::move(p)) {
		}

		std::shared_ptr<Resource> _ptr; ///< Pointer to the resource.
	};


	/// A reference of a view into a 2D image.
	struct image2d_view : public basic_resource_handle<_details::image2d> {
		friend context;
		friend recorded_resources::image2d_view;
	public:
		/// Initializes this view to empty.
		image2d_view(std::nullptr_t) : basic_resource_handle(nullptr), _mip_levels(gpu::mip_levels::all()) {
		}

		/// Creates another view of the image in another format.
		[[nodiscard]] image2d_view view_as(gpu::format fmt) const {
			return image2d_view(_ptr, fmt, _mip_levels);
		}
		/// Creates another view of the given mip levels of this image.
		[[nodiscard]] image2d_view view_mips(gpu::mip_levels mips) const {
			return image2d_view(_ptr, _view_format, mips);
		}
		/// Creates another view of the given mip levels of this image in another format.
		[[nodiscard]] image2d_view view_mips_as(gpu::format fmt, gpu::mip_levels mips) const {
			return image2d_view(_ptr, fmt, mips);
		}

		/// Returns the size of the top mip of this image.
		[[nodiscard]] cvec2s get_size() const {
			return _ptr->size;
		}
		/// Returns the format that this image is viewed as.
		[[nodiscard]] gpu::format get_viewed_as_format() const {
			return _view_format;
		}
		/// Returns the original format of this image.
		[[nodiscard]] gpu::format get_original_format() const {
			return _ptr->format;
		}

		/// Returns the number of mip levels allocated for this texture.
		[[nodiscard]] std::uint32_t get_num_mip_levels() const {
			return _ptr->num_mips;
		}
		/// Returns the mip levels that are visible for this image view.
		[[nodiscard]] const gpu::mip_levels &get_viewed_mip_levels() const {
			return _mip_levels;
		}
	private:
		/// Initializes all fields of this struct.
		image2d_view(std::shared_ptr<_details::image2d> surf, gpu::format fmt, gpu::mip_levels mips) :
			basic_resource_handle(std::move(surf)), _view_format(fmt), _mip_levels(mips) {
		}

		/// The format to view as; may be different from the original format of the surface.
		gpu::format _view_format = gpu::format::none;
		gpu::mip_levels _mip_levels; ///< Mip levels that are included in this view.
	};

	/// A reference of a buffer.
	struct buffer : public basic_resource_handle<_details::buffer> {
		friend context;
	public:
		/// Initializes the view to empty.
		buffer(std::nullptr_t) : basic_resource_handle(nullptr) {
		}

		/// Returns the size of this buffer.
		[[nodiscard]] std::uint32_t get_size_in_bytes() const {
			return _ptr->size;
		}

		/// Returns a view of this buffer as a structured buffer.
		[[nodiscard]] structured_buffer_view get_view(
			std::uint32_t stride, std::uint32_t first, std::uint32_t count
		) const;
		/// \overload
		template <typename T> [[nodiscard]] structured_buffer_view get_view(
			std::uint32_t first, std::uint32_t count
		) const;
	private:
		/// Initializes all fields of this struct.
		explicit buffer(std::shared_ptr<_details::buffer> buf) : basic_resource_handle(std::move(buf)) {
		}
	};

	/// A view into a buffer as a structured buffer.
	struct structured_buffer_view : public basic_resource_handle<_details::buffer> {
		friend buffer;
		friend context;
		friend recorded_resources::structured_buffer_view;
	public:
		/// Initializes this view to empty.
		structured_buffer_view(std::nullptr_t) : basic_resource_handle(nullptr) {
		}

		/// Returns the stride of an element in bytes.
		[[nodiscard]] std::uint32_t get_stride() const {
			return _stride;
		}
		/// Returns the first element visible to this view.
		[[nodiscard]] std::uint32_t get_first_element_index() const {
			return _first;
		}
		/// Returns the number of elements visible to this view.
		[[nodiscard]] std::uint32_t get_num_elements() const {
			return _count;
		}

		/// Moves the range of visible elements and returns the new view.
		[[nodiscard]] structured_buffer_view move_view(std::uint32_t first, std::uint32_t count) const {
			assert((first + count) * _stride <= _ptr->size);
			return structured_buffer_view(_ptr, _stride, first, count);
		}
	private:
		/// Initializes all fields of this struct.
		structured_buffer_view(
			std::shared_ptr<_details::buffer> b, std::uint32_t s, std::uint32_t f, std::uint32_t c
		) : basic_resource_handle(std::move(b)), _stride(s), _first(f), _count(c) {
		}

		std::uint32_t _stride = 0; ///< Stride between buffer elements in bytes.
		std::uint32_t _first = 0; ///< Index of the first visible buffer element.
		std::uint32_t _count = 0; ///< Index of the number of visible buffer elements.
	};

	/// A reference of a swap chain.
	struct swap_chain : public basic_resource_handle<_details::swap_chain> {
		friend context;
	public:
		/// Initializes this object to empty.
		swap_chain(std::nullptr_t) : basic_resource_handle(nullptr) {
		}

		/// Resizes this swap chain.
		void resize(cvec2s);
	private:
		/// Initializes this swap chain.
		explicit swap_chain(std::shared_ptr<_details::swap_chain> chain) : basic_resource_handle(std::move(chain)) {
		}
	};

	/// A bindless descriptor array.
	template <typename RecordedResource, typename View> struct descriptor_array :
		public basic_resource_handle<_details::descriptor_array<RecordedResource, View>> {
		friend context;
	public:
		/// Initializes this object to empty.
		descriptor_array(std::nullptr_t) : _base(nullptr) {
		}
	private:
		using _base = basic_resource_handle<_details::descriptor_array<RecordedResource, View>>; ///< Base type.
		/// Initializes this descriptor array.
		explicit descriptor_array(
			std::shared_ptr<_details::descriptor_array<RecordedResource, View>> arr
		) : _base(std::move(arr)) {
		}
	};
	/// An array of image descriptors.
	using image_descriptor_array = descriptor_array<recorded_resources::image2d_view, gpu::image2d_view>;
	/// An array of buffer descriptors.
	using buffer_descriptor_array = descriptor_array<recorded_resources::structured_buffer_view, void>;
	namespace recorded_resources {
		/// \ref renderer::image_descriptor_array.
		using image_descriptor_array = descriptor_array<image2d_view, gpu::image2d_view>;
		/// \ref renderer::buffer_descriptor_array.
		using buffer_descriptor_array = descriptor_array<structured_buffer_view, void>;
	}

	/// A bottom level acceleration structure.
	struct blas : public basic_resource_handle<_details::blas> {
		friend context;
	public:
		/// Initializes this object to empty.
		blas(std::nullptr_t) : basic_resource_handle(nullptr) {
		}
	private:
		/// Initializes this acceleration structure.
		explicit blas(std::shared_ptr<_details::blas> b) : basic_resource_handle(std::move(b)) {
		}
	};

	/// A top level acceleration structure.
	struct tlas : public basic_resource_handle<_details::tlas> {
		friend context;
	public:
		/// Initializes this object to empty.
		tlas(std::nullptr_t) : basic_resource_handle(nullptr) {
		}
	private:
		/// Initializes this acceleration structure.
		explicit tlas(std::shared_ptr<_details::tlas> t) : basic_resource_handle(std::move(t)) {
		}
	};

	/// A cached descriptor set.
	struct cached_descriptor_set : public basic_resource_handle<_details::cached_descriptor_set> {
		friend context;
	public:
		/// Initializes this object to empty.
		cached_descriptor_set(std::nullptr_t) : basic_resource_handle(nullptr) {
		}
	private:
		/// Initializes the descriptor set.
		explicit cached_descriptor_set(std::shared_ptr<_details::cached_descriptor_set> s) :
			basic_resource_handle(std::move(s)) {
		}
	};


	/// Describes a reference to a BLAS from a TLAS. Corresponds to the parameters of
	/// \ref gpu::device::get_bottom_level_acceleration_structure_description().
	struct blas_reference {
		/// Initializes this reference to empty.
		blas_reference(std::nullptr_t) : acceleration_structure(nullptr) {
		}
		/// Initializes all fields of this struct.
		blas_reference(blas as, mat44f trans, std::uint32_t as_id, std::uint8_t as_mask, std::uint32_t hg_offset) :
			acceleration_structure(std::move(as)), transform(trans),
			id(as_id), mask(as_mask), hit_group_offset(hg_offset) {
		}

		blas acceleration_structure; ///< The acceleration structure.
		mat44f transform = uninitialized; ///< Transform of this instance.
		std::uint32_t id = 0; ///< ID of this instance.
		std::uint8_t mask = 0; ///< Ray mask.
		std::uint32_t hit_group_offset = 0; ///< Offset in the hit group.
	};


	namespace recorded_resources {
		template <
			typename Resource
		> basic_handle<Resource>::basic_handle(const basic_resource_handle<Resource> &arr) :
			_ptr(arr._ptr.get()) {
		}
	}


	template <typename T> structured_buffer_view buffer::get_view(std::uint32_t first, std::uint32_t count) const {
		return get_view(sizeof(T), first, count);
	}
}
