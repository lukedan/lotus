#pragma once

/// \file
/// Material context.

#include "lotus/memory/stack_allocator.h"
#include "lotus/gpu/descriptors.h"
#include "lotus/gpu/device.h"
#include "lotus/gpu/frame_buffer.h"
#include "lotus/gpu/resources.h"

namespace lotus::renderer {
	// forward declaration of resource types
	namespace recorded_resources {
		struct image2d_view;
		struct structured_buffer_view;
	}
	namespace _details {
		struct image2d;
		struct buffer;
		struct structured_buffer_view;
		struct swap_chain;
		template <typename, typename = std::nullopt_t> struct descriptor_array;
		struct blas;
		struct tlas;
		struct cached_descriptor_set;

		/// Array of image descriptors.
		using image_descriptor_array = descriptor_array<recorded_resources::image2d_view, gpu::image2d_view>;
		/// Array of buffer descriptors.
		using buffer_descriptor_array = descriptor_array<recorded_resources::structured_buffer_view>;
	}


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
		buffer,                   ///< A buffer.
		swap_chain,               ///< A swap chain.
		image2d_descriptor_array, ///< An array of image2d descriptors.
		buffer_descriptor_array,  ///< An array of buffer descriptors.
		blas,                     ///< A bottom-level descriptor array.
		tlas,                     ///< A top-level descriptor array.
		cached_descriptor_set,    ///< A descriptor set that has been cached.

		num_enumerators ///< Number of enumerators.
	};


	namespace _details {
		/// Indicates how an image is accessed.
		struct image_access {
			/// No initialization.
			image_access(uninitialized_t) {
			}
			/// Initializes all fields of this struct.
			constexpr image_access(
				gpu::synchronization_point_mask sp, gpu::image_access_mask m, gpu::image_layout l
			) : sync_points(sp), access(m), layout(l) {
			}
			/// Returns an object that corresponds to the initial state of a resource.
			[[nodiscard]] constexpr inline static image_access initial() {
				return image_access(
					gpu::synchronization_point_mask::none, gpu::image_access_mask::none, gpu::image_layout::undefined
				);
			}

			/// Default comparisons.
			[[nodiscard]] friend std::strong_ordering operator<=>(
				const image_access&, const image_access&
			) = default;

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
			constexpr buffer_access(gpu::synchronization_point_mask sp, gpu::buffer_access_mask m) :
				sync_points(sp), access(m) {
			}
			/// Returns an object that corresponds to the initial state of a resource.
			[[nodiscard]] constexpr inline static buffer_access initial() {
				return buffer_access(gpu::synchronization_point_mask::none, gpu::buffer_access_mask::none);
			}

			/// Default comparisons.
			[[nodiscard]] friend std::strong_ordering operator<=>(
				const buffer_access&, const buffer_access&
			) = default;

			gpu::synchronization_point_mask sync_points; ///< Where this resource is accessed.
			gpu::buffer_access_mask access; ///< How this resource is accessed.
		};
	}


	/// Aggregates graphics pipeline states that are not resource binding related.
	struct graphics_pipeline_state {
		/// No initialization.
		graphics_pipeline_state(std::nullptr_t) : rasterizer_options(nullptr), depth_stencil_options(nullptr) {
		}
		/// Initializes all fields of this struct.
		graphics_pipeline_state(
			std::vector<gpu::render_target_blend_options> b,
			gpu::rasterizer_options r,
			gpu::depth_stencil_options ds
		) : blend_options(std::move(b)), rasterizer_options(r), depth_stencil_options(ds) {
		}

		/// Default equality and inequality.
		[[nodiscard]] friend bool operator==(
			const graphics_pipeline_state&, const graphics_pipeline_state&
		) = default;

		// TODO allocator
		std::vector<gpu::render_target_blend_options> blend_options; ///< Blending options.
		gpu::rasterizer_options rasterizer_options; ///< Rasterizer options.
		gpu::depth_stencil_options depth_stencil_options; ///< Depth stencil options.
	};
}
namespace std {
	/// Hash function for \ref lotus::renderer::gpu::pipeline_state.
	template <> struct hash<lotus::renderer::graphics_pipeline_state> {
		/// Hashes the given state object.
		[[nodiscard]] constexpr size_t operator()(const lotus::renderer::graphics_pipeline_state &state) const {
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
			std::optional<gpu::comparison_function> comp = std::nullopt
		) :
			minification(min), magnification(mag), mipmapping(mip),
			mip_lod_bias(lod_bias), min_lod(minlod), max_lod(maxlod), max_anisotropy(max_aniso),
			addressing_u(addr_u), addressing_v(addr_v), addressing_w(addr_w),
			border_color(border), comparison(comp) {
		}

		/// Default comparisons.
		[[nodiscard]] friend constexpr bool operator==(const sampler_state&, const sampler_state&) = default;

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
