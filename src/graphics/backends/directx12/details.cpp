#include "lotus/graphics/backends/directx12/details.h"

/// \file
/// Implementation of miscellaneous DirectX 12 related functions.

#include <comdef.h>

#include "lotus/system/platforms/windows/details.h"
#include "lotus/graphics/backends/directx12/device.h"

namespace lotus::graphics::backends::directx12::_details {
	void assert_dx(HRESULT hr) {
		if (FAILED(hr)) {
			_com_error err(hr);
			auto msg = system::platforms::windows::_details::tstring_to_u8string(
				err.ErrorMessage(), std::allocator<char8_t>()
			);
			std::cerr <<
				"DirectX error " <<
				std::hex << hr << ": " <<
				std::string_view(reinterpret_cast<const char*>(msg.c_str()), msg.size()) <<
				std::endl;
			std::abort();
		}
	}


	namespace conversions {
		DXGI_FORMAT for_pixel_format(pixel_format fmt) {
			if (fmt == pixel_format::none) {
				return DXGI_FORMAT_UNKNOWN;
			}
			// TODO
			return DXGI_FORMAT_R8G8B8A8_UNORM;
		}

		D3D12_BLEND for_blend_factor(blend_factor factor) {
			static const D3D12_BLEND table[] {
				D3D12_BLEND_ZERO,
				D3D12_BLEND_ONE,
				D3D12_BLEND_SRC_COLOR,
				D3D12_BLEND_INV_SRC_COLOR,
				D3D12_BLEND_DEST_COLOR,
				D3D12_BLEND_INV_DEST_COLOR,
				D3D12_BLEND_SRC_ALPHA,
				D3D12_BLEND_INV_SRC_ALPHA,
				D3D12_BLEND_DEST_ALPHA,
				D3D12_BLEND_INV_DEST_ALPHA,
			};
			static_assert(
				std::size(table) == static_cast<std::size_t>(blend_factor::num_enumerators),
				"Incorrect size for lookup table for D3D12_BLEND"
			);
			return table[static_cast<std::size_t>(factor)];
		}

		D3D12_BLEND_OP for_blend_operation(blend_operation op) {
			static const D3D12_BLEND_OP table[] {
				D3D12_BLEND_OP_ADD,
				D3D12_BLEND_OP_SUBTRACT,
				D3D12_BLEND_OP_REV_SUBTRACT,
				D3D12_BLEND_OP_MIN,
				D3D12_BLEND_OP_MAX,
			};
			static_assert(
				std::size(table) == static_cast<std::size_t>(blend_operation::num_enumerators),
				"Incorrect size for lookup table for D3D12_BLEND_OP"
			);
			return table[static_cast<std::size_t>(op)];
		}

		D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE for_pass_load_operation(pass_load_operation op) {
			static const D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE table[]{
				D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD,
				D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE,
				D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR,
			};
			static_assert(
				std::size(table) == static_cast<std::size_t>(pass_load_operation::num_enumerators),
				"Incorrect size for lookup table for D3D12_RENDER_PASS_BEGINNING_ACCESS"
			);
			return table[static_cast<std::size_t>(op)];
		}

		D3D12_RENDER_PASS_ENDING_ACCESS_TYPE for_pass_store_operation(pass_store_operation op) {
			static const D3D12_RENDER_PASS_ENDING_ACCESS_TYPE table[]{
				D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD,
				D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE,
			};
			static_assert(
				std::size(table) == static_cast<std::size_t>(pass_store_operation::num_enumerators),
				"Incorrect size for lookup table for D3D12_RENDER_PASS_ENDING_ACCESS"
			);
			return table[static_cast<std::size_t>(op)];
		}

		D3D12_RESOURCE_STATES for_image_usage(image_usage st) {
			static const D3D12_RESOURCE_STATES table[]{
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_DEPTH_WRITE,
				D3D12_RESOURCE_STATE_PRESENT,
			};
			static_assert(
				std::size(table) == static_cast<std::size_t>(image_usage::num_enumerators),
				"Incorrect size for lookup table for D3D12_RESOURCE_STATES"
			);
			return table[static_cast<std::size_t>(st)];
		}

		D3D12_RESOURCE_STATES for_buffer_usage(buffer_usage st) {
			static const D3D12_RESOURCE_STATES table[]{
				D3D12_RESOURCE_STATE_INDEX_BUFFER,
				D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
				D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER |
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
					D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_COPY_SOURCE,
				D3D12_RESOURCE_STATE_COPY_DEST,
			};
			static_assert(
				std::size(table) == static_cast<std::size_t>(buffer_usage::num_enumerators),
				"Incorrect size for lookup table for D3D12_RESOURCE_STATES"
			);
			return table[static_cast<std::size_t>(st)];
		}

		D3D12_HEAP_TYPE for_heap_type(heap_type ty) {
			static const D3D12_HEAP_TYPE table[]{
				D3D12_HEAP_TYPE_DEFAULT,
				D3D12_HEAP_TYPE_UPLOAD,
				D3D12_HEAP_TYPE_READBACK,
			};
			static_assert(
				std::size(table) == static_cast<std::size_t>(heap_type::num_enumerators),
				"Incorrect size for lookup table for D3D12_HEAP_TYPE"
			);
			return table[static_cast<std::size_t>(ty)];
		}


		D3D12_RENDER_TARGET_BLEND_DESC for_render_target_blend_options(
			const render_target_blend_options &opt
		) {
			D3D12_RENDER_TARGET_BLEND_DESC result = {};
			result.BlendEnable = opt.enabled;
			result.SrcBlend = for_blend_factor(opt.source_color);
			result.DestBlend = for_blend_factor(opt.destination_color);
			result.BlendOp = for_blend_operation(opt.color_operation);
			result.SrcBlendAlpha = for_blend_factor(opt.source_alpha);
			result.DestBlendAlpha = for_blend_factor(opt.destination_alpha);
			result.BlendOp = for_blend_operation(opt.color_operation);
			result.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL; // TODO
			return result;
		}

		D3D12_BLEND_DESC for_blend_options(const blend_options &opt) {
			D3D12_BLEND_DESC result = {};
			// TODO handle logic operations
			constexpr std::size_t _num_render_targets =
				std::min(std::size(result.RenderTarget), num_color_render_targets);
			result.IndependentBlendEnable = true;
			for (std::size_t i = 0; i < _num_render_targets; ++i) {
				result.RenderTarget[i] = for_render_target_blend_options(opt.render_target_options[i]);
			}
			return result;
		}

		D3D12_RENDER_PASS_RENDER_TARGET_DESC for_render_target_pass_options(
			const render_target_pass_options &opt
		) {
			D3D12_RENDER_PASS_RENDER_TARGET_DESC result = {};
			result.BeginningAccess.Clear.ClearValue.Format = for_pixel_format(opt.format);
			result.BeginningAccess.Type = for_pass_load_operation(opt.load_operation);
			result.EndingAccess.Type = for_pass_store_operation(opt.store_operation);
			return result;
		}

		D3D12_RENDER_PASS_DEPTH_STENCIL_DESC for_depth_stencil_pass_options(
			const depth_stencil_pass_options &opt
		) {
			D3D12_RENDER_PASS_DEPTH_STENCIL_DESC result = {};
			DXGI_FORMAT format = for_pixel_format(opt.format);
			if (is_empty(opt.format.get_pixel_type() & pixel_type::depth_bit)) {
				result.DepthBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS;
				result.DepthEndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS;
			} else {
				result.DepthBeginningAccess.Clear.ClearValue.Format = format;
				result.DepthBeginningAccess.Type = for_pass_load_operation(opt.depth_load_operation);
				result.DepthEndingAccess.Type = for_pass_store_operation(opt.depth_store_operation);
			}
			if (is_empty(opt.format.get_pixel_type() & pixel_type::stencil_bit)) {
				result.StencilBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS;
				result.StencilEndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS;
			} else {
				result.StencilBeginningAccess.Clear.ClearValue.Format = format;
				result.StencilBeginningAccess.Type = for_pass_load_operation(opt.stencil_load_operation);
				result.StencilEndingAccess.Type = for_pass_store_operation(opt.stencil_store_operation);
			}
			return result;
		}
	}


	descriptor_heap::descriptor_heap(
		device *device, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT capacity
	) : _device(device->_device), _next(0) {
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type           = type;
		desc.NumDescriptors = capacity;
		desc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		desc.NodeMask       = 0;
		_details::assert_dx(_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&_heap)));
	}

	descriptor descriptor_heap::allocate() {
		auto result = _heap->GetCPUDescriptorHandleForHeapStart();
		UINT descriptor_size = _device->GetDescriptorHandleIncrementSize(_heap->GetDesc().Type);
		std::size_t index = 0;
		if (!_free.empty()) {
			index = _free.back();
			_free.pop_back();
		} else {
			index = _next;
			++_next;
		}
		result.ptr += descriptor_size * index;
		return descriptor(result);
	}

	void descriptor_heap::destroy(descriptor desc) {
		UINT descriptor_size = _device->GetDescriptorHandleIncrementSize(_heap->GetDesc().Type);
		_free.emplace_back(static_cast<std::uint16_t>(
			(desc._descriptor.ptr - _heap->GetCPUDescriptorHandleForHeapStart().ptr) / descriptor_size
		));
		desc._descriptor = descriptor::_destroyed;
	}
}
