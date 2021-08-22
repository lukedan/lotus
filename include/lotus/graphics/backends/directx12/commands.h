#pragma once

/// \file
/// DirectX 12 command queues.

#include <span>

#include <d3d12.h>

#include "lotus/color.h"
#include "details.h"
#include "pass.h"
#include "resources.h"
#include "frame_buffer.h"
#include "synchronization.h"

namespace lotus::graphics::backends::directx12 {
	class device;
	class command_queue;
	class command_allocator;

	/// A \p ID3D12CommandList.
	class command_list {
		friend device;
		friend command_queue;
	protected:
		/// No initialization.
		command_list(std::nullptr_t) {
		}

		/// Calls \p ID3D12GraphicsCommandList::Reset().
		void reset(command_allocator&);

		/// Does nothing.
		void start() {
		}
		/// Calls \p ID3D12GraphicsCommandList4::BeginRenderPass().
		void begin_pass(const pass_resources&, const frame_buffer&, std::span<linear_rgba_f>, float, std::uint8_t);

		/// Calls \p ID3D12GraphicsCommandList::IASetVertexBuffers().
		void bind_vertex_buffers(std::size_t, std::span<vertex_buffer>);

		/// Calls \p ID3D12GraphicsCommandList::CopyBufferRegion().
		void copy_buffer(buffer &from, std::size_t off1, buffer &to, std::size_t off2, std::size_t size);

		/// Calls \p ID3D12GraphicsCommandList::ResourceBarrier() to insert a resource barrier.
		void resource_barrier(std::span<image_barrier>, std::span<buffer_barrier>);

		/// Calls \p ID3D12GraphicsCommandList4::EndRenderPass().
		void end_pass();
		/// Calls \p ID3D12GraphicsCommandList::Close().
		void finish();
	private:
		_details::com_ptr<ID3D12GraphicsCommandList4> _list; ///< The command list.
	};

	/// A \p ID3D12CommandAllocator.
	class command_allocator {
		friend device;
		friend command_list;
	protected:
		/// Calls \p ID3D12CommandAllocator::Reset().
		void reset(device&);
	private:
		_details::com_ptr<ID3D12CommandAllocator> _allocator; ///< The allocator.
	};

	/// A DirectX 12 command queue.
	class command_queue {
		friend device;
		friend context;
	protected:
		/// Calls \p ID3D12CommandQueue::ExecuteCommandLists(), then optionally signals the fence using
		/// \p ID3D12CommandQueue::Signal().
		void submit_command_lists(std::span<const graphics::command_list*>, fence*);
		/// Calls \p IDXGISwapChain::Present(), then signals the given \ref fence. Also stores the fence to be later
		/// returned by \ref swap_chain::acquire_back_buffer().
		void present(swap_chain&, graphics::fence*);
	private:
		_details::com_ptr<ID3D12CommandQueue> _queue; ///< The command queue.
	};
}
