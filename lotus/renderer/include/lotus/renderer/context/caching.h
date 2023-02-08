#pragma once

/// \file
/// Pipeline cache.

#include <unordered_map>

#include <lotus/gpu/pipeline.h>
#include <lotus/gpu/descriptors.h>
#include "lotus/renderer/common.h"
#include "resource_bindings.h"
#include "assets.h"

namespace lotus::renderer {
	/// Types that are used as keys for caching objects.
	namespace cache_keys {
		using sampler = sampler_state; ///< Key of a sampler.
		/// Key of a descriptor set layout.
		struct descriptor_set_layout {
			/// Initializes this key to empty.
			descriptor_set_layout(std::nullptr_t) {
			}
			/// Initializes the array of descriptor ranges without sorting or merging. Use \ref consolidate() when
			/// necessary to ensure that the assumption with \ref ranges is kept.
			explicit descriptor_set_layout(std::vector<gpu::descriptor_range_binding> rs) : ranges(std::move(rs)) {
			}
			/// Initializes this key for a descriptor array of unbounded size.
			[[nodiscard]] inline static descriptor_set_layout for_descriptor_array(gpu::descriptor_type type) {
				return cache_keys::descriptor_set_layout(
					{ gpu::descriptor_range_binding::create_unbounded(type, 0), }
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
			pipeline_resources pipeline_rsrc; ///< Pipeline resources.

			// input buffers
			std::vector<input_buffer_layout> input_buffers; ///< Input buffers.

			// output frame buffer
			short_vector<gpu::format, 8> color_rt_formats; ///< Color render target formats.
			/// Depth-stencil render target format.
			gpu::format dpeth_stencil_rt_format = gpu::format::none;

			assets::handle<assets::shader> vertex_shader; ///< Vertex shader.
			assets::handle<assets::shader> pixel_shader;  ///< Pixel shader.

			graphics_pipeline_state pipeline_state; ///< Blending, rasterizer, and depth-stencil state.
			gpu::primitive_topology topology = gpu::primitive_topology::num_enumerators; ///< Topology.
		};
		/// Key containing all raytracing pipeline states.
		struct raytracing_pipeline {
			/// Initializes this key to empty.
			raytracing_pipeline(std::nullptr_t) {
			}

			/// Default equality and inequality.
			[[nodiscard]] friend bool operator==(const raytracing_pipeline&, const raytracing_pipeline&) = default;


			pipeline_resources pipeline_rsrc;///< Pipeline resources.

			std::vector<shader_function> hit_group_shaders; ///< Hit gruop shaders.
			std::vector<gpu::hit_shader_group> hit_groups;  ///< Hit groups.
			std::vector<shader_function> general_shaders;   ///< General shaders.

			std::uint32_t max_recursion_depth = 0; ///< Maximum recursion depth.
			std::uint32_t max_payload_size    = 0; ///< Maximum payload size.
			std::uint32_t max_attribute_size  = 0; ///< Maximum attribute size.
		};

		/// Pipeline resources specified for a set of shaders instead of fully manually.
		struct shader_set_pipeline_resources {
			using id_storage = short_vector<assets::unique_id, 4>; ///< Sets of shader IDs.

			/// Initializes this object to empty.
			shader_set_pipeline_resources(std::nullptr_t) {
			}

			/// Default equality and inequality comparisons.
			[[nodiscard]] friend bool operator==(
				const shader_set_pipeline_resources&, const shader_set_pipeline_resources&
			) = default;

			pipeline_resources overrides; ///< Overrides for sets that use custom descriptor set layouts.
			id_storage shaders; ///< Shaders.
		};
	}
}
namespace std {
	/// Hash function for \ref lotus::renderer::cache_keys::descriptor_set_layout.
	template <> struct hash<lotus::renderer::cache_keys::descriptor_set_layout> {
		/// Hashes the given object.
		[[nodiscard]] size_t operator()(const lotus::renderer::cache_keys::descriptor_set_layout&) const;
	};
	/// Hash function for \ref lotus::renderer::cache_keys::pipeline_resources.
	template <> struct hash<lotus::renderer::cache_keys::pipeline_resources> {
		/// Hashes the given object.
		[[nodiscard]] size_t operator()(const lotus::renderer::cache_keys::pipeline_resources&) const;
	};
	/// Hash function for \ref lotus::renderer::cache_keys::graphics_pipeline.
	template <> struct hash<lotus::renderer::cache_keys::graphics_pipeline> {
		/// Hashes the given object.
		[[nodiscard]] size_t operator()(const lotus::renderer::cache_keys::graphics_pipeline&) const;
	};
	/// Hash function for \ref lotus::renderer::cache_keys::raytracing_pipeline.
	template <> struct hash<lotus::renderer::cache_keys::raytracing_pipeline> {
		/// Hashes the given object.
		[[nodiscard]] size_t operator()(const lotus::renderer::cache_keys::raytracing_pipeline&) const;
	};
	/// Hash function for \ref lotus::renderer::cache_keys::shader_set.
	template <> struct hash<lotus::renderer::cache_keys::shader_set_pipeline_resources> {
		/// Hashes the given object.
		[[nodiscard]] size_t operator()(const lotus::renderer::cache_keys::shader_set_pipeline_resources&) const;
	};
}

namespace lotus::renderer {
	/// A cache for objects used in a context.
	class context_cache {
	public:
		/// Pipeline resources associated with a shader set.
		struct shader_set_pipeline_resources {
			/// Initializes this object to empty.
			shader_set_pipeline_resources(std::nullptr_t) {
			}
			/// Initializes all fields of this struct.
			shader_set_pipeline_resources(
				const cache_keys::pipeline_resources &k, const gpu::pipeline_resources &v
			) : key(&k), value(&v) {
			}

			// TODO: so far we don't invalidate cache entries, but it will become problematic once we do
			const cache_keys::pipeline_resources *key = nullptr; ///< Cache key.
			/// Set layouts. Corresponds to entries in \ref key one-to-one.
			std::vector<const gpu::descriptor_set_layout*> layouts;
			const gpu::pipeline_resources *value = nullptr; ///< Pipeline resources object.
		};

		/// Initializes the pipeline cache.
		explicit context_cache(gpu::device &dev) :
			_device(dev), _empty_layout(dev.create_descriptor_set_layout({}, gpu::shader_stage::all)) {
		}

		/// Creates or retrieves a sampler matching the given key.
		[[nodiscard]] const gpu::sampler &get_sampler(const cache_keys::sampler&);
		/// Creates or retrieves a descriptor set layout matching the given key.
		[[nodiscard]] const gpu::descriptor_set_layout &get_descriptor_set_layout(
			const cache_keys::descriptor_set_layout&
		);
		/// Creates or retrieves a pipeline resources object matching the given key.
		[[nodiscard]] const gpu::pipeline_resources &get_pipeline_resources(
			const cache_keys::pipeline_resources &key
		) {
			return _get_pipeline_resources_impl(key)->second;
		}
		// TODO we don't always need reflection data
		/// Returns the pipeline resource object associated with the given set of shaders.
		[[nodiscard]] const shader_set_pipeline_resources &get_pipeline_resources(
			const cache_keys::shader_set_pipeline_resources&, std::span<const gpu::shader_reflection* const>
		);
		/// \overload
		[[nodiscard]] const shader_set_pipeline_resources &get_pipeline_resources(
			std::span<const assets::handle<assets::shader>>, cache_keys::pipeline_resources overrides
		);
		/// \overload
		[[nodiscard]] const shader_set_pipeline_resources &get_pipeline_resources(
			std::initializer_list<assets::handle<assets::shader>> shaders, cache_keys::pipeline_resources overrides
		) {
			return get_pipeline_resources({ shaders.begin(), shaders.end() }, std::move(overrides));
		}
		/// Creates or retrieves a graphics pipeline state matching the given key.
		[[nodiscard]] const gpu::graphics_pipeline_state &get_graphics_pipeline_state(
			const cache_keys::graphics_pipeline&
		);
		/// Creates or retrieves a graphics raytracing state matching the given key.
		[[nodiscard]] const gpu::raytracing_pipeline_state &get_raytracing_pipeline_state(
			const cache_keys::raytracing_pipeline&
		);
	private:
		gpu::device &_device; ///< The device used by this cache.
		gpu::descriptor_set_layout _empty_layout; ///< An empty descriptor set layout.

		/// Cached samplers.
		std::unordered_map<cache_keys::sampler, gpu::sampler> _samplers;
		/// Cached descriptor layouts.
		std::unordered_map<cache_keys::descriptor_set_layout, gpu::descriptor_set_layout> _layouts;
		/// Cached pipeline resources.
		std::unordered_map<cache_keys::pipeline_resources, gpu::pipeline_resources> _pipeline_resources;
		/// Cached pipeline resources, but based on shader sets.
		std::unordered_map<
			cache_keys::shader_set_pipeline_resources, shader_set_pipeline_resources
		> _shader_pipeline_resources;
		/// Cached graphics pipeline states.
		std::unordered_map<cache_keys::graphics_pipeline, gpu::graphics_pipeline_state> _graphics_pipelines;
		/// Cached raytracing pipeline states.
		std::unordered_map<cache_keys::raytracing_pipeline, gpu::raytracing_pipeline_state> _raytracing_pipelines;

		/// Creates or retrieves a pipeline resource object matching the given key.
		[[nodiscard]] std::unordered_map<
			cache_keys::pipeline_resources, gpu::pipeline_resources
		>::const_iterator _get_pipeline_resources_impl(
			const cache_keys::pipeline_resources&
		);
	};
}
