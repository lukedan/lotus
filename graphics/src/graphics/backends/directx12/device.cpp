#include "lotus/graphics/backends/directx12/device.h"

/// \file
/// Implementation of DirectX 12 devices.

#include <algorithm>

#include "lotus/utils/stack_allocator.h"
#include "lotus/system/platforms/windows/details.h"
#include "lotus/graphics/commands.h"
#include "lotus/graphics/resources.h"
#include "lotus/graphics/synchronization.h"
#include "lotus/graphics/descriptors.h"

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

	descriptor_set_layout device::create_descriptor_set_layout(
		std::span<const descriptor_range> ranges, shader_stage_mask visible_stages
	) {
		descriptor_set_layout result;
		result._ranges.resize(ranges.size());
		UINT total_count = 0;
		for (std::size_t i = 0; i < ranges.size(); ++i) {
			auto &dst = result._ranges[i];
			const auto &src = ranges[i];
			dst = {};
			dst.RangeType                         = _details::conversions::for_descriptor_type(src.type);
			dst.NumDescriptors                    = static_cast<UINT>(src.count);
			dst.BaseShaderRegister                = static_cast<UINT>(src.register_index);
			dst.Flags                             = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
			dst.OffsetInDescriptorsFromTableStart = total_count;
			total_count += dst.NumDescriptors;
		}
		result._visibility = _details::conversions::for_shader_stage_mask(visible_stages);
		return result;
	}

	pipeline_resources device::create_pipeline_resources(
		std::span<const graphics::descriptor_set_layout *const> sets
	) {
		_details::com_ptr<ID3DBlob> signature;
		{ // serialize root signature
			auto bookmark = stack_allocator::scoped_bookmark::create();

			auto root_params = stack_allocator::for_this_thread().create_vector_array<D3D12_ROOT_PARAMETER1>(
				sets.size()
			);
			auto descriptor_tables = stack_allocator::for_this_thread().create_reserved_vector_array<
				std::vector<D3D12_DESCRIPTOR_RANGE1, stack_allocator::allocator<D3D12_DESCRIPTOR_RANGE1>>
			>(sets.size());

			for (std::size_t i = 0; i < sets.size(); ++i) {
				auto &set = *static_cast<const descriptor_set_layout*>(sets[i]);

				auto &table = descriptor_tables.emplace_back(
					stack_allocator::for_this_thread().create_vector_array<D3D12_DESCRIPTOR_RANGE1>(
						set._ranges.begin(), set._ranges.end()
					)
				);
				// we're emulating Vulkan style here - descriptor set i uses the i-th register space
				for (auto &range : table) {
					range.RegisterSpace = static_cast<UINT>(i);
				}

				root_params[i].ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				root_params[i].DescriptorTable.NumDescriptorRanges = static_cast<UINT>(table.size());
				root_params[i].DescriptorTable.pDescriptorRanges   = table.data();
				root_params[i].ShaderVisibility                    = set._visibility;
			}

			D3D12_ROOT_PARAMETER1 param = {};
			param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			D3D12_VERSIONED_ROOT_SIGNATURE_DESC desc = {};
			desc.Version                = D3D_ROOT_SIGNATURE_VERSION_1_1;
			desc.Desc_1_1.NumParameters = static_cast<UINT>(root_params.size());
			desc.Desc_1_1.pParameters   = root_params.data();
			desc.Desc_1_1.Flags         = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

			_details::com_ptr<ID3DBlob> error;
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
		const blend_options &blend,
		const rasterizer_options &rasterizer,
		const depth_stencil_options &depth_stencil,
		std::span<const input_buffer_layout> input_buffers,
		primitive_topology topology,
		const pass_resources &environment,
		[[maybe_unused]] std::size_t num_viewports
	) {
		auto bookmark = stack_allocator::scoped_bookmark::create();

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
		desc.BlendState        = _details::conversions::for_blend_options(blend);
		desc.SampleMask        = std::numeric_limits<UINT>::max();
		desc.RasterizerState   = _details::conversions::for_rasterizer_options(rasterizer);
		desc.DepthStencilState = _details::conversions::for_depth_stencil_options(depth_stencil);

		// gather & convert input buffer
		std::size_t total_elements = 0;
		for (auto &buf : input_buffers) {
			total_elements += buf.elements.size();
		}
		auto element_desc = stack_allocator::for_this_thread().create_reserved_vector_array<
			D3D12_INPUT_ELEMENT_DESC
		>(total_elements);
		for (auto &buf : input_buffers) {
			auto input_rate = _details::conversions::for_input_buffer_rate(buf.input_rate);
			for (auto &elem : buf.elements) {
				auto &elem_desc = element_desc.emplace_back();
				elem_desc.SemanticName         = elem.semantic_name;
				elem_desc.SemanticIndex        = elem.semantic_index;
				elem_desc.Format               = _details::conversions::for_format(elem.element_format);
				elem_desc.InputSlot            = static_cast<UINT>(buf.buffer_index);
				elem_desc.AlignedByteOffset    = static_cast<UINT>(elem.byte_offset);
				elem_desc.InputSlotClass       = input_rate;
				elem_desc.InstanceDataStepRate = input_rate == D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA ? 0 : 1;
			}
		}
		desc.InputLayout.NumElements        = static_cast<UINT>(element_desc.size());
		desc.InputLayout.pInputElementDescs = element_desc.data();
		desc.IBStripCutValue                = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED; // TODO
		desc.PrimitiveTopologyType          = _details::conversions::for_primitive_topology_type(topology);

		desc.NumRenderTargets = static_cast<UINT>(environment._num_render_targets);
		for (std::size_t i = 0; i < environment._num_render_targets; ++i) {
			desc.RTVFormats[i] = environment._render_target_format[i];
		}
		desc.DSVFormat                       = environment._depth_stencil_format;
		
		// TODO multisample settings
		desc.SampleDesc.Count                = 1;
		desc.SampleDesc.Quality              = 0;

		desc.NodeMask                        = 0;
		desc.CachedPSO.pCachedBlob           = nullptr;
		desc.CachedPSO.CachedBlobSizeInBytes = 0;
		desc.Flags                           = D3D12_PIPELINE_STATE_FLAG_NONE;

		_details::assert_dx(_device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&result._pipeline)));
		result._root_signature = resources._signature;
		result._topology = _details::conversions::for_primitive_topology(topology);

		return result;
	}

	pass_resources device::create_pass_resources(
		std::span<const render_target_pass_options> rtv, depth_stencil_pass_options dsv
	) {
		pass_resources result;
		assert(rtv.size() <= result._render_targets.size());
		result._num_render_targets = static_cast<std::uint8_t>(rtv.size());
		for (std::size_t i = 0; i < rtv.size(); ++i) {
			result._render_targets[i]       = _details::conversions::for_render_target_pass_options(rtv[i]);
			result._render_target_format[i] = _details::conversions::for_format(rtv[i].pixel_format);
		}
		result._depth_stencil        = _details::conversions::for_depth_stencil_pass_options(dsv);
		result._depth_stencil_format = _details::conversions::for_format(dsv.pixel_format);
		result._flags                = D3D12_RENDER_PASS_FLAG_NONE;
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

	shader device::load_shader(std::span<const std::byte> data) {
		shader result;
		result._shader.pShaderBytecode = data.data();
		result._shader.BytecodeLength  = static_cast<SIZE_T>(data.size());
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

	image2d_view device::create_image2d_view_from(const image2d &img, format fmt, mip_levels mip) {
		image2d_view result = nullptr;
		result._image = img._image;
		result._format = _details::conversions::for_format(fmt);
		result._desc = {};
		result._desc.MostDetailedMip = static_cast<UINT>(mip.minimum);
		result._desc.MipLevels = static_cast<UINT>(
			mip.num_levels == mip_levels::all_mip_levels ? -1 : mip.num_levels
		);
		result._desc.PlaneSlice = 0;
		result._desc.ResourceMinLODClamp = 0.0f;
		return result;
	}

	sampler device::create_sampler(
		filtering minification, filtering magnification, filtering mipmapping,
		float mip_lod_bias, float min_lod, float max_lod, std::optional<float> max_anisotropy,
		sampler_address_mode addressing_u, sampler_address_mode addressing_v, sampler_address_mode addressing_w,
		linear_rgba_f border_color, std::optional<comparison_function> comparison
	) {
		sampler result;
		result._desc = {};
		result._desc.Filter         = _details::conversions::for_filtering(
			minification, magnification, mipmapping, max_anisotropy.has_value(), comparison.has_value()
		);
		result._desc.AddressU       = _details::conversions::for_sampler_address_mode(addressing_u);
		result._desc.AddressV       = _details::conversions::for_sampler_address_mode(addressing_v);
		result._desc.AddressW       = _details::conversions::for_sampler_address_mode(addressing_w);
		result._desc.MipLODBias     = mip_lod_bias;
		result._desc.MaxAnisotropy  = std::clamp<UINT>(
			static_cast<UINT>(std::round(max_anisotropy.value_or(0.0f))), 1, 16
		);
		result._desc.ComparisonFunc =
			comparison.has_value() ?
			_details::conversions::for_comparison_function(comparison.value()) :
			D3D12_COMPARISON_FUNC_ALWAYS;
		result._desc.BorderColor[0] = border_color.r;
		result._desc.BorderColor[1] = border_color.g;
		result._desc.BorderColor[2] = border_color.b;
		result._desc.BorderColor[3] = border_color.a;
		result._desc.MinLOD         = min_lod;
		result._desc.MaxLOD         = max_lod;
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
