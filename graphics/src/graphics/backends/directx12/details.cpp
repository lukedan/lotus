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

		D3D12_CULL_MODE for_cull_mode(cull_mode mode) {
			static const D3D12_CULL_MODE table[] {
				D3D12_CULL_MODE_NONE,
				D3D12_CULL_MODE_FRONT,
				D3D12_CULL_MODE_BACK,
			};
			static_assert(
				std::size(table) == static_cast<std::size_t>(cull_mode::num_enumerators),
				"Incorrect size for lookup table for D3D12_CULL_MODE"
			);
			return table[static_cast<std::size_t>(mode)];
		}

		D3D12_STENCIL_OP for_stencil_operation(stencil_operation op) {
			static const D3D12_STENCIL_OP table[]{
				D3D12_STENCIL_OP_KEEP,
				D3D12_STENCIL_OP_ZERO,
				D3D12_STENCIL_OP_REPLACE,
				D3D12_STENCIL_OP_INCR_SAT,
				D3D12_STENCIL_OP_DECR_SAT,
				D3D12_STENCIL_OP_INVERT,
				D3D12_STENCIL_OP_INCR,
				D3D12_STENCIL_OP_DECR,
			};
			static_assert(
				std::size(table) == static_cast<std::size_t>(stencil_operation::num_enumerators),
				"Incorrect size for lookup table for D3D12_STENCIL_OP"
			);
			return table[static_cast<std::size_t>(op)];
		}

		D3D12_INPUT_CLASSIFICATION for_input_buffer_rate(input_buffer_rate rate) {
			static const D3D12_INPUT_CLASSIFICATION table[]{
				D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
				D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,
			};
			static_assert(
				std::size(table) == static_cast<std::size_t>(input_buffer_rate::num_enumerators),
				"Incorrect size for lookup table for D3D12_INPUT_CLASSIFICATION"
			);
			return table[static_cast<std::size_t>(rate)];
		}

		D3D12_PRIMITIVE_TOPOLOGY_TYPE for_primitive_topology_type(primitive_topology topology) {
			static const D3D12_PRIMITIVE_TOPOLOGY_TYPE table[]{
				D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT,
				D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE,
				D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE,
				D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
				D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
				D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE,
				D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE,
				D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
				D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
			};
			static_assert(
				std::size(table) == static_cast<std::size_t>(primitive_topology::num_enumerators),
				"Incorrect size for lookup table for D3D12_INPUT_CLASSIFICATION"
			);
			return table[static_cast<std::size_t>(topology)];
		}

		D3D_PRIMITIVE_TOPOLOGY for_primitive_topology(primitive_topology topology) {
			static const D3D_PRIMITIVE_TOPOLOGY table[]{
				D3D_PRIMITIVE_TOPOLOGY_POINTLIST,
				D3D_PRIMITIVE_TOPOLOGY_LINELIST,
				D3D_PRIMITIVE_TOPOLOGY_LINESTRIP,
				D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
				D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,
				D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ,
				D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ,
				D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ,
				D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ,
			};
			static_assert(
				std::size(table) == static_cast<std::size_t>(primitive_topology::num_enumerators),
				"Incorrect size for lookup table for D3D12_INPUT_CLASSIFICATION"
			);
			return table[static_cast<std::size_t>(topology)];
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

		D3D12_DESCRIPTOR_RANGE_TYPE for_descriptor_type(descriptor_type ty) {
			static const D3D12_DESCRIPTOR_RANGE_TYPE table[]{
				D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
				D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
				D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
				D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
				D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
			};
			static_assert(
				std::size(table) == static_cast<std::size_t>(descriptor_type::num_enumerators),
				"Incorrect size for lookup table for D3D12_DESCRIPTOR_RANGE_TYPE"
			);
			return table[static_cast<std::size_t>(ty)];
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

		D3D12_TEXTURE_ADDRESS_MODE for_sampler_address_mode(sampler_address_mode mode) {
			static const D3D12_TEXTURE_ADDRESS_MODE table[]{
				D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				D3D12_TEXTURE_ADDRESS_MODE_MIRROR,
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
				D3D12_TEXTURE_ADDRESS_MODE_BORDER,
			};
			static_assert(
				std::size(table) == static_cast<std::size_t>(sampler_address_mode::num_enumerators),
				"Incorrect size for lookup table for D3D12_TEXTURE_ADDRESS_MODE"
			);
			return table[static_cast<std::size_t>(mode)];
		}

		D3D12_COMPARISON_FUNC for_comparison_function(comparison_function mode) {
			static const D3D12_COMPARISON_FUNC table[]{
				D3D12_COMPARISON_FUNC_NEVER,
				D3D12_COMPARISON_FUNC_LESS,
				D3D12_COMPARISON_FUNC_EQUAL,
				D3D12_COMPARISON_FUNC_LESS_EQUAL,
				D3D12_COMPARISON_FUNC_GREATER,
				D3D12_COMPARISON_FUNC_NOT_EQUAL,
				D3D12_COMPARISON_FUNC_GREATER_EQUAL,
				D3D12_COMPARISON_FUNC_ALWAYS,
			};
			static_assert(
				std::size(table) == static_cast<std::size_t>(comparison_function::num_enumerators),
				"Incorrect size for lookup table for D3D12_COMPARISON_FUNC"
			);
			return table[static_cast<std::size_t>(mode)];
		}


		D3D12_COLOR_WRITE_ENABLE for_channel_mask(channel_mask mask) {
			static const std::pair<channel_mask, D3D12_COLOR_WRITE_ENABLE> table[]{
				{ channel_mask::red,   D3D12_COLOR_WRITE_ENABLE_RED   },
				{ channel_mask::green, D3D12_COLOR_WRITE_ENABLE_GREEN },
				{ channel_mask::blue,  D3D12_COLOR_WRITE_ENABLE_BLUE  },
				{ channel_mask::alpha, D3D12_COLOR_WRITE_ENABLE_ALPHA },
			};
			std::uint8_t result = 0;
			for (auto [myval, dxval] : table) {
				if ((mask & myval) == myval) {
					result |= dxval;
				}
			}
			return static_cast<D3D12_COLOR_WRITE_ENABLE>(result);
		}

		D3D12_SHADER_VISIBILITY for_shader_stage_mask(shader_stage_mask mask) {
			static const std::pair<shader_stage_mask, D3D12_SHADER_VISIBILITY> table[]{
				{ shader_stage_mask::vertex_shader,   D3D12_SHADER_VISIBILITY_VERTEX   },
				{ shader_stage_mask::geometry_shader, D3D12_SHADER_VISIBILITY_GEOMETRY },
				{ shader_stage_mask::pixel_shader,    D3D12_SHADER_VISIBILITY_PIXEL    },
				{ shader_stage_mask::compute_shader,  D3D12_SHADER_VISIBILITY_ALL      },
			};
			std::uint8_t result = 0;
			for (auto [myval, dxval] : table) {
				if ((mask & myval) == myval) {
					result |= dxval;
				}
			}
			return static_cast<D3D12_SHADER_VISIBILITY>(result);
		}


		DXGI_FORMAT for_format(format fmt) {
			if (fmt == format::none) {
				return DXGI_FORMAT_UNKNOWN;
			}
			if (fmt == format::r8g8b8a8_unorm) {
				return DXGI_FORMAT_R8G8B8A8_UNORM;
			}
			if (fmt == format::r32g32_float) {
				return DXGI_FORMAT_R32G32_FLOAT;
			}
			if (fmt == format::r32g32b32a32_float) {
				return DXGI_FORMAT_R32G32B32A32_FLOAT;
			}
			// TODO
			assert(false);
		}

		D3D12_FILTER for_filtering(
			filtering minification, filtering magnification, filtering mipmapping, bool anisotropic, bool comparison
		) {
			constexpr auto _num_filtering_types = static_cast<std::size_t>(filtering::num_enumerators);
			//                                     minification          magnification         mipmapping
			using _table_type = const D3D12_FILTER[_num_filtering_types][_num_filtering_types][_num_filtering_types];
			static _table_type non_comparison_table{
				{
					{ D3D12_FILTER_MIN_MAG_MIP_POINT,              D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR        },
					{ D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT, D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR        },
				}, {
					{ D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT,       D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR },
					{ D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT,       D3D12_FILTER_MIN_MAG_MIP_LINEAR              },
				}
			};
			static _table_type comparison_table{
				{
					{ D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT,              D3D12_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR        },
					{ D3D12_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT, D3D12_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR        },
				}, {
					{ D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT,       D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR },
					{ D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,       D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR              },
				}
			};
			static_assert(
				static_cast<std::size_t>(filtering::num_enumerators) == 2,
				"Table for filtering type conversion needs updating"
			);
			if (anisotropic) {
				return comparison ? D3D12_FILTER_COMPARISON_ANISOTROPIC : D3D12_FILTER_ANISOTROPIC;
			}
			_table_type &table = comparison ? comparison_table : non_comparison_table;
			return table
				[static_cast<std::size_t>(minification)]
				[static_cast<std::size_t>(magnification)]
				[static_cast<std::size_t>(mipmapping)];
		}


		D3D12_VIEWPORT for_viewport(const viewport &vp) {
			cvec2f size = vp.xy.signed_size();
			D3D12_VIEWPORT result = {};
			result.TopLeftX = vp.xy.min[0];
			result.TopLeftY = vp.xy.min[1];
			result.Width    = size[0];
			result.Height   = size[1];
			result.MinDepth = vp.minimum_depth;
			result.MaxDepth = vp.maximum_depth;
			return result;
		}

		D3D12_RECT for_rect(const aab2i &rect) {
			D3D12_RECT result = {};
			result.left   = static_cast<LONG>(rect.min[0]);
			result.top    = static_cast<LONG>(rect.min[1]);
			result.right  = static_cast<LONG>(rect.max[0]);
			result.bottom = static_cast<LONG>(rect.max[1]);
			return result;
		}


		D3D12_RENDER_TARGET_BLEND_DESC for_render_target_blend_options(
			const render_target_blend_options &opt
		) {
			D3D12_RENDER_TARGET_BLEND_DESC result = {};
			result.BlendEnable           = opt.enabled;
			result.SrcBlend              = for_blend_factor(opt.source_color);
			result.DestBlend             = for_blend_factor(opt.destination_color);
			result.BlendOp               = for_blend_operation(opt.color_operation);
			result.SrcBlendAlpha         = for_blend_factor(opt.source_alpha);
			result.DestBlendAlpha        = for_blend_factor(opt.destination_alpha);
			result.BlendOp               = for_blend_operation(opt.color_operation);
			result.RenderTargetWriteMask = static_cast<UINT8>(for_channel_mask(opt.write_mask));
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

		D3D12_RASTERIZER_DESC for_rasterizer_options(const rasterizer_options &opt) {
			D3D12_RASTERIZER_DESC result = {};
			result.FillMode              = opt.is_wireframe ? D3D12_FILL_MODE_WIREFRAME : D3D12_FILL_MODE_SOLID;
			result.CullMode              = for_cull_mode(opt.culling);
			result.FrontCounterClockwise = opt.front_facing == front_facing_mode::counter_clockwise;
			result.DepthBias             = static_cast<INT>(std::round(opt.depth_bias.bias));
			result.DepthBiasClamp        = opt.depth_bias.clamp;
			result.SlopeScaledDepthBias  = opt.depth_bias.slope_scaled_bias;
			result.DepthClipEnable       = false; // TODO
			result.MultisampleEnable     = false; // TODO
			result.AntialiasedLineEnable = false;
			result.ForcedSampleCount     = 0;
			result.ConservativeRaster    = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
			return result;
		}

		D3D12_DEPTH_STENCILOP_DESC for_stencil_options(const stencil_options &op) {
			D3D12_DEPTH_STENCILOP_DESC result = {};
			result.StencilFailOp      = for_stencil_operation(op.fail);
			result.StencilDepthFailOp = for_stencil_operation(op.depth_fail);
			result.StencilPassOp      = for_stencil_operation(op.pass);
			result.StencilFunc        = for_comparison_function(op.comparison);
			return result;
		}

		D3D12_DEPTH_STENCIL_DESC for_depth_stencil_options(const depth_stencil_options &opt) {
			D3D12_DEPTH_STENCIL_DESC result = {};
			result.DepthEnable      = opt.enable_depth_testing;
			result.DepthWriteMask   = opt.write_depth ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
			result.DepthFunc        = for_comparison_function(opt.depth_comparison);
			result.StencilEnable    = opt.enable_stencil_testing;
			result.StencilReadMask  = opt.stencil_read_mask;
			result.StencilWriteMask = opt.stencil_write_mask;
			result.FrontFace        = for_stencil_options(opt.stencil_front_face);
			result.BackFace         = for_stencil_options(opt.stencil_back_face);
			return result;
		}

		D3D12_RENDER_PASS_RENDER_TARGET_DESC for_render_target_pass_options(
			const render_target_pass_options &opt
		) {
			D3D12_RENDER_PASS_RENDER_TARGET_DESC result = {};
			result.BeginningAccess.Clear.ClearValue.Format = for_format(opt.pixel_format);
			result.BeginningAccess.Type                    = for_pass_load_operation(opt.load_operation);
			result.EndingAccess.Type                       = for_pass_store_operation(opt.store_operation);
			return result;
		}

		D3D12_RENDER_PASS_DEPTH_STENCIL_DESC for_depth_stencil_pass_options(
			const depth_stencil_pass_options &opt
		) {
			D3D12_RENDER_PASS_DEPTH_STENCIL_DESC result = {};
			DXGI_FORMAT format = for_format(opt.pixel_format);
			if (is_empty(opt.pixel_format.get_data_type() & data_type::depth_bit)) {
				result.DepthBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS;
				result.DepthEndingAccess.Type    = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS;
			} else {
				result.DepthBeginningAccess.Clear.ClearValue.Format = format;
				result.DepthBeginningAccess.Type = for_pass_load_operation(opt.depth_load_operation);
				result.DepthEndingAccess.Type    = for_pass_store_operation(opt.depth_store_operation);
			}
			if (is_empty(opt.pixel_format.get_data_type() & data_type::stencil_bit)) {
				result.StencilBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS;
				result.StencilEndingAccess.Type    = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS;
			} else {
				result.StencilBeginningAccess.Clear.ClearValue.Format = format;
				result.StencilBeginningAccess.Type = for_pass_load_operation(opt.stencil_load_operation);
				result.StencilEndingAccess.Type    = for_pass_store_operation(opt.stencil_store_operation);
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
