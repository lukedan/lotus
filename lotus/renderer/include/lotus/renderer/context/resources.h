#pragma once

/// \file
/// Resource classes.

#include <vector>
#include <deque>

#include "lotus/logging.h"
#include "lotus/memory/managed_allocator.h"
#include "lotus/containers/short_vector.h"
#include "lotus/system/window.h"
#include "lotus/gpu/descriptors.h"
#include "lotus/gpu/frame_buffer.h"
#include "lotus/gpu/pipeline.h"
#include "lotus/gpu/resources.h"
#include "lotus/renderer/common.h"
#include "misc.h"
#include "resource_bindings.h"

namespace lotus::renderer {
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
			u32 index = 0; ///< The index of this image in the array.
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
				constexpr static usize invalid_chunk_index = std::numeric_limits<usize>::max();

				usize _chunk_index = invalid_chunk_index; ///< The index of the chunk.
				usize _address = 0; ///< Address of the memory block within the chunk.

				/// Initializes all fields of this struct.
				token(usize ch, usize addr) : _chunk_index(ch), _address(addr) {
				}
			};
			/// Callback function used to allocate memory chunks.
			using allocation_function = static_function<gpu::memory_block(usize)>;

			/// Initializes the pool.
			explicit pool(
				allocation_function alloc, usize chunk_sz, unique_resource_id i, std::u8string_view n
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
			[[nodiscard]] std::pair<const gpu::memory_block&, usize> get_memory_and_offset(token tk) const {
				return { _chunks[tk._chunk_index].memory, tk._address };
			}

			/// Callback for allocating memory blocks.
			static_function<gpu::memory_block(usize)> allocate_memory;
			usize chunk_size = 0; ///< Chunk size.
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

		/// Non-template base class of images managed by a context.
		struct image_base : public resource {
		public:
			/// Initializes this image to empty with the specified number of queues.
			image_base(
				std::shared_ptr<pool> p,
				u32 mips,
				gpu::format fmt,
				gpu::image_tiling t,
				gpu::image_usage_mask u,
				unique_resource_id i,
				std::u8string_view n
			) :
				resource(i, n),
				memory_pool(std::move(p)),
				memory(nullptr),
				num_mips(mips),
				format(fmt),
				tiling(t),
				usages(u),
				previous_access(1, std::vector<image_access_event>(mips, std::nullopt)) {
			}

			/// Returns the image object.
			[[nodiscard]] virtual const gpu::image_base &get_image() const = 0;

			std::shared_ptr<pool> memory_pool; ///< Memory pool to allocate this image out of.
			pool::token memory; ///< Allocated memory for this image.

			u32 num_mips = 0; ///< Number of allocated mips.
			gpu::format format = gpu::format::none; ///< Original pixel format.
			// TODO are these necessary?
			gpu::image_tiling tiling = gpu::image_tiling::optimal; ///< Tiling of this image.
			gpu::image_usage_mask usages = gpu::image_usage_mask::none; ///< Possible usages.

			/// The last events where all mips and array slices of this image were accessed. The inner array is for
			/// mips while the outer array is for array slices.
			std::vector<std::vector<image_access_event>> previous_access; // TODO: subresource range is not needed
		};
		/// Templated base class of images. Contains the handle to the \ref gpu::basic_image.
		template <gpu::image_type Type> struct typed_image_base : public image_base {
			/// Initializes this image to empty.
			typed_image_base(
				std::shared_ptr<pool> p,
				u32 mips,
				gpu::format fmt,
				gpu::image_tiling t,
				gpu::image_usage_mask u,
				unique_resource_id i,
				std::u8string_view n
			) : image_base(std::move(p), mips, fmt, t, u, i, n), image(nullptr) {
			}

			/// Returns the image object.
			[[nodiscard]] const gpu::image_base &get_image() const override {
				return image;
			}

			gpu::basic_image<Type> image; ///< The image.
		};

		/// A 2D image managed by a context.
		struct image2d : public typed_image_base<gpu::image_type::type_2d> {
		public:
			/// Initializes this image to empty.
			image2d(
				cvec2u32 sz,
				u32 mips,
				gpu::format fmt,
				gpu::image_tiling t,
				gpu::image_usage_mask u,
				std::shared_ptr<pool> p,
				unique_resource_id i,
				std::u8string_view n
			) : typed_image_base(std::move(p), mips, fmt, t, u, i, n), size(sz) {
			}

			/// Returns \ref resource_type::image2d.
			[[nodiscard]] resource_type get_type() const override {
				return resource_type::image2d;
			}

			cvec2u32 size; ///< The size of this image.

			/// References in descriptor arrays.
			short_vector<
				descriptor_array_reference<recorded_resources::image2d_view, gpu::image2d_view>, 4
			> array_references;
		};

		/// A 3D image managed by a context.
		struct image3d : public typed_image_base<gpu::image_type::type_3d> {
		public:
			/// Initializes this image to empty.
			image3d(
				cvec3u32 sz,
				u32 mips,
				gpu::format fmt,
				gpu::image_tiling t,
				gpu::image_usage_mask u,
				std::shared_ptr<pool> p,
				unique_resource_id i,
				std::u8string_view n
			) : typed_image_base(std::move(p), mips, fmt, t, u, i, n), size(sz) {
			}

			/// Returns \ref resource_type::image3d.
			[[nodiscard]] resource_type get_type() const override {
				return resource_type::image3d;
			}

			cvec3u32 size; ///< The size of this image.
		};

		/// A buffer.
		struct buffer : public resource {
			/// Initializes this buffer to empty.
			buffer(
				usize sz,
				gpu::buffer_usage_mask usg,
				std::shared_ptr<pool> p,
				unique_resource_id i,
				std::u8string_view n
			) :
				resource(i, n),
				data(nullptr),
				memory_pool(std::move(p)),
				memory(nullptr),
				size(sz),
				usages(usg),
				previous_access(std::nullopt) {
			}

			/// Returns \ref resource_type::buffer.
			[[nodiscard]] resource_type get_type() const override {
				return resource_type::buffer;
			}

			gpu::buffer data; ///< The buffer.
			std::shared_ptr<pool> memory_pool; ///< Memory pool to allocate this buffer out of.
			pool::token memory; ///< Allocated memory for this image.

			usize size; ///< The size of this buffer.
			gpu::buffer_usage_mask usages = gpu::buffer_usage_mask::none; ///< Possible usages.

			/// References in descriptor arrays.
			short_vector<
				descriptor_array_reference<recorded_resources::structured_buffer_view>, 4
			> array_references;

			// resource usage tracking
			/// Last usage of this buffer.
			buffer_access_event previous_access;
		};

		/// A swap chain associated with a window, managed by a context.
		struct swap_chain : public resource {
		public:
			/// Index indicating that a new back buffer needs to be acquired.
			constexpr static u32 invalid_image_index = std::numeric_limits<u32>::max();

			/// Data associated with a single back buffer within this chain.
			struct back_buffer {
				/// Initializes this back buffer with the given image.
				back_buffer(std::nullptr_t) : current_usage(image_access::initial()) {
				}

				image_access current_usage; ///< Current usage of the image.
			};

			/// Initializes all fields of this structure without creating a swap chain.
			swap_chain(
				system::window &wnd, u32 qi, u32 imgs, std::vector<gpu::format> fmts,
				unique_resource_id i, std::u8string_view n
			) :
				resource(i, n),
				chain(nullptr), current_size(zero), desired_size(zero), current_format(gpu::format::none),
				window(wnd), queue_index(qi), num_images(imgs), expected_formats(std::move(fmts)) {
			}

			/// Returns \ref resource_type::swap_chain.
			[[nodiscard]] resource_type get_type() const override {
				return resource_type::swap_chain;
			}

			gpu::swap_chain chain; ///< The swap chain.
			std::vector<gpu::fence> fences; ///< Synchronization primitives for each back buffer.
			std::vector<back_buffer> back_buffers; ///< Back buffers in this swap chain.

			cvec2u32 current_size; ///< Current size of swap chain images.
			cvec2u32 desired_size; ///< Desired size of swap chain images.
			gpu::format current_format = gpu::format::none; ///< Format of the swap chain images.

			/// Index of the next image that would be presented in this swap chain.
			u32 next_image_index = invalid_image_index;
			/// Holds the current image to be written to and presented in the swap chain. This is initialized during
			/// the pseudo execution phase when the swap chain is used, and cleared when it is finally presented
			/// during execution.
			gpu::image2d *current_image = nullptr;
			/// The last batch when this swap chain was presented.
			batch_index previous_present = batch_index::zero;

			system::window &window; ///< The window that owns this swap chain.
			/// The queue that this swap chain is allowed to present on.
			u32 queue_index = std::numeric_limits<u32>::max();
			u32 num_images = 0; ///< Number of images in the swap chain.
			std::vector<gpu::format> expected_formats; ///< Expected swap chain formats.
		};

		// TODO: implement the behavior described below
		/// A bindless descriptor array.
		///
		/// When writing a non-empty descriptor to an empty slot, that write can be carried out immediately. In
		/// practice, we stage these writes until batch execution.
		/// When writing a (empty or non-empty) descriptor to a non-empty slot, the context will check that the
		/// descriptor is not in use. This means that double-buffering of descriptor arrays may be necessary.
		/// When a resource is destroyed, it will automatically be removed from the descriptor array. Note that this
		/// triggers the check for whether the descriptor array is in use. In most cases, it will be easier to
		/// manually write an empty descriptor to the slot before discarding the resource.
		template <typename RecordedResource, typename View> struct descriptor_array : public resource {
		public:
			/// A slot in this array that contains a reference to a resource.
			struct slot {
				/// Initializes this reference to empty.
				slot(std::nullptr_t) : resource(nullptr), view(nullptr) {
				}

				RecordedResource resource; ///< The referenced resource.
				/// View object of the resource.
				[[no_unique_address]] static_optional<View, !std::is_same_v<View, std::nullopt_t>> view;
				u32 reference_index = 0; ///< Index of this reference in \p Descriptor::array_references.
				bool written = false; ///< Whether this slot has been updated to the device.
			};

			/// Initializes all fields of this structure without creating a descriptor set.
			descriptor_array(
				gpu::descriptor_type ty, u32 capacity, unique_resource_id i, std::u8string_view n
			) : resource(i, n), set(nullptr), type(ty) {
				// we have to do this manually because the copy constructor may be deleted
				slots.reserve(capacity);
				for (u32 di = 0; di < capacity; ++di) {
					slots.emplace_back(nullptr);
				}
			}

			/// Returns \ref resource_type::swap_chain.
			[[nodiscard]] resource_type get_type() const override {
				if constexpr (std::is_same_v<RecordedResource, recorded_resources::image2d_view>) {
					return resource_type::image2d_descriptor_array;
				} else if constexpr (std::is_same_v<RecordedResource, recorded_resources::structured_buffer_view>) {
					return resource_type::buffer_descriptor_array;
				} else {
					std::unreachable(); // not implemented
				}
			}

			gpu::descriptor_set set; ///< The descriptor set.
			const gpu::descriptor_set_layout *layout = nullptr; ///< Layout of this descriptor array.
			/// The type of this descriptor array.
			gpu::descriptor_type type = gpu::descriptor_type::num_enumerators;

			std::vector<slot> slots; ///< Contents of this descriptor array.

			/// Indices of all resources that have been used externally and may need transitions.
			std::vector<u32> altered_resources;
			/// Indices of all resources that have been modified in \ref resources but have not been written into
			/// \ref set.
			std::vector<u32> staged_writes;
		};

		/// A bottom-level acceleration structure.
		struct blas : public resource {
		public:
			/// Initializes this structure.
			blas(std::shared_ptr<pool> p, unique_resource_id i, std::u8string_view n) :
				resource(i, n), handle(nullptr), memory_pool(std::move(p)) {
			}

			/// Returns \ref resource_type::blas.
			[[nodiscard]] resource_type get_type() const override {
				return resource_type::blas;
			}

			// these are populated when we actually build the BVH
			gpu::bottom_level_acceleration_structure handle; ///< The acceleration structure.
			std::shared_ptr<buffer> memory; ///< Memory for this acceleration structure.
			std::shared_ptr<pool> memory_pool; ///< Memory pool to allocate the BLAS out of.
		};

		/// A top-level acceleration structure.
		struct tlas : public resource {
		public:
			/// Initializes this structure.
			tlas(std::shared_ptr<pool> p, unique_resource_id i, std::u8string_view n) :
				resource(i, n), handle(nullptr), memory_pool(std::move(p)) {
			}

			/// Returns \ref resource_type::tlas.
			[[nodiscard]] resource_type get_type() const override {
				return resource_type::tlas;
			}

			// these are populated when we actually build the BVH
			gpu::top_level_acceleration_structure handle; ///< The acceleration structure.
			std::shared_ptr<buffer> memory; ///< Memory for this acceleration structure.
			/// Memory pool to allocate this TLAS out of. Input data is also allocated out of this pool.
			std::shared_ptr<pool> memory_pool;
		};

		/// A dependency between commands. A dependency can be released only once, but can be acquired (waited on)
		/// multiple times.
		struct dependency : public resource {
			/// Information about the command that releases this dependency.
			struct release_info {
				/// Initializes all fields of this struct.
				release_info(u32 q, batch_index bi, queue_submission_index qsi) :
					queue(q), batch(bi), command_index(qsi) {
				}

				u32 queue; ///< Index of the queue this was released on.
				batch_index batch; ///< Batch index of the command that released this dependency.
				queue_submission_index command_index; ///< Queue index of the command that released this dependency.
			};

			/// Initializes the dependency.
			dependency(unique_resource_id i, std::u8string_view name) : resource(i, name) {
			}

			/// Returns \ref resource_type::dependency.
			[[nodiscard]] resource_type get_type() const override {
				return resource_type::dependency;
			}

			std::optional<release_info> release_event; ///< The release event of this dependency.
			/// The value indicating that this dependency has been released.
			std::optional<gpu::timeline_semaphore::value_type> release_value;
		};

		/// A cached descriptor set.
		struct cached_descriptor_set : public resource {
			/// Records how this descriptor set accesses an image.
			template <typename Img, typename View> struct image_access {
				/// Initializes all fields of this struct.
				image_access(
					std::shared_ptr<Img> img,
					gpu::format fmt,
					gpu::subresource_range sub,
					image_binding_type bt,
					u32 i
				) :
					image(std::move(img)), view(nullptr), view_format(fmt),
					subresource_range(sub), binding_type(bt), register_index(i) {
				}

				std::shared_ptr<Img> image; ///< The image.
				View view; ///< The view.
				gpu::format view_format; ///< Format that this image is viewed as.
				gpu::subresource_range subresource_range; ///< The subresource range.
				image_binding_type binding_type; ///< The type of this image binding.
				u32 register_index; ///< Register index of the descriptor.

				/// Returns a \ref _details::image_access object that corresponds to this access with the given sync
				/// point.
				[[nodiscard]] _details::image_access get_image_access(gpu::synchronization_point_mask sync) const {
					return _details::image_access::from_binding_type(subresource_range, sync, binding_type);
				}
			};
			/// Records how this descriptor set accesses a buffer.
			struct buffer_access {
				std::shared_ptr<_details::buffer> buffer; ///< The buffer.
				gpu::buffer_access_mask access; ///< How the buffer is accessed.
				u32 register_index; ///< Register index of the descriptor.
			};
			/// Records how this descriptor set uses a sampler.
			struct sampler_access {
				/// Initializes all fields of this struct.
				sampler_access(gpu::sampler s, u32 r) : sampler(std::move(s)), register_index(r) {
				}

				gpu::sampler sampler; ///< The sampler.
				u32 register_index; ///< Register index of the descriptor.
			};

			/// Initializes all fields of this struct.
			cached_descriptor_set(
				std::vector<gpu::descriptor_range_binding> rs,
				const gpu::descriptor_set_layout &l,
				unique_resource_id i,
				std::u8string_view n
			) : resource(i, n), set(nullptr), ranges(std::move(rs)), layout(&l) {
			}

			/// Returns \ref resource_type::cached_descriptor_set.
			[[nodiscard]] resource_type get_type() const override {
				return resource_type::cached_descriptor_set;
			}

			gpu::descriptor_set set; ///< The descriptor set.

			std::vector<gpu::descriptor_range_binding> ranges; ///< Sorted descriptor ranges.
			const gpu::descriptor_set_layout *layout = nullptr; ///< Layout of this descriptor set.

			// resources that are referenced by this descriptor set
			/// All 2D images referenced by this descriptor set.
			std::vector<image_access<_details::image2d, gpu::image2d_view>> used_image2ds;
			/// All 3D images referenced by this descriptor set.
			std::vector<image_access<_details::image3d, gpu::image3d_view>> used_image3ds;
			/// All buffers referenced by this descriptor set.
			std::vector<buffer_access> used_buffers;
			/// All samplers used by this descriptor set.
			std::vector<sampler_access> used_samplers;
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
		constexpr static u32 default_chunk_size = 100 * 1024 * 1024; ///< Default chunk size is 100 MiB.

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
		[[nodiscard]] u32 get_num_mip_levels() const {
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
		[[nodiscard]] usize get_size_in_bytes() const {
			return _ptr->size;
		}

		/// Returns a view of this buffer as a structured buffer.
		[[nodiscard]] structured_buffer_view get_view(u32 stride, u32 first, u32 count) const;
		/// \overload
		template <typename T> [[nodiscard]] structured_buffer_view get_view(u32 first, u32 count) const;

		/// Binds the whole buffer as a constant buffer.
		[[nodiscard]] descriptor_resource::constant_buffer bind_as_constant_buffer() const {
			return descriptor_resource::constant_buffer(*this, 0, _ptr->size);
		}
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
		[[nodiscard]] u32 get_stride() const {
			return _stride;
		}
		/// Returns the first element visible to this view.
		[[nodiscard]] u32 get_first_element_index() const {
			return _first;
		}
		/// Returns the number of elements visible to this view.
		[[nodiscard]] u32 get_num_elements() const {
			return _count;
		}

		/// Returns the underlying raw buffer.
		[[nodiscard]] buffer get_buffer() const {
			return buffer(_ptr);
		}
		/// Returns the buffer viewed as another type. This function preseves the current viewed region.
		template <typename T> [[nodiscard]] structured_buffer_view view_as() const {
			u32 first_byte = _first * _stride;
			u32 size_bytes = _count * _stride;
			crash_if(first_byte % sizeof(T) != 0);
			crash_if(size_bytes % sizeof(T) != 0);
			return structured_buffer_view(_ptr, sizeof(T), first_byte / sizeof(T), size_bytes / sizeof(T));
		}

		/// Moves the range of visible elements and returns the new view.
		[[nodiscard]] structured_buffer_view move_view(u32 first, u32 count) const {
			crash_if((first + count) * _stride > _ptr->size);
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
			std::shared_ptr<_details::buffer> b, u32 s, u32 f, u32 c
		) : basic_resource_handle(std::move(b)), _stride(s), _first(f), _count(c) {
		}

		u32 _stride = 0; ///< Stride between buffer elements in bytes.
		u32 _first = 0; ///< Index of the first visible buffer element.
		u32 _count = 0; ///< Index of the number of visible buffer elements.
	};

	/// A reference of a swap chain.
	///
	/// Each swap chain can only be presented at most once per batch. After a swap chain has been presented to, it
	/// cannot be used again in the same batch.
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

	/// A dependency.
	struct dependency : public basic_resource_handle<_details::dependency> {
		friend context;
	public:
		/// Initializes this object to empty.
		dependency(std::nullptr_t) : basic_resource_handle(nullptr) {
		}
	private:
		/// Initializes this dependency.
		explicit dependency(std::shared_ptr<_details::dependency> d) : basic_resource_handle(std::move(d)) {
		}
	};


	/// Describes a reference to a BLAS from a TLAS. Corresponds to the parameters of
	/// \ref gpu::device::get_bottom_level_acceleration_structure_description().
	struct blas_instance {
		/// Initializes this reference to empty.
		blas_instance(std::nullptr_t) : acceleration_structure(nullptr) {
		}
		/// Initializes all fields of this struct.
		blas_instance(
			blas as, mat44f trans, u32 as_id, u8 as_mask, u32 hg_offset,
			gpu::raytracing_instance_flags f
		) :
			acceleration_structure(std::move(as)), transform(trans),
			id(as_id), mask(as_mask), hit_group_offset(hg_offset), flags(f) {
		}

		recorded_resources::blas acceleration_structure; ///< The acceleration structure.
		mat44f transform = uninitialized; ///< Transform of this instance.
		u32 id = 0; ///< ID of this instance.
		u8 mask = 0; ///< Ray mask.
		u32 hit_group_offset = 0; ///< Offset in the hit group.
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
			bool warn = false;
			if (result._mip_levels.is_tail()) {
				crash_if(result._mip_levels.first_level >= result._ptr->num_mips);
				warn = result._ptr->num_mips - result._mip_levels.first_level > 1;
			} else {
				crash_if(result._mip_levels.num_levels == 0);
				crash_if(result._mip_levels.into_range().end > result._ptr->num_mips);
				warn = result._mip_levels.num_levels > 1;
			}
			if (warn) {
				log().error(
					"More than one mip specified for render target for texture {}",
					string::to_generic(result._ptr->name)
				);
			}
			result._mip_levels = gpu::mip_levels::only(result._mip_levels.first_level);
			return result;
		}
	}


	template <typename T> structured_buffer_view buffer::get_view(u32 first, u32 count) const {
		return get_view(sizeof(T), first, count);
	}
}
