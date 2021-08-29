#pragma once

/// \file
/// Device-related classes.

#include "lotus/common.h"
#include LOTUS_GRAPHICS_BACKEND_INCLUDE
#include "commands.h"
#include "descriptors.h"
#include "pass.h"
#include "pipeline.h"
#include "resources.h"
#include "frame_buffer.h"
#include "synchronization.h"

namespace lotus::graphics {
	class context;
	class adapter;
	class device;

	/// Interface to the graphics device.
	class device : public backend::device {
		friend adapter;
	public:
		/// Does not create a device.
		device(std::nullptr_t) : backend::device(nullptr) {
		}
		/// No copy construction.
		device(const device&) = delete;
		/// Move construction.
		device(device &&src) : backend::device(std::move(src)) {
		}
		/// No copy assignment.
		device &operator=(const device&) = delete;
		/// Move assignment.
		device &operator=(device &&src) {
			backend::device::operator=(std::move(src));
			return *this;
		}

		/// Creates a \ref command_queue.
		[[nodiscard]] command_queue create_command_queue() {
			return backend::device::create_command_queue();
		}
		/// Creates a \ref command_allocator.
		[[nodiscard]] command_allocator create_command_allocator() {
			return backend::device::create_command_allocator();
		}
		/// Creates a new empty \ref command_list.
		[[nodiscard]] command_list create_command_list(command_allocator &allocator) {
			return backend::device::create_command_list(allocator);
		}

		/// Creates a new empty \ref descriptor_pool.
		[[nodiscard]] descriptor_pool create_descriptor_pool() {
			return backend::device::create_descriptor_pool();
		}

		/// Loads the given compiled shader. It's assumed that the input data would live as long as the shader
		/// object.
		[[nodiscard]] shader load_shader(std::span<const std::byte> data) {
			return backend::device::load_shader(data);
		}

		/// Creates a new \ref sampler object.
		[[nodiscard]] sampler create_sampler(
			filtering minification, filtering magnification, filtering mipmapping,
			float mip_lod_bias, float min_lod, float max_lod, std::optional<float> max_anisotropy,
			sampler_address_mode addressing_u, sampler_address_mode addressing_v, sampler_address_mode addressing_w,
			linear_rgba_f border_color, std::optional<comparison_function> comparison
		) {
			return backend::device::create_sampler(
				minification, magnification, mipmapping, mip_lod_bias, min_lod, max_lod, max_anisotropy,
				addressing_u, addressing_v, addressing_w, border_color, comparison
			);
		}

		/// Creates a new \ref descriptor_set_layout object.
		[[nodiscard]] descriptor_set_layout create_descriptor_set_layout(
			std::span<const descriptor_range> ranges, shader_stage_mask visible_stages
		) {
			return backend::device::create_descriptor_set_layout(ranges, visible_stages);
		}

		/// Creates a \ref pipeline_resources object describing the resources used by a pipeline.
		[[nodiscard]] pipeline_resources create_pipeline_resources(
			std::span<const descriptor_set_layout *const> sets
		) {
			return backend::device::create_pipeline_resources(sets);
		}
		/// Creates a \ref pipeline_state object.
		[[nodiscard]] pipeline_state create_pipeline_state(
			pipeline_resources &resources,
			const shader_set &shaders,
			const blend_options &blend,
			const rasterizer_options &rasterizer,
			const depth_stencil_options &depth_stencil,
			std::span<const input_buffer_layout> input_buffers,
			primitive_topology topology,
			const pass_resources &environment,
			std::size_t num_viewports = 1
		) {
			return backend::device::create_pipeline_state(
				resources,
				shaders.vertex_shader,
				shaders.pixel_shader,
				shaders.domain_shader,
				shaders.hull_shader,
				shaders.geometry_shader,
				blend,
				rasterizer,
				depth_stencil,
				input_buffers,
				topology,
				environment,
				num_viewports
			);
		}

		/// Creates a \ref pass object.
		[[nodiscard]] pass_resources create_pass_resources(
			std::span<const render_target_pass_options> render_targets,
			depth_stencil_pass_options depth_stencil
		) {
			return backend::device::create_pass_resources(render_targets, depth_stencil);
		}

		/// Creates a \ref device_heap.
		[[nodiscard]] device_heap create_device_heap(std::size_t size, heap_type type) {
			return backend::device::create_device_heap(size, type);
		}

		/// Creates a \ref buffer with a dedicated memory allocation.
		[[nodiscard]] buffer create_committed_buffer(
			std::size_t size, heap_type committed_heap_type, buffer_usage usage
		) {
			return backend::device::create_committed_buffer(size, committed_heap_type, usage);
		}
		/// Maps the given buffer and ensures that the specified range has up-to-date data. \ref map_buffer() and
		/// \ref unmap_buffer() calls can be nested.
		[[nodiscard]] void *map_buffer(buffer &buf, std::size_t begin, std::size_t length) {
			return backend::device::map_buffer(buf, begin, length);
		}
		/// Unmaps the given buffer and ensures that flushes the specified memory range. \ref map_buffer() and
		/// \ref unmap_buffer() calls can be nested.
		[[nodiscard]] void unmap_buffer(buffer &buf, std::size_t begin, std::size_t length) {
			return backend::device::unmap_buffer(buf, begin, length);
		}

		/// Creates a view for an \ref image2d.
		[[nodiscard]] image2d_view create_image2d_view_from(
			const image2d &img, format format, mip_levels mip
		) {
			return backend::device::create_image2d_view_from(img, format, mip);
		}

		/// Creates a \ref frame_buffer.
		[[nodiscard]] frame_buffer create_frame_buffer(
			std::span<const image2d_view*> color, const image2d_view *depth_stencil, const pass_resources &pass
		) {
			return backend::device::create_frame_buffer(color, depth_stencil, pass);
		}

		/// Creates a \ref fence.
		[[nodiscard]] fence create_fence(synchronization_state state) {
			return backend::device::create_fence(state);
		}


		/// Resets the given fence.
		void reset_fence(fence &f) {
			backend::device::reset_fence(f);
		}
		/// Waits for the given fence to be signaled.
		void wait_for_fence(fence &f) {
			backend::device::wait_for_fence(f);
		}


		/// Sets the debug name of the given object.
		void set_debug_name(image &img, const char8_t *name) {
			backend::device::set_debug_name(img, name);
		}
	protected:
		/// Creates a device from a backend device.
		device(backend::device d) : backend::device(std::move(d)) {
		}
	};

	/// Represents a generic interface to an adapter that a device can be created from.
	class adapter : public backend::adapter {
		friend context;
	public:
		/// Creates an empty adapter.
		adapter(std::nullptr_t) : backend::adapter(nullptr) {
		}
		/// No copy construction.
		adapter(const adapter&) = delete;
		/// Move construction.
		adapter(adapter &&src) noexcept : backend::adapter(std::move(src)) {
		}
		/// No copy assignment.
		adapter &operator=(const adapter&) = delete;

		/// Creates a device that uses this adapter.
		[[nodiscard]] device create_device() {
			return backend::adapter::create_device();
		}
		/// Retrieves information about this adapter.
		[[nodiscard]] adapter_properties get_properties() const {
			return backend::adapter::get_properties();
		}
	protected:
		/// Creates an adapter from a backend adapter.
		adapter(backend::adapter a) : backend::adapter(std::move(a)) {
		}
	};


	inline void command_allocator::reset(device &dev) {
		backend::command_allocator::reset(dev);
	}
}
