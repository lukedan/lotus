#pragma once

/// \file
/// Common typedefs and functions for DirectX 12.

#include <cstdlib>
#include <iostream>
#include <vector>

#include <wrl/client.h>
#include <d3d12.h>

#include "lotus/graphics/common.h"

namespace lotus::graphics::backends::directx12 {
	class device;
}

namespace lotus::graphics::backends::directx12::_details {
	template <typename T> using com_ptr = Microsoft::WRL::ComPtr<T>; ///< Reference-counted pointer to a COM object.

	/// Aborts if the given \p HRESULT does not indicate success.
	void assert_dx(HRESULT);

	/// Converts generic types into DX12 types.
	namespace conversions {
		/// Converts a \ref pixel_format to a \p DXGI_FORMAT.
		[[nodiscard]] DXGI_FORMAT for_pixel_format(pixel_format);
		/// Converts a \ref blend_factor to a \p D3D12_BLEND.
		[[nodiscard]] D3D12_BLEND for_blend_factor(blend_factor);
		/// Converts a \ref blend_operation to a \p D3D12_BLEND_OP.
		[[nodiscard]] D3D12_BLEND_OP for_blend_operation(blend_operation);
		/// Converts a \ref pass_load_operation to a \p D3D12_RENDER_PASS_BEGINNING_ACCESS.
		[[nodiscard]] D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE for_pass_load_operation(pass_load_operation);
		/// Converts a \ref pass_store_operation to a \p D3D12_RENDER_PASS_ENDING_ACCESS.
		[[nodiscard]] D3D12_RENDER_PASS_ENDING_ACCESS_TYPE for_pass_store_operation(pass_store_operation);
		/// Converts a \ref image_usage to a \p D3D12_RESOURCE_STATES.
		[[nodiscard]] D3D12_RESOURCE_STATES for_image_usage(image_usage);
		/// Converts a \ref buffer_usage to a \p D3D12_RESOURCE_STATES.
		[[nodiscard]] D3D12_RESOURCE_STATES for_buffer_usage(buffer_usage);
		/// Converts a \ref heap_type to a \p D3D12_HEAP_TYPE.
		[[nodiscard]] D3D12_HEAP_TYPE for_heap_type(heap_type);

		/// Converts a \ref render_target_blend_options to a \p D3D12_RENDER_TARGET_BLEND_DESC.
		[[nodiscard]] D3D12_RENDER_TARGET_BLEND_DESC for_render_target_blend_options(
			const render_target_blend_options&
		);
		/// Converts a \ref blend_options to a \p D3D12_BLEND_DESC.
		[[nodiscard]] D3D12_BLEND_DESC for_blend_options(const blend_options&);
		/// Converts a \ref render_target_pass_options to a \p D3D12_RENDER_PASS_RENDER_TARGET_DESC.
		[[nodiscard]] D3D12_RENDER_PASS_RENDER_TARGET_DESC for_render_target_pass_options(
			const render_target_pass_options&
		);
		/// Converts a \ref depth_stencil_pass_options to a \p D3D12_DEPTH_STENCIL_RENDER_TARGET_DESC.
		[[nodiscard]] D3D12_RENDER_PASS_DEPTH_STENCIL_DESC for_depth_stencil_pass_options(
			const depth_stencil_pass_options&
		);
	}


	class descriptor_heap;
	/// Wrapper around a \ref D3D12_CPU_DESCRIPTOR_HANDLE.
	struct descriptor {
		friend descriptor_heap;
	public:
		/// Initializes this descriptor to empty.
		descriptor(std::nullptr_t) : _descriptor(_destroyed) {
		}
		/// Move constructor.
		descriptor(descriptor &&src) : _descriptor(std::exchange(src._descriptor, _destroyed)) {
		}
		/// No copy construction.
		descriptor(const descriptor&) = delete;
		/// Move assignment.
		descriptor &operator=(descriptor &&src) {
			_descriptor = std::exchange(src._descriptor, _destroyed);
			return *this;
		}
		/// No copy assignment.
		descriptor &operator=(const descriptor&) = delete;
		/// Checks that this descriptor has been freed by a \ref descriptor_heap.
		~descriptor() {
			assert(is_empty());
		}

		/// Returns a copy of \ref _descriptor.
		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE get() const {
			return _descriptor;
		}
		/// Returns whether this descriptor is empty.
		[[nodiscard]] bool is_empty() const {
			return _descriptor.ptr == _destroyed.ptr;
		}
	private:
		/// Magic value indicating that this descriptor has been properly freed.
		constexpr static D3D12_CPU_DESCRIPTOR_HANDLE _destroyed = { NULL };

		D3D12_CPU_DESCRIPTOR_HANDLE _descriptor; ///< The descriptor.

		/// Initializes \ref _descriptor.
		explicit descriptor(D3D12_CPU_DESCRIPTOR_HANDLE desc) : _descriptor(desc) {
		}
	};
	/// Manages a series of descriptors.
	class descriptor_heap {
	public:
		/// No initialization.
		descriptor_heap(uninitialized_t) {
		}
		/// Creates the descriptor heap.
		descriptor_heap(device*, D3D12_DESCRIPTOR_HEAP_TYPE, UINT);
		/// Move constructor.
		descriptor_heap(descriptor_heap &&src) :
			_device(std::exchange(src._device, nullptr)),
			_heap(std::exchange(src._heap, nullptr)),
			_free(std::move(src._free)),
			_next(src._next) {
		}
		/// No copy construction.
		descriptor_heap(const descriptor_heap&) = delete;
		/// Move assignment.
		descriptor_heap &operator=(descriptor_heap &&src) {
			_device = std::exchange(src._device, nullptr);
			_heap = std::exchange(src._heap, nullptr);
			_free = std::move(src._free);
			_next = src._next;
			return *this;
		}
		/// No copy assignment.
		descriptor_heap &operator=(const descriptor_heap&) = delete;

		/// Allocates a descritor.
		[[nodiscard]] descriptor allocate();
		void destroy(descriptor);
	protected:
		com_ptr<ID3D12Device> _device; ///< The device.
		com_ptr<ID3D12DescriptorHeap> _heap; ///< The heap of descriptors.
		// TODO allocator
		std::vector<std::uint16_t> _free; ///< An array containing descriptors that are not in use.
		std::uint16_t _next; ///< Next descriptor to allocate if \ref _free is empty.
	};
}
