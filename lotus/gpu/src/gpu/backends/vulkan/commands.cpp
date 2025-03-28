#include "lotus/gpu/backends/vulkan/commands.h"

/// \file
/// Command list implementation.

#include "lotus/memory/stack_allocator.h"
#include "lotus/system/identifier.h"
#include "lotus/gpu/descriptors.h"
#include "lotus/gpu/device.h"
#include "lotus/gpu/pipeline.h"
#include "lotus/gpu/resources.h"
#include "lotus/gpu/synchronization.h"

namespace lotus::gpu::backends::vulkan {
	void command_allocator::reset(device &dev) {
		_details::assert_vk(dev._device->resetCommandPool(_pool.get()));
	}


	command_list::~command_list() {
		if (_buffer) {
			_device->_device->freeCommandBuffers(_pool, _buffer);
		}
	}

	command_list &command_list::operator=(command_list &&src) {
		if (&src != this) {
			if (_buffer) {
				_device->_device->freeCommandBuffers(_pool, _buffer);
			}
			_buffer = std::exchange(src._buffer, nullptr);
			_pool = std::exchange(src._pool, nullptr);
			_device = std::exchange(src._device, nullptr);
		}
		return *this;
	}

	void command_list::reset_and_start(command_allocator&) {
		_details::assert_vk(_buffer.reset());
		vk::CommandBufferBeginInfo info;
		_details::assert_vk(_buffer.begin(info));
	}

	void command_list::begin_pass(const frame_buffer &buf, const frame_buffer_access &access) {
		assert(buf._color_views.size() == access.color_render_targets.size());

		auto bookmark = get_scratch_bookmark();
		auto color_attachments = bookmark.create_reserved_vector_array<vk::RenderingAttachmentInfo>(
			access.color_render_targets.size()
		);
		for (usize i = 0; i < access.color_render_targets.size(); ++i) {
			const auto &rt_access = access.color_render_targets[i];
			color_attachments.emplace_back()
				.setImageView(buf._color_views[i])
				.setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
				.setLoadOp(_details::conversions::to_attachment_load_op(rt_access.load_operation))
				.setStoreOp(_details::conversions::to_attachment_store_op(rt_access.store_operation))
				.setClearValue(_details::conversions::to_clear_value(rt_access.clear_value));
		}

		vk::RenderingInfo info;
		info
			.setRenderArea(vk::Rect2D({ 0, 0 }, _details::conversions::to_extent_2d(buf._size)))
			.setLayerCount(1)
			.setColorAttachments(color_attachments);

		vk::RenderingAttachmentInfo depth_attachment;
		vk::RenderingAttachmentInfo stencil_attachment;
		if (buf._depth_stencil_view) {
			const auto &depth_access = access.depth_render_target;
			const auto &stencil_access = access.stencil_render_target;
			if (
				depth_access.load_operation != pass_load_operation::discard ||
				depth_access.store_operation != pass_store_operation::discard
			) {
				depth_attachment
					.setImageView(buf._depth_stencil_view)
					.setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal)
					.setLoadOp(_details::conversions::to_attachment_load_op(depth_access.load_operation))
					.setStoreOp(_details::conversions::to_attachment_store_op(depth_access.store_operation))
					.setClearValue(vk::ClearDepthStencilValue(
						static_cast<float>(depth_access.clear_value), stencil_access.clear_value
					));
				info.setPDepthAttachment(&depth_attachment);
			}
			if (
				stencil_access.load_operation != pass_load_operation::discard ||
				stencil_access.store_operation != pass_store_operation::discard
			) {
				stencil_attachment
					.setImageView(buf._depth_stencil_view)
					.setImageLayout(vk::ImageLayout::eStencilAttachmentOptimal)
					.setLoadOp(_details::conversions::to_attachment_load_op(stencil_access.load_operation))
					.setStoreOp(_details::conversions::to_attachment_store_op(stencil_access.store_operation))
					.setClearValue(vk::ClearDepthStencilValue(
						static_cast<float>(depth_access.clear_value), stencil_access.clear_value
					));
				info.setPStencilAttachment(&stencil_attachment);
			}
		}

		if constexpr (system::platform::type == system::platform_type::macos) {
			_buffer.beginRenderingKHR(info, *_device->_dispatch_loader);
		} else {
			_buffer.beginRendering(info);
		}
	}

	void command_list::bind_pipeline_state(const graphics_pipeline_state &state) {
		_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, state._pipeline.get());
	}

	void command_list::bind_pipeline_state(const compute_pipeline_state &state) {
		_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, state._pipeline.get());
	}

	void command_list::bind_vertex_buffers(usize start, std::span<const vertex_buffer> buffers) {
		if (buffers.empty()) {
			return;
		}
		auto bookmark = get_scratch_bookmark();
		auto buffer_ptrs = bookmark.create_reserved_vector_array<vk::Buffer>(buffers.size());
		auto offsets = bookmark.create_reserved_vector_array<vk::DeviceSize>(buffers.size());
		for (auto &buf : buffers) {
			buffer_ptrs.emplace_back(static_cast<const buffer*>(buf.data)->_buffer.get());
			offsets.emplace_back(static_cast<vk::DeviceSize>(buf.offset));
		}
		_buffer.bindVertexBuffers(static_cast<u32>(start), buffer_ptrs, offsets);
	}

	void command_list::bind_index_buffer(const buffer &buf, usize offset_bytes, index_format fmt) {
		_buffer.bindIndexBuffer(
			buf._buffer.get(), static_cast<vk::DeviceSize>(offset_bytes), _details::conversions::to_index_type(fmt)
		);
	}

	void command_list::bind_graphics_descriptor_sets(
		const pipeline_resources &rsrc, u32 first, std::span<const gpu::descriptor_set *const> sets
	) {
		if (sets.empty()) {
			return;
		}
		auto bookmark = get_scratch_bookmark();
		auto vk_sets = bookmark.create_reserved_vector_array<vk::DescriptorSet>(sets.size());
		for (auto *set : sets) {
			vk_sets.emplace_back(static_cast<const descriptor_set*>(set)->_set.get());
		}
		_buffer.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics, rsrc._layout.get(), static_cast<u32>(first), vk_sets, {}
		);
	}

	void command_list::bind_compute_descriptor_sets(
		const pipeline_resources &rsrc, u32 first, std::span<const gpu::descriptor_set *const> sets
	) {
		if (sets.empty()) {
			return;
		}
		auto bookmark = get_scratch_bookmark();
		auto vk_sets = bookmark.create_reserved_vector_array<vk::DescriptorSet>(sets.size());
		for (auto *set : sets) {
			vk_sets.emplace_back(static_cast<const descriptor_set*>(set)->_set.get());
		}
		_buffer.bindDescriptorSets(
			vk::PipelineBindPoint::eCompute, rsrc._layout.get(), static_cast<u32>(first), vk_sets, {}
		);
	}

	void command_list::set_viewports(std::span<const viewport> vps) {
		auto bookmark = get_scratch_bookmark();
		auto viewports = bookmark.create_reserved_vector_array<vk::Viewport>(vps.size());
		for (const auto &vp : vps) {
			cvec2f size = vp.xy.signed_size();
			viewports.emplace_back(vp.xy.min[0], vp.xy.min[1], size[0], size[1], vp.minimum_depth, vp.maximum_depth);
		}
		_buffer.setViewport(0, viewports);
	}

	void command_list::set_scissor_rectangles(std::span<const aab2u32> rects) {
		auto bookmark = get_scratch_bookmark();
		auto scissors = bookmark.create_reserved_vector_array<vk::Rect2D>(rects.size());
		for (const auto &r : rects) {
			scissors.emplace_back(
				_details::conversions::to_offset_2d(r.min),
				_details::conversions::to_extent_2d(r.signed_size())
			);
		}
		_buffer.setScissor(0, scissors);
	}

	void command_list::copy_buffer(
		const buffer &from, usize off1, buffer &to, usize off2, usize size
	) {
		vk::BufferCopy copy;
		copy
			.setSrcOffset(off1)
			.setDstOffset(off2)
			.setSize(size);
		_buffer.copyBuffer(from._buffer.get(), to._buffer.get(), copy);
	}

	void command_list::copy_image2d(
		image2d &from, subresource_index sub1, aab2u32 region, image2d &to, subresource_index sub2, cvec2u32 off
	) {
		const cvec2u32 size = region.signed_size();
		vk::ImageCopy copy;
		copy
			.setSrcSubresource(_details::conversions::to_image_subresource_layers(sub1))
			.setSrcOffset(vk::Offset3D(_details::conversions::to_offset_2d(region.min), 0))
			.setDstSubresource(_details::conversions::to_image_subresource_layers(sub2))
			.setDstOffset(vk::Offset3D(_details::conversions::to_offset_2d(off), 0))
			.setExtent(vk::Extent3D(_details::conversions::to_extent_2d(size), 1));
		_buffer.copyImage(
			from._image, vk::ImageLayout::eTransferSrcOptimal, to._image, vk::ImageLayout::eTransferDstOptimal, copy
		);
	}

	void command_list::copy_buffer_to_image(
		const buffer &from, usize byte_offset, staging_buffer_metadata meta,
		image2d &to, subresource_index subresource, cvec2u32 offset
	) {
		const auto &props = format_properties::get(meta.pixel_format);
		crash_if(offset[0] % props.fragment_size[0] != 0 || offset[1] % props.fragment_size[1] != 0);
		const auto aligned_size = cvec2s(
			memory::align_up(meta.image_size[0], props.fragment_size[0]),
			memory::align_up(meta.image_size[1], props.fragment_size[1])
		).into<u32>();
		vk::BufferImageCopy copy;
		copy
			.setBufferOffset(byte_offset)
			.setBufferRowLength(aligned_size[0])
			.setBufferImageHeight(aligned_size[1])
			.setImageSubresource(_details::conversions::to_image_subresource_layers(subresource))
			.setImageOffset(vk::Offset3D(_details::conversions::to_offset_2d(offset), 0))
			.setImageExtent(vk::Extent3D(_details::conversions::to_extent_2d(meta.image_size), 1));
		_buffer.copyBufferToImage(from._buffer.get(), to._image, vk::ImageLayout::eTransferDstOptimal, copy);
	}

	void command_list::draw_instanced(u32 first_vertex, u32 vertex_count, u32 first_instance, u32 instance_count) {
		_buffer.draw(vertex_count, instance_count, first_vertex, first_instance);
	}

	void command_list::draw_indexed_instanced(
		u32 first_index, u32 index_count, i32 first_vertex, u32 first_instance, u32 instance_count
	) {
		_buffer.drawIndexed(index_count, instance_count, first_index, first_vertex, first_instance);
	}

	void command_list::run_compute_shader(u32 x, u32 y, u32 z) {
		_buffer.dispatch(x, y, z);
	}

	void command_list::resource_barrier(std::span<const image_barrier> imgs, std::span<const buffer_barrier> bufs) {
		auto bookmark = get_scratch_bookmark();
		auto img_barriers = bookmark.create_reserved_vector_array<vk::ImageMemoryBarrier2>(imgs.size());
		auto buf_barriers = bookmark.create_reserved_vector_array<vk::BufferMemoryBarrier2>(bufs.size());
		for (const auto &i : imgs) {
			auto &barrier = img_barriers.emplace_back();
			barrier
				.setSrcStageMask(_details::conversions::to_pipeline_stage_flags_2(i.from_point))
				.setSrcAccessMask(_details::conversions::to_access_flags_2(i.from_access))
				.setDstStageMask(_details::conversions::to_pipeline_stage_flags_2(i.to_point))
				.setDstAccessMask(_details::conversions::to_access_flags_2(i.to_access))
				.setOldLayout(_details::conversions::to_image_layout(i.from_layout))
				.setNewLayout(_details::conversions::to_image_layout(i.to_layout))
				.setSrcQueueFamilyIndex(_device->_queue_family_props[i.from_queue].index)
				.setDstQueueFamilyIndex(_device->_queue_family_props[i.to_queue].index)
				.setImage(static_cast<const _details::image_base*>(i.target)->_image)
				.setSubresourceRange(_details::conversions::to_image_subresource_range(i.subresources));
		}
		for (const auto &b : bufs) {
			auto &barrier = buf_barriers.emplace_back();
			barrier
				.setSrcStageMask(_details::conversions::to_pipeline_stage_flags_2(b.from_point))
				.setSrcAccessMask(_details::conversions::to_access_flags_2(b.from_access))
				.setDstStageMask(_details::conversions::to_pipeline_stage_flags_2(b.to_point))
				.setDstAccessMask(_details::conversions::to_access_flags_2(b.to_access))
				.setSrcQueueFamilyIndex(_device->_queue_family_props[b.from_queue].index)
				.setDstQueueFamilyIndex(_device->_queue_family_props[b.to_queue].index)
				.setBuffer(static_cast<const buffer*>(b.target)->_buffer.get())
				.setOffset(0)
				.setSize(VK_WHOLE_SIZE);
		}
		vk::DependencyInfo info;
		info
			.setImageMemoryBarriers(img_barriers)
			.setBufferMemoryBarriers(buf_barriers);
		if constexpr (system::platform::type == system::platform_type::macos) {
			_buffer.pipelineBarrier2KHR(info, *_device->_dispatch_loader);
		} else {
			_buffer.pipelineBarrier2(info);
		}
	}

	void command_list::end_pass() {
		if constexpr (system::platform::type == system::platform_type::macos) {
			_buffer.endRenderingKHR(*_device->_dispatch_loader);
		} else {
			_buffer.endRendering();
		}
	}

	void command_list::query_timestamp(timestamp_query_heap &h, u32 index) {
		_buffer.writeTimestamp(vk::PipelineStageFlagBits::eAllCommands, h._pool.get(), index);
	}

	void command_list::insert_marker(const char8_t *name, linear_rgba_u8 color) {
		if (bit_mask::contains<context_options::enable_debug_info>(_device->_options)) {
			auto color_f = color.into<float>();
			vk::DebugMarkerMarkerInfoEXT info;
			info
				.setPMarkerName(reinterpret_cast<const char*>(name))
				.setColor({ color_f.r, color_f.g, color_f.b, color_f.a });
			_buffer.debugMarkerInsertEXT(info, *_device->_dispatch_loader);
		}
	}

	void command_list::begin_marker_scope(const char8_t *name, linear_rgba_u8 color) {
		if (bit_mask::contains<context_options::enable_debug_info>(_device->_options)) {
			auto color_f = color.into<float>();
			vk::DebugMarkerMarkerInfoEXT info;
			info
				.setPMarkerName(reinterpret_cast<const char*>(name))
				.setColor({ color_f.r, color_f.g, color_f.b, color_f.a });
			_buffer.debugMarkerBeginEXT(info, *_device->_dispatch_loader);
		}
	}

	void command_list::end_marker_scope() {
		if (bit_mask::contains<context_options::enable_debug_info>(_device->_options)) {
			_buffer.debugMarkerEndEXT(*_device->_dispatch_loader);
		}
	}

	void command_list::finish() {
		_details::assert_vk(_buffer.end());
	}

	void command_list::build_acceleration_structure(
		const bottom_level_acceleration_structure_geometry &geom,
		bottom_level_acceleration_structure &output, buffer &scratch, usize scratch_offset
	) {
		auto bookmark = get_scratch_bookmark();
		auto build_ranges = bookmark.create_reserved_vector_array<vk::AccelerationStructureBuildRangeInfoKHR>(
			geom._geometries.size()
		);
		for (usize i = 0; i < geom._geometries.size(); ++i) {
			build_ranges.emplace_back()
				.setPrimitiveCount(geom._pimitive_counts[i])
				.setPrimitiveOffset(0)
				.setFirstVertex(0)
				.setTransformOffset(0);
		}
		vk::AccelerationStructureBuildGeometryInfoKHR build_info = geom._build_info;
		build_info
			.setDstAccelerationStructure(output._acceleration_structure.get())
			.setScratchData(_device->_device->getBufferAddress(scratch._buffer.get()) + scratch_offset);
		_buffer.buildAccelerationStructuresKHR(build_info, build_ranges.data(), *_device->_dispatch_loader);
	}

	void command_list::build_acceleration_structure(
		const buffer &instances, usize offset, usize count,
		top_level_acceleration_structure &output, buffer &scratch, usize scratch_offset
	) {
		vk::AccelerationStructureGeometryInstancesDataKHR instance_data;
		instance_data
			.setArrayOfPointers(false)
			.setData(_device->_device->getBufferAddress(instances._buffer.get()) + offset);
		vk::AccelerationStructureGeometryKHR geometry;
		geometry
			.setGeometryType(vk::GeometryTypeKHR::eInstances)
			.setFlags(vk::GeometryFlagsKHR());
		geometry.geometry.setInstances(instance_data);
		vk::AccelerationStructureBuildGeometryInfoKHR build_info;
		build_info
			.setType(vk::AccelerationStructureTypeKHR::eTopLevel)
			.setFlags(vk::BuildAccelerationStructureFlagsKHR())
			.setMode(vk::BuildAccelerationStructureModeKHR::eBuild)
			.setSrcAccelerationStructure(nullptr)
			.setDstAccelerationStructure(output._acceleration_structure.get())
			.setGeometries(geometry)
			.setScratchData(_device->_device->getBufferAddress(scratch._buffer.get()) + scratch_offset);
		vk::AccelerationStructureBuildRangeInfoKHR build_range;
		build_range
			.setPrimitiveCount(static_cast<u32>(count))
			.setPrimitiveOffset(0)
			.setFirstVertex(0)
			.setTransformOffset(0);
		_buffer.buildAccelerationStructuresKHR(build_info, &build_range, *_device->_dispatch_loader);
	}

	void command_list::bind_pipeline_state(const raytracing_pipeline_state &state) {
		_buffer.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, state._pipeline.get());
	}

	void command_list::bind_ray_tracing_descriptor_sets(
		const pipeline_resources &rsrc, u32 first, std::span<const gpu::descriptor_set *const> sets
	) {
		auto bookmark = get_scratch_bookmark();
		auto vk_sets = bookmark.create_reserved_vector_array<vk::DescriptorSet>(sets.size());
		for (auto *set : sets) {
			vk_sets.emplace_back(static_cast<const descriptor_set*>(set)->_set.get());
		}
		_buffer.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, rsrc._layout.get(), first, vk_sets, {});
	}

	void command_list::trace_rays(
		constant_buffer_view ray_generation,
		shader_record_view miss_shaders, shader_record_view hit_groups,
		usize width, usize height, usize depth
	) {
		_buffer.traceRaysKHR(
			vk::StridedDeviceAddressRegionKHR(
				_device->_device->getBufferAddress(ray_generation.data->_buffer.get()) + ray_generation.offset,
				ray_generation.size, ray_generation.size
			),
			vk::StridedDeviceAddressRegionKHR(
				_device->_device->getBufferAddress(miss_shaders.data->_buffer.get()) + miss_shaders.offset,
				miss_shaders.stride, miss_shaders.stride * miss_shaders.count
			),
			vk::StridedDeviceAddressRegionKHR(
				_device->_device->getBufferAddress(hit_groups.data->_buffer.get()) + hit_groups.offset,
				hit_groups.stride, hit_groups.stride * hit_groups.count
			),
			vk::StridedDeviceAddressRegionKHR(),
			static_cast<u32>(width),
			static_cast<u32>(height),
			static_cast<u32>(depth),
			*_device->_dispatch_loader
		);
	}


	void command_queue::submit_command_lists(
		std::span<const gpu::command_list *const> lists, queue_synchronization synch
	) {
		auto bookmark = get_scratch_bookmark();
		auto vk_lists = bookmark.create_reserved_vector_array<vk::CommandBuffer>(lists.size());
		for (const auto *list : lists) {
			vk_lists.emplace_back(list->_buffer);
		}
		auto wait_sems = bookmark.create_reserved_vector_array<vk::Semaphore>(synch.wait_semaphores.size());
		auto wait_sem_values = bookmark.create_reserved_vector_array<gpu::timeline_semaphore::value_type>(
			synch.wait_semaphores.size()
		);
		auto wait_stages = bookmark.create_reserved_vector_array<vk::PipelineStageFlags>(
			synch.wait_semaphores.size()
		);
		for (const auto &sem : synch.wait_semaphores) {
			wait_sems.emplace_back(sem.semaphore->_semaphore.get());
			wait_sem_values.emplace_back(sem.value);
			wait_stages.emplace_back(vk::PipelineStageFlagBits::eAllCommands);
		}
		auto sig_sems = bookmark.create_reserved_vector_array<vk::Semaphore>(synch.notify_semaphores.size());
		auto sig_sem_values = bookmark.create_reserved_vector_array<gpu::timeline_semaphore::value_type>(
			synch.notify_semaphores.size()
		);
		for (const auto &sem : synch.notify_semaphores) {
			sig_sems.emplace_back(sem.semaphore->_semaphore.get());
			sig_sem_values.emplace_back(sem.value);
		}
		vk::TimelineSemaphoreSubmitInfo sem_info;
		sem_info
			.setWaitSemaphoreValues(wait_sem_values)
			.setSignalSemaphoreValues(sig_sem_values);
		vk::SubmitInfo info;
		info
			.setPNext(&sem_info)
			.setCommandBuffers(vk_lists)
			.setWaitSemaphores(wait_sems)
			.setWaitDstStageMask(wait_stages)
			.setSignalSemaphores(sig_sems);
		_details::assert_vk(_queue.submit(info, synch.notify_fence ? synch.notify_fence->_fence.get() : nullptr));
	}

	swap_chain_status command_queue::present(swap_chain &target) {
		vk::PresentInfoKHR info;
		u32 index = target._frame_index;
		info
			.setSwapchains(target._swapchain.get())
			.setImageIndices(index);
		vk::Result res = _queue.presentKHR(&info);
		if (res == vk::Result::eSuboptimalKHR) {
			return swap_chain_status::suboptimal;
		}
		if (res == vk::Result::eErrorOutOfDateKHR || res == vk::Result::eErrorSurfaceLostKHR) {
			return swap_chain_status::unavailable;
		}
		_details::assert_vk(res);
		return swap_chain_status::ok;
	}

	void command_queue::signal(fence &f) {
		_details::assert_vk(_queue.submit({}, f._fence.get()));
	}

	void command_queue::signal(timeline_semaphore &sem, gpu::timeline_semaphore::value_type value) {
		vk::Semaphore sem_handle = sem._semaphore.get();
		vk::TimelineSemaphoreSubmitInfo sem_info;
		sem_info
			.setSignalSemaphoreValues(value);
		vk::SubmitInfo info;
		info
			.setSignalSemaphores(sem_handle);
		_details::assert_vk(_queue.submit(info));
	}
}
