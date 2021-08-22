#include "lotus/graphics/backends/directx12/commands.h"

/// \file
/// Implementation of command related functions.

#include "lotus/utils/stack_allocator.h"
#include "lotus/graphics/resources.h"
#include "lotus/graphics/commands.h"
#include "lotus/graphics/backends/directx12/pass.h"

namespace lotus::graphics::backends::directx12 {
	void command_queue::submit_command_lists(std::span<const graphics::command_list*> lists, fence *f) {
		auto bookmark = stack_allocator::scoped_bookmark::create();
		auto dx_lists = stack_allocator::for_this_thread().create_vector_array<ID3D12CommandList*>(
			lists.size(), nullptr
		);
		for (std::size_t i = 0; i < lists.size(); ++i) {
			dx_lists[i] = lists[i]->_list.Get();
		}
		_queue->ExecuteCommandLists(static_cast<UINT>(lists.size()), dx_lists.data());

		if (f) {
			_details::assert_dx(_queue->Signal(f->_fence.Get(), static_cast<UINT64>(synchronization_state::set)));
		}
	}

	void command_queue::present(swap_chain &chain, graphics::fence *f) {
		_details::assert_dx(chain._swap_chain->Present(0, 0));
		if (f) {
			_queue->Signal(f->_fence.Get(), static_cast<UINT64>(synchronization_state::set));
		}
		chain._on_presented[chain._swap_chain->GetCurrentBackBufferIndex()] = f;
	}


	void command_list::reset(command_allocator &allocator) {
		_details::assert_dx(_list->Reset(allocator._allocator.Get(), nullptr));
	}

	void command_list::begin_pass(
		const pass_resources &p, const frame_buffer &fb,
		std::span<linear_rgba_f> clear_colors, float clear_depth, std::uint8_t clear_stencil
	) {
		assert(clear_colors.size() == p._num_render_targets);
		pass_resources pass = p;
		for (std::uint8_t i = 0; i < pass._num_render_targets; ++i) {
			assert(!fb._color[i].is_empty());
			pass._render_targets[i].cpuDescriptor = fb._color[i].get();
			pass._render_targets[i].BeginningAccess.Clear.ClearValue.Color[0] = clear_colors[i].r;
			pass._render_targets[i].BeginningAccess.Clear.ClearValue.Color[1] = clear_colors[i].g;
			pass._render_targets[i].BeginningAccess.Clear.ClearValue.Color[2] = clear_colors[i].b;
			pass._render_targets[i].BeginningAccess.Clear.ClearValue.Color[3] = clear_colors[i].a;
		}
		assert(
			pass._num_render_targets == num_color_render_targets ||
			fb._color[pass._num_render_targets].is_empty()
		);
		const D3D12_RENDER_PASS_DEPTH_STENCIL_DESC *ds_desc = nullptr;
		if (!fb._depth_stencil.is_empty()) {
			pass._depth_stencil.cpuDescriptor = fb._depth_stencil.get();
			D3D12_DEPTH_STENCIL_VALUE clear_value = {};
			clear_value.Depth = clear_depth;
			clear_value.Stencil = clear_stencil;
			pass._depth_stencil.DepthBeginningAccess.Clear.ClearValue.DepthStencil = clear_value;
			pass._depth_stencil.StencilBeginningAccess.Clear.ClearValue.DepthStencil = clear_value;
			ds_desc = &pass._depth_stencil;
		}
		_list->BeginRenderPass(pass._num_render_targets, pass._render_targets.data(), ds_desc, pass._flags);
	}

	void command_list::resource_barrier(std::span<image_barrier> images, std::span<buffer_barrier> buffers) {
		auto bookmark = stack_allocator::scoped_bookmark::create();
		auto resources = stack_allocator::for_this_thread().create_vector_array<D3D12_RESOURCE_BARRIER>(
			images.size() + buffers.size()
		);
		std::size_t count = 0;
		for (const auto &img : images) {
			resources[count] = {};
			resources[count].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			resources[count].Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			resources[count].Transition.pResource   = static_cast<_details::image*>(img.target)->_image.Get();
			resources[count].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES; // TODO
			resources[count].Transition.StateBefore = _details::conversions::for_image_usage(img.from_state);
			resources[count].Transition.StateAfter  = _details::conversions::for_image_usage(img.to_state);

			++count;
		}
		for (const auto &buf : buffers) {
			resources[count] = {};
			resources[count].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			resources[count].Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			resources[count].Transition.pResource   = static_cast<buffer*>(buf.target)->_buffer.Get();
			resources[count].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			resources[count].Transition.StateBefore = _details::conversions::for_buffer_usage(buf.from_state);
			resources[count].Transition.StateAfter  = _details::conversions::for_buffer_usage(buf.to_state);

			++count;
		}
		assert(count == resources.size());
		_list->ResourceBarrier(static_cast<UINT>(count), resources.data());
	}

	void command_list::bind_vertex_buffers(std::size_t start, std::span<vertex_buffer> buffers) {
		auto bookmark = stack_allocator::scoped_bookmark::create();
		auto bindings = stack_allocator::for_this_thread().create_vector_array<D3D12_VERTEX_BUFFER_VIEW>(
			buffers.size(), D3D12_VERTEX_BUFFER_VIEW()
		);
		for (std::size_t i = 0; i < buffers.size(); ++i) {
			auto *buf = static_cast<buffer*>(buffers[i].data);
			bindings[i] = {};
			bindings[i].BufferLocation = buf->_buffer->GetGPUVirtualAddress();
			bindings[i].SizeInBytes    = buf->_buffer->GetDesc().Width;
			bindings[i].StrideInBytes  = static_cast<UINT>(buffers[i].stride);
		}
		_list->IASetVertexBuffers(
			static_cast<UINT>(start), static_cast<UINT>(buffers.size()), bindings.data()
		);
	}

	void command_list::copy_buffer(buffer &from, std::size_t off1, buffer &to, std::size_t off2, std::size_t size) {
		_list->CopyBufferRegion(
			to._buffer.Get(), static_cast<UINT64>(off2),
			from._buffer.Get(), static_cast<UINT64>(off1),
			static_cast<UINT64>(size)
		);
	}

	void command_list::end_pass() {
		_list->EndRenderPass();
	}

	void command_list::finish() {
		_details::assert_dx(_list->Close());
	}


	void command_allocator::reset(device&) {
		_details::assert_dx(_allocator->Reset());
	}
}
