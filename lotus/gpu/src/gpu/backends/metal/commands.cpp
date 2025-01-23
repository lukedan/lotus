#include "lotus/gpu/backends/metal/commands.h"

/// \file
/// Implementation of Metal command buffers.

#include <Metal/Metal.hpp>

#include "lotus/gpu/commands.h"
#include "lotus/gpu/descriptors.h"
#include "lotus/gpu/synchronization.h"
#include "metal_irconverter_include.h"

namespace lotus::gpu::backends::metal {
	void command_allocator::reset(device&) {
	}


	void command_list::reset_and_start(command_allocator &alloc) {
		_buf = NS::RetainPtr(alloc._q->commandBuffer());
	}

	void command_list::begin_pass(const frame_buffer &fb, const frame_buffer_access &access) {
		crash_if(fb._color_rts.size() != access.color_render_targets.size());

		auto descriptor = NS::TransferPtr(MTL::RenderPassDescriptor::alloc()->init());
		// color render targets
		for (std::size_t i = 0; i < fb._color_rts.size(); ++i) {
			const color_render_target_access &rt_access = access.color_render_targets[i];

			MTL::RenderPassColorAttachmentDescriptor *desc = descriptor->colorAttachments()->object(i);
			desc->setTexture(fb._color_rts[i]);
			std::visit(
				[&](const auto &v) {
					desc->setClearColor(MTL::ClearColor(v[0], v[1], v[2], v[3]));
				},
				rt_access.clear_value.value
			);
			desc->setLevel(0); // TODO
			desc->setSlice(0); // TODO
			desc->setLoadAction(_details::conversions::to_load_action(rt_access.load_operation));
			desc->setStoreAction(_details::conversions::to_store_action(rt_access.store_operation));
		}
		if (fb._depth_stencil_rt) {
			const depth_render_target_access &depth_access = access.depth_render_target;
			const stencil_render_target_access &stencil_access = access.stencil_render_target;
			if (
				depth_access.load_operation != pass_load_operation::discard ||
				depth_access.store_operation != pass_store_operation::discard
			) {
				MTL::RenderPassDepthAttachmentDescriptor *desc = descriptor->depthAttachment();
				desc->setTexture(fb._depth_stencil_rt);
				desc->setClearDepth(depth_access.clear_value);
				desc->setLevel(0); // TODO
				desc->setSlice(0); // TODO
				desc->setLoadAction(_details::conversions::to_load_action(depth_access.load_operation));
				desc->setStoreAction(_details::conversions::to_store_action(depth_access.store_operation));
			}
			if (
				stencil_access.load_operation != pass_load_operation::discard ||
				stencil_access.store_operation != pass_store_operation::discard
			) {
				MTL::RenderPassStencilAttachmentDescriptor *desc = descriptor->stencilAttachment();
				desc->setTexture(fb._depth_stencil_rt);
				desc->setClearStencil(stencil_access.clear_value);
				desc->setLevel(0); // TODO
				desc->setSlice(0); // TODO
				desc->setLoadAction(_details::conversions::to_load_action(stencil_access.load_operation));
				desc->setStoreAction(_details::conversions::to_store_action(stencil_access.store_operation));
			}
		}
		descriptor->setRenderTargetWidth(fb._size[0]);
		descriptor->setRenderTargetHeight(fb._size[1]);

		_pass_encoder = NS::RetainPtr(_buf->renderCommandEncoder(descriptor.get()));
	}

	void command_list::bind_pipeline_state(const graphics_pipeline_state &s) {
		_pass_encoder->setRenderPipelineState(s._pipeline.get());
		_pass_encoder->setDepthStencilState(s._ds_state.get());
		// set rasterizer state
		_pass_encoder->setTriangleFillMode(
			s._rasterizer_options.is_wireframe ? MTL::TriangleFillModeLines : MTL::TriangleFillModeFill
		);
		_pass_encoder->setFrontFacingWinding(_details::conversions::to_winding(s._rasterizer_options.front_facing));
		_pass_encoder->setCullMode(_details::conversions::to_cull_mode(s._rasterizer_options.culling));
		_pass_encoder->setDepthBias(
			s._rasterizer_options.depth_bias.bias,
			s._rasterizer_options.depth_bias.slope_scaled_bias,
			s._rasterizer_options.depth_bias.clamp
		);
		// set topology
		_topology = s._topology;
	}

	void command_list::bind_pipeline_state(const compute_pipeline_state &s) {
		_compute_pipeline          = s._pipeline;
		_compute_thread_group_size = s._thread_group_size;
	}

	void command_list::bind_vertex_buffers(std::size_t start, std::span<const vertex_buffer> buffers) {
		if constexpr (true) {
			for (std::size_t i = 0; i < buffers.size(); ++i) {
				// TODO stride not supported?
				_pass_encoder->setVertexBuffer(
					buffers[i].data->_buf.get(),
					buffers[i].offset,
					//buffers[i].stride,
					kIRVertexBufferBindPoint + start + i
				);
			}
		} else {
			auto bookmark = get_scratch_bookmark();
			auto ptrs    = bookmark.create_vector_array<const MTL::Buffer*>(buffers.size());
			auto offsets = bookmark.create_vector_array<NS::UInteger>(buffers.size());
			auto strides = bookmark.create_vector_array<NS::UInteger>(buffers.size());
			for (std::size_t i = 0; i < buffers.size(); ++i) {
				ptrs[i]    = buffers[i].data->_buf.get();
				offsets[i] = buffers[i].offset;
				strides[i] = buffers[i].stride;
			}
			// TODO this does not work, but there's no documentation
			_pass_encoder->setVertexBuffers(
				ptrs.data(),
				offsets.data(),
				strides.data(),
				NS::Range(kIRVertexBufferBindPoint + start, buffers.size())
			);
		}
	}

	void command_list::bind_index_buffer(const buffer &buf, std::size_t offset_bytes, index_format fmt) {
		_index_buffer       = buf._buf.get();
		_index_offset_bytes = offset_bytes;
		_index_format       = fmt;
	}

	void command_list::bind_graphics_descriptor_sets(
		const pipeline_resources&,
		std::size_t first,
		std::span<const gpu::descriptor_set *const> sets
	) {
		auto bookmark = get_scratch_bookmark();
		auto bindings = _create_argument_buffer(bookmark, first, sets);
		const std::size_t bindings_bytes = bindings.size() * sizeof(std::uint64_t);
		_pass_encoder->setVertexBytes(bindings.data(), bindings_bytes, kIRArgumentBufferBindPoint);
		_pass_encoder->setFragmentBytes(bindings.data(), bindings_bytes, kIRArgumentBufferBindPoint);
	}

	void command_list::bind_compute_descriptor_sets(
		const pipeline_resources&, std::size_t first, std::span<const gpu::descriptor_set *const> sets
	) {
		_compute_first_descriptor_set = first;
		_compute_sets.clear();
		_compute_sets.insert(_compute_sets.end(), sets.begin(), sets.end());
	}

	void command_list::set_viewports(std::span<const viewport>) {
		// TODO
	}

	void command_list::set_scissor_rectangles(std::span<const aab2i>) {
		// TODO
	}

	void command_list::copy_buffer(
		const buffer &from, std::size_t off1, buffer &to, std::size_t off2, std::size_t size
	) {
		auto encoder = NS::RetainPtr(_buf->blitCommandEncoder());
		encoder->copyFromBuffer(from._buf.get(), off1, to._buf.get(), off2, size);
		encoder->endEncoding();
	}

	void command_list::copy_image2d(
		image2d &from, subresource_index sub1, aab2u32 region, image2d &to, subresource_index sub2, cvec2u32 off
	) {
		auto encoder = NS::RetainPtr(_buf->blitCommandEncoder());
		encoder->copyFromTexture(
			from._tex.get(),
			sub1.array_slice,
			sub1.mip_level,
			MTL::Origin(region.min[0], region.min[1], 0),
			MTL::Size(region.signed_size()[0], region.signed_size()[1], 1),
			to._tex.get(),
			sub2.array_slice,
			sub2.mip_level,
			MTL::Origin(off[0], off[1], 0)
		);
		encoder->endEncoding();
	}

	void command_list::copy_buffer_to_image(
		const buffer &from,
		std::size_t byte_offset,
		staging_buffer_metadata metadata,
		image2d &to,
		subresource_index subresource,
		cvec2u32 off
	) {
		auto encoder = NS::RetainPtr(_buf->blitCommandEncoder());
		encoder->copyFromBuffer(
			from._buf.get(),
			byte_offset,
			metadata.row_pitch_in_bytes,
			0,
			MTL::Size(metadata.image_size[0], metadata.image_size[1], 1),
			to._tex.get(),
			subresource.array_slice,
			subresource.mip_level,
			MTL::Origin(off[0], off[1], 0)
		);
		encoder->endEncoding();
	}

	void command_list::draw_instanced(
		std::size_t first_vertex, std::size_t vertex_count, std::size_t first_instance, std::size_t instance_count
	) {
		// this function is used instead of the member function to bind additional auxiliary buffers for HLSL
		// interoperability; same for draw_indexed_instanced()
		IRRuntimeDrawPrimitives(
			_pass_encoder.get(),
			_details::conversions::to_primitive_type(_topology),
			first_vertex,
			vertex_count,
			instance_count,
			first_instance
		);
	}

	void command_list::draw_indexed_instanced(
		std::size_t first_index,
		std::size_t index_count,
		std::size_t first_vertex,
		std::size_t first_instance,
		std::size_t instance_count
	) {
		crash_if(_index_offset_bytes % 4 != 0); // Metal does not support non-multiple-of-4 offsets
		IRRuntimeDrawIndexedPrimitives(
			_pass_encoder.get(),
			_details::conversions::to_primitive_type(_topology),
			index_count,
			_details::conversions::to_index_type(_index_format),
			_index_buffer,
			_index_offset_bytes,
			instance_count,
			first_vertex,
			first_instance
		);
	}

	void command_list::run_compute_shader(std::uint32_t x, std::uint32_t y, std::uint32_t z) {
		auto bookmark = get_scratch_bookmark();
		auto data = _create_argument_buffer(bookmark, _compute_first_descriptor_set, _compute_sets);
		std::size_t data_size = data.size() * sizeof(std::uint64_t);

		auto encoder = NS::RetainPtr(_buf->computeCommandEncoder());
		encoder->setComputePipelineState(_compute_pipeline.get());
		// bind resources
		encoder->setBytes(data.data(), data_size, kIRArgumentBufferBindPoint);
		encoder->dispatchThreadgroups(
			MTL::Size(x, y, z),
			_details::conversions::to_size(_compute_thread_group_size.into<NS::UInteger>())
		);
		encoder->endEncoding();
	}

	void command_list::resource_barrier(std::span<const image_barrier>, std::span<const buffer_barrier>) {
		auto event = NS::TransferPtr(_buf->device()->newEvent());
		_buf->encodeSignalEvent(event.get(), 1);
		_buf->encodeWait(event.get(), 1);
		// TODO
	}

	void command_list::end_pass() {
		_pass_encoder->endEncoding();
		_pass_encoder = nullptr;
	}

	void command_list::query_timestamp(timestamp_query_heap&, std::uint32_t index) {
		// TODO
	}

	void command_list::resolve_queries(timestamp_query_heap&, std::uint32_t first, std::uint32_t count) {
		// TODO
	}

	void command_list::insert_marker(const char8_t*, linear_rgba_u8) {
		// TODO
	}

	void command_list::begin_marker_scope(const char8_t*, linear_rgba_u8) {
		// TODO
	}

	void command_list::end_marker_scope() {
		// TODO
	}

	void command_list::finish() {
	}

	void command_list::build_acceleration_structure(
		const bottom_level_acceleration_structure_geometry &geom,
		bottom_level_acceleration_structure &output,
		buffer &scratch,
		std::size_t scratch_offset
	) {
		auto encoder = NS::RetainPtr(_buf->accelerationStructureCommandEncoder());
		encoder->buildAccelerationStructure(
			output._as.get(), geom._descriptor.get(), scratch._buf.get(), scratch_offset
		);
		encoder->endEncoding();
	}

	void command_list::build_acceleration_structure(
		const buffer &instances,
		std::size_t offset,
		std::size_t count,
		top_level_acceleration_structure &output,
		buffer &scratch,
		std::size_t scratch_offset
	) {
		using _count_type = NS::UInteger;

		// this buffer is not added to the residency set; manually calling useResource() later
		auto count_buffer = NS::TransferPtr(_buf->device()->newBuffer(sizeof(_count_type), 0));
		*static_cast<_count_type*>(count_buffer->contents()) = count;
		count_buffer->setLabel(NS::String::string("Build TLAS Count Buffer", NS::UTF8StringEncoding));

		auto desc = NS::TransferPtr(MTL::IndirectInstanceAccelerationStructureDescriptor::alloc()->init());
		desc->setInstanceCountBuffer(count_buffer.get());
		desc->setInstanceCountBufferOffset(0);
		desc->setInstanceDescriptorBuffer(instances._buf.get());
		desc->setInstanceDescriptorBufferOffset(offset);
		desc->setInstanceDescriptorStride(sizeof(instance_description));
		desc->setInstanceDescriptorType(MTL::AccelerationStructureInstanceDescriptorTypeIndirect);
		desc->setMaxInstanceCount(count);

		// update the instance id buffer
		// TODO this needs to be a compute dispatch
		const auto *header = static_cast<IRRaytracingAccelerationStructureGPUHeader*>(output._header->contents());
		auto *instance_ids = reinterpret_cast<std::uint32_t*>(header->addressOfInstanceContributions);
		const auto *instance_descriptors = static_cast<instance_description*>(instances._buf->contents());
		for (std::size_t i = 0; i < count; ++i) {
			instance_ids[i] = instance_descriptors[i]._descriptor.userID;
		}

		auto encoder = NS::RetainPtr(_buf->accelerationStructureCommandEncoder());
		encoder->useResource(count_buffer.get(), MTL::ResourceUsageRead);
		encoder->buildAccelerationStructure(
			output._as.get(), desc.get(), scratch._buf.get(), scratch_offset
		);
		encoder->endEncoding();
	}

	void command_list::bind_pipeline_state(const raytracing_pipeline_state&) {
		// TODO
	}

	void command_list::bind_ray_tracing_descriptor_sets(const pipeline_resources&, std::size_t first, std::span<const gpu::descriptor_set *const>) {
		// TODO
	}

	void command_list::trace_rays(
		constant_buffer_view ray_generation,
		shader_record_view miss_shaders,
		shader_record_view hit_groups,
		std::size_t width,
		std::size_t height,
		std::size_t depth
	) {
		// TODO
	}

	memory::stack_allocator::vector_type<std::uint64_t> command_list::_create_argument_buffer(
		memory::stack_allocator::scoped_bookmark &bookmark,
		std::uint32_t first,
		std::span<const gpu::descriptor_set *const> sets
	) {
		auto bindings = bookmark.create_vector_array<std::uint64_t>(first + sets.size());
		for (std::size_t i = 0; i < sets.size(); ++i) {
			bindings[first + i] = sets[i]->_arg_buffer->gpuAddress();
		}
		return bindings;
	}


	double command_queue::get_timestamp_frequency() {
		// TODO
	}

	void command_queue::submit_command_lists(
		std::span<const gpu::command_list *const> cmd_lists, queue_synchronization sync
	) {
		if (!sync.wait_semaphores.empty()) {
			// use a command buffer to submit the waits
			auto cmd_buf = NS::RetainPtr(_q->commandBuffer());
			for (const timeline_semaphore_synchronization &sem : sync.wait_semaphores) {
				cmd_buf->encodeWait(sem.semaphore->_event.get(), sem.value);
			}
			cmd_buf->commit();
		}
		for (const gpu::command_list *list : cmd_lists) {
			list->_buf->commit();
		}
		if (sync.notify_fence) {
			std::abort(); // TODO
		}
		if (!sync.notify_semaphores.empty()) {
			// use a command buffer to submit the signals
			auto cmd_buf = NS::RetainPtr(_q->commandBuffer());
			for (const timeline_semaphore_synchronization &sem : sync.notify_semaphores) {
				cmd_buf->encodeSignalEvent(sem.semaphore->_event.get(), sem.value);
			}
			cmd_buf->commit();
		}
	}

	swap_chain_status command_queue::present(swap_chain &chain) {
		auto cmd_buf = NS::RetainPtr(_q->commandBuffer());
		cmd_buf->presentDrawable(chain._drawable.get());
		cmd_buf->commit();
		return swap_chain_status::ok;
	}

	void command_queue::signal(fence&) {
		// TODO
	}

	void command_queue::signal(timeline_semaphore &sem, gpu::_details::timeline_semaphore_value_type val) {
		auto cmd_buf = NS::RetainPtr(_q->commandBuffer());
		cmd_buf->encodeSignalEvent(sem._event.get(), val);
		cmd_buf->commit();
	}

	queue_capabilities command_queue::get_capabilities() const {
		return queue_capabilities::timestamp_query;
	}
}
