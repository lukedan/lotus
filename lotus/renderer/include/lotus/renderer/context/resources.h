#pragma once

#include <vector>

#include "lotus/logging.h"
#include "lotus/memory/managed_allocator.h"
#include "lotus/system/window.h"
#include "lotus/gpu/descriptors.h"
#include "lotus/gpu/frame_buffer.h"
#include "lotus/gpu/pipeline.h"
#include "lotus/gpu/resources.h"
#include "lotus/renderer/common.h"
#include "execution.h"
#include "resource_bindings.h"

namespace lotus::renderer {
	/// Used to uniquely identify a resource.
	enum class unique_resource_id : std::uint64_t {
		invalid = 0 ///< An invalid ID.
	};


	/// Internal data structures used by the rendering context.
	namespace _details {
		/// Returns the descriptor type that corresponds to the image binding.
		[[nodiscard]] gpu::descriptor_type to_descriptor_type(image_binding_type);

		/// A reference to a usage of this surface in a descriptor array.
		template <typename RecordedResource, typename View = std::nullopt_t> struct descriptor_array_reference {
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
			resource(unique_resource_id i, std::u8string_view n) : id(i), name(n) {
			}
			/// Default destructor.
			virtual ~resource() = default;

			/// Returns the type of this resource.
			[[nodiscard]] virtual resource_type get_type() const = 0;

			unique_resource_id id = unique_resource_id::invalid; ///< Unique ID of this resource.
			// TODO make this debug only?
			std::u8string name; ///< The name of this resource.
		};

		/// A pool that resources can be allocated out of.
		struct pool : public resource {
		public:
			/// A token of an allocation.
			struct token {
				friend pool;
			public:
				/// Initializes this token to empty.
				constexpr token(std::nullptr_t) {
				}

				/// Returns \p true if this represents a valid allocation.
				[[nodiscard]] constexpr bool is_valid() const {
					return _chunk_index != invalid_chunk_index;
				}
				/// \overload
				[[nodiscard]] constexpr explicit operator bool() const {
					return is_valid();
				}
			private:
				/// Index indicating an invalid token.
				constexpr static std::uint32_t invalid_chunk_index = std::numeric_limits<std::uint32_t>::max();

				std::uint32_t _chunk_index = invalid_chunk_index; ///< The index of the chunk.
				std::uint32_t _address = 0; ///< Address of the memory block within the chunk.

				/// Initializes all fields of this struct.
				token(std::uint32_t ch, std::uint32_t addr) : _chunk_index(ch), _address(addr) {
				}
			};
			/// Callback function used to allocate memory chunks.
			using allocation_function = static_function<gpu::memory_block(std::size_t)>;

			/// Initializes the pool.
			explicit pool(
				allocation_function alloc, std::uint32_t chunk_sz, unique_resource_id i, std::u8string_view n
			) : resource(i, n), allocate_memory(std::move(alloc)), chunk_size(chunk_sz) {
			}

			/// Returns \ref resource_type::pool.
			[[nodiscard]] resource_type get_type() const override {
				return resource_type::pool;
			}

			/// Allocates a memory block.
			[[nodiscard]] token allocate(memory::size_alignment);
			/// Frees the given memory block.
			void free(token);

			/// Given a \ref token, returns the corresponding memory block and its offset within it.
			[[nodiscard]] std::pair<const gpu::memory_block&, std::uint32_t> get_memory_and_offset(token tk) const {
				return { _chunks[tk._chunk_index].memory, tk._address };
			}

			/// Callback for allocating memory blocks.
			static_function<gpu::memory_block(std::size_t)> allocate_memory;
			std::uint32_t chunk_size = 0; ///< Chunk size.
			bool debug_log_allocations = false;
		private:
			/// A chunk of GPU memory managed by this pool.
			struct _chunk {
				// TODO what data should we include in the allocations?
				using allocator_t = memory::managed_allocator<int>;

				/// Initializes all fields of this struct.
				_chunk(gpu::memory_block mem, allocator_t alloc) :
					memory(std::move(mem)), allocator(std::move(alloc)) {
				}

				gpu::memory_block memory; ///< The memory block.
				allocator_t allocator; ///< Allocator.
			};

			std::deque<_chunk> _chunks; ///< Allocated chunks.
		};

		/// Base class of images managed by a context.
		template <gpu::image_type Type> struct image_base : public resource {
		public:
			/// Initializes this image to empty.
			image_base(
				std::shared_ptr<pool> p,
				std::uint32_t mips, gpu::format fmt, gpu::image_tiling t, gpu::image_usage_mask u,
				unique_resource_id i, std::u8string_view n
			) :
				resource(i, n),
				image(nullptr), memory_pool(std::move(p)), memory(nullptr),
				num_mips(mips), format(fmt), tiling(t), usages(u) {
			}

			gpu::basic_image<Type> image; ///< The image.
			std::shared_ptr<pool> memory_pool; ///< Memory pool to allocate this image out of.
			pool::token memory; ///< Allocated memory for this image.

			std::uint32_t num_mips = 0; ///< Number of allocated mips.
			gpu::format format = gpu::format::none; ///< Original pixel format.
			// TODO are these necessary?
			gpu::image_tiling tiling = gpu::image_tiling::optimal; ///< Tiling of this image.
			gpu::image_usage_mask usages = gpu::image_usage_mask::none; ///< Possible usages.
		};

		/// A 2D image managed by a context.
		struct image2d : public image_base<gpu::image_type::type_2d> {
		public:
			/// Initializes this image to empty.
			image2d(
				cvec2u32 sz,
				std::uint32_t mips,
				gpu::format fmt,
				gpu::image_tiling t,
				gpu::image_usage_mask u,
				std::shared_ptr<pool> p,
				unique_resource_id i,
				std::u8string_view n
			) :
				image_base(std::move(p), mips, fmt, t, u, i, n),
				size(sz), current_usages(mips, image_access::initial()) {
			}

			/// Returns \ref resource_type::image2d.
			[[nodiscard]] resource_type get_type() const override {
				return resource_type::image2d;
			}

			cvec2u32 size; ///< The size of this image.

			std::vector<image_access> current_usages; ///< Current usage of each mip of the image.
			/// References in descriptor arrays.
			std::vector<
				descriptor_array_reference<recorded_resources::image2d_view, gpu::image2d_view>
			> array_references;
		};

		/// A 3D image managed by a context.
		struct image3d : public image_base<gpu::image_type::type_3d> {
		public:
			/// Initializes this image to empty.
			image3d(
				cvec3u32 sz,
				std::uint32_t mips,
				gpu::format fmt,
				gpu::image_tiling t,
				gpu::image_usage_mask u,
				std::shared_ptr<pool> p,
				unique_resource_id i,
				std::u8string_view n
			) :
				image_base(std::move(p), mips, fmt, t, u, i, n),
				size(sz), current_usages(mips, image_access::initial()) {
			}

			/// Returns \ref resource_type::image3d.
			[[nodiscard]] resource_type get_type() const override {
				return resource_type::image3d;
			}

			cvec3u32 size; ///< The size of this image.

			std::vector<image_access> current_usages; ///< Current usage of each mip of the image.
		};

		/// A buffer.
		struct buffer : public resource {
			/// Initializes this buffer to empty.
			buffer(
				std::uint32_t sz, gpu::buffer_usage_mask usg, std::shared_ptr<pool> p,
				unique_resource_id i, std::u8string_view n
			) :
				resource(i, n), data(nullptr), memory_pool(std::move(p)), memory(nullptr),
				access(buffer_access::initial()), size(sz), usages(usg) {
			}

			/// Returns \ref resource_type::buffer.
			[[nodiscard]] resource_type get_type() const override {
				return resource_type::buffer;
			}

			gpu::buffer data; ///< The buffer.
			std::shared_ptr<pool> memory_pool; ///< Memory pool to allocate this buffer out of.
			pool::token memory; ///< Allocated memory for this image.

			/// Current usage of this buffer.
			buffer_access access;
			/// Usage hint for this buffer - when transitioning to one of the states in this mask, this resource will
			/// be instead transitioned to all usages in this mask, making it unnecessary to transition again when
			/// switching between them.
			gpu::buffer_access_mask usage_hint = gpu::buffer_access_mask::none;

			std::uint32_t size; ///< The size of this buffer.
			gpu::buffer_usage_mask usages = gpu::buffer_usage_mask::none; ///< Possible usages.

			/// References in descriptor arrays.
			std::vector<
				descriptor_array_reference<recorded_resources::structured_buffer_view>
			> array_references;
		};

		/// A swap chain associated with a window, managed by a context.
		struct swap_chain : public resource {
		public:
			/// Image index indicating that a next image has not been acquired.
			constexpr static std::uint32_t invalid_image_index = std::numeric_limits<std::uint32_t>::max();

			/// Initializes all fields of this structure without creating a swap chain.
			swap_chain(
				system::window &wnd, std::uint32_t imgs, std::vector<gpu::format> fmts,
				unique_resource_id i, std::u8string_view n
			) :
				resource(i, n),
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

			cvec2u32 current_size; ///< Current size of swap chain images.
			cvec2u32 desired_size; ///< Desired size of swap chain images.
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
				static_optional<View, !std::is_same_v<View, std::nullopt_t>> view; ///< View object of the resource.
				std::uint32_t reference_index = 0; ///< Index of this reference in \p Descriptor::array_references.
			};

			/// Initializes all fields of this structure without creating a descriptor set.
			descriptor_array(
				gpu::descriptor_type ty, std::uint32_t cap, unique_resource_id i, std::u8string_view n
			) : resource(i, n), set(nullptr), capacity(cap), type(ty) {
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
			blas(
				std::vector<geometry_buffers_view> in, std::shared_ptr<pool> p,
				unique_resource_id i, std::u8string_view n
			) :
				resource(i, n), handle(nullptr), geometry(nullptr), build_sizes(uninitialized),
				memory_pool(std::move(p)), input(std::move(in)) {
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
			std::shared_ptr<pool> memory_pool; ///< Memory pool to allocate the BLAS out of.

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
				std::shared_ptr<pool> p,
				unique_resource_id i,
				std::u8string_view n
			) :
				resource(i, n), handle(nullptr), input_data(nullptr), build_sizes(uninitialized),
				memory_pool(std::move(p)), input_data_token(nullptr),
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
			std::shared_ptr<pool> memory_pool;
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
				unique_resource_id i,
				std::u8string_view n
			) :
				resource(i, n),
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
			std::vector<gpu::image2d_view> image2d_views; ///< 2D image views used by this descriptor set.
			std::vector<gpu::image3d_view> image3d_views; ///< 3D image views used by this descriptor set.
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

		/// Returns the unique ID of the resource.
		[[nodiscard]] unique_resource_id get_unique_id() const {
			return _ptr->id;
		}

		/// Returns whether this object holds a valid image view.
		[[nodiscard]] bool is_valid() const {
			return _ptr != nullptr;
		}
		/// \overload
		[[nodiscard]] explicit operator bool() const {
			return is_valid();
		}

		/// Default equality comparisons.
		[[nodiscard]] friend bool operator==(const basic_resource_handle&, const basic_resource_handle&) = default;
	protected:
		/// Initializes this resource handle.
		explicit basic_resource_handle(std::shared_ptr<Resource> p) : _ptr(std::move(p)) {
		}

		std::shared_ptr<Resource> _ptr; ///< Pointer to the resource.
	};


	/// A reference of a resource pool.
	struct pool : public basic_resource_handle<_details::pool> {
		friend context;
	public:
		constexpr static std::uint32_t default_chunk_size = 100 * 1024 * 1024; ///< Default chunk size is 100 MiB.

		/// Initializes this handle to empty.
		pool(std::nullptr_t) : basic_resource_handle(nullptr) {
		}

		/// Returns a reference to a \p bool controlling whether allocations should be logged.
		[[nodiscard]] bool &debug_log_allocations() const {
			return _ptr->debug_log_allocations;
		}
	private:
		/// Initializes the base handle.
		explicit pool(std::shared_ptr<_details::pool> p) : basic_resource_handle(std::move(p)) {
		}
	};

	/// A reference of a view into an image.
	template <gpu::image_type Type> struct image_view_base :
		public basic_resource_handle<_details::image_data_t<Type>> {

		friend recorded_resources::basic_image_view<Type>;
	public:
		/// Returns the format that this image is viewed as.
		[[nodiscard]] gpu::format get_viewed_as_format() const {
			return _view_format;
		}
		/// Returns the original format of this image.
		[[nodiscard]] gpu::format get_original_format() const {
			return this->_ptr->format;
		}

		/// Returns the number of mip levels allocated for this texture.
		[[nodiscard]] std::uint32_t get_num_mip_levels() const {
			return this->_ptr->num_mips;
		}
		/// Returns the mip levels that are visible for this image view.
		[[nodiscard]] const gpu::mip_levels &get_viewed_mip_levels() const {
			return _mip_levels;
		}

		/// Shorthand for creating a \ref descriptor_resource::basic_image.
		[[nodiscard]] descriptor_resource::basic_image<Type> bind(image_binding_type ty) const {
			return descriptor_resource::basic_image<Type>(*this, ty);
		}
		/// Shorthand for creating a read-only \ref descriptor_resource::basic_image.
		[[nodiscard]] descriptor_resource::basic_image<Type> bind_as_read_only() const {
			return bind(image_binding_type::read_only);
		}
		/// Shorthand for creating a read-write \ref descriptor_resource::basic_image.
		[[nodiscard]] descriptor_resource::basic_image<Type> bind_as_read_write() const {
			return bind(image_binding_type::read_write);
		}

		/// Default equality comparison.
		[[nodiscard]] friend bool operator==(const image_view_base&, const image_view_base&) = default;
	protected:
		using _image_ptr = std::shared_ptr<_details::image_data_t<Type>>; ///< Handle type.
		/// Initializes this view to empty.
		image_view_base(std::nullptr_t) :
			basic_resource_handle<_details::image_data_t<Type>>(nullptr), _mip_levels(gpu::mip_levels::all()) {
		}
		/// Initializes all fields of this struct.
		image_view_base(_image_ptr img, gpu::format fmt, gpu::mip_levels mips) :
			basic_resource_handle<_details::image_data_t<Type>>(std::move(img)),
			_view_format(fmt), _mip_levels(mips) {
		}
		/// Prevents objects of this type from being created directly.
		~image_view_base() = default;

		/// The format to view as; may be different from the original format of the image.
		gpu::format _view_format = gpu::format::none;
		gpu::mip_levels _mip_levels; ///< Mip levels that are included in this view.
	};
	/// Implements conversions between different views of the same image. This is necessary because we don't want the
	/// CRTP parameter in other (templated) parts of the interface.
	template <gpu::image_type Type, typename Derived> struct image_view_base2 : public image_view_base<Type> {
	public:
		/// Creates another view of the image in another format.
		[[nodiscard]] Derived view_as(gpu::format fmt) const {
			return Derived(this->_ptr, fmt, this->_mip_levels);
		}
		/// Creates another view of the given mip levels of this image.
		[[nodiscard]] Derived view_mips(gpu::mip_levels mips) const {
			return Derived(this->_ptr, this->_view_format, mips);
		}
		/// Creates another view of the given mip levels of this image in another format.
		[[nodiscard]] Derived view_mips_as(gpu::format fmt, gpu::mip_levels mips) const {
			return Derived(this->_ptr, fmt, mips);
		}
	protected:
		using image_view_base<Type>::image_view_base;
		/// Prevents objects of this type from being created directly.
		~image_view_base2() = default;
	};
	/// 2D image views.
	struct image2d_view : public image_view_base2<gpu::image_type::type_2d, image2d_view> {
		friend context;
		friend image_view_base2;
	public:
		/// Initializes this view to empty.
		image2d_view(std::nullptr_t) : image_view_base2(nullptr) {
		}

		/// Returns the size of the top mip of this image.
		[[nodiscard]] cvec2u32 get_size() const {
			return _ptr->size;
		}
	private:
		/// Initializes all fields of this struct.
		image2d_view(_image_ptr img, gpu::format fmt, gpu::mip_levels mips) :
			image_view_base2(std::move(img), fmt, mips) {
		}
	};
	/// 3D image views.
	struct image3d_view : public image_view_base2<gpu::image_type::type_3d, image3d_view> {
		friend context;
		friend image_view_base2;
	public:
		/// Initializes this view to empty.
		image3d_view(std::nullptr_t) : image_view_base2(nullptr) {
		}

		/// Returns the size of the top mip of this image.
		[[nodiscard]] cvec3u32 get_size() const {
			return _ptr->size;
		}
	private:
		/// Initializes all fields of this struct.
		image3d_view(_image_ptr img, gpu::format fmt, gpu::mip_levels mips) :
			image_view_base2(std::move(img), fmt, mips) {
		}
	};

	/// A reference of a buffer.
	struct buffer : public basic_resource_handle<_details::buffer> {
		friend context;
		friend structured_buffer_view;
	public:
		/// Initializes the view to empty.
		buffer(std::nullptr_t) : basic_resource_handle(nullptr) {
		}

		/// Returns the size of this buffer.
		[[nodiscard]] std::uint32_t get_size_in_bytes() const {
			return _ptr->size;
		}

		/// Sets the usage hint of this buffer. Note that this method updates the value immediately; the value will
		/// become effective before the next batch is executed and will not change during the execution of commands
		/// unless modified using the \ref context.
		void set_usage_hint(gpu::buffer_access_mask hint) const {
			_ptr->usage_hint = hint;
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

		/// \sa buffer::set_usage_hint()
		void set_usage_hint(gpu::buffer_access_mask hint) const {
			_ptr->usage_hint = hint;
		}

		/// Returns the underlying raw buffer.
		[[nodiscard]] buffer get_buffer() const {
			return buffer(_ptr);
		}
		/// Returns the buffer viewed as another type. This function preseves the current viewed region.
		template <typename T> [[nodiscard]] structured_buffer_view view_as() const {
			std::uint32_t first_byte = _first * _stride;
			std::uint32_t size_bytes = _count * _stride;
			crash_if(first_byte % sizeof(T) != 0);
			crash_if(size_bytes % sizeof(T) != 0);
			return structured_buffer_view(_ptr, sizeof(T), first_byte / sizeof(T), size_bytes / sizeof(T));
		}

		/// Moves the range of visible elements and returns the new view.
		[[nodiscard]] structured_buffer_view move_view(std::uint32_t first, std::uint32_t count) const {
			assert((first + count) * _stride <= _ptr->size);
			return structured_buffer_view(_ptr, _stride, first, count);
		}

		/// Shorthand for creating a \ref descriptor_resource::structured_buffer.
		[[nodiscard]] descriptor_resource::structured_buffer bind(buffer_binding_type ty) const {
			return descriptor_resource::structured_buffer(*this, ty);
		};
		/// Shorthand for creating a read-only \ref descriptor_resource::structured_buffer.
		[[nodiscard]] descriptor_resource::structured_buffer bind_as_read_only() const {
			return bind(buffer_binding_type::read_only);
		};
		/// Shorthand for creating a read-write \ref descriptor_resource::structured_buffer.
		[[nodiscard]] descriptor_resource::structured_buffer bind_as_read_write() const {
			return bind(buffer_binding_type::read_write);
		};
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
		void resize(cvec2u32);
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
		blas_reference(
			blas as, mat44f trans, std::uint32_t as_id, std::uint8_t as_mask, std::uint32_t hg_offset,
			gpu::raytracing_instance_flags f
		) :
			acceleration_structure(std::move(as)), transform(trans),
			id(as_id), mask(as_mask), hit_group_offset(hg_offset), flags(f) {
		}

		blas acceleration_structure; ///< The acceleration structure.
		mat44f transform = uninitialized; ///< Transform of this instance.
		std::uint32_t id = 0; ///< ID of this instance.
		std::uint8_t mask = 0; ///< Ray mask.
		std::uint32_t hit_group_offset = 0; ///< Offset in the hit group.
		gpu::raytracing_instance_flags flags = gpu::raytracing_instance_flags::none; ///< Instance flags.
	};


	namespace recorded_resources {
		template <
			typename Resource
		> basic_handle<Resource>::basic_handle(const basic_resource_handle<Resource> &h) : _ptr(h._ptr.get()) {
		}


		template <gpu::image_type Type> basic_image_view<Type>::basic_image_view(
			const renderer::image_view_base<Type> &view
		) : _base(view), _view_format(view._view_format), _mip_levels(view._mip_levels) {
		}

		template <
			gpu::image_type Type
		> basic_image_view<Type> basic_image_view<Type>::highest_mip_with_warning() const {
			basic_image_view<Type> result = *this;
			if (result._mip_levels.get_num_levels() != 1) {
				if (result._ptr->num_mips - result._mip_levels.minimum > 1) {
					auto num_levels =
						std::min<std::uint32_t>(result._ptr->num_mips, result._mip_levels.maximum) -
						result._mip_levels.minimum;
					log().error<u8"More than one ({}) mip specified for render target for texture {}">(
						num_levels, string::to_generic(result._ptr->name)
					);
				}
				result._mip_levels = gpu::mip_levels::only(result._mip_levels.minimum);
			}
			return result;
		}
	}


	template <typename T> structured_buffer_view buffer::get_view(std::uint32_t first, std::uint32_t count) const {
		return get_view(sizeof(T), first, count);
	}
}
