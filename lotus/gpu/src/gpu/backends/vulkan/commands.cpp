#include "lotus/gpu/backends/vulkan/commands.h"

/// \file
/// Command list implementation.

#include "lotus/memory/stack_allocator.h"
#include "lotus/gpu/descriptors.h"
#include "lotus/gpu/device.h"
#include "lotus/gpu/pipeline.h"
#include "lotus/gpu/resources.h"

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
		for (std::size_t i = 0; i < access.color_render_targets.size(); ++i) {
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
			depth_attachment
				.setImageView(buf._depth_stencil_view)
				.setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal)
				.setLoadOp(_details::conversions::to_attachment_load_op(depth_access.load_operation))
				.setStoreOp(_details::conversions::to_attachment_store_op(depth_access.store_operation))
				.setClearValue(vk::ClearDepthStencilValue(
					static_cast<float>(depth_access.clear_value), stencil_access.clear_value
				));
			stencil_attachment
				.setImageView(buf._depth_stencil_view)
				.setImageLayout(vk::ImageLayout::eStencilAttachmentOptimal)
				.setLoadOp(_details::conversions::to_attachment_load_op(stencil_access.load_operation))
				.setStoreOp(_details::conversions::to_attachment_store_op(stencil_access.store_operation))
				.setClearValue(vk::ClearDepthStencilValue(
					static_cast<float>(depth_access.clear_value), stencil_access.clear_value
				));
			info
				.setPDepthAttachment(&depth_attachment)
				.setPStencilAttachment(&stencil_attachment);
		}

		_buffer.beginRendering(info);
	}

	void command_list::bind_pipeline_state(const graphics_pipeline_state &state) {
		_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, state._pipeline.get());
	}

	void command_list::bind_pipeline_state(const compute_pipeline_state &state) {
		_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, state._pipeline.get());
	}

	void command_list::bind_vertex_buffers(std::size_t start, std::span<const vertex_buffer> buffers) {
		if (buffers.empty()) {
			return;
		}
		auto bookmark = get_scratch_bookmark();
		auto buffer_ptrs = bookmark.create_reserved_vector_array<vk::Buffer>(buffers.size());
		auto offsets = bookmark.create_reserved_vector_array<vk::DeviceSize>(buffers.size());
		for (auto &buf : buffers) {
			buffer_ptrs.emplace_back(static_cast<const buffer*>(buf.data)->_buffer);
			offsets.emplace_back(static_cast<vk::DeviceSize>(buf.offset));
		}
		_buffer.bindVertexBuffers(static_cast<std::uint32_t>(start), buffer_ptrs, offsets);
	}

	void command_list::bind_index_buffer(const buffer &buf, std::size_t offset, index_format fmt) {
		_buffer.bindIndexBuffer(
			buf._buffer, static_cast<vk::DeviceSize>(offset), _details::conversions::to_index_type(fmt)
		);
	}

	void command_list::bind_graphics_descriptor_sets(
		const pipeline_resources &rsrc, std::size_t first, std::span<const gpu::descriptor_set *const> sets
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
			vk::PipelineBindPoint::eGraphics, rsrc._layout.get(), static_cast<std::uint32_t>(first), vk_sets, {}
		);
	}

	void command_list::bind_compute_descriptor_sets(
		const pipeline_resources &rsrc, std::size_t first, std::span<const gpu::descriptor_set *const> sets
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
			vk::PipelineBindPoint::eCompute, rsrc._layout.get(), static_cast<std::uint32_t>(first), vk_sets, {}
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

	void command_list::set_scissor_rectangles(std::span<const aab2i> rects) {
		auto bookmark = get_scratch_bookmark();
		auto scissors = bookmark.create_reserved_vector_array<vk::Rect2D>(rects.size());
		for (const auto &r : rects) {
			cvec2i size = r.signed_size();
			scissors.emplace_back(vk::Offset2D(r.min[0], r.min[1]), vk::Extent2D(size[0], size[1]));
		}
		_buffer.setScissor(0, scissors);
	}

	void command_list::copy_buffer(
		const buffer &from, std::size_t off1, buffer &to, std::size_t off2, std::size_t size
	) {
		vk::BufferCopy copy;
		copy
			.setSrcOffset(off1)
			.setDstOffset(off2)
			.setSize(size);
		_buffer.copyBuffer(from._buffer, to._buffer, copy);
	}

	void command_list::copy_image2d(
		image2d &from, subresource_index sub1, aab2s region, image2d &to, subresource_index sub2, cvec2s off
	) {
		cvec2s size = region.signed_size();
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
		const buffer &from, std::size_t byte_offset, staging_buffer_metadata meta,
		image2d &to, subresource_index subresource, cvec2s offset
	) {
		const auto &props = format_properties::get(meta._format);
		crash_if(offset[0] % props.fragment_size[0] != 0 || offset[1] % props.fragment_size[1] != 0);
		auto aligned_size = cvec2s(
			memory::align_up(meta._size[0], props.fragment_size[0]),
			memory::align_up(meta._size[1], props.fragment_size[1])
		).into<std::uint32_t>();
		vk::BufferImageCopy copy;
		copy
			.setBufferOffset(byte_offset)
			.setBufferRowLength(aligned_size[0])
			.setBufferImageHeight(aligned_size[1])
			.setImageSubresource(_details::conversions::to_image_subresource_layers(subresource))
			.setImageOffset(vk::Offset3D(_details::conversions::to_offset_2d(offset), 0))
			.setImageExtent(vk::Extent3D(_details::conversions::to_extent_2d(meta._size), 1));
		_buffer.copyBufferToImage(from._buffer, to._image, vk::ImageLayout::eTransferDstOptimal, copy);
	}

	void command_list::draw_instanced(
		std::size_t first_vertex, std::size_t vertex_count,
		std::size_t first_instance, std::size_t instance_count
	) {
		_buffer.draw(
			static_cast<std::uint32_t>(vertex_count), static_cast<std::uint32_t>(instance_count),
			static_cast<std::uint32_t>(first_vertex), static_cast<std::uint32_t>(first_instance)
		);
	}

	void command_list::draw_indexed_instanced(
		std::size_t first_index, std::size_t index_count,
		std::size_t first_vertex,
		std::size_t first_instance, std::size_t instance_count
	) {
		_buffer.drawIndexed(
			static_cast<std::uint32_t>(index_count), static_cast<std::uint32_t>(instance_count),
			static_cast<std::uint32_t>(first_index), static_cast<std::uint32_t>(first_vertex),
			static_cast<std::uint32_t>(first_instance)
		);
	}

	void command_list::run_compute_shader(std::uint32_t x, std::uint32_t y, std::uint32_t z) {
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
				.setSrcQueueFamilyIndex(0) // TODO
				.setDstQueueFamilyIndex(0)
				.setImage(static_cast<_details::image*>(i.target)->_image)
				.setSubresourceRange(_details::conversions::to_image_subresource_range(i.subresources));
		}
		for (const auto &b : bufs) {
			auto &barrier = buf_barriers.emplace_back();
			barrier
				.setSrcStageMask(_details::conversions::to_pipeline_stage_flags_2(b.from_point))
				.setSrcAccessMask(_details::conversions::to_access_flags_2(b.from_access))
				.setDstStageMask(_details::conversions::to_pipeline_stage_flags_2(b.to_point))
				.setDstAccessMask(_details::conversions::to_access_flags_2(b.to_access))
				.setSrcQueueFamilyIndex(0) // TODO
				.setDstQueueFamilyIndex(0)
				.setBuffer(static_cast<buffer*>(b.target)->_buffer)
				.setOffset(0)
				.setSize(VK_WHOLE_SIZE);
		}
		vk::DependencyInfo info;
		info
			.setImageMemoryBarriers(img_barriers)
			.setBufferMemoryBarriers(buf_barriers);
		_buffer.pipelineBarrier2(info);
	}

	void command_list::end_pass() {
		_buffer.endRendering();
	}

	void command_list::insert_marker(const char8_t *name, linear_rgba_u8 color) {
		if (!is_empty(_device->_options & context_options::enable_debug_info)) {
			auto color_f = color.into<float>();
			vk::DebugMarkerMarkerInfoEXT info;
			info
				.setPMarkerName(reinterpret_cast<const char*>(name))
				.setColor({ color_f.r, color_f.g, color_f.b, color_f.a });
			_buffer.debugMarkerInsertEXT(info, *_device->_dispatch_loader);
		}
	}

	void command_list::begin_marker_scope(const char8_t *name, linear_rgba_u8 color) {
		if (!is_empty(_device->_options & context_options::enable_debug_info)) {
			auto color_f = color.into<float>();
			vk::DebugMarkerMarkerInfoEXT info;
			info
				.setPMarkerName(reinterpret_cast<const char*>(name))
				.setColor({ color_f.r, color_f.g, color_f.b, color_f.a });
			_buffer.debugMarkerBeginEXT(info, *_device->_dispatch_loader);
		}
	}

	void command_list::end_marker_scope() {
		if (!is_empty(_device->_options & context_options::enable_debug_info)) {
			_buffer.debugMarkerEndEXT(*_device->_dispatch_loader);
		}
	}

	void command_list::finish() {
		_details::assert_vk(_buffer.end());
	}

	void command_list::build_acceleration_structure(
		const bottom_level_acceleration_structure_geometry &geom,
		bottom_level_acceleration_structure &output, buffer &scratch, std::size_t scratch_offset
	) {
		auto bookmark = get_scratch_bookmark();
		auto build_ranges = bookmark.create_reserved_vector_array<vk::AccelerationStructureBuildRangeInfoKHR>(
			geom._geometries.size()
		);
		for (std::size_t i = 0; i < geom._geometries.size(); ++i) {
			build_ranges.emplace_back()
				.setPrimitiveCount(geom._pimitive_counts[i])
				.setPrimitiveOffset(0)
				.setFirstVertex(0)
				.setTransformOffset(0);
		}
		vk::AccelerationStructureBuildGeometryInfoKHR build_info = geom._build_info;
		build_info
			.setDstAccelerationStructure(output._acceleration_structure.get())
			.setScratchData(_device->_device->getBufferAddress(scratch._buffer) + scratch_offset);
		_buffer.buildAccelerationStructuresKHR(build_info, build_ranges.data(), *_device->_dispatch_loader);
	}

	void command_list::build_acceleration_structure(
		const buffer &instances, std::size_t offset, std::size_t count,
		top_level_acceleration_structure &output, buffer &scratch, std::size_t scratch_offset
	) {
		vk::AccelerationStructureGeometryInstancesDataKHR instance_data;
		instance_data
			.setArrayOfPointers(false)
			.setData(_device->_device->getBufferAddress(instances._buffer) + offset);
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
			.setScratchData(_device->_device->getBufferAddress(scratch._buffer) + scratch_offset);
		vk::AccelerationStructureBuildRangeInfoKHR build_range;
		build_range
			.setPrimitiveCount(static_cast<std::uint32_t>(count))
			.setPrimitiveOffset(0)
			.setFirstVertex(0)
			.setTransformOffset(0);
		_buffer.buildAccelerationStructuresKHR(build_info, &build_range, *_device->_dispatch_loader);
	}

	void command_list::bind_pipeline_state(const raytracing_pipeline_state &state) {
		_buffer.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, state._pipeline.get());
	}

	void command_list::bind_ray_tracing_descriptor_sets(
		const pipeline_resources &rsrc, std::size_t first, std::span<const gpu::descriptor_set *const> sets
	) {
		auto bookmark = get_scratch_bookmark();
		auto vk_sets = bookmark.create_reserved_vector_array<vk::DescriptorSet>(sets.size());
		for (auto *set : sets) {
			vk_sets.emplace_back(static_cast<const descriptor_set*>(set)->_set.get());
		}
		_buffer.bindDescriptorSets(
			vk::PipelineBindPoint::eRayTracingKHR, rsrc._layout.get(), static_cast<std::uint32_t>(first), vk_sets, {}
		);
	}

	void command_list::trace_rays(
		constant_buffer_view ray_generation,
		shader_record_view miss_shaders, shader_record_view hit_groups,
		std::size_t width, std::size_t height, std::size_t depth
	) {
		_buffer.traceRaysKHR(
			vk::StridedDeviceAddressRegionKHR(
				_device->_device->getBufferAddress(ray_generation.data->_buffer) + ray_generation.offset,
				ray_generation.size, ray_generation.size
			),
			vk::StridedDeviceAddressRegionKHR(
				_device->_device->getBufferAddress(miss_shaders.data->_buffer) + miss_shaders.offset,
				miss_shaders.stride, miss_shaders.stride * miss_shaders.count
			),
			vk::StridedDeviceAddressRegionKHR(
				_device->_device->getBufferAddress(hit_groups.data->_buffer) + hit_groups.offset,
				hit_groups.stride, hit_groups.stride * hit_groups.count
			),
			vk::StridedDeviceAddressRegionKHR(),
			static_cast<std::uint32_t>(width),
			static_cast<std::uint32_t>(height),
			static_cast<std::uint32_t>(depth),
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
		auto wait_sem_values = bookmark.create_reserved_vector_array<std::uint64_t>(synch.wait_semaphores.size());
		for (const auto &sem : synch.wait_semaphores) {
			wait_sems.emplace_back(sem.semaphore->_semaphore.get());
			wait_sem_values.emplace_back(sem.value);
		}
		auto sig_sems = bookmark.create_reserved_vector_array<vk::Semaphore>(synch.notify_semaphores.size());
		auto sig_sem_values = bookmark.create_reserved_vector_array<std::uint64_t>(synch.notify_semaphores.size());
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
			.setSignalSemaphores(sig_sems);
		_details::assert_vk(_queue.submit(info, synch.notify_fence ? synch.notify_fence->_fence.get() : nullptr));
	}

	swap_chain_status command_queue::present(swap_chain &target) {
		vk::PresentInfoKHR info;
		std::uint32_t index = target._frame_index;
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

	void command_queue::signal(timeline_semaphore &sem, std::uint64_t value) {
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
