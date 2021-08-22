#include "lotus/graphics/backends/directx12/device.h"

/// \file
/// Implementation of DirectX 12 devices.

#include <algorithm>

#include "lotus/utils/stack_allocator.h"
#include "lotus/system/platforms/windows/details.h"
#include "lotus/graphics/commands.h"
#include "lotus/graphics/resources.h"
#include "lotus/graphics/synchronization.h"

namespace lotus::graphics::backends::directx12 {
	command_queue device::create_command_queue() {
		command_queue result;
		D3D12_COMMAND_QUEUE_DESC desc = {};
		desc.Type     = D3D12_COMMAND_LIST_TYPE_DIRECT;
		desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		desc.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE;
		desc.NodeMask = 0;
		_details::assert_dx(_device->CreateCommandQueue(&desc, IID_PPV_ARGS(&result._queue)));
		return result;
	}

	command_allocator device::create_command_allocator() {
		command_allocator result;
		_details::assert_dx(_device->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&result._allocator)
		));
		return result;
	}

	command_list device::create_command_list(command_allocator &alloc) {
		command_list result = nullptr;
		_details::assert_dx(_device->CreateCommandList(
			0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc._allocator.Get(), nullptr, IID_PPV_ARGS(&result._list)
		));
		return result;
	}

	pipeline_resources device::create_pipeline_resources() {
		_details::com_ptr<ID3DBlob> signature;
		{ // serialize root signature
			_details::com_ptr<ID3DBlob> error;
			D3D12_VERSIONED_ROOT_SIGNATURE_DESC desc = {};
			desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
			desc.Desc_1_1.NumParameters = 0;
			HRESULT hr = D3D12SerializeVersionedRootSignature(&desc, &signature, &error);
			if (FAILED(hr)) {
				std::cerr <<
					"Failed to serialize root signature: " <<
					std::string_view(static_cast<const char*>(error->GetBufferPointer()), error->GetBufferSize()) <<
					std::endl;
				_details::assert_dx(hr);
			}
		}

		pipeline_resources result;
		_details::assert_dx(_device->CreateRootSignature(
			0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&result._signature)
		));
		return result;
	}

	pipeline_state device::create_pipeline_state(
		pipeline_resources &resources,
		const shader *vertex_shader,
		const shader *pixel_shader,
		const shader *domain_shader,
		const shader *hull_shader,
		const shader *geometry_shader,
		const blend_options &blend
	) {
		pipeline_state result;
		D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
		desc.pRootSignature = resources._signature.Get();
		if (vertex_shader) {
			desc.VS = vertex_shader->_shader;
		}
		if (pixel_shader) {
			desc.PS = pixel_shader->_shader;
		}
		if (domain_shader) {
			desc.DS = domain_shader->_shader;
		}
		if (hull_shader) {
			desc.HS = hull_shader->_shader;
		}
		if (geometry_shader) {
			desc.GS = geometry_shader->_shader;
		}
		// TODO stream output?
		desc.BlendState = _details::conversions::for_blend_options(blend);
		// TODO
		_device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&result._pipeline));
		return result;
	}

	pass_resources device::create_pass_resources(
		std::span<render_target_pass_options> rtv, depth_stencil_pass_options dsv
	) {
		pass_resources result;
		assert(rtv.size() <= result._render_targets.size());
		result._num_render_targets = static_cast<std::uint8_t>(rtv.size());
		for (std::size_t i = 0; i < rtv.size(); ++i) {
			result._render_targets[i] = _details::conversions::for_render_target_pass_options(rtv[i]);
		}
		result._depth_stencil = _details::conversions::for_depth_stencil_pass_options(dsv);
		result._flags = D3D12_RENDER_PASS_FLAG_NONE;
		return result;
	}

	descriptor_pool device::create_descriptor_pool() {
		descriptor_pool result;
		// TODO parameters
		UINT num_shader_rsrc_descriptors = 0;
		UINT num_sampler_descriptors = 0;
		if (num_shader_rsrc_descriptors > 0) {
			D3D12_DESCRIPTOR_HEAP_DESC shader_rsrc_desc = {};
			shader_rsrc_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			shader_rsrc_desc.NumDescriptors = num_shader_rsrc_descriptors;
			shader_rsrc_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			shader_rsrc_desc.NodeMask = 0;
			_details::assert_dx(_device->CreateDescriptorHeap(
				&shader_rsrc_desc, IID_PPV_ARGS(&result._shader_resource_heap)
			));
		}
		if (num_sampler_descriptors > 0) {
			D3D12_DESCRIPTOR_HEAP_DESC sampler_desc = {};
			sampler_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
			sampler_desc.NumDescriptors = num_sampler_descriptors;
			sampler_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			sampler_desc.NodeMask = 0;
			_details::assert_dx(_device->CreateDescriptorHeap(
				&sampler_desc, IID_PPV_ARGS(&result._sampler_heap)
			));
		}
		return result;
	}

	device_heap device::create_device_heap(std::size_t size, heap_type type) {
		device_heap result;
		D3D12_HEAP_DESC desc = {};
		desc.SizeInBytes = size;
		desc.Properties.Type                 = _details::conversions::for_heap_type(type);
		desc.Properties.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		desc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		desc.Properties.CreationNodeMask     = 0;
		desc.Properties.VisibleNodeMask      = 0;
		if (type == heap_type::device_only) {
			desc.Flags = D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES | D3D12_HEAP_FLAG_ALLOW_SHADER_ATOMICS;
		} else {
			desc.Flags = D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES;
		}
		_details::assert_dx(_device->CreateHeap(&desc, IID_PPV_ARGS(&result._heap)));
		return result;
	}

	buffer device::create_committed_buffer(std::size_t size, heap_type type, buffer_usage usage) {
		buffer result;
		D3D12_HEAP_PROPERTIES heap_properties = {};
		heap_properties.Type                 = _details::conversions::for_heap_type(type);
		heap_properties.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heap_properties.CreationNodeMask     = 0;
		heap_properties.VisibleNodeMask      = 0;
		D3D12_RESOURCE_DESC desc = {};
		desc.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Alignment          = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
		desc.Width              = size;
		desc.Height             = 1;
		desc.DepthOrArraySize   = 1;
		desc.MipLevels          = 1;
		desc.Format             = DXGI_FORMAT_UNKNOWN;
		desc.SampleDesc.Count   = 1;
		desc.SampleDesc.Quality = 0;
		desc.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.Flags              = D3D12_RESOURCE_FLAG_NONE;
		D3D12_RESOURCE_STATES states = _details::conversions::for_buffer_usage(usage);
		D3D12_HEAP_FLAGS heap_flags = D3D12_HEAP_FLAG_NONE;
		if (type == heap_type::device_only) {
			desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			heap_flags = D3D12_HEAP_FLAG_ALLOW_SHADER_ATOMICS;
		} else {
			if (type == heap_type::upload) {
				assert(usage == buffer_usage::copy_source);
				states = D3D12_RESOURCE_STATE_GENERIC_READ;
			}
		}
		_details::assert_dx(_device->CreateCommittedResource(
			&heap_properties, heap_flags, &desc, states, nullptr, IID_PPV_ARGS(&result._buffer)
		));
		return result;
	}

	void *device::map_buffer(buffer &buf, std::size_t begin, std::size_t length) {
		void *result = nullptr;
		D3D12_RANGE range = {};
		range.Begin = static_cast<SIZE_T>(begin);
		range.End   = static_cast<SIZE_T>(begin + length);
		_details::assert_dx(buf._buffer->Map(0, &range, &result));
		return static_cast<std::byte*>(result) + begin;
	}

	void device::unmap_buffer(buffer &buf, std::size_t begin, std::size_t length) {
		D3D12_RANGE range = {};
		range.Begin = static_cast<SIZE_T>(begin);
		range.End   = static_cast<SIZE_T>(begin + length);
		buf._buffer->Unmap(0, &range);
	}

	image2d_view device::create_image2d_view_from(const image2d &img, pixel_format fmt, mip_levels mip) {
		image2d_view result = nullptr;
		result._image = img._image;
		result._format = _details::conversions::for_pixel_format(fmt);
		result._desc = {};
		result._desc.MostDetailedMip = static_cast<UINT>(mip.minimum);
		result._desc.MipLevels = static_cast<UINT>(
			mip.num_levels == mip_levels::all_mip_levels ? -1 : mip.num_levels
		);
		result._desc.PlaneSlice = 0;
		result._desc.ResourceMinLODClamp = 0.0f;
		return result;
	}

	frame_buffer device::create_frame_buffer(
		std::span<const graphics::image2d_view*> color, const image2d_view *depth_stencil, const pass_resources&
	) {
		frame_buffer result(this);
		for (std::size_t i = 0; i < color.size(); ++i) {
			D3D12_RENDER_TARGET_VIEW_DESC desc = {};
			color[i]->_fill_rtv_desc(desc);
			result._color[i] = _rtv_descriptors.allocate();
			_device->CreateRenderTargetView(color[i]->_image.Get(), &desc, result._color[i].get());
		}
		if (depth_stencil) {
			D3D12_DEPTH_STENCIL_VIEW_DESC desc = {};
			depth_stencil->_fill_dsv_desc(desc);
			result._depth_stencil = _dsv_descriptors.allocate();
			_device->CreateDepthStencilView(depth_stencil->_image.Get(), &desc, result._depth_stencil.get());
		}
		return result;
	}

	fence device::create_fence(synchronization_state state) {
		fence result = nullptr;
		_details::assert_dx(_device->CreateFence(
			static_cast<UINT64>(state), D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&result._fence)
		));
		return result;
	}

	void device::reset_fence(fence &f) {
		_details::assert_dx(f._fence->Signal(static_cast<UINT64>(synchronization_state::unset)));
	}

	void device::wait_for_fence(fence &f) {
		_details::assert_dx(f._fence->SetEventOnCompletion(
			static_cast<UINT64>(synchronization_state::set), nullptr
		));
	}

	device::device(_details::com_ptr<ID3D12Device8> dev) :
		_device(std::move(dev)),
		_rtv_descriptors(this, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, descriptor_heap_size),
		_dsv_descriptors(this, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, descriptor_heap_size) {
	}

	void device::_set_debug_name(ID3D12Object &obj, const char8_t *name) {
		_details::assert_dx(obj.SetPrivateData(
			WKPDID_D3DDebugObjectName, static_cast<UINT>(std::strlen(reinterpret_cast<const char*>(name))), name
		));
	}


	device adapter::create_device() {
		_details::com_ptr<ID3D12Device8> result;
		_details::assert_dx(D3D12CreateDevice(_adapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&result)));
		return device(std::move(result));
	}

	adapter_properties adapter::get_properties() const {
		adapter_properties result;
		DXGI_ADAPTER_DESC1 desc;
		_details::assert_dx(_adapter->GetDesc1(&desc));
		// TODO
		return result;
	}
}
