#pragma once

/// \file
/// Material context.

#include "lotus/utils/stack_allocator.h"
#include "lotus/gpu/descriptors.h"
#include "lotus/gpu/device.h"
#include "lotus/gpu/frame_buffer.h"
#include "lotus/gpu/resources.h"

namespace lotus::renderer {
	/// Indicates whether debug names would be registered for resources.
	constexpr static bool should_register_debug_names = true;

	/// The type of a descriptor set.
	enum class descriptor_set_type {
		normal, ///< A normal descriptor set.
		/// The last range in this descriptor set can have a variable amount of descriptors, determined at time of
		/// descriptor set creation.
		variable_descriptor_count,
	};

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
		constexpr size_t operator()(const lotus::renderer::graphics_pipeline_state &state) const {
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
