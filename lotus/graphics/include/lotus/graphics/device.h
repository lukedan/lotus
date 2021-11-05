#pragma once

/// \file
/// Device-related classes.

#include <optional>

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

		/// Acquires the next back buffer and returns its index in this swap chain. This should only be called once
		/// per frame.
		[[nodiscard]] back_buffer_info acquire_back_buffer(swap_chain &swapchain) {
			return backend::device::acquire_back_buffer(swapchain);
		}

		/// Creates a \ref command_queue.
		[[nodiscard]] command_queue create_command_queue() {
			return backend::device::create_command_queue();
		}
		/// Creates a \ref command_allocator.
		[[nodiscard]] command_allocator create_command_allocator() {
			return backend::device::create_command_allocator();
		}
		/// Creates a new empty \ref command_list and immediately starts recording commands.
		[[nodiscard]] command_list create_and_start_command_list(command_allocator &allocator) {
			return backend::device::create_and_start_command_list(allocator);
		}

		/// Creates a new empty \ref descriptor_pool.
		[[nodiscard]] descriptor_pool create_descriptor_pool(
			std::span<const descriptor_range> capacity, std::size_t max_num_sets
		) {
			return backend::device::create_descriptor_pool(capacity, max_num_sets);
		}
		/// \overload
		[[nodiscard]] descriptor_pool create_descriptor_pool(
			std::initializer_list<descriptor_range> capacity, std::size_t max_num_sets
		) {
			return create_descriptor_pool({ capacity.begin(), capacity.end() }, max_num_sets);
		}
		/// Allocates a new \ref descriptor_set from the given \ref descriptor_pool.
		[[nodiscard]] descriptor_set create_descriptor_set(
			descriptor_pool &pool, const descriptor_set_layout &layout
		) {
			return backend::device::create_descriptor_set(pool, layout);
		}

		/// Updates the descriptors in the set with the given read-only images.
		void write_descriptor_set_images(
			descriptor_set &set, const descriptor_set_layout &layout,
			std::size_t first_register, std::span<const image_view *const> images
		) {
			backend::device::write_descriptor_set_images(set, layout, first_register, images);
		}
		/// \overload
		void write_descriptor_set_images(
			descriptor_set &set, const descriptor_set_layout &layout,
			std::size_t first_register, std::initializer_list<const image_view*> images
		) {
			write_descriptor_set_images(set, layout, first_register, { images.begin(), images.end() });
		}
		/// Updates the descriptors in the set with the given read-only buffers.
		void write_descriptor_set_buffers(
			descriptor_set &set, const descriptor_set_layout &layout,
			std::size_t first_register, std::span<const buffer_view> buffers
		) {
			backend::device::write_descriptor_set_buffers(set, layout, first_register, buffers);
		}
		/// \overload
		void write_descriptor_set_buffers(
			descriptor_set &set, const descriptor_set_layout &layout,
			std::size_t first_register, std::initializer_list<buffer_view> buffers
		) {
			write_descriptor_set_buffers(set, layout, first_register, { buffers.begin(), buffers.end() });
		}
		/// Updates the descriptors in the set with the given constant buffers.
		void write_descriptor_set_constant_buffers(
			descriptor_set &set, const descriptor_set_layout &layout,
			std::size_t first_register, std::span<const constant_buffer_view> buffers
		) {
			backend::device::write_descriptor_set_constant_buffers(set, layout, first_register, buffers);
		}
		/// \overload
		void write_descriptor_set_constant_buffers(
			descriptor_set &set, const descriptor_set_layout &layout,
			std::size_t first_register, std::initializer_list<constant_buffer_view> buffers
		) {
			write_descriptor_set_constant_buffers(set, layout, first_register, { buffers.begin(), buffers.end() });
		}
		/// Updates the descriptors in the set with the given samplers.
		void write_descriptor_set_samplers(
			descriptor_set &set, const descriptor_set_layout &layout,
			std::size_t first_register, std::span<const sampler *const> samplers
		) {
			backend::device::write_descriptor_set_samplers(set, layout, first_register, samplers);
		}
		/// \overload
		void write_descriptor_set_samplers(
			descriptor_set &set, const descriptor_set_layout &layout,
			std::size_t first_register, std::initializer_list<const sampler*> samplers
		) {
			write_descriptor_set_samplers(set, layout, first_register, { samplers.begin(), samplers.end() });
		}

		/// Loads the given compiled shader. It's assumed that the input data would live as long as the shader
		/// object.
		[[nodiscard]] shader load_shader(std::span<const std::byte> data) {
			return backend::device::load_shader(data);
		}
		/// Loads shader reflection from the given data.
		[[nodiscard]] shader_reflection load_shader_reflection(std::span<const std::byte> data) {
			return backend::device::load_shader_reflection(data);
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
			std::span<const descriptor_range_binding> ranges, shader_stage visible_stages
		) {
			return backend::device::create_descriptor_set_layout(ranges, visible_stages);
		}
		/// \overload
		[[nodiscard]] descriptor_set_layout create_descriptor_set_layout(
			std::initializer_list<descriptor_range_binding> ranges, shader_stage visible_stages
		) {
			return create_descriptor_set_layout({ ranges.begin(), ranges.end() }, visible_stages);
		}

		/// Creates a \ref pipeline_resources object describing the resources used by a pipeline.
		[[nodiscard]] pipeline_resources create_pipeline_resources(
			std::span<const descriptor_set_layout *const> sets
		) {
			return backend::device::create_pipeline_resources(sets);
		}
		/// \overload
		[[nodiscard]] pipeline_resources create_pipeline_resources(
			std::initializer_list<const descriptor_set_layout*> sets
		) {
			return create_pipeline_resources({ sets.begin(), sets.end() });
		}
		/// Creates a \ref graphics_pipeline_state object.
		[[nodiscard]] graphics_pipeline_state create_graphics_pipeline_state(
			const pipeline_resources &resources,
			const shader_set &shaders,
			std::span<const render_target_blend_options> blend,
			const rasterizer_options &rasterizer,
			const depth_stencil_options &depth_stencil,
			std::span<const input_buffer_layout> input_buffers,
			primitive_topology topology,
			const pass_resources &environment,
			std::size_t num_viewports = 1
		) {
			return backend::device::create_graphics_pipeline_state(
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
		/// \overload
		[[nodiscard]] graphics_pipeline_state create_graphics_pipeline_state(
			const pipeline_resources &resources,
			const shader_set &shaders,
			std::initializer_list<render_target_blend_options> blend,
			const rasterizer_options &rasterizer,
			const depth_stencil_options &depth_stencil,
			std::initializer_list<input_buffer_layout> input_buffers,
			primitive_topology topology,
			const pass_resources &environment,
			std::size_t num_viewports = 1
		) {
			return create_graphics_pipeline_state(
				resources, shaders,
				{ blend.begin(), blend.end() },
				rasterizer, depth_stencil,
				{ input_buffers.begin(), input_buffers.end() },
				topology, environment, num_viewports
			);
		}
		/// Creates a \ref compute_pipeline_state object.
		[[nodiscard]] compute_pipeline_state create_compute_pipeline_state(
			const pipeline_resources &resources, const shader &compute_shader
		) {
			return backend::device::create_compute_pipeline_state(resources, compute_shader);
		}

		/// Creates a \ref pass object.
		[[nodiscard]] pass_resources create_pass_resources(
			std::span<const render_target_pass_options> render_targets,
			depth_stencil_pass_options depth_stencil
		) {
			return backend::device::create_pass_resources(render_targets, depth_stencil);
		}
		/// \overload
		[[nodiscard]] pass_resources create_pass_resources(
			std::initializer_list<render_target_pass_options> render_targets,
			depth_stencil_pass_options depth_stencil
		) {
			return create_pass_resources({ render_targets.begin(), render_targets.end() }, depth_stencil);
		}

		/// Creates a \ref device_heap.
		[[nodiscard]] device_heap create_device_heap(std::size_t size, heap_type type) {
			return backend::device::create_device_heap(size, type);
		}

		/// Creates a \ref buffer with a dedicated memory allocation.
		[[nodiscard]] buffer create_committed_buffer(
			std::size_t size, heap_type committed_heap_type, buffer_usage::mask allowed_usage
		) {
			return backend::device::create_committed_buffer(
				size, committed_heap_type, allowed_usage
			);
		}
		/// Creates a \ref image2d with a dedicated memory allocation. This image can only be created on the GPU.
		[[nodiscard]] image2d create_committed_image2d(
			std::size_t width, std::size_t height, std::size_t array_slices, std::size_t mip_levels,
			format fmt, image_tiling tiling, image_usage::mask allowed_usage
		) {
			return backend::device::create_committed_image2d(
				width, height, array_slices, mip_levels,
				fmt, tiling, allowed_usage
			);
		}
		/// Creates a buffer that can be used to upload/download image data to/from the GPU. The image data is
		/// assumed to be row-major and have the returned layout.
		/// 
		/// \return The buffer and its layout properties.
		[[nodiscard]] staging_buffer create_committed_staging_buffer(
			std::size_t width, std::size_t height, format fmt, heap_type committed_heap_type,
			buffer_usage::mask allowed_usage
		) {
			staging_buffer result = nullptr;
			auto [buf, pitch, size] = backend::device::create_committed_staging_buffer(
				width, height, fmt, committed_heap_type, allowed_usage
			);
			result.data = std::move(buf);
			result.row_pitch = pitch;
			result.total_size = size;
			return result;
		}

		/// Maps the given buffer and ensures that the specified range has up-to-date data from the device.
		/// \ref map_buffer() and \ref unmap_buffer() calls can be nested. Note that the returned address is to the
		/// start of the buffer, instead of the requested memory range.
		[[nodiscard]] void *map_buffer(buffer &buf, std::size_t begin, std::size_t length) {
			return backend::device::map_buffer(buf, begin, length);
		}
		/// Unmaps the given buffer and ensures that the specified memory range is flushed to the device.
		/// \ref map_buffer() and \ref unmap_buffer() calls can be nested.
		void unmap_buffer(buffer &buf, std::size_t begin, std::size_t length) {
			return backend::device::unmap_buffer(buf, begin, length);
		}
		/// Maps the given image and ensures that the specified range has up-to-date data. \ref map_image2d() and
		/// \ref unmap_image2d() calls can be nested. Note that the returned address is to the start of the
		/// subresource, instead of the requested memory range.
		[[nodiscard]] void *map_image2d(
			image2d &img, subresource_index subresource, std::size_t begin, std::size_t length
		) {
			return backend::device::map_image2d(img, subresource, begin, length);
		}
		/// Unmaps the given image and ensures that flushes the specified memory range. \ref map_image2d() and
		/// \ref unmap_image2d() calls can be nested.
		void unmap_image2d(
			image2d &img, subresource_index subresource, std::size_t begin, std::size_t length
		) {
			return backend::device::unmap_image2d(img, subresource, begin, length);
		}

		/// Creates a view for an \ref image2d.
		[[nodiscard]] image2d_view create_image2d_view_from(
			const image2d &img, format format, mip_levels mip
		) {
			return backend::device::create_image2d_view_from(img, format, mip);
		}

		/// Creates a \ref frame_buffer.
		[[nodiscard]] frame_buffer create_frame_buffer(
			std::span<const image2d_view *const> color, const image2d_view *depth_stencil,
			cvec2s size, const pass_resources &pass
		) {
			return backend::device::create_frame_buffer(color, depth_stencil, size, pass);
		}
		/// \overload
		[[nodiscard]] frame_buffer create_frame_buffer(
			std::initializer_list<const image2d_view*> color, const image2d_view *depth_stencil,
			const cvec2s &size, const pass_resources &pass
		) {
			return create_frame_buffer({ color.begin(), color.end() }, depth_stencil, size, pass);
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
		void set_debug_name(buffer &buf, const char8_t *name) {
			backend::device::set_debug_name(buf, name);
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
