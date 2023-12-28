#pragma once

/// \file
/// Device-related classes.

#include <optional>

#include "lotus/common.h"
#include LOTUS_GPU_BACKEND_INCLUDE
#include "acceleration_structure.h"
#include "commands.h"
#include "descriptors.h"
#include "pipeline.h"
#include "resources.h"
#include "frame_buffer.h"
#include "synchronization.h"

namespace lotus::gpu {
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
		/// Resizes all buffers in the swap chain.
		void resize_swap_chain_buffers(swap_chain &swapchain, cvec2u32 size) {
			backend::device::resize_swap_chain_buffers(swapchain, size);
		}

		/// Creates a \ref command_allocator for the given queue type.
		[[nodiscard]] command_allocator create_command_allocator(queue_type ty) {
			return backend::device::create_command_allocator(ty);
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
		/// Allocates a new \ref descriptor_set from the given \ref descriptor_pool, where one descriptor range in
		/// the set has dynamic (unbounded) size that is specified using the additional parameter.
		[[nodiscard]] descriptor_set create_descriptor_set(
			descriptor_pool &pool, const descriptor_set_layout &layout, std::size_t dynamic_size
		) {
			return backend::device::create_descriptor_set(pool, layout, dynamic_size);
		}

		/// Updates the descriptors in the set with the given read-only images.
		void write_descriptor_set_read_only_images(
			descriptor_set &set, const descriptor_set_layout &layout,
			std::size_t first_register, std::span<const image_view_base *const> images
		) {
			backend::device::write_descriptor_set_read_only_images(set, layout, first_register, images);
		}
		/// \overload
		void write_descriptor_set_read_only_images(
			descriptor_set &set, const descriptor_set_layout &layout,
			std::size_t first_register, std::initializer_list<const image_view_base*> images
		) {
			write_descriptor_set_read_only_images(set, layout, first_register, { images.begin(), images.end() });
		}
		/// Updates the descriptors in the set with the given read-write images.
		void write_descriptor_set_read_write_images(
			descriptor_set &set, const descriptor_set_layout &layout,
			std::size_t first_register, std::span<const image_view_base *const> images
		) {
			backend::device::write_descriptor_set_read_write_images(set, layout, first_register, images);
		}
		/// \overload
		void write_descriptor_set_read_write_images(
			descriptor_set &set, const descriptor_set_layout &layout,
			std::size_t first_register, std::initializer_list<const image_view_base*> images
		) {
			write_descriptor_set_read_write_images(set, layout, first_register, { images.begin(), images.end() });
		}
		/// Retrieves a member function pointer to the function that writes image descriptors of the specified type.
		inline static void (device::*get_write_image_descriptor_function(descriptor_type type))(
			descriptor_set&, const descriptor_set_layout&, std::size_t, std::span<const image_view_base *const>
		) {
			switch (type) {
			case descriptor_type::read_only_image:
				return &write_descriptor_set_read_only_images;
			case descriptor_type::read_write_image:
				return &write_descriptor_set_read_write_images;
			default:
				return nullptr;
			}
		}
		/// Retrieves a member function pointer to the function that writes structured buffer descriptors of the
		/// specified type.
		inline static void (device::*get_write_structured_buffer_descriptor_function(descriptor_type type))(
			descriptor_set&, const descriptor_set_layout&, std::size_t, std::span<const structured_buffer_view>
		) {
			switch (type) {
			case descriptor_type::read_only_buffer:
				return &write_descriptor_set_read_only_structured_buffers;
			case descriptor_type::read_write_buffer:
				return &write_descriptor_set_read_write_structured_buffers;
			default:
				return nullptr;
			}
		}

		/// Updates the descriptors in the set with the given read-only structured buffers.
		void write_descriptor_set_read_only_structured_buffers(
			descriptor_set &set, const descriptor_set_layout &layout,
			std::size_t first_register, std::span<const structured_buffer_view> buffers
		) {
			backend::device::write_descriptor_set_read_only_structured_buffers(set, layout, first_register, buffers);
		}
		/// \overload
		void write_descriptor_set_read_only_structured_buffers(
			descriptor_set &set, const descriptor_set_layout &layout,
			std::size_t first_register, std::initializer_list<structured_buffer_view> buffers
		) {
			write_descriptor_set_read_only_structured_buffers(
				set, layout, first_register, { buffers.begin(), buffers.end() }
			);
		}
		/// Updates the descriptors in the set with the given read-write structured buffers.
		void write_descriptor_set_read_write_structured_buffers(
			descriptor_set &set, const descriptor_set_layout &layout,
			std::size_t first_register, std::span<const structured_buffer_view> buffers
		) {
			backend::device::write_descriptor_set_read_write_structured_buffers(
				set, layout, first_register, buffers
			);
		}
		/// \overload
		void write_descriptor_set_read_write_structured_buffers(
			descriptor_set &set, const descriptor_set_layout &layout,
			std::size_t first_register, std::initializer_list<structured_buffer_view> buffers
		) {
			write_descriptor_set_read_write_structured_buffers(
				set, layout, first_register, { buffers.begin(), buffers.end() }
			);
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
		[[nodiscard]] shader_binary load_shader(std::span<const std::byte> data) {
			return backend::device::load_shader(data);
		}

		/// Creates a new \ref sampler object.
		[[nodiscard]] sampler create_sampler(
			filtering minification, filtering magnification, filtering mipmapping,
			float mip_lod_bias, float min_lod, float max_lod, std::optional<float> max_anisotropy,
			sampler_address_mode addressing_u, sampler_address_mode addressing_v, sampler_address_mode addressing_w,
			linear_rgba_f border_color, comparison_function comparison
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
			const frame_buffer_layout &fb_layout,
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
				fb_layout,
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
			const frame_buffer_layout &fb_layout,
			std::size_t num_viewports = 1
		) {
			return create_graphics_pipeline_state(
				resources, shaders,
				{ blend.begin(), blend.end() },
				rasterizer, depth_stencil,
				{ input_buffers.begin(), input_buffers.end() },
				topology, fb_layout, num_viewports
			);
		}
		/// Creates a \ref compute_pipeline_state object.
		[[nodiscard]] compute_pipeline_state create_compute_pipeline_state(
			const pipeline_resources &resources, const shader_binary &compute_shader
		) {
			return backend::device::create_compute_pipeline_state(resources, compute_shader);
		}

		/// Enumerates available memory types. The returned span will not be moved as long as the device is still
		/// valid.
		///
		/// \return The list of memory types, ordered by their performance.
		[[nodiscard]] std::span<
			const std::pair<memory_type_index, memory_properties>
		> enumerate_memory_types() const {
			return backend::device::enumerate_memory_types();
		}
		/// Creates a \ref device_heap.
		[[nodiscard]] memory_block allocate_memory(std::size_t size, memory_type_index mem_type) {
			return backend::device::allocate_memory(size, mem_type);
		}

		/// Creates a \ref buffer with a dedicated memory allocation.
		[[nodiscard]] buffer create_committed_buffer(
			std::size_t size, memory_type_index mem_type, buffer_usage_mask allowed_usages
		) {
			return backend::device::create_committed_buffer(size, mem_type, allowed_usages);
		}
		/// Creates a \ref image2d with a dedicated memory allocation. This image can only be created on the GPU.
		[[nodiscard]] image2d create_committed_image2d(
			cvec2u32 size, std::uint32_t mip_levels,
			format fmt, image_tiling tiling, image_usage_mask allowed_usages
		) {
			return backend::device::create_committed_image2d(
				size, mip_levels, fmt, tiling, allowed_usages
			);
		}
		/// Creates a \ref image3d with a dedicated memory allocation. This image can only be created on the GPU.
		[[nodiscard]] image3d create_committed_image3d(
			cvec3u32 size, std::uint32_t mip_levels,
			format fmt, image_tiling tiling, image_usage_mask allowed_usages
		) {
			return backend::device::create_committed_image3d(
				size, mip_levels, fmt, tiling, allowed_usages
			);
		}
		/// Creates a buffer that can be used to upload/download image data to/from the GPU. The image data is
		/// assumed to be row-major and have the returned layout.
		/// 
		/// \return The buffer and its layout properties.
		[[nodiscard]] staging_buffer create_committed_staging_buffer(
			cvec2u32 size, format fmt, memory_type_index mem_type,
			buffer_usage_mask allowed_usages
		) {
			staging_buffer result = nullptr;
			auto [buf, meta, size_bytes] = backend::device::create_committed_staging_buffer(
				size, fmt, mem_type, allowed_usages
			);
			result.data = std::move(buf);
			result.meta = meta;
			result.total_size = size_bytes;
			return result;
		}

		/// Queries the memory requirements of the given 2D image.
		[[nodiscard]] memory::size_alignment get_image2d_memory_requirements(
			cvec2u32 size, std::uint32_t mip_levels,
			format fmt, image_tiling tiling, image_usage_mask usages
		) {
			return backend::device::get_image2d_memory_requirements(size, mip_levels, fmt, tiling, usages);
		}
		/// Queries the memory requirements of the given 3D image.
		[[nodiscard]] memory::size_alignment get_image3d_memory_requirements(
			cvec3u32 size, std::uint32_t mip_levels,
			format fmt, image_tiling tiling, image_usage_mask usages
		) {
			return backend::device::get_image3d_memory_requirements(size, mip_levels, fmt, tiling, usages);
		}
		/// Queries the memory requirements of the given buffer.
		[[nodiscard]] memory::size_alignment get_buffer_memory_requirements(
			std::size_t size, buffer_usage_mask usages
		) {
			return backend::device::get_buffer_memory_requirements(size, usages);
		}
		/// Creates a buffer placed at the given memory location.
		[[nodiscard]] buffer create_placed_buffer(
			std::size_t size, buffer_usage_mask allowed_usages, const memory_block &mem, std::size_t offset
		) {
			return backend::device::create_placed_buffer(size, allowed_usages, mem, offset);
		}
		/// Creates a 2D image placed at the given memory location.
		[[nodiscard]] image2d create_placed_image2d(
			cvec2u32 size, std::uint32_t mip_levels,
			format fmt, image_tiling tiling, image_usage_mask allowed_usages,
			const memory_block &mem, std::size_t offset
		) {
			return backend::device::create_placed_image2d(
				size, mip_levels, fmt, tiling, allowed_usages, mem, offset
			);
		}
		/// Creates a 3D image placed at the given memory location.
		[[nodiscard]] image3d create_placed_image3d(
			cvec3u32 size, std::uint32_t mip_levels,
			format fmt, image_tiling tiling, image_usage_mask allowed_usages,
			const memory_block &mem, std::size_t offset
		) {
			return backend::device::create_placed_image3d(
				size, mip_levels, fmt, tiling, allowed_usages, mem, offset
			);
		}

		/// Maps the entire given buffer. \ref map_buffer() and \ref unmap_buffer() calls can be nested.
		///
		/// \return Address to the beginning of the buffer.
		[[nodiscard]] std::byte *map_buffer(buffer &buf) {
			return backend::device::map_buffer(buf);
		}
		/// Unmaps the given buffer. \ref map_buffer() and \ref unmap_buffer() calls can be nested.
		void unmap_buffer(buffer &buf) {
			return backend::device::unmap_buffer(buf);
		}
		/// Flushes the given memory range in the buffer so that the CPU is able to read the latest data.
		void flush_mapped_buffer_to_host(buffer &buf, std::size_t begin, std::size_t length) {
			return backend::device::flush_mapped_buffer_to_host(buf, begin, length);
		}
		/// Flushes the given memory range in the buffer so that the GPU is able to read the latest data.
		void flush_mapped_buffer_to_device(buffer &buf, std::size_t begin, std::size_t length) {
			return backend::device::flush_mapped_buffer_to_device(buf, begin, length);
		}

		/// Creates a view for an \ref image2d.
		[[nodiscard]] image2d_view create_image2d_view_from(
			const image2d &img, format fmt, mip_levels mip
		) {
			return backend::device::create_image2d_view_from(img, fmt, mip);
		}
		/// Creates a view for an \ref image3d.
		[[nodiscard]] image3d_view create_image3d_view_from(
			const image3d &img, format fmt, mip_levels mip
		) {
			return backend::device::create_image3d_view_from(img, fmt, mip);
		}

		/// Creates a \ref frame_buffer.
		[[nodiscard]] frame_buffer create_frame_buffer(
			std::span<const image2d_view *const> color, const image2d_view *depth_stencil, cvec2u32 size
		) {
			return backend::device::create_frame_buffer(color, depth_stencil, size);
		}
		/// \overload
		[[nodiscard]] frame_buffer create_frame_buffer(
			std::initializer_list<const image2d_view*> color, const image2d_view *depth_stencil, cvec2u32 size
		) {
			return create_frame_buffer({ color.begin(), color.end() }, depth_stencil, size);
		}

		/// Creates a \ref fence.
		[[nodiscard]] fence create_fence(synchronization_state state) {
			return backend::device::create_fence(state);
		}
		/// Creates a \ref timeline_semaphore.
		[[nodiscard]] timeline_semaphore create_timeline_semaphore(timeline_semaphore::value_type value) {
			return backend::device::create_timeline_semaphore(value);
		}


		/// Resets the given fence.
		void reset_fence(fence &f) {
			backend::device::reset_fence(f);
		}
		/// Waits for the given fence to be signaled.
		void wait_for_fence(fence &f) {
			backend::device::wait_for_fence(f);
		}

		/// Signals the timeline semaphore from the CPU side.
		void signal_timeline_semaphore(timeline_semaphore &sem, timeline_semaphore::value_type value) {
			backend::device::signal_timeline_semaphore(sem, value);
		}
		/// Queries the current value of the given \ref timeline_semaphore.
		[[nodiscard]] timeline_semaphore::value_type query_timeline_semaphore(timeline_semaphore &sem) {
			return backend::device::query_timeline_semaphore(sem);
		}
		/// Waits until the given timeline semaphore has reached a value equal to or greater than the given value.
		void wait_for_timeline_semaphore(timeline_semaphore &sem, timeline_semaphore::value_type value) {
			backend::device::wait_for_timeline_semaphore(sem, value);
		}


		/// Creates a timestamp query heap with the specified size.
		[[nodiscard]] timestamp_query_heap create_timestamp_query_heap(std::uint32_t size) {
			return backend::device::create_timestamp_query_heap(size);
		}
		/// Reads all timestamp results back to the given buffer. \ref command_list::resolve_timestamp_queries() must
		/// have been called for the results to be valid.
		void fetch_query_results(timestamp_query_heap &h, std::uint32_t first, std::span<std::uint64_t> timestamps) {
			return backend::device::fetch_query_results(h, first, timestamps);
		}


		/// Sets the debug name of the given object.
		void set_debug_name(buffer &buf, const char8_t *name) {
			backend::device::set_debug_name(buf, name);
		}
		/// \overload
		void set_debug_name(buffer &buf, const std::u8string &name) {
			set_debug_name(buf, name.c_str());
		}
		/// Sets the debug name of the given object.
		void set_debug_name(image_base &img, const char8_t *name) {
			backend::device::set_debug_name(img, name);
		}
		/// \overload
		void set_debug_name(image_base &img, const std::u8string &name) {
			set_debug_name(img, name.c_str());
		}
		/// Sets the debug name of the given object.
		void set_debug_name(image_view_base &img, const char8_t *name) {
			backend::device::set_debug_name(img, name);
		}
		/// \overload
		void set_debug_name(image_view_base &img, const std::u8string &name) {
			set_debug_name(img, name.c_str());
		}


		// ray-tracing related
		/// Creates an acceleration structure geometry description from the given buffer views.
		[[nodiscard]] bottom_level_acceleration_structure_geometry
			create_bottom_level_acceleration_structure_geometry(std::span<const raytracing_geometry_view> data) {
			return backend::device::create_bottom_level_acceleration_structure_geometry(data);
		}
		/// \overload
		[[nodiscard]] bottom_level_acceleration_structure_geometry
			create_bottom_level_acceleration_structure_geometry(
				std::initializer_list<raytracing_geometry_view> data
		) {
			return create_bottom_level_acceleration_structure_geometry({ data.begin(), data.end() });
		}

		/// Returns an \ref instance_description for a bottom-level acceleration structure.
		[[nodiscard]] instance_description get_bottom_level_acceleration_structure_description(
			bottom_level_acceleration_structure &as,
			mat44f trans, std::uint32_t id, std::uint8_t mask, std::uint32_t hit_group_offset,
			raytracing_instance_flags flags
		) const {
			return backend::device::get_bottom_level_acceleration_structure_description(
				as, trans, id, mask, hit_group_offset, flags
			);
		}

		/// Queries size information for the given bottom level acceleration structure.
		[[nodiscard]] acceleration_structure_build_sizes get_bottom_level_acceleration_structure_build_sizes(
			const bottom_level_acceleration_structure_geometry &geom
		) {
			return backend::device::get_bottom_level_acceleration_structure_build_sizes(geom);
		}
		/// Queries size information for the given top level acceleration structure. This function will *not* inspect
		/// any GPU-side data, so it's safe to use uninitialized buffers.
		[[nodiscard]] acceleration_structure_build_sizes get_top_level_acceleration_structure_build_sizes(
			std::size_t instance_count
		) {
			return backend::device::get_top_level_acceleration_structure_build_sizes(instance_count);
		}
		/// Creates an uninitialized bottom-level acceleration structure object.
		[[nodiscard]] bottom_level_acceleration_structure create_bottom_level_acceleration_structure(
			buffer &buf, std::size_t offset, std::size_t size
		) {
			return backend::device::create_bottom_level_acceleration_structure(buf, offset, size);
		}
		/// Creates an uninitialized top-level acceleration structure object.
		[[nodiscard]] top_level_acceleration_structure create_top_level_acceleration_structure(
			buffer &buf, std::size_t offset, std::size_t size
		) {
			return backend::device::create_top_level_acceleration_structure(buf, offset, size);
		}

		/// Updates the descriptors in the set with the given acceleration structures.
		void write_descriptor_set_acceleration_structures(
			descriptor_set &set, const descriptor_set_layout &layout, std::size_t first_register,
			std::span<top_level_acceleration_structure *const> acceleration_structures
		) {
			backend::device::write_descriptor_set_acceleration_structures(
				set, layout, first_register, acceleration_structures
			);
		}
		/// \overload
		void write_descriptor_set_acceleration_structures(
			descriptor_set &set, const descriptor_set_layout &layout, std::size_t first_register,
			std::initializer_list<top_level_acceleration_structure*> acceleration_structures
		) {
			write_descriptor_set_acceleration_structures(
				set, layout, first_register, { acceleration_structures.begin(), acceleration_structures.end() }
			);
		}

		/// Returns a handle to the shader group at the given index.
		[[nodiscard]] shader_group_handle get_shader_group_handle(
			const raytracing_pipeline_state &pipeline, std::size_t index
		) {
			return backend::device::get_shader_group_handle(pipeline, index);
		}

		/// Creates a \ref raytracing_pipeline_state.
		[[nodiscard]] raytracing_pipeline_state create_raytracing_pipeline_state(
			std::span<const shader_function> hit_group_shaders, std::span<const hit_shader_group> hit_groups,
			std::span<const shader_function> general_shaders,
			std::size_t max_recursion_depth, std::size_t max_payload_size, std::size_t max_attribute_size,
			const pipeline_resources &rsrc
		) {
			return backend::device::create_raytracing_pipeline_state(
				hit_group_shaders, hit_groups, general_shaders,
				max_recursion_depth, max_payload_size, max_attribute_size, rsrc
			);
		}
		/// \overload
		[[nodiscard]] raytracing_pipeline_state create_raytracing_pipeline_state(
			std::initializer_list<shader_function> hit_group_shaders,
			std::initializer_list<hit_shader_group> hit_groups,
			std::initializer_list<shader_function> general_shaders,
			std::size_t max_recursion_depth, std::size_t max_payload_size, std::size_t max_attribute_size,
			const pipeline_resources &rsrc
		) {
			return create_raytracing_pipeline_state(
				{ hit_group_shaders.begin(), hit_group_shaders.end() }, { hit_groups.begin(), hit_groups.end() },
				{ general_shaders.begin(), general_shaders.end() },
				max_recursion_depth, max_payload_size, max_attribute_size, rsrc
			);
		}
	protected:
		/// Creates a device from a backend device.
		device(backend::device d) : backend::device(std::move(d)) {
		}
	};

	/// Lightweight handle to an adapter that a device can be created from.
	class adapter : public backend::adapter {
		friend context;
	public:
		/// Creates an empty adapter.
		adapter(std::nullptr_t) : backend::adapter(nullptr) {
		}
		/// Move construction.
		adapter(adapter &&src) noexcept : backend::adapter(std::move(src)) {
		}
		/// Copy construction.
		adapter(const adapter &src) : backend::adapter(src) {
		}
		/// Move assignment.
		adapter &operator=(adapter &&src) {
			backend::adapter::operator=(std::move(src));
			return *this;
		}
		/// Copy assignment.
		adapter &operator=(const adapter &src) {
			backend::adapter::operator=(src);
			return *this;
		}

		/// Creates a device that uses this adapter.
		[[nodiscard]] std::pair<device, std::vector<command_queue>> create_device(
			std::span<const queue_type> queue_types
		) {
			// TODO allocator
			auto &&[dev, backend_qs] = backend::adapter::create_device(queue_types);
			std::vector<command_queue> qs(backend_qs.size(), nullptr);
			for (std::size_t i = 0; i < backend_qs.size(); ++i) {
				qs[i] = command_queue(std::move(backend_qs[i]), static_cast<std::uint32_t>(i), queue_types[i]);
			}
			return { device(std::move(dev)), std::move(qs) };
		}
		/// \overload
		[[nodiscard]] std::pair<device, std::vector<command_queue>> create_device(
			std::initializer_list<queue_type> queue_types
		) {
			return create_device({ queue_types.begin(), queue_types.end() });
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
