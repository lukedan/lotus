#include "lotus/graphics/backends/vulkan/commands.h"

/// \file
/// Command list implementation.

#include "lotus/utils/stack_allocator.h"
#include "lotus/graphics/descriptors.h"
#include "lotus/graphics/device.h"
#include "lotus/graphics/pass.h"
#include "lotus/graphics/pipeline.h"
#include "lotus/graphics/resources.h"

namespace lotus::graphics::backends::vulkan {
	void command_allocator::reset(device &dev) {
		_details::assert_vk(dev._device->resetCommandPool(_pool.get()));
	}


	void command_list::reset_and_start(command_allocator&) {
		_details::assert_vk(_buffer->reset());
		vk::CommandBufferBeginInfo info;
		_details::assert_vk(_buffer->begin(info));
	}

	void command_list::begin_pass(
		const pass_resources &rsrc, const frame_buffer &buf,
		std::span<const linear_rgba_f> clear, float clear_depth, std::uint8_t clear_stencil
	) {
		auto bookmark = stack_allocator::scoped_bookmark::create();
		auto clear_values =
			stack_allocator::for_this_thread().create_reserved_vector_array<vk::ClearValue>(clear.size() + 1);
		for (const auto &v : clear) {
			// TODO integer types
			clear_values.emplace_back(vk::ClearColorValue(std::array<float, 4>({ v.r, v.g, v.b, v.a })));
		}
		clear_values.emplace_back(vk::ClearDepthStencilValue(clear_depth, clear_stencil));

		vk::RenderPassBeginInfo info;
		info
			.setRenderPass(rsrc._pass.get())
			.setFramebuffer(buf._framebuffer.get())
			.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), buf._size))
			.setClearValues(clear_values);
		_buffer->beginRenderPass(info, vk::SubpassContents::eInline);
	}

	void command_list::bind_pipeline_state(const graphics_pipeline_state &state) {
		_buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, state._pipeline.get());
	}

	void command_list::bind_pipeline_state(const compute_pipeline_state &state) {
		_buffer->bindPipeline(vk::PipelineBindPoint::eCompute, state._pipeline.get());
	}

	void command_list::bind_vertex_buffers(std::size_t start, std::span<const vertex_buffer> buffers) {
		auto bookmark = stack_allocator::scoped_bookmark::create();
		auto buffer_ptrs =
			stack_allocator::for_this_thread().create_reserved_vector_array<vk::Buffer>(buffers.size());
		auto offsets =
			stack_allocator::for_this_thread().create_reserved_vector_array<vk::DeviceSize>(buffers.size());
		for (auto &buf : buffers) {
			buffer_ptrs.emplace_back(static_cast<buffer*>(buf.data)->_buffer);
			offsets.emplace_back(static_cast<vk::DeviceSize>(buf.offset));
		}
		_buffer->bindVertexBuffers(static_cast<std::uint32_t>(start), buffer_ptrs, offsets);
	}

	void command_list::bind_index_buffer(const buffer &buf, std::size_t offset, index_format fmt) {
		_buffer->bindIndexBuffer(
			buf._buffer, static_cast<vk::DeviceSize>(offset), _details::conversions::for_index_format(fmt)
		);
	}

	void command_list::bind_graphics_descriptor_sets(
		const pipeline_resources &rsrc, std::size_t first, std::span<const graphics::descriptor_set *const> sets
	) {
		auto bookmark = stack_allocator::scoped_bookmark::create();
		auto vk_sets = stack_allocator::for_this_thread().create_reserved_vector_array<vk::DescriptorSet>(sets.size());
		for (auto *set : sets) {
			vk_sets.emplace_back(static_cast<const descriptor_set*>(set)->_set.get());
		}
		_buffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, rsrc._layout.get(), first, vk_sets, {});
	}

	void command_list::bind_compute_descriptor_sets(
		const pipeline_resources &rsrc, std::size_t first, std::span<const graphics::descriptor_set *const> sets
	) {
		auto bookmark = stack_allocator::scoped_bookmark::create();
		auto vk_sets = stack_allocator::for_this_thread().create_reserved_vector_array<vk::DescriptorSet>(sets.size());
		for (auto *set : sets) {
			vk_sets.emplace_back(static_cast<const descriptor_set*>(set)->_set.get());
		}
		_buffer->bindDescriptorSets(vk::PipelineBindPoint::eCompute, rsrc._layout.get(), first, vk_sets, {});
	}

	void command_list::set_viewports(std::span<const viewport> vps) {
		auto bookmark = stack_allocator::scoped_bookmark::create();
		auto viewports = stack_allocator::for_this_thread().create_reserved_vector_array<vk::Viewport>(vps.size());
		for (const auto &vp : vps) {
			cvec2f size = vp.xy.signed_size();
			viewports.emplace_back(vp.xy.min[0], vp.xy.min[1], size[0], size[1], vp.minimum_depth, vp.maximum_depth);
		}
		_buffer->setViewport(0, viewports);
	}

	void command_list::set_scissor_rectangles(std::span<const aab2i> rects) {
		auto bookmark = stack_allocator::scoped_bookmark::create();
		auto scissors = stack_allocator::for_this_thread().create_reserved_vector_array<vk::Rect2D>(rects.size());
		for (const auto &r : rects) {
			cvec2i size = r.signed_size();
			scissors.emplace_back(vk::Offset2D(r.min[0], r.min[1]), vk::Extent2D(size[0], size[1]));
		}
		_buffer->setScissor(0, scissors);
	}

	void command_list::copy_buffer(buffer &from, std::size_t off1, buffer &to, std::size_t off2, std::size_t size) {
		vk::BufferCopy copy;
		copy
			.setSrcOffset(off1)
			.setDstOffset(off2)
			.setSize(size);
		_buffer->copyBuffer(from._buffer, to._buffer, copy);
	}

	void command_list::copy_image2d(
		image2d &from, subresource_index sub1, aab2s region, image2d &to, subresource_index sub2, cvec2s off
	) {
		cvec2s size = region.signed_size();
		vk::ImageCopy copy;
		copy
			.setSrcSubresource(_details::conversions::to_image_subresource_layers(sub1))
			.setSrcOffset(vk::Offset3D(region.min[0], region.min[1], 0))
			.setDstSubresource(_details::conversions::to_image_subresource_layers(sub2))
			.setDstOffset(vk::Offset3D(off[0], off[1], 0))
			.setExtent(vk::Extent3D(size[0], size[1], 1));
		_buffer->copyImage(
			from._image, vk::ImageLayout::eTransferSrcOptimal, to._image, vk::ImageLayout::eTransferDstOptimal, copy
		);
	}

	void command_list::copy_buffer_to_image(
		buffer &from, std::size_t byte_offset, std::size_t row_pitch, aab2s region,
		image2d &to, subresource_index subresource, cvec2s offset
	) {
		cvec2s size = region.signed_size();
		vk::BufferImageCopy copy;
		copy
			.setBufferOffset(byte_offset)
			.setBufferRowLength(size[0]) // HACK we need a proper way to determine its width
			.setBufferImageHeight(size[1])
			.setImageSubresource(_details::conversions::to_image_subresource_layers(subresource))
			.setImageOffset(vk::Offset3D(region.min[0], region.min[1], 0))
			.setImageExtent(vk::Extent3D(size[0], size[1], 1));
		_buffer->copyBufferToImage(from._buffer, to._image, vk::ImageLayout::eTransferDstOptimal, copy);
	}

	void command_list::draw_instanced(
		std::size_t first_vertex, std::size_t vertex_count,
		std::size_t first_instance, std::size_t instance_count
	) {
		_buffer->draw(vertex_count, instance_count, first_vertex, first_instance);
	}

	void command_list::draw_indexed_instanced(
		std::size_t first_index, std::size_t index_count,
		std::size_t first_vertex,
		std::size_t first_instance, std::size_t instance_count
	) {
		_buffer->drawIndexed(index_count, instance_count, first_index, first_vertex, first_instance);
	}

	void command_list::run_compute_shader(std::uint32_t x, std::uint32_t y, std::uint32_t z) {
		_buffer->dispatch(x, y, z);
	}

	void command_list::resource_barrier(std::span<const image_barrier> imgs, std::span<const buffer_barrier> bufs) {
		auto bookmark = stack_allocator::scoped_bookmark::create();
		auto img_barriers =
			stack_allocator::for_this_thread().create_reserved_vector_array<vk::ImageMemoryBarrier>(imgs.size());
		auto buf_barriers =
			stack_allocator::for_this_thread().create_reserved_vector_array<vk::BufferMemoryBarrier>(bufs.size());
		for (const auto &i : imgs) {
			auto [from_access, from_layout] = _details::conversions::to_image_access_layout(i.from_state);
			auto [to_access, to_layout] = _details::conversions::to_image_access_layout(i.to_state);
			vk::ImageMemoryBarrier barrier;
			barrier
				.setSrcAccessMask(from_access)
				.setDstAccessMask(to_access)
				.setOldLayout(from_layout)
				.setNewLayout(to_layout)
				.setSrcQueueFamilyIndex(0) // TODO
				.setDstQueueFamilyIndex(0)
				.setImage(static_cast<_details::image*>(i.target)->_image)
				.setSubresourceRange(_details::conversions::to_image_subresource_range(i.subresource));
			img_barriers.emplace_back(barrier);
		}
		for (const auto &b : bufs) {
			vk::BufferMemoryBarrier barrier;
			barrier
				.setSrcAccessMask(_details::conversions::to_access_flags(b.from_state))
				.setDstAccessMask(_details::conversions::to_access_flags(b.to_state))
				.setSrcQueueFamilyIndex(0) // TODO
				.setDstQueueFamilyIndex(0)
				.setBuffer(static_cast<buffer*>(b.target)->_buffer)
				.setOffset(0)
				.setSize(VK_WHOLE_SIZE);
			buf_barriers.emplace_back(barrier);
		}
		_buffer->pipelineBarrier(
			vk::PipelineStageFlagBits::eAllGraphics, vk::PipelineStageFlagBits::eAllGraphics, vk::DependencyFlags(),
			{}, buf_barriers, img_barriers
		);
	}

	void command_list::end_pass() {
		_buffer->endRenderPass();
	}

	void command_list::finish() {
		_details::assert_vk(_buffer->end());
	}


	void command_queue::submit_command_lists(
		std::span<const graphics::command_list *const> lists, fence *on_completion
	) {
		auto bookmark = stack_allocator::scoped_bookmark::create();
		auto arr = stack_allocator::for_this_thread().create_reserved_vector_array<vk::CommandBuffer>(lists.size());
		for (const auto *list : lists) {
			arr.emplace_back(list->_buffer.get());
		}
		vk::SubmitInfo info;
		info.setCommandBuffers(arr);
		_details::assert_vk(_queue.submit(info, on_completion ? on_completion->_fence.get() : nullptr));
	}

	void command_queue::present(swap_chain &target) {
		vk::PresentInfoKHR info;
		std::uint32_t index = target._frame_index;
		info
			.setSwapchains(target._swapchain.get())
			.setImageIndices(index);
		_details::assert_vk(_queue.presentKHR(info));
	}

	void command_queue::signal(fence &f) {
		_details::assert_vk(_queue.submit({}, f._fence.get()));
	}
}
