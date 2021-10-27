#pragma once

/// \file
/// Command pools and command lists.

#include "lotus/color.h"
#include "details.h"
#include "pass.h"
#include "pipeline.h"
#include "frame_buffer.h"

namespace lotus::graphics::backends::vulkan {
	class command_list;
	class command_queue;
	class device;


	/// Contains a \p vk::UniqueCommandPool.
	class command_allocator {
		friend device;
	protected:
		/// Calls \p vk::Device::resetCommandPool().
		void reset(device&);
	private:
		vk::UniqueCommandPool _pool; ///< The command pool.
	};

	/// Contains a \p vk::UniqueCommandBuffer.
	class command_list {
		friend command_queue;
		friend device;
	protected:
		/// Creates an empty object.
		command_list(std::nullptr_t) {
		}

		/// Calls \p vk::CommandBuffer::reset() and \p vk::CommandBuffer::begin().
		void reset_and_start(command_allocator&);

		/// Calls \p vk::CommandBuffer::beginRenderPass().
		void begin_pass(
			const pass_resources&, const frame_buffer&,
			std::span<const linear_rgba_f> clear, float clear_depth, std::uint8_t clear_stencil
		);

		/// Calls \p vk::CommandBuffer::bindPipeline() with \p vk::PipelineBindPoint::eGraphics.
		void bind_pipeline_state(const graphics_pipeline_state&);
		/// Calls \p vk::CommandBuffer::bindPipeline() with \p vk::PipelineBindPoint::eCompute.
		void bind_pipeline_state(const compute_pipeline_state&);
		/// Calls \p vk::CommandBuffer::bindVertexBuffers().
		void bind_vertex_buffers(std::size_t start, std::span<const vertex_buffer>);
		/// Calls \p vk::CommandBuffer::bindIndexBuffer().
		void bind_index_buffer(const buffer&, std::size_t offset, index_format);
		/// Calls \p vk::CommandBuffer::bindDescriptorSets().
		void bind_graphics_descriptor_sets(
			const pipeline_resources&, std::size_t first, std::span<const graphics::descriptor_set *const>
		);
		/// Calls \p vk::CommandBuffer::bindDescriptorSets().
		void bind_compute_descriptor_sets(
			const pipeline_resources&, std::size_t first, std::span<const graphics::descriptor_set *const>
		);

		/// Calls \p vk::CommandBuffer::setViewport().
		void set_viewports(std::span<const viewport>);
		/// Calls \p vk::CommandBuffer::setScissor().
		void set_scissor_rectangles(std::span<const aab2i>);

		/// Calls \p vk::CommandBuffer::copyBuffer().
		void copy_buffer(buffer &from, std::size_t off1, buffer &to, std::size_t off2, std::size_t size);
		/// Calls \p vk::CommandBuffer::copyImage().
		void copy_image2d(
			image2d &from, subresource_index sub1, aab2s region, image2d &to, subresource_index sub2, cvec2s off
		);
		/// Calls \p vk::CommandBuffer::copyBufferToImage().
		void copy_buffer_to_image(
			buffer &from, std::size_t byte_offset, std::size_t row_pitch, aab2s region,
			image2d &to, subresource_index subresource, cvec2s off
		);

		/// Calls \p vk::CommandBuffer::draw().
		void draw_instanced(
			std::size_t first_vertex, std::size_t vertex_count,
			std::size_t first_instance, std::size_t instance_count
		);
		/// Calls \p vk::CommandBuffer::drawIndexed().
		void draw_indexed_instanced(
			std::size_t first_index, std::size_t index_count,
			std::size_t first_vertex,
			std::size_t first_instance, std::size_t instance_count
		);
		/// Calls \p vk::CommandBuffer::dispatch().
		void run_compute_shader(std::uint32_t x, std::uint32_t y, std::uint32_t z);

		/// Calls \p vk::CommandBuffer::pipelineBarrier().
		void resource_barrier(std::span<const image_barrier>, std::span<const buffer_barrier>);

		/// Calls \p vk::CommandBuffer::endRenderPass().
		void end_pass();
		/// Calls \p vk::CommandBuffer::end().
		void finish();
	private:
		vk::UniqueCommandBuffer _buffer; ///< The command buffer.
	};

	/// Contains a \p vk::Queue.
	class command_queue {
		friend device;
	protected:
		/// Calls \p vk::Queue::submit().
		void submit_command_lists(std::span<const graphics::command_list *const>, fence *on_completion);
		/// Calls \p vk::Queue::presentKHR().
		void present(swap_chain&);
		/// Calls \p vk::Queue::submit() without any command lists.
		void signal(fence&);
	private:
		vk::Queue _queue; ///< The queue.
	};
}
