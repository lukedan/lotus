#include "lotus/graphics/backends/directx12/commands.h"

/// \file
/// Implementation of command related functions.

#include "lotus/utils/stack_allocator.h"
#include "lotus/graphics/descriptors.h"
#include "lotus/graphics/resources.h"
#include "lotus/graphics/commands.h"
#include "lotus/graphics/backends/directx12/descriptors.h"

namespace lotus::graphics::backends::directx12 {
	void command_allocator::reset(device&) {
		_details::assert_dx(_allocator->Reset());
	}


	void command_list::reset_and_start(command_allocator &allocator) {
		_details::assert_dx(_list->Reset(allocator._allocator.Get(), nullptr));
		_list->SetDescriptorHeaps(static_cast<UINT>(_descriptor_heaps.size()), _descriptor_heaps.data());
	}

	void command_list::begin_pass(const frame_buffer &fb, const frame_buffer_access &access) {
		assert(fb._color.get_count() == access.color_render_targets.size());
		std::size_t num_rts = fb._color.get_count();
		auto bookmark = stack_allocator::for_this_thread().bookmark();
		auto color_rts = bookmark.create_vector_array<D3D12_RENDER_PASS_RENDER_TARGET_DESC>(num_rts);
		for (std::size_t i = 0; i < num_rts; ++i) {
			auto &desc = color_rts[i];
			desc = {};
			const auto &rt_access = access.color_render_targets[i];
			desc.cpuDescriptor                           =
				fb._color.get_cpu(static_cast<_details::descriptor_range::index_t>(i));
			desc.BeginningAccess.Type                    =
				_details::conversions::to_render_pass_beginning_access_type(rt_access.load_operation);
			desc.BeginningAccess.Clear.ClearValue.Format = fb._color_formats[i];
			std::visit([&](const auto &color4) {
				for (std::size_t i = 0; i < color4.dimensionality; ++i) {
					desc.BeginningAccess.Clear.ClearValue.Color[i] = static_cast<FLOAT>(color4[i]);
				}
			}, rt_access.clear_value.value);
			desc.EndingAccess.Type                       =
				_details::conversions::to_render_pass_ending_access_type(rt_access.store_operation);
		}
		const D3D12_RENDER_PASS_DEPTH_STENCIL_DESC *ds_desc_ptr = nullptr;
		D3D12_RENDER_PASS_DEPTH_STENCIL_DESC ds_desc = {};
		if (!fb._depth_stencil.is_empty()) {
			ds_desc_ptr = &ds_desc;
			ds_desc.cpuDescriptor                                                = fb._depth_stencil.get_cpu(0);

			ds_desc.DepthBeginningAccess.Type                                    =
				_details::conversions::to_render_pass_beginning_access_type(
					access.depth_render_target.load_operation
				);
			ds_desc.DepthBeginningAccess.Clear.ClearValue.Format                 = fb._depth_stencil_format;
			ds_desc.DepthBeginningAccess.Clear.ClearValue.DepthStencil.Depth     =
				static_cast<FLOAT>(access.depth_render_target.clear_value);
			ds_desc.DepthEndingAccess.Type                                       =
				_details::conversions::to_render_pass_ending_access_type(access.depth_render_target.store_operation);

			ds_desc.StencilBeginningAccess.Type                                  =
				_details::conversions::to_render_pass_beginning_access_type(
					access.stencil_render_target.load_operation
				);
			ds_desc.StencilBeginningAccess.Clear.ClearValue.Format               = fb._depth_stencil_format;
			ds_desc.StencilBeginningAccess.Clear.ClearValue.DepthStencil.Stencil =
				static_cast<UINT8>(access.stencil_render_target.clear_value);
			ds_desc.StencilEndingAccess.Type                                     =
				_details::conversions::to_render_pass_ending_access_type(
					access.stencil_render_target.store_operation
				);
		}
		_list->BeginRenderPass(
			static_cast<UINT>(color_rts.size()), color_rts.data(), ds_desc_ptr,
			D3D12_RENDER_PASS_FLAG_ALLOW_UAV_WRITES // TODO more/less flags?
		);
	}

	void command_list::resource_barrier(
		std::span<const image_barrier> images, std::span<const buffer_barrier> buffers
	) {
		auto bookmark = stack_allocator::for_this_thread().bookmark();
		auto resources = bookmark.create_reserved_vector_array<D3D12_RESOURCE_BARRIER>(
			images.size() + buffers.size()
		);
		for (const auto &img : images) {
			D3D12_RESOURCE_STATES from_state = _details::conversions::to_resource_states(img.from_state);
			D3D12_RESOURCE_STATES to_state   = _details::conversions::to_resource_states(img.to_state);
			if (from_state == to_state) {
				continue;
			}
			auto &barrier = resources.emplace_back();
			barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barrier.Transition.pResource   = static_cast<_details::image*>(img.target)->_image.Get();
			barrier.Transition.Subresource = _details::compute_subresource_index(
				img.subresource, static_cast<_details::image*>(img.target)->_image.Get()
			);
			barrier.Transition.StateBefore = from_state;
			barrier.Transition.StateAfter  = to_state;
		}
		for (const auto &buf : buffers) {
			auto &barrier = resources.emplace_back();
			barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barrier.Transition.pResource   = static_cast<buffer*>(buf.target)->_buffer.Get();
			barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			barrier.Transition.StateBefore = _details::conversions::to_resource_states(buf.from_state);
			barrier.Transition.StateAfter  = _details::conversions::to_resource_states(buf.to_state);
		}
		if (resources.empty()) {
			return;
		}
		_list->ResourceBarrier(static_cast<UINT>(resources.size()), resources.data());
	}

	void command_list::bind_pipeline_state(const graphics_pipeline_state &state) {
		_list->SetGraphicsRootSignature(state._root_signature.Get());
		_list->SetPipelineState(state._pipeline.Get());
		_list->IASetPrimitiveTopology(state._topology);
	}

	void command_list::bind_pipeline_state(const compute_pipeline_state &state) {
		_list->SetComputeRootSignature(state._root_signature.Get());
		_list->SetPipelineState(state._pipeline.Get());
	}

	void command_list::bind_vertex_buffers(std::size_t start, std::span<const vertex_buffer> buffers) {
		auto bookmark = stack_allocator::for_this_thread().bookmark();
		auto bindings = bookmark.create_vector_array<D3D12_VERTEX_BUFFER_VIEW>(buffers.size());
		for (std::size_t i = 0; i < buffers.size(); ++i) {
			auto &vert_buf = buffers[i];
			const auto *buf = static_cast<const buffer*>(vert_buf.data);
			bindings[i] = {};
			bindings[i].BufferLocation = buf->_buffer->GetGPUVirtualAddress() + vert_buf.offset;
			bindings[i].SizeInBytes    = static_cast<UINT>(buf->_buffer->GetDesc().Width - vert_buf.offset);
			bindings[i].StrideInBytes  = static_cast<UINT>(vert_buf.stride);
		}
		_list->IASetVertexBuffers(
			static_cast<UINT>(start), static_cast<UINT>(buffers.size()), bindings.data()
		);
	}

	void command_list::bind_index_buffer(const buffer &buf, std::size_t offset, index_format fmt) {
		D3D12_INDEX_BUFFER_VIEW buf_view = {};
		buf_view.BufferLocation = buf._buffer->GetGPUVirtualAddress() + offset;
		buf_view.SizeInBytes    = static_cast<UINT>(buf._buffer->GetDesc().Width - offset);
		buf_view.Format         = _details::conversions::to_format(fmt);
		_list->IASetIndexBuffer(&buf_view);
	}

	void command_list::bind_graphics_descriptor_sets(
		const pipeline_resources &rsrc, std::size_t first, std::span<const graphics::descriptor_set *const> sets
	) {
		for (std::size_t i = 0; i < sets.size(); ++i) {
			std::size_t set_index = first + i;
			auto *set = static_cast<const descriptor_set*>(sets[i]);
			const auto &indices = rsrc._descriptor_table_binding[set_index];
			assert(
				set->_shader_resource_descriptors.is_empty() ==
				(indices.resource_index == pipeline_resources::_invalid_root_param)
			);
			assert(
				set->_sampler_descriptors.is_empty() ==
				(indices.sampler_index == pipeline_resources::_invalid_root_param)
			);
			if (!set->_shader_resource_descriptors.is_empty()) {
				_list->SetGraphicsRootDescriptorTable(
					indices.resource_index, set->_shader_resource_descriptors.get_gpu(0)
				);
			}
			if (!set->_sampler_descriptors.is_empty()) {
				_list->SetGraphicsRootDescriptorTable(
					indices.sampler_index, set->_sampler_descriptors.get_gpu(0)
				);
			}
		}
	}

	void command_list::bind_compute_descriptor_sets(
		const pipeline_resources &rsrc, std::size_t first, std::span<const graphics::descriptor_set *const> sets
	) {
		for (std::size_t i = 0; i < sets.size(); ++i) {
			std::size_t set_index = first + i;
			auto *set = static_cast<const descriptor_set*>(sets[i]);
			const auto &indices = rsrc._descriptor_table_binding[set_index];
			assert(
				set->_shader_resource_descriptors.is_empty() ==
				(indices.resource_index == pipeline_resources::_invalid_root_param)
			);
			assert(
				set->_sampler_descriptors.is_empty() ==
				(indices.sampler_index == pipeline_resources::_invalid_root_param)
			);
			if (!set->_shader_resource_descriptors.is_empty()) {
				_list->SetComputeRootDescriptorTable(
					indices.resource_index, set->_shader_resource_descriptors.get_gpu(0)
				);
			}
			if (!set->_sampler_descriptors.is_empty()) {
				_list->SetComputeRootDescriptorTable(
					indices.sampler_index, set->_sampler_descriptors.get_gpu(0)
				);
			}
		}
	}

	void command_list::set_viewports(std::span<const viewport> vps) {
		auto bookmark = stack_allocator::for_this_thread().bookmark();
		auto vec = bookmark.create_vector_array<D3D12_VIEWPORT>(vps.size());
		for (std::size_t i = 0; i < vps.size(); ++i) {
			vec[i] = _details::conversions::to_viewport(vps[i]);
		}
		_list->RSSetViewports(static_cast<UINT>(vec.size()), vec.data());
	}

	void command_list::set_scissor_rectangles(std::span<const aab2i> rects) {
		auto bookmark = stack_allocator::for_this_thread().bookmark();
		auto vec = bookmark.create_vector_array<D3D12_RECT>(rects.size());
		for (std::size_t i = 0; i < rects.size(); ++i) {
			vec[i] = _details::conversions::to_rect(rects[i]);
		}
		_list->RSSetScissorRects(static_cast<UINT>(vec.size()), vec.data());
	}

	void command_list::copy_buffer(buffer &from, std::size_t off1, buffer &to, std::size_t off2, std::size_t size) {
		_list->CopyBufferRegion(
			to._buffer.Get(), static_cast<UINT64>(off2),
			from._buffer.Get(), static_cast<UINT64>(off1),
			static_cast<UINT64>(size)
		);
	}

	void command_list::copy_image2d(
		image2d &from, subresource_index sub1, aab2s region, image2d &to, subresource_index sub2, cvec2s off
	) {
		D3D12_TEXTURE_COPY_LOCATION dest = {};
		dest.pResource        = to._image.Get();
		dest.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dest.SubresourceIndex = _details::compute_subresource_index(sub2, to._image.Get());
		D3D12_TEXTURE_COPY_LOCATION source = {};
		source.pResource        = from._image.Get();
		source.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		source.SubresourceIndex = _details::compute_subresource_index(sub1, from._image.Get());
		D3D12_BOX src_box = {};
		src_box.left   = static_cast<UINT>(region.min[0]);
		src_box.top    = static_cast<UINT>(region.min[1]);
		src_box.front  = 0;
		src_box.right  = static_cast<UINT>(region.max[0]);
		src_box.bottom = static_cast<UINT>(region.max[1]);
		src_box.back   = 1;
		_list->CopyTextureRegion(
			&dest, static_cast<UINT>(off[0]), static_cast<UINT>(off[1]), 0, &source, &src_box
		);
	}

	void command_list::copy_buffer_to_image(
		buffer &from, std::size_t byte_offset, staging_buffer_pitch row_pitch, aab2s region,
		image2d &to, subresource_index subresource, cvec2s off
	) {
		D3D12_TEXTURE_COPY_LOCATION dest = {};
		dest.pResource        = to._image.Get();
		dest.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dest.SubresourceIndex = _details::compute_subresource_index(subresource, to._image.Get());
		D3D12_TEXTURE_COPY_LOCATION source = {};
		source.pResource = from._buffer.Get();
		source.Type      = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		source.PlacedFootprint.Offset = static_cast<UINT64>(byte_offset);
		cvec2s size = region.signed_size();
		source.PlacedFootprint.Footprint.Format   = to._image->GetDesc().Format;
		source.PlacedFootprint.Footprint.Width    = static_cast<UINT>(size[0]);
		source.PlacedFootprint.Footprint.Height   = static_cast<UINT>(size[1]);
		source.PlacedFootprint.Footprint.Depth    = 1;
		source.PlacedFootprint.Footprint.RowPitch = row_pitch._pitch;
		D3D12_BOX src_box = {};
		src_box.left   = static_cast<UINT>(off[0]);
		src_box.top    = static_cast<UINT>(off[1]);
		src_box.right  = static_cast<UINT>(size[0]);
		src_box.bottom = static_cast<UINT>(size[1]);
		src_box.back   = 1;
		_list->CopyTextureRegion(
			&dest, static_cast<UINT>(region.min[0]), static_cast<UINT>(region.min[1]), 0, &source, &src_box
		);
	}

	void command_list::draw_instanced(
		std::size_t first_vertex, std::size_t vertex_count,
		std::size_t first_instance, std::size_t instance_count
	) {
		_list->DrawInstanced(
			static_cast<UINT>(vertex_count), static_cast<UINT>(instance_count),
			static_cast<UINT>(first_vertex), static_cast<UINT>(first_instance)
		);
	}

	void command_list::draw_indexed_instanced(
		std::size_t first_index, std::size_t index_count,
		std::size_t first_vertex,
		std::size_t first_instance, std::size_t instance_count
	) {
		_list->DrawIndexedInstanced(
			static_cast<UINT>(index_count), static_cast<UINT>(instance_count),
			static_cast<UINT>(first_index), static_cast<UINT>(first_vertex), static_cast<UINT>(first_instance)
		);
	}

	void command_list::run_compute_shader(std::uint32_t x, std::uint32_t y, std::uint32_t z) {
		_list->Dispatch(x, y, z);
	}

	void command_list::end_pass() {
		_list->EndRenderPass();
	}

	void command_list::finish() {
		_details::assert_dx(_list->Close());
	}

	void command_list::build_acceleration_structure(
		const bottom_level_acceleration_structure_geometry &geom,
		bottom_level_acceleration_structure &output, buffer &scratch, std::size_t scratch_offset
	) {
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc = {};
		desc.DestAccelerationStructureData    = output._buffer->GetGPUVirtualAddress() + output._offset;
		desc.Inputs                           = geom._inputs;
		desc.SourceAccelerationStructureData  = 0;
		desc.ScratchAccelerationStructureData = scratch._buffer->GetGPUVirtualAddress() + scratch_offset;
		_list->BuildRaytracingAccelerationStructure(&desc, 0, nullptr);
	}

	void command_list::build_acceleration_structure(
		const buffer &instances, std::size_t offset, std::size_t count,
		top_level_acceleration_structure &output, buffer &scratch, std::size_t scratch_offset
	) {
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc = {};
		desc.DestAccelerationStructureData    = output._buffer->GetGPUVirtualAddress() + output._offset;
		desc.Inputs.Type                      = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
		desc.Inputs.Flags                     = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
		desc.Inputs.NumDescs                  = static_cast<UINT>(count);
		desc.Inputs.DescsLayout               = D3D12_ELEMENTS_LAYOUT_ARRAY;
		desc.Inputs.InstanceDescs             = instances._buffer->GetGPUVirtualAddress() + offset;
		desc.SourceAccelerationStructureData  = 0;
		desc.ScratchAccelerationStructureData = scratch._buffer->GetGPUVirtualAddress() + scratch_offset;
		_list->BuildRaytracingAccelerationStructure(&desc, 0, nullptr);
	}

	void command_list::bind_pipeline_state(const raytracing_pipeline_state &state) {
		_list->SetComputeRootSignature(state._root_signature.Get());
		_list->SetPipelineState1(state._state.Get());
	}

	void command_list::trace_rays(
		constant_buffer_view ray_generation,
		shader_record_view miss_shaders, shader_record_view hit_groups,
		std::size_t width, std::size_t height, std::size_t depth
	) {
		D3D12_DISPATCH_RAYS_DESC desc = {};
		desc.RayGenerationShaderRecord.StartAddress =
			ray_generation.data->_buffer->GetGPUVirtualAddress() + ray_generation.offset;
		desc.RayGenerationShaderRecord.SizeInBytes  = ray_generation.size;
		desc.MissShaderTable.StartAddress           =
			miss_shaders.data->_buffer->GetGPUVirtualAddress() + miss_shaders.offset;
		desc.MissShaderTable.SizeInBytes            = miss_shaders.count * miss_shaders.stride;
		desc.MissShaderTable.StrideInBytes          = miss_shaders.stride;
		desc.HitGroupTable.StartAddress             =
			hit_groups.data->_buffer->GetGPUVirtualAddress() + hit_groups.offset;
		desc.HitGroupTable.SizeInBytes              = hit_groups.count * hit_groups.stride;
		desc.HitGroupTable.StrideInBytes            = hit_groups.stride;
		desc.CallableShaderTable.StartAddress       = 0;
		desc.CallableShaderTable.SizeInBytes        = 0;
		desc.CallableShaderTable.StrideInBytes      = 0;
		desc.Width                                  = static_cast<UINT>(width);
		desc.Height                                 = static_cast<UINT>(height);
		desc.Depth                                  = static_cast<UINT>(depth);
		_list->DispatchRays(&desc);
	}


	void command_queue::signal(fence &f) {
		_details::assert_dx(_queue->Signal(f._fence.Get(), static_cast<UINT64>(synchronization_state::set)));
	}

	void command_queue::signal(timeline_semaphore &sem, std::uint64_t val) {
		_details::assert_dx(_queue->Signal(sem._semaphore.Get(), val));
	}

	void command_queue::submit_command_lists(
		std::span<const graphics::command_list *const> lists, queue_synchronization synch
	) {
		for (const auto &sem : synch.wait_semaphores) {
			_details::assert_dx(_queue->Wait(sem.semaphore->_semaphore.Get(), sem.value));
		}

		auto bookmark = stack_allocator::for_this_thread().bookmark();
		auto dx_lists = bookmark.create_vector_array<ID3D12CommandList*>(lists.size(), nullptr);
		for (std::size_t i = 0; i < lists.size(); ++i) {
			dx_lists[i] = lists[i]->_list.Get();
		}
		_queue->ExecuteCommandLists(static_cast<UINT>(lists.size()), dx_lists.data());

		for (const auto &sem : synch.notify_semaphores) {
			_details::assert_dx(_queue->Signal(sem.semaphore->_semaphore.Get(), sem.value));
		}
		if (synch.notify_fence) {
			_details::assert_dx(_queue->Signal(
				synch.notify_fence->_fence.Get(), static_cast<UINT64>(synchronization_state::set)
			));
		}
	}

	swap_chain_status command_queue::present(swap_chain &chain) {
		UINT index = chain._swap_chain->GetCurrentBackBufferIndex();
		_details::assert_dx(chain._swap_chain->Present(0, 0));
		auto &sync = chain._synchronization[index];
		sync.notify_fence = sync.next_fence;
		if (sync.notify_fence) {
			_queue->Signal(sync.notify_fence->_fence.Get(), static_cast<UINT64>(synchronization_state::set));
		}
		return swap_chain_status::ok;
	}
}
