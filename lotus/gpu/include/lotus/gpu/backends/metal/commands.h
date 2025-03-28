#pragma once

/// \file
/// Metal command buffers.

#include "lotus/color.h"
#include "lotus/gpu/common.h"
#include "details.h"
#include "acceleration_structure.h"
#include "frame_buffer.h"
#include "pipeline.h"
#include "resources.h"
#include "synchronization.h"
#include "descriptors.h"

namespace lotus::gpu::backends::metal {
	class adapter;
	class command_list;
	class command_queue;
	class device;

	/// Metal command buffers are specific to command queues - this object holds a weak reference to the
	/// \p MTL::CommandQueue.
	class command_allocator {
		friend command_list;
		friend device;
	protected:
		/// Initializes this object to empty.
		command_allocator(std::nullptr_t) {
		}

		/// Does nothing.
		void reset(device&);
	private:
		MTL::CommandQueue *_q = nullptr; ///< The command queue.

		/// Initializes \ref _q.
		explicit command_allocator(MTL::CommandQueue *q) : _q(q) {
		}
	};

	/// Holds a \p MTL::CommandBuffer.
	class command_list {
		friend command_queue;
		friend device;
	protected:
		/// Initializes this object to empty.
		command_list(std::nullptr_t) {
		}

		/// Replaces \ref _buf with a new command list allocated using \p MTL::CommandQueue::commandBuffer().
		void reset_and_start(command_allocator&);

		/// Creates a new \p MTL::RenderPassDescriptor that corresponds to the given input and creates a
		/// \p MTL::RenderCommandEncoder with it.
		void begin_pass(const frame_buffer&, const frame_buffer_access&);

		/// Binds all graphics pipeline related state to \ref _pass_encoder.
		void bind_pipeline_state(const graphics_pipeline_state&);
		/// Records the given pipeline state object to be bound during dispatches.
		void bind_pipeline_state(const compute_pipeline_state&);
		/// Calls \p MTL::RenderCommandEncoder::setVertexBuffers().
		void bind_vertex_buffers(usize start, std::span<const vertex_buffer>);
		/// Updates \ref _index_buffer, \ref _index_offset_bytes, and \ref _index_format.
		void bind_index_buffer(const buffer&, usize offset_bytes, index_format);
		/// Binds descriptor sets to \ref _pass_encoder.
		void bind_graphics_descriptor_sets(
			const pipeline_resources&, u32 first, std::span<const gpu::descriptor_set *const>
		);
		/// Records the given descriptor sets to be bound during dispatches.
		void bind_compute_descriptor_sets(
			const pipeline_resources&, u32 first, std::span<const gpu::descriptor_set *const>
		);

		/// Calls \p MTL::RenderCommandEncoder::setViewports().
		void set_viewports(std::span<const viewport>);
		/// Calls \p MTL::RenderCommandEncoder::setScissorRects().
		void set_scissor_rectangles(std::span<const aab2u32>);

		/// Creates a \p MTL::BlitCommandEncoder to encode a copy command.
		void copy_buffer(const buffer &from, usize off1, buffer &to, usize off2, usize size);
		/// Creates a \p MTL::BlitCommandEncoder to encode a copy command.
		void copy_image2d(
			image2d &from, subresource_index sub1, aab2u32 region, image2d &to, subresource_index sub2, cvec2u32 off
		);
		/// Creates a \p MTL::BlitCommandEncoder to encode a copy command.
		void copy_buffer_to_image(
			const buffer &from,
			usize byte_offset,
			staging_buffer_metadata,
			image2d &to,
			subresource_index subresource,
			cvec2u32 off
		);

		/// Calls \p MTL::RenderCommandEncoder::drawPrimitives().
		void draw_instanced(u32 first_vertex, u32 vertex_count, u32 first_instance, u32 instance_count);
		/// Calls \p MTL::RenderCommandEncoder::drawIndexedPrimitives().
		void draw_indexed_instanced(
			u32 first_index, u32 index_count, i32 first_vertex, u32 first_instance, u32 instance_count
		);
		/// Creates a new \p MTL::ComputeCommandEncoder, binds resources, and calls \p dispatchThreadgroups().
		void run_compute_shader(u32 x, u32 y, u32 z);

		void resource_barrier(std::span<const image_barrier>, std::span<const buffer_barrier>); // TODO

		/// Calls \p MTL::RenderCommandEncoder::endEncoding() and releases the encoder.
		void end_pass();

		void query_timestamp(timestamp_query_heap&, u32 index); // TODO
		/// Does nothing. Metal supports resolving these queries either on the GPU timeline or on the CPU timeline;
		/// we opt to do the latter.
		void resolve_queries(timestamp_query_heap&, u32 first, u32 count);

		void insert_marker(const char8_t*, linear_rgba_u8 color); // TODO
		void begin_marker_scope(const char8_t*, linear_rgba_u8 color); // TODO
		void end_marker_scope(); // TODO

		/// Does nothing.
		void finish();


		// ray-tracing related
		/// Creates a \p MTL::AccelerationStructureCommandEncoder and calls \p buildAccelerationStructure().
		void build_acceleration_structure(
			const bottom_level_acceleration_structure_geometry&,
			bottom_level_acceleration_structure &output,
			buffer &scratch,
			usize scratch_offset
		);
		/// Creates a \p MTL::IndirectAccelerationStructureInstanceDescriptor and builds an acceleration structure
		/// with it.
		void build_acceleration_structure(
			const buffer &instances,
			usize offset,
			usize count,
			top_level_acceleration_structure &output,
			buffer &scratch,
			usize scratch_offset
		);

		void bind_pipeline_state(const raytracing_pipeline_state&); // TODO
		/// Records the given descriptor sets to be bound during ray tracingdispatches.
		void bind_ray_tracing_descriptor_sets(
			const pipeline_resources&, u32 first, std::span<const gpu::descriptor_set *const>
		);
		void trace_rays(constant_buffer_view ray_generation, shader_record_view miss_shaders, shader_record_view hit_groups, usize width, usize height, usize depth); // TODO


		/// Checks whether this object is valid.
		[[nodiscard]] bool is_valid() const {
			return !!_buf;
		}
	private:
		NS::SharedPtr<MTL::CommandBuffer> _buf; ///< The command buffer.

		NS::SharedPtr<MTL::RenderCommandEncoder> _pass_encoder; ///< Encoder for the render pass.
		MTL::Buffer *_index_buffer = nullptr; ///< Currently bound index buffer.
		usize _index_offset_bytes = 0; ///< Currently bound index buffer offset.
		index_format _index_format = index_format::num_enumerators; ///< Currently bound index buffer format.
		/// Primitive topology of the last bound graphics pipeline.
		primitive_topology _topology = primitive_topology::num_enumerators;
		std::vector<u64> _graphics_sets; ///< Currently bound graphics descriptor sets.
		/// Whether the latest version of \ref _graphics_sets is bound to the active command encoder.
		bool _graphics_sets_bound = false;

		NS::SharedPtr<MTL::ComputePipelineState> _compute_pipeline; ///< Currently bound compute pipeline state.
		cvec3u32 _compute_thread_group_size = zero; ///< Thread group size of the currently bound compute pipeline.
		std::vector<u64> _compute_sets; ///< Currently bound compute descriptor sets.

		/// Creates an argument buffer for the given set of descriptor tables.
		void _update_descriptor_set_bindings(
			std::vector<u64> &bindings,
			u32 first,
			std::span<const gpu::descriptor_set *const> sets
		);
		/// Refreshes graphics descriptor bindings to \ref _pass_encoder if necessary.
		void _maybe_refresh_graphics_descriptor_set_bindings();

		/// Initializes \ref _buf and \ref _mapping.
		explicit command_list(NS::SharedPtr<MTL::CommandBuffer> buf) : _buf(std::move(buf)) {
		}
	};

	/// Holds a \p MTL::CommandQueue.
	class command_queue {
		friend adapter;
		friend device;
	protected:
		/// Initializes this object to empty.
		command_queue(std::nullptr_t) {
		}

		[[nodiscard]] double get_timestamp_frequency(); // TODO

		/// Creates command lists to handle synchronization, and calls \p MTL::CommandBuffer::commit() to submit all
		/// the command lists.
		void submit_command_lists(std::span<const gpu::command_list *const>, queue_synchronization);
		/// Creates a new command list, calls \p MTL::CommandBuffer::presentDrawable(), then submits it.
		[[nodiscard]] swap_chain_status present(swap_chain&);

		/// Creates a new command buffer and signals the given fence.
		void signal(fence&);
		/// Creates a new command buffer and signals the given semaphore.
		void signal(timeline_semaphore&, gpu::_details::timeline_semaphore_value_type);

		/// All Metal command queues support timestamp queries.
		[[nodiscard]] queue_capabilities get_capabilities() const;

		/// Checks if this object is valid.
		[[nodiscard]] bool is_valid() const {
			return !!_q;
		}
	private:
		NS::SharedPtr<MTL::CommandQueue> _q; ///< The command queue.

		/// Initializes \ref _q.
		explicit command_queue(NS::SharedPtr<MTL::CommandQueue> q) : _q(std::move(q)) {
		}
	};
}
