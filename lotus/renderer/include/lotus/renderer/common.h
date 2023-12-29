#pragma once

/// \file
/// Material context.

#include "lotus/memory/stack_allocator.h"
#include "lotus/containers/short_vector.h"
#include "lotus/gpu/descriptors.h"
#include "lotus/gpu/device.h"
#include "lotus/gpu/frame_buffer.h"
#include "lotus/gpu/resources.h"

namespace lotus::renderer {
	// forward declaration
	namespace _details {
		struct image_base;
		template <gpu::image_type> struct typed_image_base;
		struct image2d;
		struct image3d;
		struct buffer;
		struct structured_buffer_view;
		struct swap_chain;
		template <typename, typename = std::nullopt_t> struct descriptor_array;
		struct blas;
		struct tlas;
		struct cached_descriptor_set;
		struct dependency;

		/// Image data type associated with a specific \ref gpu::image_type.
		template <gpu::image_type> struct image_data;
		/// \ref image_data for 2D images.
		template <> struct image_data<gpu::image_type::type_2d> {
			using type = image2d; ///< 2D images.
		};
		/// \ref image_data for 3D images.
		template <> struct image_data<gpu::image_type::type_3d> {
			using type = image3d; ///< 3D images.
		};
		/// Shorthand for \ref image_data::type.
		template <gpu::image_type Type> using image_data_t = image_data<Type>::type;
	}

	namespace recorded_resources {
		template <typename> struct basic_handle;

		template <gpu::image_type> struct basic_image_view;
		using image2d_view = basic_image_view<gpu::image_type::type_2d>; ///< \ref renderer::image2d_view.
		using image3d_view = basic_image_view<gpu::image_type::type_3d>; ///< \ref renderer::image3d_view.
		struct structured_buffer_view;

		/// \ref renderer::descriptor_array.
		template <typename RecordedResource, typename View = std::nullopt_t> using descriptor_array = basic_handle<
			_details::descriptor_array<RecordedResource, View>
		>;

		/// \ref renderer::image_descriptor_array.
		using image_descriptor_array = descriptor_array<image2d_view, gpu::image2d_view>;
		/// \ref renderer::buffer_descriptor_array.
		using buffer_descriptor_array = descriptor_array<structured_buffer_view>;
	}

	class context;
	struct input_buffer_binding;
	struct index_buffer_binding;
	struct all_resource_bindings;

	template <typename> struct basic_resource_handle;
	template <gpu::image_type> struct image_view_base;
	struct buffer;
	struct structured_buffer_view;
	struct swap_chain;
	template <typename, typename = std::nullopt_t> struct descriptor_array;
	/// An array of image descriptors.
	using image_descriptor_array = descriptor_array<recorded_resources::image2d_view, gpu::image2d_view>;
	/// An array of buffer descriptors.
	using buffer_descriptor_array = descriptor_array<recorded_resources::structured_buffer_view>;
	struct blas;
	struct tlas;
	struct cached_descriptor_set;
	struct dependency;

	namespace assets {
		class manager;
		template <typename> struct handle;
	}

	namespace _details {
		/// Array of image descriptors.
		using image_descriptor_array = descriptor_array<recorded_resources::image2d_view, gpu::image2d_view>;
		/// Array of buffer descriptors.
		using buffer_descriptor_array = descriptor_array<recorded_resources::structured_buffer_view>;
	}

	namespace execution {
		struct descriptor_set_builder;
		class batch_context;
		class queue_context;
		class queue_pseudo_context;
		namespace manual_dependencies {
			struct queue_analysis;
		}
	}
	// end of forward declaration


	/// Indicates whether debug names would be registered for resources.
	constexpr static bool should_register_debug_names = true;

	/// The type of a descriptor set.
	enum class descriptor_set_type {
		normal, ///< A normal descriptor set.
		/// The last range in this descriptor set can have a variable amount of descriptors, determined at time of
		/// descriptor set creation.
		variable_descriptor_count,
	};

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

	/// The type of a resource.
	enum class resource_type {
		pool,                     ///< A memory pool.
		image2d,                  ///< A 2D image.
		image3d,                  ///< A 3D image.
		buffer,                   ///< A buffer.
		swap_chain,               ///< A swap chain.
		image2d_descriptor_array, ///< An array of image2d descriptors.
		buffer_descriptor_array,  ///< An array of buffer descriptors.
		blas,                     ///< A bottom-level descriptor array.
		tlas,                     ///< A top-level descriptor array.
		cached_descriptor_set,    ///< A descriptor set that has been cached.
		dependency,               ///< Dependency between commands.

		num_enumerators ///< Number of enumerators.
	};

	/// Used to uniquely identify a resource.
	enum class unique_resource_id : std::uint64_t {
		invalid = 0 ///< An invalid ID.
	};
	/// Used to mark the order of commands globally (i.e., between batches).
	enum class global_submission_index : std::uint32_t {
		zero = 0, ///< Zero.
		/// Maximum numeric value.
		max = std::numeric_limits<std::underlying_type_t<global_submission_index>>::max(),
	};
	/// Used to mark the order of comands on a single queue within a batch.
	enum class queue_submission_index : std::uint32_t {
		zero = 0, ///< Zero.
		invalid = std::numeric_limits<std::underlying_type_t<queue_submission_index>>::max() ///< Invalid index.
	};


	namespace _details {
		/// Returns the next queue index.
		[[nodiscard]] inline queue_submission_index next(queue_submission_index idx) {
			return static_cast<queue_submission_index>(std::to_underlying(idx) + 1);
		}
		/// Returns the next global index.
		[[nodiscard]] inline global_submission_index next(global_submission_index idx) {
			return static_cast<global_submission_index>(std::to_underlying(idx) + 1);
		}

		/// Records how a command accesses an image.
		struct image_access {
			/// No initialization.
			image_access(uninitialized_t) : subresource_range(uninitialized) {
			}
			/// Initializes all fields of this struct.
			constexpr image_access(
				gpu::subresource_range sub_range,
				gpu::synchronization_point_mask sp,
				gpu::image_access_mask m,
				gpu::image_layout l
			) : subresource_range(sub_range), sync_points(sp), access(m), layout(l) {
			}
			/// Returns an object that corresponds to the initial state of a resource.
			[[nodiscard]] constexpr inline static image_access initial() {
				return image_access(
					gpu::subresource_range::empty(),
					gpu::synchronization_point_mask::none,
					gpu::image_access_mask::none,
					gpu::image_layout::undefined
				);
			}

			/// Default equality comparison.
			[[nodiscard]] friend bool operator==(const image_access&, const image_access&) = default;

			gpu::subresource_range subresource_range; ///< The range of subresources that are accessed.
			gpu::synchronization_point_mask sync_points; ///< Where this resource is accessed.
			gpu::image_access_mask access; ///< How this resource is accessed.
			gpu::image_layout layout; ///< Layout of this image.
		};
		/// Indicates how a buffer is accessed.
		struct buffer_access {
			/// No initialization.
			buffer_access(uninitialized_t) {
			}
			/// Initializes all fields of this struct.
			constexpr buffer_access(
				gpu::synchronization_point_mask sp, gpu::buffer_access_mask m
			) : sync_points(sp), access(m) {
			}
			/// Returns an object that corresponds to the initial state of a resource.
			[[nodiscard]] constexpr inline static buffer_access initial() {
				return buffer_access(gpu::synchronization_point_mask::none, gpu::buffer_access_mask::none);
			}

			/// Default equality comparison.
			[[nodiscard]] friend bool operator==(const buffer_access&, const buffer_access&) = default;

			gpu::synchronization_point_mask sync_points; ///< Where this resource is accessed.
			gpu::buffer_access_mask access; ///< How this resource is accessed.
		};

		/// Records an access event, including how a resource is accessed and when it happened.
		template <typename Access> struct basic_access_event {
			/// No initialization.
			basic_access_event(uninitialized_t) : access(uninitialized) {
			}
			/// Initializes all fields of this struct.
			constexpr basic_access_event(Access acc, global_submission_index gi, queue_submission_index qi) :
				access(acc), global_index(gi), queue_index(qi) {
			}

			Access access; ///< How the resource is accessed.
			global_submission_index global_index; ///< Global submission index associated with the event.
			queue_submission_index queue_index; ///< Queue submission index associated with the event.
		};
		using image_access_event = basic_access_event<image_access>; ///< Shorthand for image access events.
		using buffer_access_event = basic_access_event<buffer_access>; ///< Shorthand for buffer access events.
	}


	/// Aggregates graphics pipeline states that are not resource binding related.
	struct graphics_pipeline_state {
		/// Storage for blend options.
		using blend_options_storage = short_vector<gpu::render_target_blend_options, 8>;

		/// No initialization.
		graphics_pipeline_state(std::nullptr_t) : rasterizer_options(nullptr), depth_stencil_options(nullptr) {
		}
		/// Initializes all fields of this struct.
		graphics_pipeline_state(
			blend_options_storage b,
			gpu::rasterizer_options r,
			gpu::depth_stencil_options ds
		) : blend_options(std::move(b)), rasterizer_options(r), depth_stencil_options(ds) {
		}

		/// Default equality and inequality.
		[[nodiscard]] friend bool operator==(
			const graphics_pipeline_state&, const graphics_pipeline_state&
		) = default;

		// TODO allocator
		blend_options_storage blend_options; ///< Blending options.
		gpu::rasterizer_options rasterizer_options; ///< Rasterizer options.
		gpu::depth_stencil_options depth_stencil_options; ///< Depth stencil options.
	};
}
namespace std {
	/// Hash function for \ref lotus::renderer::pipeline_state.
	template <> struct hash<lotus::renderer::graphics_pipeline_state> {
		/// Hashes the given state object.
		[[nodiscard]] size_t operator()(const lotus::renderer::graphics_pipeline_state &state) const {
			size_t hash = lotus::hash_combine({
				lotus::compute_hash(state.rasterizer_options),
				lotus::compute_hash(state.depth_stencil_options),
			});
			for (const auto &opt : state.blend_options) {
				hash = lotus::hash_combine(hash, lotus::compute_hash(opt));
			}
			return hash;
		}
	};
}

namespace lotus::renderer {
	/// A sampler.
	struct sampler_state {
		/// Initializes the sampler value to a default point sampler.
		constexpr sampler_state(std::nullptr_t) {
		}
		/// Initializes all fields of this struct.
		constexpr sampler_state(
			gpu::filtering min = gpu::filtering::linear,
			gpu::filtering mag = gpu::filtering::linear,
			gpu::filtering mip = gpu::filtering::linear,
			float lod_bias = 0.0f, float minlod = 0.0f, float maxlod = std::numeric_limits<float>::max(),
			std::optional<float> max_aniso = std::nullopt,
			gpu::sampler_address_mode addr_u = gpu::sampler_address_mode::repeat,
			gpu::sampler_address_mode addr_v = gpu::sampler_address_mode::repeat,
			gpu::sampler_address_mode addr_w = gpu::sampler_address_mode::repeat,
			linear_rgba_f border = zero,
			gpu::comparison_function comp = gpu::comparison_function::none
		) :
			border_color(border),
			mip_lod_bias(lod_bias), min_lod(minlod), max_lod(maxlod),
			max_anisotropy(max_aniso),
			minification(min), magnification(mag), mipmapping(mip),
			addressing_u(addr_u), addressing_v(addr_v), addressing_w(addr_w),
			comparison(comp) {
		}

		/// Default comparisons.
		[[nodiscard]] friend constexpr bool operator==(const sampler_state&, const sampler_state&) = default;

		linear_rgba_f border_color = zero; ///< Border color used when sampling outside of the image.
		float mip_lod_bias = 0.0f; ///< LOD bias.
		float min_lod      = 0.0f; ///< Minimum LOD of this sampler.
		float max_lod      = 0.0f; ///< Maximum LOD of this sampler.
		[[no_unique_address]] std::optional<float> max_anisotropy; ///< Maximum anisotropy.
		gpu::filtering minification  = gpu::filtering::nearest; ///< Minification filtering.
		gpu::filtering magnification = gpu::filtering::nearest; ///< Magnification filtering.
		gpu::filtering mipmapping    = gpu::filtering::nearest; ///< Mipmapping filtering.
		gpu::sampler_address_mode addressing_u = gpu::sampler_address_mode::repeat; ///< U addressing mode.
		gpu::sampler_address_mode addressing_v = gpu::sampler_address_mode::repeat; ///< V addressing mode.
		gpu::sampler_address_mode addressing_w = gpu::sampler_address_mode::repeat; ///< W addressing mode.
		gpu::comparison_function comparison = gpu::comparison_function::none; ///< Depth comparison function.
	};
}
namespace std {
	/// Hash function for \ref lotus::renderer::sampler_state.
	template <> struct hash<lotus::renderer::sampler_state> {
		/// Computes the hash function.
		[[nodiscard]] constexpr size_t operator()(const lotus::renderer::sampler_state &state) const {
			return lotus::hash_combine({
				lotus::compute_hash(state.minification),
				lotus::compute_hash(state.magnification),
				lotus::compute_hash(state.mipmapping),
				lotus::compute_hash(state.mip_lod_bias),
				lotus::compute_hash(state.min_lod),
				lotus::compute_hash(state.max_lod),
				lotus::compute_hash(state.max_anisotropy),
				lotus::compute_hash(state.addressing_u),
				lotus::compute_hash(state.addressing_v),
				lotus::compute_hash(state.addressing_w),
				lotus::compute_hash(state.border_color),
				lotus::compute_hash(state.comparison),
			});
		}
	};
}
