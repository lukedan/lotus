#pragma once

/// \file
/// Command pools and command lists.

#include "lotus/color.h"
#include "details.h"
#include "acceleration_structure.h"
#include "pipeline.h"
#include "frame_buffer.h"

namespace lotus::gpu::backends::vulkan {
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
	public:
		/// Destructor.
		~command_list();
	protected:
		/// Creates an empty object.
		command_list(std::nullptr_t) {
		}
		/// Move construction.
		command_list(command_list &&src) :
			_buffer(std::exchange(src._buffer, nullptr)),
			_pool(std::exchange(src._pool, nullptr)),
			_device(std::exchange(src._device, nullptr)) {
		}
		/// No copy construction.
		command_list(const command_list&) = delete;
		/// Move assignment.
		command_list &operator=(command_list&&);
		/// No copy assignment.
		command_list &operator=(const command_list&) = delete;

		/// Calls \p vk::CommandBuffer::reset() and \p vk::CommandBuffer::begin().
		void reset_and_start(command_allocator&);

		/// Calls \p vk::CommandBuffer::beginRendering().
		void begin_pass(const frame_buffer&, const frame_buffer_access&);

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
			const pipeline_resources&, std::size_t first, std::span<const gpu::descriptor_set *const>
		);
		/// Calls \p vk::CommandBuffer::bindDescriptorSets().
		void bind_compute_descriptor_sets(
			const pipeline_resources&, std::size_t first, std::span<const gpu::descriptor_set *const>
		);

		/// Calls \p vk::CommandBuffer::setViewport().
		void set_viewports(std::span<const viewport>);
		/// Calls \p vk::CommandBuffer::setScissor().
		void set_scissor_rectangles(std::span<const aab2i>);

		/// Calls \p vk::CommandBuffer::copyBuffer().
		void copy_buffer(const buffer &from, std::size_t off1, buffer &to, std::size_t off2, std::size_t size);
		/// Calls \p vk::CommandBuffer::copyImage().
		void copy_image2d(
			image2d &from, subresource_index sub1, aab2s region, image2d &to, subresource_index sub2, cvec2s off
		);
		/// Calls \p vk::CommandBuffer::copyBufferToImage().
		void copy_buffer_to_image(
			const buffer &from, std::size_t byte_offset, staging_buffer_metadata,
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

		/// Calls \p vk::CommandBuffer::endRendering().
		void end_pass();

		/// Calls \p vk::CommandBuffer::writeTimestamp().
		void query_timestamp(timestamp_query_heap&, std::uint32_t index);
		/// No-op.
		void resolve_queries(timestamp_query_heap&, std::uint32_t, std::uint32_t) {
		}

		/// Calls \p vk::CommandBuffer::debugMarkerInsertEXT().
		void insert_marker(const char8_t*, linear_rgba_u8);
		/// Calls \p vk::CommandBuffer::debugMarkerBeginEXT().
		void begin_marker_scope(const char8_t*, linear_rgba_u8);
		/// Calls \p vk::CommandBuffer::debugMarkerEndEXT().
		void end_marker_scope();

		/// Calls \p vk::CommandBuffer::end().
		void finish();


		// ray-tracing related
		/// Calls \p vk::CommandBuffer::buildAccelerationStructuresKHR().
		void build_acceleration_structure(
			const bottom_level_acceleration_structure_geometry &geom,
			bottom_level_acceleration_structure &output, buffer &scratch, std::size_t scratch_offset
		);
		/// Calls \p vk::CommandBuffer::buildAccelerationStructuresKHR().
		void build_acceleration_structure(
			const buffer &instances, std::size_t offset, std::size_t count,
			top_level_acceleration_structure &output, buffer &scratch, std::size_t scratch_offset
		);

		/// Calls \p vk::CommandBuffer::bindPipeline().
		void bind_pipeline_state(const raytracing_pipeline_state&);
		/// Calls \p vk::CommandBuffer::bindDescriptorSets().
		void bind_ray_tracing_descriptor_sets(
			const pipeline_resources&, std::size_t first, std::span<const gpu::descriptor_set *const>
		);
		/// Calls \p vk::CommandBuffer::traceRaysKHR().
		void trace_rays(
			constant_buffer_view ray_generation,
			shader_record_view miss_shaders, shader_record_view hit_groups,
			std::size_t width, std::size_t height, std::size_t depth
		);

		/// Checks that this object holds a valid command list.
		[[nodiscard]] bool is_valid() const {
			return !!_buffer;
		}
	private:
		vk::CommandBuffer _buffer; ///< The command buffer.
		vk::CommandPool _pool; ///< The command pool that this buffer is allocated from.
		device *_device; ///< The device that created this command buffer.
	};

	/// Contains a \p vk::Queue.
	class command_queue {
		friend device;
	protected:
		/// Returns \ref _timestamp_frequency.
		[[nodiscard]] double get_timestamp_frequency() {
			return _timestamp_frequency;
		}

		/// Calls \p vk::Queue::submit().
		void submit_command_lists(std::span<const gpu::command_list *const>, queue_synchronization synch);
		/// Calls \p vk::Queue::presentKHR().
		[[nodiscard]] swap_chain_status present(swap_chain&);

		/// Calls \p vk::Queue::submit() without any command lists.
		void signal(fence&);
		/// Calls \p vk::Queue::submit() without any command lists.
		void signal(timeline_semaphore&, std::uint64_t);
	private:
		vk::Queue _queue; ///< The queue.
		double _timestamp_frequency = 0.0f; ///< Timestamp frequency.
	};
}
