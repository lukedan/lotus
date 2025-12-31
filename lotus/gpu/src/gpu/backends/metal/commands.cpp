#include "lotus/gpu/backends/metal/commands.h"

/// \file
/// Implementation of Metal command buffers.

#include <numeric>

#include "lotus/gpu/commands.h"
#include "lotus/gpu/descriptors.h"
#include "lotus/gpu/synchronization.h"

namespace lotus::gpu::backends::metal {
	void command_allocator::reset(device&) {
	}


	void command_list::begin_pass(const frame_buffer &fb, const frame_buffer_access &access) {
		crash_if(fb._color_rts.size() != access.color_render_targets.size());

		auto descriptor = NS::TransferPtr(MTL4::RenderPassDescriptor::alloc()->init());
		// color render targets
		for (usize i = 0; i < fb._color_rts.size(); ++i) {
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
			const MTL::PixelFormat ds_fmt = fb._depth_stencil_rt->pixelFormat();
			// these conditions are matched to the ones in create_graphics_pipeline_state() exactly,
			// or we get validation warnings
			if (_details::does_pixel_format_have_depth(ds_fmt)) {
				MTL::RenderPassDepthAttachmentDescriptor *desc = descriptor->depthAttachment();
				desc->setTexture(fb._depth_stencil_rt);
				desc->setClearDepth(depth_access.clear_value);
				desc->setLevel(0); // TODO
				desc->setSlice(0); // TODO
				desc->setLoadAction(_details::conversions::to_load_action(depth_access.load_operation));
				desc->setStoreAction(_details::conversions::to_store_action(depth_access.store_operation));
			}
			if (_details::does_pixel_format_have_stencil(ds_fmt)) {
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

		constexpr static MTL::Stages _graphics_stages =
			MTL::StageVertex   |
			MTL::StageFragment |
			MTL::StageTile     |
			MTL::StageObject   |
			MTL::StageMesh;

		_pass_encoder = _buf->renderCommandEncoder(descriptor.get());
		_pass_encoder->setArgumentTable(_get_graphics_argument_table(), _graphics_stages);
		if (_pending_graphics_barriers != 0) {
			_pass_encoder->barrierAfterQueueStages(
				_pending_graphics_barriers, _graphics_stages, MTL4::VisibilityOptionResourceAlias
			);
			_pending_graphics_barriers = 0;
		}
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

	void command_list::bind_vertex_buffers(usize start, std::span<const vertex_buffer> buffers) {
		MTL4::ArgumentTable *arg_table = _get_graphics_argument_table();
		for (usize i = 0; i < buffers.size(); ++i) {
			const vertex_buffer &buffer = buffers[i];
			arg_table->setAddress(
				buffer.data->_buf->gpuAddress() + buffer.offset,
				buffer.stride,
				kIRVertexBufferBindPoint + start + i
			);
		}
	}

	void command_list::bind_index_buffer(const buffer &buf, usize offset_bytes, index_format fmt) {
		_index_addr   = buf._buf->gpuAddress() + offset_bytes;
		_index_format = fmt;
	}

	void command_list::bind_graphics_descriptor_sets(
		const pipeline_resources&, u32 first, std::span<const gpu::descriptor_set *const> sets
	) {
		_update_descriptor_set_bindings(_graphics_sets, first, sets);
		_graphics_sets_bound = false;
	}

	void command_list::bind_compute_descriptor_sets(
		const pipeline_resources&, u32 first, std::span<const gpu::descriptor_set *const> sets
	) {
		_update_descriptor_set_bindings(_compute_sets, first, sets);
		_compute_sets_bound = false;
	}

	void command_list::set_viewports(std::span<const viewport> vps) {
		auto bookmark = get_scratch_bookmark();

		auto viewports = bookmark.create_reserved_vector_array<MTL::Viewport>(vps.size());
		for (const viewport &vp : vps) {
			viewports.emplace_back(_details::conversions::to_viewport(vp));
		}
		_pass_encoder->setViewports(viewports.data(), viewports.size());
	}

	void command_list::set_scissor_rectangles(std::span<const aab2u32> rects) {
		auto bookmark = get_scratch_bookmark();

		auto scissor_rects = bookmark.create_reserved_vector_array<MTL::ScissorRect>(rects.size());
		for (const aab2u32 &r : rects) {
			scissor_rects.emplace_back(_details::conversions::to_scissor_rect(r));
		}
		_pass_encoder->setScissorRects(scissor_rects.data(), scissor_rects.size());
	}

	void command_list::copy_buffer(const buffer &from, usize off1, buffer &to, usize off2, usize size) {
		_scoped_compute_encoder encoder = _start_compute_pass();
		encoder->copyFromBuffer(from._buf.get(), off1, to._buf.get(), off2, size);
	}

	void command_list::copy_image2d(
		image2d &from, subresource_index sub1, aab2u32 region, image2d &to, subresource_index sub2, cvec2u32 off
	) {
		_scoped_compute_encoder encoder = _start_compute_pass();
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
	}

	void command_list::copy_buffer_to_image(
		const buffer &from,
		usize byte_offset,
		staging_buffer_metadata metadata,
		image2d &to,
		subresource_index subresource,
		cvec2u32 off
	) {
		_scoped_compute_encoder encoder = _start_compute_pass();
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
	}

	void command_list::draw_instanced(u32 first_vertex, u32 vertex_count, u32 first_instance, u32 instance_count) {
		_maybe_refresh_graphics_descriptor_set_bindings();
		// this function is used instead of the member function to bind additional auxiliary buffers for HLSL
		// interoperability; same for draw_indexed_instanced()
		/*IRRuntimeDrawPrimitives(
			_pass_encoder.get(),
			_details::conversions::to_primitive_type(_topology),
			first_vertex,
			vertex_count,
			instance_count,
			first_instance
		);*/
		// TODO: vertexid/instanceid related
		_pass_encoder->drawPrimitives(
			_details::conversions::to_primitive_type(_topology),
			first_vertex,
			vertex_count,
			instance_count,
			first_instance
		);
	}

	void command_list::draw_indexed_instanced(
		u32 first_index, u32 index_count, i32 first_vertex, u32 first_instance, u32 instance_count
	) {
		const usize index_offset_bytes = first_index * get_index_format_size(_index_format);
		_maybe_refresh_graphics_descriptor_set_bindings();
		/*IRRuntimeDrawIndexedPrimitives(
			_pass_encoder.get(),
			_details::conversions::to_primitive_type(_topology),
			index_count,
			_details::conversions::to_index_type(_index_format),
			_index_buffer,
			index_offset,
			instance_count,
			first_vertex,
			first_instance
		);*/
		// TODO: vertexid/instanceid related
		_pass_encoder->drawIndexedPrimitives(
			_details::conversions::to_primitive_type(_topology),
			index_count,
			_details::conversions::to_index_type(_index_format),
			_index_addr + index_offset_bytes,
			100000000, // TODO
			instance_count,
			first_vertex,
			first_instance
		);
	}

	void command_list::run_compute_shader(u32 x, u32 y, u32 z) {
		_scoped_compute_encoder encoder = _start_compute_pass();
		encoder->setComputePipelineState(_compute_pipeline.get());
		// bind resources
		encoder->setArgumentTable(_get_compute_argument_table());
		// after the argument table is guaranteed to be initialized
		_maybe_refresh_compute_descriptor_set_bindings();
		encoder->dispatchThreadgroups(
			MTL::Size(x, y, z),
			_details::conversions::to_size(_compute_thread_group_size.into<NS::UInteger>())
		);
	}

	void command_list::resource_barrier(
		std::span<const image_barrier> img_barriers, std::span<const buffer_barrier> buf_barriers
	) {
		// stages that may not have writes from shaders: ResourceState, Blit, AccelerationStructure
		constexpr static MTL::Stages _custom_shader_write_stages =
			MTL::StageVertex   |
			MTL::StageFragment |
			MTL::StageTile     |
			MTL::StageObject   |
			MTL::StageMesh     |
			MTL::StageDispatch |
			MTL::StageMachineLearning;

		MTL::Stages wait_stages = 0;

		for (const image_barrier &img : img_barriers) {
			if (img.from_layout == image_layout::present) {
				_used_swapchain_images.emplace_back(
					static_cast<const _details::basic_image_base*>(img.target)->_tex->gpuResourceID()
				);
			}

			// read-write/write-write barriers
			if (bit_mask::contains_any(img.to_access, image_access_mask::write_bits)) {
				wait_stages |= MTL::StageAll; // not optimal
			} else {
				// write-read barriers
				if (bit_mask::contains<image_access_mask::copy_destination>(img.from_access)) {
					wait_stages |= MTL::StageBlit;
				}
				if (bit_mask::contains_any(
					img.from_access,
					image_access_mask::color_render_target | image_access_mask::depth_stencil_read_write
				)) {
					wait_stages |= MTL::StageFragment;
				}
				if (bit_mask::contains<image_access_mask::shader_write>(img.from_access)) {
					wait_stages |= _custom_shader_write_stages;
				}
			}
		}
		for (const buffer_barrier &buf : buf_barriers) {
			if (wait_stages == MTL::StageAll) {
				break; // no need to check
			}

			// read-write/write-write barriers
			if (bit_mask::contains_any(buf.to_access, buffer_access_mask::write_bits)) {
				wait_stages |= MTL::StageAll;
			} else {
				// write-read barriers
				if (bit_mask::contains<buffer_access_mask::copy_destination>(buf.from_access)) {
					wait_stages |= MTL::StageBlit;
				}
				if (bit_mask::contains<buffer_access_mask::shader_write>(buf.from_access)) {
					wait_stages |= _custom_shader_write_stages;
				}
			}
		}

		_pending_compute_barriers |= wait_stages;
		_pending_graphics_barriers |= wait_stages;
	}

	void command_list::end_pass() {
		_pass_encoder->endEncoding();
		_pass_encoder = nullptr;
	}

	void command_list::query_timestamp(timestamp_query_heap &heap, u32 index) {
		_buf->writeTimestampIntoHeap(heap._heap.get(), index);
	}

	void command_list::resolve_queries(timestamp_query_heap&, u32, u32) {
	}

	void command_list::insert_marker(const char8_t*, linear_rgba_u8) {
		// TODO
	}

	void command_list::begin_marker_scope(const char8_t *desc, linear_rgba_u8) {
		// TODO color
		_buf->pushDebugGroup(_details::conversions::to_string(desc).get());
	}

	void command_list::end_marker_scope() {
		_buf->popDebugGroup();
	}

	void command_list::finish() {
		_buf->endCommandBuffer();
	}

	void command_list::build_acceleration_structure(
		const bottom_level_acceleration_structure_geometry &geom,
		bottom_level_acceleration_structure &output,
		buffer &scratch,
		usize scratch_offset
	) {
		_scoped_compute_encoder encoder = _start_compute_pass();
		encoder->buildAccelerationStructure(
			output._as.get(), geom._descriptor.get(), scratch._buf->gpuAddress() + scratch_offset
		);
	}

	void command_list::build_acceleration_structure(
		const buffer &instances,
		usize offset,
		usize count,
		top_level_acceleration_structure &output,
		buffer &scratch,
		usize scratch_offset
	) {
		using _count_type = NS::UInteger;

		// this buffer is not added to the residency set; manually calling useResource() later
		auto count_buffer = NS::TransferPtr(_buf->device()->newBuffer(sizeof(_count_type), 0));
		*static_cast<_count_type*>(count_buffer->contents()) = count;
		count_buffer->setLabel(NS::String::string("Build TLAS Count Buffer", NS::UTF8StringEncoding));

		auto desc = NS::TransferPtr(MTL4::IndirectInstanceAccelerationStructureDescriptor::alloc()->init());
		desc->setInstanceCountBuffer(count_buffer->gpuAddress());
		desc->setInstanceDescriptorBuffer(instances._buf->gpuAddress() + offset);
		desc->setInstanceDescriptorStride(sizeof(instance_description));
		desc->setInstanceDescriptorType(MTL::AccelerationStructureInstanceDescriptorTypeIndirect);
		desc->setMaxInstanceCount(count);

		// update the instance id buffer
		// TODO this needs to be a compute dispatch
		const auto *header = static_cast<IRRaytracingAccelerationStructureGPUHeader*>(output._header->contents());
		auto *instance_ids = reinterpret_cast<u32*>(header->addressOfInstanceContributions);
		const auto *instance_descriptors = static_cast<instance_description*>(instances._buf->contents());
		for (usize i = 0; i < count; ++i) {
			instance_ids[i] = instance_descriptors[i]._descriptor.userID;
		}

		_scoped_compute_encoder encoder = _start_compute_pass();
		encoder->buildAccelerationStructure(
			output._as.get(), desc.get(), scratch._buf->gpuAddress() + scratch_offset
		);
	}

	void command_list::bind_pipeline_state(const raytracing_pipeline_state&) {
		// TODO
	}

	void command_list::bind_ray_tracing_descriptor_sets(
		const pipeline_resources&, u32 first, std::span<const gpu::descriptor_set *const> sets
	) {
		_update_descriptor_set_bindings(_compute_sets, first, sets);
	}

	void command_list::trace_rays(
		constant_buffer_view ray_generation,
		shader_record_view miss_shaders,
		shader_record_view hit_groups,
		usize width,
		usize height,
		usize depth
	) {
		IRDispatchRaysArgument argument = {};

		auto to_virtual_address_range = [](constant_buffer_view view) {
			IRVirtualAddressRange result;
			result.StartAddress = view.data->_buf->gpuAddress() + view.offset;
			result.SizeInBytes  = view.size;
			return result;
		};
		auto to_virtual_address_range_and_stride = [](shader_record_view view) {
			IRVirtualAddressRangeAndStride result;
			result.StartAddress  = view.data->_buf->gpuAddress() + view.offset;
			result.SizeInBytes   = view.stride * view.count;
			result.StrideInBytes = view.stride;
			return result;
		};

		IRDispatchRaysDescriptor &desc = argument.DispatchRaysDesc;
		desc.RayGenerationShaderRecord = to_virtual_address_range(ray_generation);
		desc.MissShaderTable           = to_virtual_address_range_and_stride(miss_shaders);
		desc.HitGroupTable             = to_virtual_address_range_and_stride(hit_groups);
		desc.Width                     = static_cast<uint>(width);
		desc.Height                    = static_cast<uint>(height);
		desc.Depth                     = static_cast<uint>(depth);

		// TODO GRS
		// TODO ResDescHeap
		// TODO SmpDescHeap
		// TODO

		// compute thread group size
		const auto exec_width = static_cast<usize>(_compute_pipeline->threadExecutionWidth());
		MTL::Size thread_group_size(
			std::min(exec_width, width), std::min(exec_width, height), std::min(exec_width, depth)
		);
		// try to reduce the thread group size as much as possible
		while (true) {
			const usize num_threads = thread_group_size.width * thread_group_size.height * thread_group_size.depth;
			if (num_threads % exec_width != 0) {
				break; // cannot reduce further
			}
			// maximum factor by which thread group size can be reduced
			const usize quotient = num_threads / exec_width;
			// find a common denominator for any dimension
			if (const usize gcd = std::gcd(thread_group_size.depth, quotient); gcd > 1) {
				thread_group_size.depth /= gcd;
				continue;
			}
			if (const usize gcd = std::gcd(thread_group_size.height, quotient); gcd > 1) {
				thread_group_size.height /= gcd;
				continue;
			}
			if (const usize gcd = std::gcd(thread_group_size.width, quotient); gcd > 1) {
				thread_group_size.width /= gcd;
				continue;
			}
			// if none is found, it cannot be reduced further
			break;
		}

		MTL4::ArgumentTable *arg_table = _get_compute_argument_table();
		_maybe_refresh_compute_descriptor_set_bindings();
		const MTL::GPUAddress argument_addr = _allocate_temporary_buffer(std::as_bytes(std::span(&argument, 1)));
		arg_table->setAddress(argument_addr, kIRRayDispatchArgumentsBindPoint);

		_scoped_compute_encoder encoder = _start_compute_pass();
		encoder->setArgumentTable(arg_table);
		encoder->dispatchThreads(MTL::Size(width, height, depth), thread_group_size);
	}

	void command_list::_update_descriptor_set_bindings(
		std::vector<u64> &bindings,
		u32 first,
		std::span<const gpu::descriptor_set *const> sets
	) {
		if (first + sets.size() > bindings.size()) {
			bindings.resize(first + sets.size(), 0);
		}
		for (usize i = 0; i < sets.size(); ++i) {
			bindings[first + i] = sets[i]->_arg_buffer->gpuAddress();
		}
	}

	MTL4::ArgumentTable *command_list::_get_graphics_argument_table() {
		if (!_graphics_bindings) {
			auto arg_table_descriptor = NS::TransferPtr(MTL4::ArgumentTableDescriptor::alloc()->init());
			arg_table_descriptor->setSupportAttributeStrides(true); // needed for vertex buffers
			arg_table_descriptor->setMaxBufferBindCount(31); // TODO
			NS::Error *error = nullptr;
			_graphics_bindings = NS::TransferPtr(_buf->device()->newArgumentTable(arg_table_descriptor.get(), &error));
			if (error) {
				log().error("Failed to create argument table: {}", error->localizedDescription()->utf8String());
			}
		}
		return _graphics_bindings.get();
	}

	MTL4::ArgumentTable *command_list::_get_compute_argument_table() {
		if (!_compute_bindings) {
			auto arg_table_descriptor = NS::TransferPtr(MTL4::ArgumentTableDescriptor::alloc()->init());
			arg_table_descriptor->setMaxBufferBindCount(31); // TODO
			NS::Error *error = nullptr;
			_compute_bindings = NS::TransferPtr(_buf->device()->newArgumentTable(arg_table_descriptor.get(), &error));
			if (error) {
				log().error("Failed to create argument table: {}", error->localizedDescription()->utf8String());
			}
		}
		return _compute_bindings.get();
	}

	MTL::GPUAddress command_list::_allocate_temporary_buffer(std::span<const std::byte> data) {
		NS::SharedPtr<MTL::Buffer> &buf = _binding_buffers.emplace_back(NS::TransferPtr(
			_buf->device()->newBuffer(data.data(), data.size(), MTL::ResourceStorageModeShared)
		));
		_residency_set->addAllocation(buf.get());
		return buf->gpuAddress();
	}

	void command_list::_maybe_refresh_graphics_descriptor_set_bindings() {
		if (_graphics_sets_bound) {
			return;
		}
		const MTL::GPUAddress data_addr = _allocate_temporary_buffer(std::as_bytes(std::span(_graphics_sets)));
		_graphics_bindings->setAddress(data_addr, kIRArgumentBufferBindPoint);
		_graphics_sets_bound = true;
	}

	void command_list::_maybe_refresh_compute_descriptor_set_bindings() {
		if (_compute_sets_bound) {
			return;
		}
		const MTL::GPUAddress data_addr = _allocate_temporary_buffer(std::as_bytes(std::span(_compute_sets)));
		_compute_bindings->setAddress(data_addr, kIRArgumentBufferBindPoint);
		_compute_sets_bound = true;
	}

	command_list::_scoped_compute_encoder command_list::_start_compute_pass() {
		MTL4::ComputeCommandEncoder *encoder = _buf->computeCommandEncoder();
		if (_pending_compute_barriers != 0) {
			constexpr static MTL::Stages _compute_stages =
				MTL::StageAccelerationStructure |
				MTL::StageBlit                  |
				MTL::StageDispatch;
			encoder->barrierAfterQueueStages(
				_pending_compute_barriers, _compute_stages, MTL4::VisibilityOptionResourceAlias
			);
			_pending_compute_barriers = 0;
		}
		return _scoped_compute_encoder(encoder);
	}


	f64 command_queue::get_timestamp_frequency() {
		return static_cast<f64>(_q->device()->queryTimestampFrequency());
	}

	void command_queue::submit_command_lists(
		std::span<const gpu::command_list *const> cmd_lists, queue_synchronization sync
	) {
		// handle waits
		for (const timeline_semaphore_synchronization &sem : sync.wait_semaphores) {
			_q->wait(sem.semaphore->_event.get(), sem.value);
		}

		// submit command lists
		if (!cmd_lists.empty()) {
			auto bookmark = get_scratch_bookmark();
			auto mtl_lists = bookmark.create_reserved_vector_array<const MTL4::CommandBuffer*>(cmd_lists.size());
			auto flush = [&]() {
				if (mtl_lists.empty()) {
					return;
				}
				_q->commit(mtl_lists.data(), mtl_lists.size());
				mtl_lists.clear();
			};

			for (const gpu::command_list *list : cmd_lists) {
				list->_residency_set->commit();

				// insert any swap chain waits
				if (!list->_used_swapchain_images.empty()) {
					flush();
					for (const MTL::ResourceID id : list->_used_swapchain_images) {
						auto iter = _drawable_mapping->find(id);
						crash_if(iter == _drawable_mapping->end());
						_q->wait(iter->second);
					}
				}

				mtl_lists.emplace_back(list->_buf.get());

				// handle pending barriers
				if (list->_pending_graphics_barriers != 0 || list->_pending_compute_barriers != 0) {
					flush();
					{ // insert a wait
						auto event = NS::TransferPtr(_q->device()->newEvent());
						_q->signalEvent(event.get(), 1);
						_q->wait(event.get(), 1);
					}
				}
			}
			flush();
		}

		// handle signals
		if (sync.notify_fence) {
			signal(*sync.notify_fence);
		}
		for (const timeline_semaphore_synchronization &sem : sync.notify_semaphores) {
			signal(*sem.semaphore, sem.value);
		}
	}

	swap_chain_status command_queue::present(swap_chain &chain) {
		{ // remove this drawable from the mapping
			auto it = _drawable_mapping->find(chain._drawable->texture()->gpuResourceID());
			crash_if(it == _drawable_mapping->end());
			_drawable_mapping->erase(it);
		}
		_q->signalDrawable(chain._drawable.get());
		chain._drawable->present();
		chain._drawable = nullptr;
		return swap_chain_status::ok;
	}

	void command_queue::signal(fence &f) {
		_q->signalEvent(f._event.get(), 1);
	}

	void command_queue::signal(timeline_semaphore &sem, gpu::_details::timeline_semaphore_value_type val) {
		_q->signalEvent(sem._event.get(), val);
	}

	queue_capabilities command_queue::get_capabilities() const {
		return queue_capabilities::timestamp_query;
	}
}
