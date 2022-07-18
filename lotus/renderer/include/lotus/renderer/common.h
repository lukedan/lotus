#pragma once

/// \file
/// Material context.

#include "lotus/utils/stack_allocator.h"
#include "lotus/graphics/descriptors.h"
#include "lotus/graphics/device.h"
#include "lotus/graphics/frame_buffer.h"
#include "lotus/graphics/resources.h"

namespace lotus::renderer {
	/// Indicates whether debug names would be registered for resources.
	constexpr static bool should_register_debug_names = is_debugging;

	/// All descriptor bindings of a specific shader, categorized into sets.
	struct shader_descriptor_bindings {
		/// All descriptor bindgs in a particular set, sorted by register index.
		struct set {
			/// Descriptor ranges in this space sorted by register index.
			std::vector<graphics::descriptor_range_binding> ranges; // TODO allocator?
			graphics::descriptor_set_layout layout = nullptr; ///< Descriptor set layout of this set.
			std::uint32_t space; ///< The space that all descriptors are in.
		};

		/// Collects bindings from the given \ref graphics::shader_reflection object.
		[[nodiscard]] inline static shader_descriptor_bindings collect_from(
			const graphics::shader_reflection &refl
		) {
			// descriptor set index and range binding
			using _range = std::pair<std::uint32_t, graphics::descriptor_range_binding>;
			std::vector<_range> bindings;
			refl.enumerate_resource_bindings([&bindings](const graphics::shader_resource_binding &binding) {
				assert(binding.register_count > 0);
				bindings.emplace_back(
					static_cast<std::uint32_t>(binding.register_space),
					graphics::descriptor_range_binding::create(
						binding.type, binding.register_count, binding.first_register
					)
				);
			});
			// sort by set index then register index
			std::sort(bindings.begin(), bindings.end(), [](const _range &lhs, const _range &rhs) {
				if (lhs.first == rhs.first) {
					return lhs.second.register_index < rhs.second.register_index;
				}
				return lhs.first < rhs.first;
			});
			// collect result
			shader_descriptor_bindings result;
			for (const auto &range : bindings) {
				if (result.sets.empty() || result.sets.back().space != range.first) {
					result.sets.emplace_back().space = range.first;
				}
				auto &ranges = result.sets.back().ranges;
				bool merge =
					!ranges.empty() &&
					ranges.back().range.type == range.second.range.type &&
					ranges.back().get_last_register_index() + 1 == range.second.register_index &&
					range.second.range.count != graphics::descriptor_range::unbounded_count;
				if (merge) { // merge into the last range
					ranges.back().range.count += range.second.range.count;
				} else {
					ranges.emplace_back(range.second);
				}
			}
			return result;
		}

		/// Creates \ref set::layout for all the descriptor sets.
		void create_layouts(graphics::device &dev) {
			for (auto &s : sets) {
				s.layout = dev.create_descriptor_set_layout(s.ranges, graphics::shader_stage::all);
			}
		}

		// TODO allocator?
		std::vector<set> sets; ///< Descriptor sets.
	};

	/// Aggregates graphics pipeline states that are not resource binding related.
	struct graphics_pipeline_state {
		/// No initialization.
		graphics_pipeline_state(std::nullptr_t) : rasterizer_options(nullptr), depth_stencil_options(nullptr) {
		}
		/// Initializes all fields of this struct.
		graphics_pipeline_state(
			std::vector<graphics::render_target_blend_options> b,
			graphics::rasterizer_options r,
			graphics::depth_stencil_options ds
		) : blend_options(std::move(b)), rasterizer_options(r), depth_stencil_options(ds) {
		}

		// TODO allocator
		std::vector<graphics::render_target_blend_options> blend_options; ///< Blending options.
		graphics::rasterizer_options rasterizer_options; ///< Rasterizer options.
		graphics::depth_stencil_options depth_stencil_options; ///< Depth stencil options.
	};
}
namespace std {
	/// Hash function for \ref lotus::renderer::graphics::pipeline_state.
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
