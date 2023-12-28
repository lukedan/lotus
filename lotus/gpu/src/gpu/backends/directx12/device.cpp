#include "lotus/gpu/backends/directx12/device.h"

/// \file
/// Implementation of DirectX 12 devices.

#include <algorithm>

#include "lotus/memory/stack_allocator.h"
#include "lotus/logging.h"
#include "lotus/system/platforms/windows/details.h"
#include "lotus/gpu/commands.h"
#include "lotus/gpu/resources.h"
#include "lotus/gpu/synchronization.h"
#include "lotus/gpu/descriptors.h"

// specify that we want preview SDK
extern "C" {
	_declspec(dllexport) extern const UINT D3D12SDKVersion = 706;
	_declspec(dllexport) extern const char* D3D12SDKPath = ".\\";
}

namespace lotus::gpu::backends::directx12 {
	back_buffer_info device::acquire_back_buffer(swap_chain &s) {
		back_buffer_info result = uninitialized;
		result.index        = static_cast<std::size_t>(s._swap_chain->GetCurrentBackBufferIndex());
		result.on_presented = s._synchronization[result.index].notify_fence;
		result.status       = swap_chain_status::ok;
		return result;
	}

	void device::resize_swap_chain_buffers(swap_chain &s, cvec2u32 size) {
		_details::assert_dx(s._swap_chain->ResizeBuffers(
			static_cast<UINT>(s.get_image_count()), static_cast<UINT>(size[0]), static_cast<UINT>(size[1]),
			DXGI_FORMAT_UNKNOWN, 0
		));
		for (auto &sync : s._synchronization) {
			sync.next_fence = nullptr;
			sync.notify_fence = nullptr;
		}
	}

	command_allocator device::create_command_allocator(queue_type ty) {
		command_allocator result = nullptr;
		result._type = _details::conversions::to_command_list_type(ty);
		_details::assert_dx(_device->CreateCommandAllocator(result._type, IID_PPV_ARGS(&result._allocator)));
		return result;
	}

	command_list device::create_and_start_command_list(command_allocator &alloc) {
		command_list result = nullptr;
		_details::assert_dx(_device->CreateCommandList(
			0, alloc._type, alloc._allocator.Get(), nullptr, IID_PPV_ARGS(&result._list)
		));
		result._descriptor_heaps[0] = _srv_descriptors.get_heap();
		result._descriptor_heaps[1] = _sampler_descriptors.get_heap();
		result._list->SetDescriptorHeaps(
			static_cast<UINT>(result._descriptor_heaps.size()), result._descriptor_heaps.data()
		);
		return result;
	}

	descriptor_set_layout device::create_descriptor_set_layout(
		std::span<const descriptor_range_binding> ranges, shader_stage visible_stages
	) {
		descriptor_set_layout result = nullptr;
		result._ranges.resize(ranges.size());
		for (std::size_t i = 0; i < ranges.size(); ++i) {
			auto &dst = result._ranges[i];
			const auto &src = ranges[i];
			dst = {};
			dst.RangeType          = _details::conversions::to_descriptor_range_type(src.range.type);
			dst.NumDescriptors     =
				src.range.count == descriptor_range::unbounded_count ? UINT_MAX : static_cast<UINT>(src.range.count);
			dst.BaseShaderRegister = static_cast<UINT>(src.register_index);
			dst.Flags              = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
		}
		// sort the ranges first by type, then by register index, so that we can find a range in O(logn) time
		std::sort(
			result._ranges.begin(), result._ranges.end(),
			[](const D3D12_DESCRIPTOR_RANGE1 &lhs, const D3D12_DESCRIPTOR_RANGE1 &rhs) {
				if (lhs.RangeType == rhs.RangeType) {
					return lhs.BaseShaderRegister < rhs.BaseShaderRegister;
				}
				return lhs.RangeType < rhs.RangeType;
			}
		);
		{ // assign descriptor indices
			std::size_t unbounded_index = result._ranges.size(); // index of the range with unbounded size
			UINT total_count = 0;
			auto it = result._ranges.begin();
			for (; it != result._ranges.end() && it->RangeType != D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER; ++it) {
				if (it->NumDescriptors == UINT_MAX) {
					assert(unbounded_index == result._ranges.size()); // cannot have multiple
					unbounded_index = it - result._ranges.begin();
				} else {
					it->OffsetInDescriptorsFromTableStart = total_count;
					total_count += it->NumDescriptors;
				}
			}
			result._num_shader_resource_descriptors = total_count;
			result._num_shader_resource_ranges = it - result._ranges.begin();
			// put the unbounded one at the very back
			if (unbounded_index < result._ranges.size()) {
				result._ranges[unbounded_index].OffsetInDescriptorsFromTableStart = total_count;
				result._unbounded_range_is_sampler = false;
			}

			// samplers
			total_count = 0;
			for (; it != result._ranges.end(); ++it) {
				assert(it->RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER);
				if (it->NumDescriptors == UINT_MAX) { // not sure why anyone would want bindless samplers, but hey
					assert(unbounded_index == result._ranges.size()); // cannot have multiple
					unbounded_index = it - result._ranges.begin();
				} else {
					it->OffsetInDescriptorsFromTableStart = total_count;
					total_count += it->NumDescriptors;
				}
			}
			// put the unbounded one at the very back
			if (unbounded_index < result._ranges.size() && unbounded_index > result._num_shader_resource_ranges) {
				result._ranges[unbounded_index].OffsetInDescriptorsFromTableStart = total_count;
				result._unbounded_range_is_sampler = true;
			}
			result._num_sampler_descriptors = total_count;
		}
		result._visibility = _details::conversions::to_shader_visibility(visible_stages);
		return result;
	}

	pipeline_resources device::create_pipeline_resources(
		std::span<const gpu::descriptor_set_layout *const> sets
	) {
		_details::com_ptr<ID3DBlob> signature;
		std::vector<pipeline_resources::_root_param_indices> indices(sets.size(), nullptr);
		{ // serialize root signature
			auto bookmark = get_scratch_bookmark();

			auto root_params = bookmark.create_reserved_vector_array<D3D12_ROOT_PARAMETER1>(sets.size() * 2);
			auto descriptor_tables = bookmark.create_reserved_vector_array<
				memory::stack_allocator::vector_type<D3D12_DESCRIPTOR_RANGE1>
			>(sets.size() * 2);

			for (std::size_t i = 0; i < sets.size(); ++i) {
				// here we're emulating Vulkan style - the i-th register space points to two tables, one for shader
				// resources and one for samplers
				auto &set = *static_cast<const descriptor_set_layout*>(sets[i]);

				if (set._num_shader_resource_ranges > 0) { // shader resources
					auto &shader_resource_table = descriptor_tables.emplace_back(
						bookmark.create_vector_array<D3D12_DESCRIPTOR_RANGE1>(
							set._ranges.begin(), set._ranges.begin() + set._num_shader_resource_ranges
						)
					);
					for (auto &range : shader_resource_table) {
						range.RegisterSpace = static_cast<UINT>(i);
					}
					indices[i].resource_index = static_cast<std::uint8_t>(root_params.size());
					auto &root_param = root_params.emplace_back();
					root_param.ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
					root_param.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(shader_resource_table.size());
					root_param.DescriptorTable.pDescriptorRanges   = shader_resource_table.data();
					root_param.ShaderVisibility                    = set._visibility;
				}

				if (set._num_shader_resource_ranges < set._ranges.size()) { // samplers
					auto &sampler_table = descriptor_tables.emplace_back(
						bookmark.create_vector_array<D3D12_DESCRIPTOR_RANGE1>(
							set._ranges.begin() + set._num_shader_resource_ranges, set._ranges.end()
						)
					);
					for (auto &range : sampler_table) {
						range.RegisterSpace = static_cast<UINT>(i);
					}
					indices[i].sampler_index = static_cast<std::uint8_t>(root_params.size());
					auto &root_param = root_params.emplace_back();
					root_param.ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
					root_param.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(sampler_table.size());
					root_param.DescriptorTable.pDescriptorRanges   = sampler_table.data();
					root_param.ShaderVisibility                    = set._visibility;
				}
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
				log().error(
					"Failed to serialize root signature: {}",
					std::string_view(static_cast<const char*>(error->GetBufferPointer()), error->GetBufferSize())
				);
				_details::assert_dx(hr);
			}
		}

		pipeline_resources result = nullptr;
		_details::assert_dx(_device->CreateRootSignature(
			0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&result._signature)
		));
		result._descriptor_table_binding = std::move(indices);
		return result;
	}

	graphics_pipeline_state device::create_graphics_pipeline_state(
		const pipeline_resources &resources,
		const shader_binary *vertex_shader,
		const shader_binary *pixel_shader,
		const shader_binary *domain_shader,
		const shader_binary *hull_shader,
		const shader_binary *geometry_shader,
		std::span<const render_target_blend_options> blend,
		const rasterizer_options &rasterizer,
		const depth_stencil_options &depth_stencil,
		std::span<const input_buffer_layout> input_buffers,
		primitive_topology topology,
		const frame_buffer_layout &fb_layout,
		[[maybe_unused]] std::size_t num_viewports
	) {
		auto bookmark = get_scratch_bookmark();

		graphics_pipeline_state result = nullptr;

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
		desc.BlendState        = _details::conversions::to_blend_description(blend);
		desc.SampleMask        = std::numeric_limits<UINT>::max();
		desc.RasterizerState   = _details::conversions::to_rasterizer_description(rasterizer);
		desc.DepthStencilState = _details::conversions::to_depth_stencil_description(depth_stencil);

		// gather & convert input buffer
		std::size_t total_elements = 0;
		for (auto &buf : input_buffers) {
			total_elements += buf.elements.size();
		}
		auto element_desc = bookmark.create_reserved_vector_array<D3D12_INPUT_ELEMENT_DESC>(total_elements);
		for (auto &buf : input_buffers) {
			auto input_rate = _details::conversions::to_input_classification(buf.input_rate);
			for (auto &elem : buf.elements) {
				auto &elem_desc = element_desc.emplace_back();
				elem_desc.SemanticName         = reinterpret_cast<LPCSTR>(elem.semantic_name);
				elem_desc.SemanticIndex        = elem.semantic_index;
				elem_desc.Format               = _details::conversions::to_format(elem.element_format);
				elem_desc.InputSlot            = static_cast<UINT>(buf.buffer_index);
				elem_desc.AlignedByteOffset    = static_cast<UINT>(elem.byte_offset);
				elem_desc.InputSlotClass       = input_rate;
				elem_desc.InstanceDataStepRate = input_rate == D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA ? 0 : 1;
			}
		}
		desc.InputLayout.NumElements        = static_cast<UINT>(element_desc.size());
		desc.InputLayout.pInputElementDescs = element_desc.data();
		desc.IBStripCutValue                = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED; // TODO
		desc.PrimitiveTopologyType          = _details::conversions::to_primitive_topology_type(topology);

		desc.NumRenderTargets = static_cast<UINT>(fb_layout.color_render_target_formats.size());
		for (std::size_t i = 0; i < fb_layout.color_render_target_formats.size(); ++i) {
			desc.RTVFormats[i] = _details::conversions::to_format(fb_layout.color_render_target_formats[i]);
		}
		desc.DSVFormat                       =
			_details::conversions::to_format(fb_layout.depth_stencil_render_target_format);
		
		// TODO multisample settings
		desc.SampleDesc.Count                = 1;
		desc.SampleDesc.Quality              = 0;

		desc.NodeMask                        = 0;
		desc.CachedPSO.pCachedBlob           = nullptr;
		desc.CachedPSO.CachedBlobSizeInBytes = 0;
		desc.Flags                           = D3D12_PIPELINE_STATE_FLAG_NONE;

		_details::assert_dx(_device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&result._pipeline)));
		result._root_signature = resources._signature;
		result._topology = _details::conversions::to_primitive_topology(topology);

		return result;
	}

	compute_pipeline_state device::create_compute_pipeline_state(const pipeline_resources &rsrc, const shader_binary &shader) {
		compute_pipeline_state result = nullptr;

		D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
		desc.pRootSignature                  = rsrc._signature.Get();
		desc.CS                              = shader._shader;
		desc.NodeMask                        = 0;
		desc.CachedPSO.pCachedBlob           = nullptr;
		desc.CachedPSO.CachedBlobSizeInBytes = 0;
		desc.Flags                           = D3D12_PIPELINE_STATE_FLAG_NONE;

		_details::assert_dx(_device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&result._pipeline)));
		result._descriptor_table_binding = rsrc._descriptor_table_binding;
		result._root_signature = rsrc._signature;

		return result;
	}

	descriptor_pool device::create_descriptor_pool(std::span<const descriptor_range> /*ranges*/, std::size_t) {
		descriptor_pool result = nullptr;
		// TODO set max values
		return result;
	}

	descriptor_set device::create_descriptor_set(descriptor_pool &/*pool*/, const descriptor_set_layout &layout) {
		// check that we don't have unbounded ranges
		if constexpr (is_debugging) {
			for (const auto &range : layout._ranges) {
				assert(range.NumDescriptors != UINT_MAX);
			}
		}

		// TODO check max values
		descriptor_set result(*this);

		// allocate descriptors
		if (layout._num_shader_resource_descriptors > 0) { // shader resources
			result._shader_resource_descriptors = _srv_descriptors.allocate(
				static_cast<_details::descriptor_range::index_t>(layout._num_shader_resource_descriptors)
			);
		}
		if (layout._num_sampler_descriptors > 0) { // samplers
			result._sampler_descriptors = _sampler_descriptors.allocate(
				static_cast<_details::descriptor_range::index_t>(layout._num_sampler_descriptors)
			);
		}

		// TODO update pool

		return result;
	}

	descriptor_set device::create_descriptor_set(
		descriptor_pool&, const descriptor_set_layout &layout, std::size_t dynamic_count
	) {
		// check that we do have unbounded ranges
		if constexpr (is_debugging) {
			bool has_unbounded = false;
			for (const auto &range : layout._ranges) {
				if (range.NumDescriptors == UINT_MAX) {
					has_unbounded = true;
					break;
				}
			}
			assert(has_unbounded);
		}

		// TODO check max values
		descriptor_set result(*this);

		// allocate descriptors
		// shader resources
		const auto srv_count = static_cast<_details::descriptor_range::index_t>(
			layout._num_shader_resource_descriptors + (layout._unbounded_range_is_sampler ? 0 : dynamic_count)
		);
		if (srv_count > 0) {
			result._shader_resource_descriptors = _srv_descriptors.allocate(srv_count);
		}
		// samplers
		const auto sampler_count = static_cast<_details::descriptor_range::index_t>(
			layout._num_sampler_descriptors + (layout._unbounded_range_is_sampler ? dynamic_count : 0)
		);
		if (sampler_count > 0) {
			result._sampler_descriptors = _sampler_descriptors.allocate(sampler_count);
		}

		// TODO update pool

		return result;
	}

	void device::write_descriptor_set_read_only_images(
		descriptor_set &set, const descriptor_set_layout &layout,
		std::size_t first_register, std::span<const image_view_base *const> images
	) {
		auto range_it = layout._find_register_range(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, first_register, images.size());
		UINT increment = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		D3D12_CPU_DESCRIPTOR_HANDLE current_descriptor =
			set._shader_resource_descriptors.get_cpu(static_cast<_details::descriptor_range::index_t>(
				range_it->OffsetInDescriptorsFromTableStart + (first_register - range_it->BaseShaderRegister)
			));
		for (auto *base_view : images) {
			if (base_view) {
				auto *view = static_cast<const _details::image_view_base*>(base_view);
				if (view->_image) {
					D3D12_SHADER_RESOURCE_VIEW_DESC desc = view->_srv_desc;
					// make sure we're viewing depth textures with the correct format
					switch (desc.Format) {
					case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
						desc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
						break;
					case DXGI_FORMAT_D24_UNORM_S8_UINT:
						desc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
						break;
					}
					_device->CreateShaderResourceView(view->_image.Get(), &desc, current_descriptor);
				}
			}
			current_descriptor.ptr += increment;
		}
	}

	void device::write_descriptor_set_read_write_images(
		descriptor_set &set, const descriptor_set_layout &layout,
		std::size_t first_register, std::span<const image_view_base *const> images
	) {
		auto range_it = layout._find_register_range(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, first_register, images.size());
		UINT increment = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		D3D12_CPU_DESCRIPTOR_HANDLE current_descriptor =
			set._shader_resource_descriptors.get_cpu(static_cast<_details::descriptor_range::index_t>(
				range_it->OffsetInDescriptorsFromTableStart + (first_register - range_it->BaseShaderRegister)
			));
		for (auto *base_view : images) {
			if (base_view) {
				auto *view = static_cast<const _details::image_view_base*>(base_view);
				if (view->_image) {
					D3D12_UNORDERED_ACCESS_VIEW_DESC desc = view->_uav_desc;
					// make sure we're viewing depth textures with the correct format
					switch (desc.Format) {
					case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
						desc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
						break;
					}
					_device->CreateUnorderedAccessView(view->_image.Get(), nullptr, &desc, current_descriptor);
				}
			}
			current_descriptor.ptr += increment;
		}
	}

	void device::write_descriptor_set_read_only_structured_buffers(
		descriptor_set &set, const descriptor_set_layout &layout,
		std::size_t first_register, std::span<const structured_buffer_view> buffers
	) {
		auto range_it = layout._find_register_range(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, first_register, buffers.size());
		UINT increment = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		D3D12_CPU_DESCRIPTOR_HANDLE current_descriptor =
			set._shader_resource_descriptors.get_cpu(static_cast<_details::descriptor_range::index_t>(
				range_it->OffsetInDescriptorsFromTableStart + (first_register - range_it->BaseShaderRegister)
			));
		for (const auto &buf : buffers) {
			if (buf.data) {
				auto *buf_data = static_cast<const buffer*>(buf.data);
				if (buf_data->_buffer) {
					D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
					desc.Format                     = DXGI_FORMAT_UNKNOWN;
					desc.ViewDimension              = D3D12_SRV_DIMENSION_BUFFER;
					desc.Shader4ComponentMapping    = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
					desc.Buffer.FirstElement        = static_cast<UINT64>(buf.first);
					desc.Buffer.NumElements         = static_cast<UINT>(buf.count);
					desc.Buffer.StructureByteStride = static_cast<UINT>(buf.stride);
					desc.Buffer.Flags               = D3D12_BUFFER_SRV_FLAG_NONE;
					_device->CreateShaderResourceView(buf_data->_buffer.Get(), &desc, current_descriptor);
				}
			}
			current_descriptor.ptr += increment;
		}
	}

	void device::write_descriptor_set_read_write_structured_buffers(
		descriptor_set &set, const descriptor_set_layout &layout,
		std::size_t first_register, std::span<const structured_buffer_view> buffers
	) {
		auto range_it = layout._find_register_range(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, first_register, buffers.size());
		UINT increment = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		D3D12_CPU_DESCRIPTOR_HANDLE current_descriptor =
			set._shader_resource_descriptors.get_cpu(static_cast<_details::descriptor_range::index_t>(
				range_it->OffsetInDescriptorsFromTableStart + (first_register - range_it->BaseShaderRegister)
			));
		for (const auto &buf : buffers) {
			if (buf.data) {
				auto *buf_data = static_cast<const buffer*>(buf.data);
				if (buf_data->_buffer) {
					D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {};
					desc.Format                      = DXGI_FORMAT_UNKNOWN;
					desc.ViewDimension               = D3D12_UAV_DIMENSION_BUFFER;
					desc.Buffer.FirstElement         = static_cast<UINT64>(buf.first);
					desc.Buffer.NumElements          = static_cast<UINT>(buf.count);
					desc.Buffer.StructureByteStride  = static_cast<UINT>(buf.stride);
					desc.Buffer.CounterOffsetInBytes = 0;
					desc.Buffer.Flags                = D3D12_BUFFER_UAV_FLAG_NONE;
					_device->CreateUnorderedAccessView(buf_data->_buffer.Get(), nullptr, &desc, current_descriptor);
				}
			}
			current_descriptor.ptr += increment;
		}
	}

	void device::write_descriptor_set_constant_buffers(
		descriptor_set &set, const descriptor_set_layout &layout,
		std::size_t first_register, std::span<const constant_buffer_view> buffers
	) {
		auto range_it = layout._find_register_range(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, first_register, buffers.size());
		UINT increment = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		D3D12_CPU_DESCRIPTOR_HANDLE current_descriptor = 
			set._shader_resource_descriptors.get_cpu(static_cast<_details::descriptor_range::index_t>(
				range_it->OffsetInDescriptorsFromTableStart + (first_register - range_it->BaseShaderRegister)
			));
		for (const auto &buf : buffers) {
			if (buf.data) {
				auto *buf_data = static_cast<const buffer*>(buf.data);
				D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {};
				desc.BufferLocation = buf_data->_buffer->GetGPUVirtualAddress() + buf.offset;
				desc.SizeInBytes    =
					static_cast<UINT>(memory::align_up(buf.size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT));
				_device->CreateConstantBufferView(&desc, current_descriptor);
			} else {
				_device->CreateConstantBufferView(nullptr, current_descriptor);
			}
			current_descriptor.ptr += increment;
		}
	}

	void device::write_descriptor_set_samplers(
		descriptor_set &set, const descriptor_set_layout &layout,
		std::size_t first_register, std::span<const gpu::sampler *const> samplers
	) {
		auto range_it = layout._find_register_range(
			D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, first_register, samplers.size()
		);
		UINT increment = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
		D3D12_CPU_DESCRIPTOR_HANDLE current_descriptor =
			set._sampler_descriptors.get_cpu(static_cast<_details::descriptor_range::index_t>(
				range_it->OffsetInDescriptorsFromTableStart + (first_register - range_it->BaseShaderRegister)
			));
		for (auto *base_sampler : samplers) {
			auto *samp = static_cast<const sampler*>(base_sampler);
			_device->CreateSampler(&samp->_desc, current_descriptor);
			current_descriptor.ptr += increment;
		}
	}

	shader_binary device::load_shader(std::span<const std::byte> data) {
		shader_binary result = nullptr;
		result._code = std::vector<std::byte>(data.begin(), data.end());
		result._shader.pShaderBytecode = result._code.data();
		result._shader.BytecodeLength  = static_cast<SIZE_T>(result._code.size());
		return result;
	}

	static std::pair<memory_type_index, memory_properties> _default_memory_types[]{
		std::pair(static_cast<memory_type_index>(D3D12_HEAP_TYPE_DEFAULT),  memory_properties::device_local),
		std::pair(static_cast<memory_type_index>(D3D12_HEAP_TYPE_UPLOAD),   memory_properties::host_visible),
		std::pair(static_cast<memory_type_index>(D3D12_HEAP_TYPE_READBACK), memory_properties::host_visible | memory_properties::host_cached),
	};
	std::span<const std::pair<memory_type_index, memory_properties>> device::enumerate_memory_types() const {
		return _default_memory_types;
	}

	memory_block device::allocate_memory(std::size_t size, memory_type_index memid) {
		memory_block result;
		D3D12_HEAP_DESC desc = {};
		desc.SizeInBytes     = size;
		desc.Properties.Type = static_cast<D3D12_HEAP_TYPE>(memid);
		if (desc.Properties.Type == D3D12_HEAP_TYPE_DEFAULT) {
			desc.Flags = D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES | D3D12_HEAP_FLAG_ALLOW_SHADER_ATOMICS;
		} else {
			desc.Flags = D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES;
		}
		_details::assert_dx(_device->CreateHeap(&desc, IID_PPV_ARGS(&result._heap)));
		return result;
	}

	buffer device::create_committed_buffer(
		std::size_t size, memory_type_index mem_id, buffer_usage_mask all_usages
	) {
		buffer result = nullptr;
		auto heap_type = static_cast<D3D12_HEAP_TYPE>(mem_id);
		D3D12_HEAP_PROPERTIES heap_properties = _details::default_heap_properties(heap_type);
		D3D12_RESOURCE_DESC1 desc = _details::resource_desc::for_buffer(size, all_usages);
		D3D12_HEAP_FLAGS heap_flags = D3D12_HEAP_FLAG_NONE;
		_details::resource_desc::adjust_resource_flags_for_buffer(heap_type, all_usages, &heap_flags);
		_details::assert_dx(_device->CreateCommittedResource3(
			&heap_properties, heap_flags, &desc, D3D12_BARRIER_LAYOUT_UNDEFINED, nullptr,
			nullptr, 0, nullptr, IID_PPV_ARGS(&result._buffer)
		), _device.Get());
		return result;
	}

	image2d device::create_committed_image2d(
		cvec2u32 size, std::uint32_t mip_levels,
		format fmt, image_tiling tiling, image_usage_mask all_usages
	) {
		image2d result = nullptr;
		D3D12_HEAP_PROPERTIES heap_properties = _details::default_heap_properties(D3D12_HEAP_TYPE_DEFAULT);
		D3D12_RESOURCE_DESC1 desc = _details::resource_desc::for_image2d(size, mip_levels, fmt, tiling, all_usages);
		D3D12_HEAP_FLAGS heap_flags = D3D12_HEAP_FLAG_NONE;
		_details::resource_desc::adjust_resource_flags_for_image(fmt, all_usages, &heap_flags);
		_details::assert_dx(_device->CreateCommittedResource3(
			&heap_properties, heap_flags, &desc, D3D12_BARRIER_LAYOUT_UNDEFINED, nullptr,
			nullptr, 0, nullptr, IID_PPV_ARGS(&result._image)
		), _device.Get());
		return result;
	}

	image3d device::create_committed_image3d(
		cvec3u32 size, std::uint32_t mip_levels,
		format fmt, image_tiling tiling, image_usage_mask all_usages
	) {
		image3d result = nullptr;
		D3D12_HEAP_PROPERTIES heap_properties = _details::default_heap_properties(D3D12_HEAP_TYPE_DEFAULT);
		D3D12_RESOURCE_DESC1 desc = _details::resource_desc::for_image3d(size, mip_levels, fmt, tiling, all_usages);
		D3D12_HEAP_FLAGS heap_flags = D3D12_HEAP_FLAG_NONE;
		_details::resource_desc::adjust_resource_flags_for_image(fmt, all_usages, &heap_flags);
		_details::assert_dx(_device->CreateCommittedResource3(
			&heap_properties, heap_flags, &desc, D3D12_BARRIER_LAYOUT_UNDEFINED, nullptr,
			nullptr, 0, nullptr, IID_PPV_ARGS(&result._image)
		), _device.Get());
		return result;
	}

	std::tuple<buffer, staging_buffer_metadata, std::size_t> device::create_committed_staging_buffer(
		cvec2u32 size, format fmt, memory_type_index mem_id, buffer_usage_mask all_usages
	) {
		// TODO will different usages affect size calculation?
		D3D12_RESOURCE_DESC1 image_desc = _details::resource_desc::for_image2d(
			size, 1, fmt, image_tiling::row_major, image_usage_mask::copy_destination
		);
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
		UINT64 total_bytes = 0;
		_device->GetCopyableFootprints1(&image_desc, 0, 1, 0, &footprint, nullptr, nullptr, &total_bytes);
		assert(footprint.Offset == 0); // assume we always start immediatly - no reason not to

		buffer result = create_committed_buffer(static_cast<std::size_t>(total_bytes), mem_id, all_usages);
		staging_buffer_metadata result_meta = uninitialized;
		result_meta._pitch  = footprint.Footprint.RowPitch;
		result_meta._size   = size;
		result_meta._format = fmt;
		return std::make_tuple(std::move(result), result_meta, total_bytes);
	}

	memory::size_alignment device::get_image2d_memory_requirements(
		cvec2u32 size, std::uint32_t mip_levels, format fmt, image_tiling tiling, image_usage_mask usages
	) {
		D3D12_RESOURCE_DESC1 desc = _details::resource_desc::for_image2d(size, mip_levels, fmt, tiling, usages);
		D3D12_RESOURCE_ALLOCATION_INFO1 info = {};
		_device->GetResourceAllocationInfo2(0, 1, &desc, &info);
		return memory::size_alignment(info.SizeInBytes, info.Alignment);
	}

	memory::size_alignment device::get_image3d_memory_requirements(
		cvec3u32 size, std::uint32_t mip_levels, format fmt, image_tiling tiling, image_usage_mask usages
	) {
		D3D12_RESOURCE_DESC1 desc = _details::resource_desc::for_image3d(size, mip_levels, fmt, tiling, usages);
		D3D12_RESOURCE_ALLOCATION_INFO1 info = {};
		_device->GetResourceAllocationInfo2(0, 1, &desc, &info);
		return memory::size_alignment(info.SizeInBytes, info.Alignment);
	}

	memory::size_alignment device::get_buffer_memory_requirements(std::size_t size, buffer_usage_mask usages) {
		D3D12_RESOURCE_DESC1 desc = _details::resource_desc::for_buffer(size, usages);
		D3D12_RESOURCE_ALLOCATION_INFO1 info = {};
		_device->GetResourceAllocationInfo2(0, 1, &desc, &info);
		return memory::size_alignment(info.SizeInBytes, info.Alignment);
	}

	buffer device::create_placed_buffer(
		std::size_t size, buffer_usage_mask usages, const memory_block &mem, std::size_t offset
	) {
		buffer result = nullptr;
		D3D12_RESOURCE_DESC1 desc = _details::resource_desc::for_buffer(size, usages);
		_details::assert_dx(_device->CreatePlacedResource2(
			mem._heap.Get(), offset, &desc, D3D12_BARRIER_LAYOUT_UNDEFINED, nullptr,
			0, nullptr, IID_PPV_ARGS(&result._buffer)
		));
		return result;
	}

	image2d device::create_placed_image2d(
		cvec2u32 size, std::uint32_t mip_levels,
		format fmt, image_tiling tiling, image_usage_mask usages, const memory_block &mem, std::size_t offset
	) {
		image2d result = nullptr;
		D3D12_RESOURCE_DESC1 desc = _details::resource_desc::for_image2d(size, mip_levels, fmt, tiling, usages);
		_details::assert_dx(_device->CreatePlacedResource2(
			mem._heap.Get(), offset, &desc, D3D12_BARRIER_LAYOUT_UNDEFINED, nullptr,
			0, nullptr, IID_PPV_ARGS(&result._image)
		));
		return result;
	}

	image3d device::create_placed_image3d(
		cvec3u32 size, std::uint32_t mip_levels,
		format fmt, image_tiling tiling, image_usage_mask usages, const memory_block &mem, std::size_t offset
	) {
		image3d result = nullptr;
		D3D12_RESOURCE_DESC1 desc = _details::resource_desc::for_image3d(size, mip_levels, fmt, tiling, usages);
		_details::assert_dx(_device->CreatePlacedResource2(
			mem._heap.Get(), offset, &desc, D3D12_BARRIER_LAYOUT_UNDEFINED, nullptr,
			0, nullptr, IID_PPV_ARGS(&result._image)
		));
		return result;
	}

	std::byte *device::map_buffer(buffer &buf, std::size_t begin, std::size_t length) {
		void *result = nullptr;
		D3D12_RANGE range = {};
		range.Begin = static_cast<SIZE_T>(begin);
		range.End   = static_cast<SIZE_T>(begin + length);
		_details::assert_dx(buf._buffer->Map(0, &range, &result));
		return static_cast<std::byte*>(result);
	}

	void device::unmap_buffer(buffer &buf, std::size_t begin, std::size_t length) {
		D3D12_RANGE range = {};
		range.Begin = static_cast<SIZE_T>(begin);
		range.End   = static_cast<SIZE_T>(begin + length);
		buf._buffer->Unmap(0, &range);
	}

	std::byte *device::map_image2d(
		image2d &img, subresource_index subresource, std::size_t begin, std::size_t length
	) {
		void *result = nullptr;
		D3D12_RANGE range = {};
		range.Begin = static_cast<SIZE_T>(begin);
		range.End   = static_cast<SIZE_T>(begin + length);
		_details::assert_dx(img._image->Map(
			_details::compute_subresource_index(subresource, img._image.Get()), &range, &result
		));
		return static_cast<std::byte*>(result);
	}

	void device::unmap_image2d(image2d &img, subresource_index subresource, std::size_t begin, std::size_t length) {
		D3D12_RANGE range = {};
		range.Begin = static_cast<SIZE_T>(begin);
		range.End   = static_cast<SIZE_T>(begin + length);
		img._image->Unmap(_details::compute_subresource_index(subresource, img._image.Get()), &range);
	}

	image2d_view device::create_image2d_view_from(const image2d &img, format fmt, mip_levels mip) {
		auto mips = mip.get_num_levels();
		D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
		srv_desc.Format                        = _details::conversions::to_format(fmt);
		srv_desc.ViewDimension                 = D3D12_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Shader4ComponentMapping       = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srv_desc.Texture2D.MostDetailedMip     = static_cast<UINT>(mip.first_level);
		srv_desc.Texture2D.MipLevels           = mip.get_num_levels_as<UINT>().value_or(-1);
		srv_desc.Texture2D.PlaneSlice          = 0;
		srv_desc.Texture2D.ResourceMinLODClamp = 0.0f;

		D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
		uav_desc.Format               = srv_desc.Format;
		uav_desc.ViewDimension        = D3D12_UAV_DIMENSION_TEXTURE2D;
		uav_desc.Texture2D.MipSlice   = srv_desc.Texture2D.MostDetailedMip;
		uav_desc.Texture2D.PlaneSlice = 0;

		image2d_view result = nullptr;
		result._image = img._image;
		result._srv_desc = srv_desc;
		result._uav_desc = uav_desc;
		return result;
	}

	image3d_view device::create_image3d_view_from(const image3d &img, format fmt, mip_levels mips) {
		D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
		srv_desc.Format                        = _details::conversions::to_format(fmt);
		srv_desc.ViewDimension                 = D3D12_SRV_DIMENSION_TEXTURE3D;
		srv_desc.Shader4ComponentMapping       = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srv_desc.Texture3D.MostDetailedMip     = static_cast<UINT>(mips.first_level);
		srv_desc.Texture3D.MipLevels           = mips.get_num_levels_as<UINT>().value_or(-1);
		srv_desc.Texture3D.ResourceMinLODClamp = 0.0f;

		D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
		uav_desc.Format                = srv_desc.Format;
		uav_desc.ViewDimension         = D3D12_UAV_DIMENSION_TEXTURE3D;
		uav_desc.Texture3D.MipSlice    = srv_desc.Texture3D.MostDetailedMip;
		uav_desc.Texture3D.FirstWSlice = 0;
		uav_desc.Texture3D.WSize       = static_cast<UINT>(-1);

		image3d_view result = nullptr;
		result._image = img._image;
		result._srv_desc = srv_desc;
		result._uav_desc = uav_desc;
		return result;
	}

	sampler device::create_sampler(
		filtering minification, filtering magnification, filtering mipmapping,
		float mip_lod_bias, float min_lod, float max_lod, std::optional<float> max_anisotropy,
		sampler_address_mode addressing_u, sampler_address_mode addressing_v, sampler_address_mode addressing_w,
		linear_rgba_f border_color, comparison_function comparison
	) {
		sampler result = nullptr;
		result._desc = {};
		result._desc.Filter         = _details::conversions::to_filter(
			minification, magnification, mipmapping,
			max_anisotropy.has_value(), comparison != comparison_function::none
		);
		result._desc.AddressU       = _details::conversions::to_texture_address_mode(addressing_u);
		result._desc.AddressV       = _details::conversions::to_texture_address_mode(addressing_v);
		result._desc.AddressW       = _details::conversions::to_texture_address_mode(addressing_w);
		result._desc.MipLODBias     = mip_lod_bias;
		result._desc.MaxAnisotropy  = std::clamp<UINT>(
			static_cast<UINT>(std::round(max_anisotropy.value_or(0.0f))), 1, 16
		);
		result._desc.ComparisonFunc = _details::conversions::to_comparison_function(comparison);
		result._desc.BorderColor[0] = border_color.r;
		result._desc.BorderColor[1] = border_color.g;
		result._desc.BorderColor[2] = border_color.b;
		result._desc.BorderColor[3] = border_color.a;
		result._desc.MinLOD         = min_lod;
		result._desc.MaxLOD         = max_lod;
		return result;
	}

	frame_buffer device::create_frame_buffer(
		std::span<const gpu::image2d_view *const> color, const image2d_view *depth_stencil, cvec2u32
	) {
		frame_buffer result(*this);
		result._color_formats.resize(color.size());
		result._color = _rtv_descriptors.allocate(static_cast<_details::descriptor_range::index_t>(color.size()));
		for (std::size_t i = 0; i < color.size(); ++i) {
			crash_if(color[i]->_srv_desc.Texture2D.MipLevels != 1);
			D3D12_RENDER_TARGET_VIEW_DESC desc = {};
			desc.Format               = color[i]->_srv_desc.Format;
			desc.ViewDimension        = D3D12_RTV_DIMENSION_TEXTURE2D;
			desc.Texture2D.MipSlice   = color[i]->_srv_desc.Texture2D.MostDetailedMip;
			desc.Texture2D.PlaneSlice = color[i]->_srv_desc.Texture2D.PlaneSlice;
			_device->CreateRenderTargetView(
				color[i]->_image.Get(), &desc,
				result._color.get_cpu(static_cast<_details::descriptor_range::index_t>(i))
			);
			result._color_formats[i] = color[i]->_srv_desc.Format;
		}
		if (depth_stencil) {
			result._depth_stencil = _dsv_descriptors.allocate(1);
			crash_if(depth_stencil->_srv_desc.Texture2D.MipLevels != 1);
			D3D12_DEPTH_STENCIL_VIEW_DESC desc = {};
			desc.Format             = depth_stencil->_srv_desc.Format;
			desc.ViewDimension      = D3D12_DSV_DIMENSION_TEXTURE2D;
			desc.Flags              = D3D12_DSV_FLAG_NONE;
			desc.Texture2D.MipSlice = depth_stencil->_srv_desc.Texture2D.MostDetailedMip;
			_device->CreateDepthStencilView(depth_stencil->_image.Get(), &desc, result._depth_stencil.get_cpu(0));
			result._depth_stencil_format = depth_stencil->_srv_desc.Format;
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

	timeline_semaphore device::create_timeline_semaphore(gpu::timeline_semaphore::value_type value) {
		timeline_semaphore result = nullptr;
		_details::assert_dx(_device->CreateFence(value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&result._semaphore)));
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

	void device::signal_timeline_semaphore(timeline_semaphore &sem, gpu::timeline_semaphore::value_type val) {
		_details::assert_dx(sem._semaphore->Signal(val));
	}

	gpu::timeline_semaphore::value_type device::query_timeline_semaphore(timeline_semaphore &sem) {
		return sem._semaphore->GetCompletedValue();
	}

	void device::wait_for_timeline_semaphore(timeline_semaphore &sem, gpu::timeline_semaphore::value_type val) {
		_details::assert_dx(sem._semaphore->SetEventOnCompletion(val, nullptr));
	}

	timestamp_query_heap device::create_timestamp_query_heap(std::uint32_t size) {
		timestamp_query_heap result = nullptr;

		D3D12_QUERY_HEAP_DESC desc = {};
		desc.Type     = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
		desc.Count    = size;
		desc.NodeMask = 0;
		_details::assert_dx(_device->CreateQueryHeap(&desc, IID_PPV_ARGS(&result._heap)), _device.Get());

		D3D12_HEAP_PROPERTIES heap_properties = _details::default_heap_properties(D3D12_HEAP_TYPE_READBACK);
		D3D12_RESOURCE_DESC1 buf_desc =
			_details::resource_desc::for_buffer(size, buffer_usage_mask::copy_destination);
		_details::assert_dx(_device->CreateCommittedResource3(
			&heap_properties, D3D12_HEAP_FLAG_NONE, &buf_desc, D3D12_BARRIER_LAYOUT_UNDEFINED, nullptr,
			nullptr, 0, nullptr, IID_PPV_ARGS(&result._resource)
		), _device.Get());

		return result;
	}

	void device::fetch_query_results(
		timestamp_query_heap &h, std::uint32_t first, std::span<std::uint64_t> results
	) {
		D3D12_RANGE read_range;
		read_range.Begin = sizeof(std::uint64_t) * first;
		read_range.End   = sizeof(std::uint64_t) * (first + results.size());
		void *data = nullptr;
		_details::assert_dx(h._resource->Map(0, &read_range, &data), _device.Get());

		for (std::size_t i = 0; i < results.size(); ++i) {
			results[i] = static_cast<const std::uint64_t*>(data)[i + first];
		}

		h._resource->Unmap(0, nullptr);
	}

	bottom_level_acceleration_structure_geometry device::create_bottom_level_acceleration_structure_geometry(
		std::span<const raytracing_geometry_view> data
	) {
		bottom_level_acceleration_structure_geometry result = nullptr;

		result._geometries.reserve(data.size());
		for (const auto &view : data) {
			const auto &vert = view.vertex_buffer;
			const auto &idx = view.index_buffer;

			auto &desc = result._geometries.emplace_back();
			desc.Type                                 = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
			desc.Flags                                =
				_details::conversions::to_raytracing_geometry_flags(view.flags);
			desc.Triangles.Transform3x4               = 0;
			desc.Triangles.VertexFormat               = _details::conversions::to_format(vert.vertex_format);
			desc.Triangles.VertexCount                = static_cast<UINT>(vert.count);
			desc.Triangles.VertexBuffer.StartAddress  = vert.data->_buffer->GetGPUVirtualAddress() + vert.offset;
			desc.Triangles.VertexBuffer.StrideInBytes = vert.stride;
			if (idx.data) {
				desc.Triangles.IndexFormat = _details::conversions::to_format(idx.element_format);
				desc.Triangles.IndexCount  = static_cast<UINT>(idx.count);
				desc.Triangles.IndexBuffer = idx.data->_buffer->GetGPUVirtualAddress() + idx.offset;
			} else {
				desc.Triangles.IndexFormat = DXGI_FORMAT_UNKNOWN;
				desc.Triangles.IndexCount  = 0;
				desc.Triangles.IndexBuffer = 0;
			}
		}

		result._inputs.Type           = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		result._inputs.Flags          = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
		result._inputs.NumDescs       = static_cast<UINT>(result._geometries.size());
		result._inputs.DescsLayout    = D3D12_ELEMENTS_LAYOUT_ARRAY;
		result._inputs.pGeometryDescs = result._geometries.data();

		return result;
	}

	instance_description device::get_bottom_level_acceleration_structure_description(
		bottom_level_acceleration_structure &as,
		mat44f trans, std::uint32_t id, std::uint8_t mask, std::uint32_t hit_group_offset,
		raytracing_instance_flags flags
	) const {
		instance_description result = uninitialized;
		for (std::size_t row = 0; row < 3; ++row) {
			for (std::size_t col = 0; col < 4; ++col) {
				result._desc.Transform[row][col] = trans(row, col);
			}
		}
		result._desc.InstanceID                          = id;
		result._desc.InstanceMask                        = mask;
		result._desc.InstanceContributionToHitGroupIndex = hit_group_offset;
		result._desc.Flags                               =
			_details::conversions::to_raytracing_instance_flags(flags);
		result._desc.AccelerationStructure               = as._buffer->GetGPUVirtualAddress() + as._offset;
		return result;
	}

	acceleration_structure_build_sizes device::get_bottom_level_acceleration_structure_build_sizes(
		const bottom_level_acceleration_structure_geometry &geom
	) {
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info;
		_device->GetRaytracingAccelerationStructurePrebuildInfo(&geom._inputs, &info);

		acceleration_structure_build_sizes result = uninitialized;
		result.acceleration_structure_size = info.ResultDataMaxSizeInBytes;
		result.build_scratch_size          = info.ScratchDataSizeInBytes;
		result.update_scratch_size         = info.UpdateScratchDataSizeInBytes;
		return result;
	}

	acceleration_structure_build_sizes device::get_top_level_acceleration_structure_build_sizes(
		std::size_t instance_count
	) {
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
		inputs.Type          = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
		inputs.Flags         = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
		inputs.NumDescs      = static_cast<UINT>(instance_count);
		inputs.DescsLayout   = D3D12_ELEMENTS_LAYOUT_ARRAY;
		// "[The implementation] may not inspect/dereference any GPU virtual addresses, other than to check to see if
		// a pointer is NULL or not"
		inputs.InstanceDescs = static_cast<D3D12_GPU_VIRTUAL_ADDRESS>(0xDEADBEEF);
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info;
		_device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

		acceleration_structure_build_sizes result = uninitialized;
		result.acceleration_structure_size = info.ResultDataMaxSizeInBytes;
		result.build_scratch_size          = info.ScratchDataSizeInBytes;
		result.update_scratch_size         = info.UpdateScratchDataSizeInBytes;
		return result;
	}

	bottom_level_acceleration_structure device::create_bottom_level_acceleration_structure(
		buffer &buf, std::size_t off, std::size_t // size doesn't matter
	) {
		bottom_level_acceleration_structure res = nullptr;
		res._buffer = buf._buffer;
		res._offset = off;
		return res;
	}

	top_level_acceleration_structure device::create_top_level_acceleration_structure(
		buffer &buf, std::size_t off, std::size_t // size doesn't matter
	) {
		top_level_acceleration_structure res = nullptr;
		res._buffer = buf._buffer;
		res._offset = off;
		return res;
	}

	void device::write_descriptor_set_acceleration_structures(
		descriptor_set &set, const descriptor_set_layout &layout, std::size_t first_register,
		std::span<gpu::top_level_acceleration_structure *const> as
	) {
		auto range_it = layout._find_register_range(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, first_register, as.size());
		UINT increment = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		D3D12_CPU_DESCRIPTOR_HANDLE current_descriptor = 
			set._shader_resource_descriptors.get_cpu(static_cast<_details::descriptor_range::index_t>(
				range_it->OffsetInDescriptorsFromTableStart + (first_register - range_it->BaseShaderRegister)
			));
		for (auto *buf : as) {
			D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
			desc.Format                                   = DXGI_FORMAT_UNKNOWN;
			desc.ViewDimension                            = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
			desc.Shader4ComponentMapping                  = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			desc.RaytracingAccelerationStructure.Location = buf->_buffer->GetGPUVirtualAddress() + buf->_offset;
			_device->CreateShaderResourceView(nullptr, &desc, current_descriptor);
			current_descriptor.ptr += increment;
		}
	}

	shader_group_handle device::get_shader_group_handle(
		const raytracing_pipeline_state &pipeline, std::size_t index
	) {
		shader_group_handle result = uninitialized;
		void *ptr = pipeline._properties->GetShaderIdentifier(_details::shader_record_name(index));
		crash_if(!ptr);
		std::memcpy(result._id.data(), ptr, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
		return result;
	}

	raytracing_pipeline_state device::create_raytracing_pipeline_state(
		std::span<const shader_function> hit_group_shaders, std::span<const hit_shader_group> hit_groups,
		std::span<const shader_function> general_shaders,
		std::size_t max_recursion, std::size_t max_payload_size, std::size_t max_attribute_size,
		const pipeline_resources &rsrc
	) {
		raytracing_pipeline_state result = nullptr;

		result._descriptor_table_binding = rsrc._descriptor_table_binding;
		result._root_signature = rsrc._signature;

		std::size_t num_subobjects = hit_group_shaders.size() + hit_groups.size() + general_shaders.size() + 3;

		auto bookmark = get_scratch_bookmark();
		auto subobjects = bookmark.create_reserved_vector_array<D3D12_STATE_SUBOBJECT>(num_subobjects);

		// root signature
		D3D12_GLOBAL_ROOT_SIGNATURE root_signature = {};
		root_signature.pGlobalRootSignature = rsrc._signature.Get();
		{
			auto &obj = subobjects.emplace_back();
			obj.Type  = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
			obj.pDesc = &root_signature;
		}

		// pipeline settings
		D3D12_RAYTRACING_SHADER_CONFIG shader_config = {};
		shader_config.MaxPayloadSizeInBytes   = static_cast<UINT>(max_payload_size);
		shader_config.MaxAttributeSizeInBytes = static_cast<UINT>(max_attribute_size);
		{
			auto &obj = subobjects.emplace_back();
			obj.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
			obj.pDesc = &shader_config;
		}
		D3D12_RAYTRACING_PIPELINE_CONFIG pipeline_config = {};
		pipeline_config.MaxTraceRecursionDepth = static_cast<UINT>(max_recursion);
		{
			auto &obj = subobjects.emplace_back();
			obj.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
			obj.pDesc = &pipeline_config;
		}

		// shaders
		// these arrays must be reserved or the objects may be moved and cause pointers to be invalidated
		std::size_t num_shaders = hit_group_shaders.size() + general_shaders.size();
		auto shader_libs = bookmark.create_reserved_vector_array<D3D12_DXIL_LIBRARY_DESC>(num_shaders);
		auto shader_exports = bookmark.create_reserved_vector_array<D3D12_EXPORT_DESC>(num_shaders);
		auto shader_names = bookmark.create_reserved_vector_array<
			memory::stack_allocator::string_type<WCHAR>
		>(num_shaders);
		auto add_shader = [&](const shader_function &sh, LPCWSTR export_name) {
			auto name = shader_names.emplace_back(
				system::platforms::windows::_details::u8string_to_wstring(
					sh.entry_point, bookmark.create_std_allocator<WCHAR>()
				)
			);

			auto &exp = shader_exports.emplace_back();
			exp.ExportToRename = name.c_str();
			exp.Name = export_name;

			auto &lib = shader_libs.emplace_back();
			lib.DXILLibrary = sh.code->_shader;
			lib.NumExports = 1;
			lib.pExports = &exp;

			auto &obj = subobjects.emplace_back();
			obj.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
			obj.pDesc = &lib;
		};
		// hit group shaders
		for (std::size_t i = 0; i < hit_group_shaders.size(); ++i) {
			add_shader(hit_group_shaders[i], _details::shader_name(i));
		}

		// hit groups
		auto hit_shader_groups = bookmark.create_reserved_vector_array<D3D12_HIT_GROUP_DESC>(hit_groups.size());
		for (std::size_t i = 0; i < hit_groups.size(); ++i) {
			const auto &group = hit_groups[i];

			auto &gp = hit_shader_groups.emplace_back();
			gp.HitGroupExport = _details::shader_record_name(i);
			gp.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
			gp.AnyHitShaderImport =
				group.any_hit_shader_index == hit_shader_group::no_shader ?
				nullptr : _details::shader_name(group.any_hit_shader_index);
			gp.ClosestHitShaderImport =
				group.closest_hit_shader_index == hit_shader_group::no_shader ?
				nullptr : _details::shader_name(group.closest_hit_shader_index);
			gp.IntersectionShaderImport = nullptr;

			auto &obj = subobjects.emplace_back();
			obj.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
			obj.pDesc = &gp;
		}

		// general shaders
		for (std::size_t i = 0; i < general_shaders.size(); ++i) {
			add_shader(general_shaders[i], _details::shader_record_name(i + hit_groups.size()));
		}

		// make sure we haven't invalidated any pointers
		assert(shader_libs.size() == num_shaders);
		assert(subobjects.size() == num_subobjects);

		D3D12_STATE_OBJECT_DESC desc = {};
		desc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
		desc.NumSubobjects = static_cast<UINT>(subobjects.size());
		desc.pSubobjects = subobjects.data();
		_details::assert_dx(_device->CreateStateObject(&desc, IID_PPV_ARGS(&result._state)));
		_details::assert_dx(result._state->QueryInterface(IID_PPV_ARGS(&result._properties)));
		return result;
	}

	device::device(_details::com_ptr<ID3D12Device10> dev) : _device(std::move(dev)) {
		_rtv_descriptors.initialize(*_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, descriptor_heap_size);
		_dsv_descriptors.initialize(*_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, descriptor_heap_size);
		_srv_descriptors.initialize(*_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, descriptor_heap_size);
		_sampler_descriptors.initialize(*_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, sampler_heap_size);
	}

	void device::_set_debug_name(ID3D12Object &obj, const char8_t *name) {
		_details::assert_dx(obj.SetPrivateData(
			WKPDID_D3DDebugObjectName, static_cast<UINT>(std::strlen(reinterpret_cast<const char*>(name))), name
		));
	}


	std::pair<device, std::vector<command_queue>> adapter::create_device(std::span<const queue_type> queues) {
		// create device
		_details::com_ptr<ID3D12Device10> result;
		_details::assert_dx(D3D12CreateDevice(_adapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&result)));
		_details::com_ptr<ID3D12InfoQueue1> info_queue;
		HRESULT res = result->QueryInterface(IID_PPV_ARGS(&info_queue));
		if (res == S_OK) {
			DWORD dummy = 0;
			_details::assert_dx(info_queue->RegisterMessageCallback(
				[](
					D3D12_MESSAGE_CATEGORY category,
					D3D12_MESSAGE_SEVERITY severity,
					D3D12_MESSAGE_ID id,
					LPCSTR description,
					void*
				) {
					switch (severity) {
					case D3D12_MESSAGE_SEVERITY_CORRUPTION:
						[[fallthrough]];
					case D3D12_MESSAGE_SEVERITY_ERROR:
						log().error(
							"DirectX message: category: {}, severity: {}, id: {}, \"{}\"",
							static_cast<std::underlying_type_t<D3D12_MESSAGE_CATEGORY>>(category),
							static_cast<std::underlying_type_t<D3D12_MESSAGE_SEVERITY>>(severity),
							static_cast<std::underlying_type_t<D3D12_MESSAGE_ID>>(id),
							description
						);
						break;
					default:
						log().debug(
							"DirectX message: category: {}, severity: {}, id: {}, \"{}\"",
							static_cast<std::underlying_type_t<D3D12_MESSAGE_CATEGORY>>(category),
							static_cast<std::underlying_type_t<D3D12_MESSAGE_SEVERITY>>(severity),
							static_cast<std::underlying_type_t<D3D12_MESSAGE_ID>>(id),
							description
						);
						break;
					}
				},
				D3D12_MESSAGE_CALLBACK_FLAG_NONE,
				nullptr,
				&dummy
			));
		} else {
			log().error("Failed to register DirectX message callback: {}", res);
		}

		// create queues
		std::vector<command_queue> qs(queues.size(), nullptr);
		for (std::size_t i = 0; i < queues.size(); ++i) {
			D3D12_COMMAND_QUEUE_DESC desc = {};
			desc.Type     = _details::conversions::to_command_list_type(queues[i]);
			desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
			desc.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE;
			desc.NodeMask = 0;
			_details::assert_dx(result->CreateCommandQueue(&desc, IID_PPV_ARGS(&qs[i]._queue)));
		}

		return { device(std::move(result)), std::move(qs) };
	}

	adapter_properties adapter::get_properties() const {
		DXGI_ADAPTER_DESC1 desc;
		_details::assert_dx(_adapter->GetDesc1(&desc));

		adapter_properties result = uninitialized;
		result.name                                =
			system::platforms::windows::_details::wstring_to_u8string(desc.Description, std::allocator<char8_t>());
		result.is_software                         = desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE;
		result.is_discrete                         = true; // TODO
		result.constant_buffer_alignment           = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
		result.acceleration_structure_alignment    = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT;
		result.shader_group_handle_size            = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
		result.shader_group_handle_alignment       = D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT;
		result.shader_group_handle_table_alignment = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;

		// TODO
		return result;
	}
}
