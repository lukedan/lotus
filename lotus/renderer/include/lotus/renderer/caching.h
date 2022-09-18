#pragma once

/// \file
/// Pipeline cache.

#include <unordered_map>

#include <lotus/gpu/pipeline.h>
#include <lotus/gpu/descriptors.h>
#include "common.h"
#include "resources.h"
#include "assets.h"

namespace lotus::renderer {
	/// Types that are used as keys for caching objects.
	namespace cache_keys {
		/// Key of a descriptor set layout.
		struct descriptor_set_layout {
			/// Initializes this key to empty.
			descriptor_set_layout(std::nullptr_t) {
			}
			/// Initializes the array of descriptor ranges without sorting or merging. Use \ref consolidate() when
			/// necessary to ensure that the assumption with \ref ranges is kept.
			explicit descriptor_set_layout(
				std::vector<gpu::descriptor_range_binding> rs,
				descriptor_set_type ty = descriptor_set_type::normal
			) : ranges(std::move(rs)), type(ty) {
			}
			/// Initializes this key for the given descriptor array.
			template <
				typename RecordedResource, typename View
			> [[nodiscard]] static descriptor_set_layout from_descriptor_array(
				const _details::descriptor_array<RecordedResource, View> &d
			) {
				return cache_keys::descriptor_set_layout(
					{ gpu::descriptor_range_binding::create_unbounded(d.type, 0), },
					descriptor_set_type::variable_descriptor_count
				);
			}

			/// Sorts and merges the ranges.
			void consolidate();

			/// Default equality and inequality.
			[[nodiscard]] friend bool operator==(
				const descriptor_set_layout&, const descriptor_set_layout&
			) = default;

			/// Descriptor ranges bound in this layout, that has been sorted and merged.
			std::vector<gpu::descriptor_range_binding> ranges;
			descriptor_set_type type = descriptor_set_type::normal; ///< The type of this descriptor set.
		};
		/// Key of pipeline resources.
		struct pipeline_resources {
			/// The key of a single set.
			struct set {
				/// Initializes all fields of this struct.
				set(descriptor_set_layout l, std::uint32_t s) : layout(std::move(l)), space(s) {
				}

				/// Default equality and inequality.
				[[nodiscard]] friend bool operator==(const set&, const set&) = default;

				descriptor_set_layout layout; ///< Layout of the set.
				std::uint32_t space = 0; ///< Space of the set.
			};

			/// Sorts all sets.
			void sort() {
				std::sort(sets.begin(), sets.end(), [](const set &lhs, const set &rhs) {
					return lhs.space < rhs.space;
				});
			}

			/// Default equality and inequality.
			[[nodiscard]] friend bool operator==(
				const pipeline_resources&, const pipeline_resources&
			) = default;

			std::vector<set> sets; ///< The vector of sets. These are sorted based on their register spaces.
		};
		/// Key containing all pipeline parameters.
		struct graphics_pipeline {
			/// Version of \ref gpu::input_buffer_layout that owns the array of
			/// \ref gpu::input_buffer_element.
			struct input_buffer_layout {
				/// Conversion from a \ref gpu::input_buffer_layout.
				explicit input_buffer_layout(const gpu::input_buffer_layout&);
				input_buffer_layout(
					std::span<const gpu::input_buffer_element> elems,
					std::uint32_t s,
					std::uint32_t id,
					gpu::input_buffer_rate rate
				) : elements(elems.begin(), elems.end()), stride(s), buffer_index(id), input_rate(rate) {
				}

				/// Default equality and inequality comparisons.
				[[nodiscard]] friend bool operator==(
					const input_buffer_layout&, const input_buffer_layout&
				) = default;

				std::vector<gpu::input_buffer_element> elements; ///< Input elements.
				std::uint32_t stride = 0; ///< Stride of a vertex.
				std::uint32_t buffer_index = 0; ///< Buffer index.
				gpu::input_buffer_rate input_rate = gpu::input_buffer_rate::per_vertex; ///< Input rate.
			};

			/// Initializes this key to empty.
			graphics_pipeline(std::nullptr_t) :
				vertex_shader(nullptr), pixel_shader(nullptr), pipeline_state(nullptr) {
			}

			/// Default equality and inequality.
			[[nodiscard]] friend bool operator==(
				const graphics_pipeline&, const graphics_pipeline&
			) = default;


			// input descriptors
			pipeline_resources pipeline_resources; ///< Pipeline resources.

			// input buffers
			std::vector<input_buffer_layout> input_buffers; ///< Input buffers.

			// output frame buffer
			std::vector<gpu::format> color_rt_formats; ///< Color render target formats.
			/// Depth-stencil render target format.
			gpu::format dpeth_stencil_rt_format = gpu::format::none;

			assets::handle<assets::shader> vertex_shader; ///< Vertex shader.
			assets::handle<assets::shader> pixel_shader;  ///< Vertex shader.

			graphics_pipeline_state pipeline_state; ///< Blending, rasterizer, and depth-stencil state.
			gpu::primitive_topology topology = gpu::primitive_topology::num_enumerators; ///< Topology.
		};
	}
}
namespace std {
	/// Hash function for \ref lotus::renderer::cache_keys::descriptor_set_layout.
	template <> struct std::hash<lotus::renderer::cache_keys::descriptor_set_layout> {
		/// Hashes the given object.
		[[nodiscard]] std::size_t operator()(const lotus::renderer::cache_keys::descriptor_set_layout&) const;
	};
	/// Hash function for \ref lotus::renderer::cache_keys::pipeline_resources.
	template <> struct std::hash<lotus::renderer::cache_keys::pipeline_resources> {
		/// Hashes the given object.
		[[nodiscard]] std::size_t operator()(const lotus::renderer::cache_keys::pipeline_resources&) const;
	};
	/// Hash function for \ref lotus::renderer::cache_keys::graphics_pipeline.
	template <> struct std::hash<lotus::renderer::cache_keys::graphics_pipeline> {
		/// Hashes the given object.
		[[nodiscard]] std::size_t operator()(const lotus::renderer::cache_keys::graphics_pipeline&) const;
	};
}

namespace lotus::renderer {
	/// A cache for objects used in a context.
	class context_cache {
	public:
		/// Initializes the pipeline cache.
		explicit context_cache(gpu::device &dev) :
			_device(dev), _empty_layout(dev.create_descriptor_set_layout({}, gpu::shader_stage::all)) {
		}

		/// Creates or retrieves a descriptor set layout matching the given key.
		[[nodiscard]] const gpu::descriptor_set_layout &get_descriptor_set_layout(
			const cache_keys::descriptor_set_layout&
		);
		/// Creates or retrieves a pipeline resources object matching the given key.
		[[nodiscard]] const gpu::pipeline_resources &get_pipeline_resources(
			const cache_keys::pipeline_resources&
		);
		/// Creates or retrieves a graphics pipeline state matching the given key.
		[[nodiscard]] const gpu::graphics_pipeline_state &get_graphics_pipeline_state(
			const cache_keys::graphics_pipeline&
		);
	private:
		gpu::device &_device; ///< The device used by this cache.
		gpu::descriptor_set_layout _empty_layout; ///< An empty descriptor set layout.

		/// Cached descriptor layouts.
		std::unordered_map<cache_keys::descriptor_set_layout, gpu::descriptor_set_layout> _layouts;
		/// Cached pipeline resources.
		std::unordered_map<cache_keys::pipeline_resources, gpu::pipeline_resources> _pipeline_resources;
		/// Cached graphics pipeline states.
		std::unordered_map<cache_keys::graphics_pipeline, gpu::graphics_pipeline_state> _graphics_pipelines;
	};
}
