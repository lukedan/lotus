#pragma once

/// \file
/// Utilities for descriptor management.

#include "lotus/gpu/common.h"
#include "lotus/renderer/common.h"
#include "lotus/renderer/context/misc.h"
#include "lotus/renderer/context/resources.h"
#include "caching.h"

namespace lotus::renderer::execution {
	/// Indicates a descriptor set bind point.
	enum class descriptor_set_bind_point {
		graphics,   ///< The descriptor sets are used for graphics operations.
		compute,    ///< The descriptor sets are used for compute operations.
		raytracing, ///< The descriptor sets are used for ray tracing operations.
	};

	/// Utility class for building descriptor sets.
	struct descriptor_set_builder {
	public:
		/// Initializes this struct by creating an empty descriptor set.
		descriptor_set_builder(renderer::context&, const gpu::descriptor_set_layout&, gpu::descriptor_pool&);

		/// Creates a descriptor binding for an 2D image.
		void create_binding(u32 reg, const descriptor_resource::image2d&);
		/// Creates a descriptor binding from a 3D image.
		void create_binding(u32 reg, const descriptor_resource::image3d&);
		/// Creates a descriptor binding for a swap chain image.
		void create_binding(u32 reg, const descriptor_resource::swap_chain&);
		/// Creates a descriptor binding for a constant buffer.
		void create_binding(u32 reg, const descriptor_resource::constant_buffer&);
		/// Creates a descriptor binding for a structured buffer.
		void create_binding(u32 reg, const descriptor_resource::structured_buffer&);
		/// Creates a descriptor binding for an acceleration structure.
		void create_binding(u32 reg, const recorded_resources::tlas&);
		/// Creates a descriptor binding for a sampler.
		void create_binding(u32 reg, const sampler_state&);

		/// Creates an array of descriptor bindings.
		void create_bindings(std::span<const all_resource_bindings::numbered_binding>);

		/// Finishes building the descriptor set and returns the result. This object is left in an unspecified state.
		[[nodiscard]] gpu::descriptor_set take() && {
			return std::move(_result);
		}

		/// Returns the descriptor type of an image binding.
		template <gpu::image_type Type> [[nodiscard]] static gpu::descriptor_type get_descriptor_type(
			const descriptor_resource::basic_image<Type> &img
		) {
			return to_descriptor_type(img.binding_type);
		}
		/// Returns the descriptor type of a swap chain.
		[[nodiscard]] static gpu::descriptor_type get_descriptor_type(const descriptor_resource::swap_chain &chain) {
			return to_descriptor_type(chain.binding_type);
		}
		/// Returns \ref gpu::descriptor_type::constant_buffer.
		[[nodiscard]] static gpu::descriptor_type get_descriptor_type(const descriptor_resource::constant_buffer&) {
			return gpu::descriptor_type::constant_buffer;
		}
		/// Returns the descriptor type of a buffer binding.
		[[nodiscard]] static gpu::descriptor_type get_descriptor_type(
			const descriptor_resource::structured_buffer &buf
		) {
			return to_descriptor_type(buf.binding_type);
		}
		/// Returns \ref gpu::descriptor_type::acceleration_structure.
		[[nodiscard]] static gpu::descriptor_type get_descriptor_type(const recorded_resources::tlas&) {
			return gpu::descriptor_type::acceleration_structure;
		}
		/// Returns \ref gpu::descriptor_type::sampler.
		[[nodiscard]] static gpu::descriptor_type get_descriptor_type(const sampler_state&) {
			return gpu::descriptor_type::sampler;
		}


		/// Collects all descriptor ranges and returns a key for a descriptor set layout.
		[[nodiscard]] static execution::cache_keys::descriptor_set_layout get_descriptor_set_layout_key(
			std::span<const all_resource_bindings::numbered_binding>
		);
	private:
		renderer::context &_ctx; ///< Associated rendering context.
		const gpu::descriptor_set_layout &_layout; ///< Layout of the target descriptor set.

		gpu::descriptor_set _result; ///< The resulting descriptor set.
	};
}
