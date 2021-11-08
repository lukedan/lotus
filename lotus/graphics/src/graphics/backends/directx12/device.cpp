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
	back_buffer_info device::acquire_back_buffer(swap_chain &s) {
		back_buffer_info result = uninitialized;
		result.index = static_cast<std::size_t>(s._swap_chain->GetCurrentBackBufferIndex());
		result.on_presented = s._synchronization[result.index].notify_fence;
		return result;
	}

	void device::resize_swap_chain_buffers(swap_chain &s, cvec2s size) {
		_details::assert_dx(s._swap_chain->ResizeBuffers(
			s.get_image_count(), size[0], size[1], DXGI_FORMAT_UNKNOWN, 0
		));
		for (auto &sync : s._synchronization) {
			sync.next_fence = nullptr;
			sync.notify_fence = nullptr;
		}
	}

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

	command_list device::create_and_start_command_list(command_allocator &alloc) {
		command_list result = nullptr;
		_details::assert_dx(_device->CreateCommandList(
			0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc._allocator.Get(), nullptr, IID_PPV_ARGS(&result._list)
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
			dst.RangeType                         = _details::conversions::for_descriptor_type(src.range.type);
			dst.NumDescriptors                    = static_cast<UINT>(src.range.count);
			dst.BaseShaderRegister                = static_cast<UINT>(src.register_index);
			dst.Flags                             = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
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
			UINT total_count = 0;
			auto it = result._ranges.begin();
			for (; it != result._ranges.end(); ++it) {
				if (it->RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER) {
					break;
				}
				it->OffsetInDescriptorsFromTableStart = total_count;
				total_count += it->NumDescriptors;
			}
			result._num_shader_resource_descriptors = total_count;
			result._num_shader_resource_ranges = it - result._ranges.begin();
			total_count = 0;
			for (; it != result._ranges.end(); ++it) {
				assert(it->RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER);
				it->OffsetInDescriptorsFromTableStart = total_count;
				total_count += it->NumDescriptors;
			}
			result._num_sampler_descriptors = total_count;
		}
		result._visibility = _details::conversions::to_shader_visibility(visible_stages);
		return result;
	}

	pipeline_resources device::create_pipeline_resources(
		std::span<const graphics::descriptor_set_layout *const> sets
	) {
		_details::com_ptr<ID3DBlob> signature;
		std::vector<pipeline_resources::_root_param_indices> indices(sets.size(), nullptr);
		{ // serialize root signature
			auto bookmark = stack_allocator::for_this_thread().bookmark();

			auto root_params = bookmark.create_reserved_vector_array<D3D12_ROOT_PARAMETER1>(sets.size() * 2);
			auto descriptor_tables = bookmark.create_reserved_vector_array<
				std::vector<D3D12_DESCRIPTOR_RANGE1, stack_allocator::allocator<D3D12_DESCRIPTOR_RANGE1>>
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
				std::cerr <<
					"Failed to serialize root signature: " <<
					std::string_view(static_cast<const char*>(error->GetBufferPointer()), error->GetBufferSize()) <<
					std::endl;
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
		const shader *vertex_shader,
		const shader *pixel_shader,
		const shader *domain_shader,
		const shader *hull_shader,
		const shader *geometry_shader,
		std::span<const render_target_blend_options> blend,
		const rasterizer_options &rasterizer,
		const depth_stencil_options &depth_stencil,
		std::span<const input_buffer_layout> input_buffers,
		primitive_topology topology,
		const pass_resources &environment,
		[[maybe_unused]] std::size_t num_viewports
	) {
		auto bookmark = stack_allocator::for_this_thread().bookmark();

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
		desc.BlendState        = _details::conversions::for_blend_options(blend);
		desc.SampleMask        = std::numeric_limits<UINT>::max();
		desc.RasterizerState   = _details::conversions::for_rasterizer_options(rasterizer);
		desc.DepthStencilState = _details::conversions::for_depth_stencil_options(depth_stencil);

		// gather & convert input buffer
		std::size_t total_elements = 0;
		for (auto &buf : input_buffers) {
			total_elements += buf.elements.size();
		}
		auto element_desc = bookmark.create_reserved_vector_array<D3D12_INPUT_ELEMENT_DESC>(total_elements);
		for (auto &buf : input_buffers) {
			auto input_rate = _details::conversions::for_input_buffer_rate(buf.input_rate);
			for (auto &elem : buf.elements) {
				auto &elem_desc = element_desc.emplace_back();
				elem_desc.SemanticName         = reinterpret_cast<LPCSTR>(elem.semantic_name);
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

	compute_pipeline_state device::create_compute_pipeline_state(const pipeline_resources &rsrc, const shader &shader) {
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

	pass_resources device::create_pass_resources(
		std::span<const render_target_pass_options> rtv, depth_stencil_pass_options dsv
	) {
		pass_resources result = nullptr;
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

	descriptor_pool device::create_descriptor_pool(std::span<const descriptor_range> ranges, std::size_t) {
		descriptor_pool result;
		// TODO set max values
		return result;
	}

	descriptor_set device::create_descriptor_set(descriptor_pool &pool, const descriptor_set_layout &layout) {
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

	void device::write_descriptor_set_images(
		descriptor_set &set, const descriptor_set_layout &layout,
		std::size_t first_register, std::span<const image_view *const> images
	) {
		auto range_it = layout._find_register_range(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, first_register, images.size());
		UINT increment = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		D3D12_CPU_DESCRIPTOR_HANDLE current_descriptor =
			set._shader_resource_descriptors.get_cpu(static_cast<_details::descriptor_range::index_t>(
				range_it->OffsetInDescriptorsFromTableStart + (first_register - range_it->BaseShaderRegister)
			));
		for (auto *base_view : images) {
			auto *view = static_cast<const _details::image_view*>(base_view);
			// make sure we're viewing depth textures with the correct format
			D3D12_SHADER_RESOURCE_VIEW_DESC desc = view->_desc;
			switch (desc.Format) {
			case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
				desc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
				break;
			}
			_device->CreateShaderResourceView(view->_image.Get(), &desc, current_descriptor);
			current_descriptor.ptr += increment;
		}
	}

	void device::write_descriptor_set_buffers(
		descriptor_set &set, const descriptor_set_layout &layout,
		std::size_t first_register, std::span<const buffer_view> buffers
	) {
		auto range_it = layout._find_register_range(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, first_register, buffers.size());
		UINT increment = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		D3D12_CPU_DESCRIPTOR_HANDLE current_descriptor =
			set._shader_resource_descriptors.get_cpu(static_cast<_details::descriptor_range::index_t>(
				range_it->OffsetInDescriptorsFromTableStart + (first_register - range_it->BaseShaderRegister)
			));
		for (const auto &buf : buffers) {
			auto *buf_data = static_cast<const buffer*>(buf.data);
			D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
			desc.Format                     = DXGI_FORMAT_UNKNOWN;
			desc.ViewDimension              = D3D12_SRV_DIMENSION_BUFFER;
			desc.Shader4ComponentMapping    = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			desc.Buffer.FirstElement        = static_cast<UINT64>(buf.first);
			desc.Buffer.NumElements         = static_cast<UINT>(buf.size);
			desc.Buffer.StructureByteStride = static_cast<UINT>(buf.stride);
			desc.Buffer.Flags               = D3D12_BUFFER_SRV_FLAG_NONE;
			_device->CreateShaderResourceView(buf_data->_buffer.Get(), &desc, current_descriptor);
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
			auto *buf_data = static_cast<const buffer*>(buf.data);
			D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {};
			desc.BufferLocation = buf_data->_buffer->GetGPUVirtualAddress() + buf.offset;
			desc.SizeInBytes    =
				static_cast<UINT>(align_size(buf.size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT));
			_device->CreateConstantBufferView(&desc, current_descriptor);
			current_descriptor.ptr += increment;
		}
	}

	void device::write_descriptor_set_samplers(
		descriptor_set &set, const descriptor_set_layout &layout,
		std::size_t first_register, std::span<const graphics::sampler *const> samplers
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

	shader device::load_shader(std::span<const std::byte> data) {
		shader result = nullptr;
		result._code = std::vector<std::byte>(data.begin(), data.end());
		result._shader.pShaderBytecode = result._code.data();
		result._shader.BytecodeLength  = static_cast<SIZE_T>(result._code.size());
		return result;
	}

	device_heap device::create_device_heap(std::size_t size, heap_type type) {
		device_heap result;
		D3D12_HEAP_DESC desc = {};
		desc.SizeInBytes = size;
		desc.Properties  = _details::heap_type_to_properties(type);
		if (type == heap_type::device_only) {
			desc.Flags = D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES | D3D12_HEAP_FLAG_ALLOW_SHADER_ATOMICS;
		} else {
			desc.Flags = D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES;
		}
		_details::assert_dx(_device->CreateHeap(&desc, IID_PPV_ARGS(&result._heap)));
		return result;
	}

	buffer device::create_committed_buffer(
		std::size_t size, heap_type type, buffer_usage::mask all_usages
	) {
		buffer result = nullptr;
		D3D12_HEAP_PROPERTIES heap_properties = _details::heap_type_to_properties(type);
		D3D12_RESOURCE_DESC desc = _details::resource_desc::for_buffer(size);
		D3D12_RESOURCE_STATES states = D3D12_RESOURCE_STATE_COMMON;
		D3D12_HEAP_FLAGS heap_flags = D3D12_HEAP_FLAG_NONE;
		_details::resource_desc::adjust_resource_flags_for_buffer(
			type, all_usages, desc, states, &heap_flags
		);
		_details::assert_dx(_device->CreateCommittedResource(
			&heap_properties, heap_flags, &desc, states, nullptr, IID_PPV_ARGS(&result._buffer)
		));
		return result;
	}

	image2d device::create_committed_image2d(
		std::size_t width, std::size_t height, std::size_t array_slices, std::size_t mip_levels,
		format fmt, image_tiling tiling, image_usage::mask all_usages
	) {
		image2d result = nullptr;
		D3D12_HEAP_PROPERTIES heap_properties = _details::heap_type_to_properties(heap_type::device_only);
		D3D12_RESOURCE_DESC desc = _details::resource_desc::for_image2d(
			width, height, array_slices, mip_levels, fmt, tiling
		);
		D3D12_RESOURCE_STATES states = D3D12_RESOURCE_STATE_COMMON;
		D3D12_HEAP_FLAGS heap_flags = D3D12_HEAP_FLAG_NONE;
		_details::resource_desc::adjust_resource_flags_for_image2d(fmt, all_usages, desc, &heap_flags);
		_details::assert_dx(_device->CreateCommittedResource(
			&heap_properties, heap_flags, &desc, states, nullptr, IID_PPV_ARGS(&result._image)
		));
		return result;
	}

	std::tuple<buffer, staging_buffer_pitch, std::size_t> device::create_committed_staging_buffer(
		std::size_t width, std::size_t height, format fmt, heap_type type,
		buffer_usage::mask allowed_usage
	) {
		D3D12_RESOURCE_DESC image_desc = _details::resource_desc::for_image2d(
			width, height, 1, 1, fmt, image_tiling::row_major
		);
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
		UINT64 total_bytes = 0;
		_device->GetCopyableFootprints(&image_desc, 0, 1, 0, &footprint, nullptr, nullptr, &total_bytes);
		assert(footprint.Offset == 0); // assume we always start immediatly - no reason not to

		buffer result = create_committed_buffer(static_cast<std::size_t>(total_bytes), type, allowed_usage);
		staging_buffer_pitch result_pitch = uninitialized;
		result_pitch._pitch = footprint.Footprint.RowPitch;
		return std::make_tuple(std::move(result), result_pitch, total_bytes);
	}

	void *device::map_buffer(buffer &buf, std::size_t begin, std::size_t length) {
		void *result = nullptr;
		D3D12_RANGE range = {};
		range.Begin = static_cast<SIZE_T>(begin);
		range.End   = static_cast<SIZE_T>(begin + length);
		_details::assert_dx(buf._buffer->Map(0, &range, &result));
		return result;
	}

	void device::unmap_buffer(buffer &buf, std::size_t begin, std::size_t length) {
		D3D12_RANGE range = {};
		range.Begin = static_cast<SIZE_T>(begin);
		range.End   = static_cast<SIZE_T>(begin + length);
		buf._buffer->Unmap(0, &range);
	}

	void *device::map_image2d(image2d &img, subresource_index subresource, std::size_t begin, std::size_t length) {
		void *result = nullptr;
		D3D12_RANGE range = {};
		range.Begin = static_cast<SIZE_T>(begin);
		range.End   = static_cast<SIZE_T>(begin + length);
		_details::assert_dx(img._image->Map(
			_details::compute_subresource_index(subresource, img._image.Get()), &range, &result
		));
		return result;
	}

	void device::unmap_image2d(image2d &img, subresource_index subresource, std::size_t begin, std::size_t length) {
		D3D12_RANGE range = {};
		range.Begin = static_cast<SIZE_T>(begin);
		range.End   = static_cast<SIZE_T>(begin + length);
		img._image->Unmap(_details::compute_subresource_index(subresource, img._image.Get()), &range);
	}

	image2d_view device::create_image2d_view_from(const image2d &img, format fmt, mip_levels mip) {
		D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
		desc.Format                        = _details::conversions::for_format(fmt);
		desc.ViewDimension                 = D3D12_SRV_DIMENSION_TEXTURE2D;
		desc.Shader4ComponentMapping       = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		desc.Texture2D.MostDetailedMip     = static_cast<UINT>(mip.minimum);
		desc.Texture2D.MipLevels           =
			static_cast<UINT>(mip.num_levels == mip_levels::all_mip_levels ? -1 : mip.num_levels);
		desc.Texture2D.PlaneSlice          = 0;
		desc.Texture2D.ResourceMinLODClamp = 0.0f;

		image2d_view result = nullptr;
		result._image = img._image;
		result._desc = desc;
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
		std::span<const graphics::image2d_view *const> color, const image2d_view *depth_stencil,
		cvec2s, const pass_resources&
	) {
		frame_buffer result(*this);
		result._color = _rtv_descriptors.allocate(static_cast<_details::descriptor_range::index_t>(color.size()));
		for (std::size_t i = 0; i < color.size(); ++i) {
			D3D12_RENDER_TARGET_VIEW_DESC desc = {};
			color[i]->_fill_rtv_desc(desc);
			_device->CreateRenderTargetView(
				color[i]->_image.Get(), &desc,
				result._color.get_cpu(static_cast<_details::descriptor_range::index_t>(i))
			);
		}
		if (depth_stencil) {
			result._depth_stencil = _dsv_descriptors.allocate(1);
			D3D12_DEPTH_STENCIL_VIEW_DESC desc = {};
			depth_stencil->_fill_dsv_desc(desc);
			_device->CreateDepthStencilView(depth_stencil->_image.Get(), &desc, result._depth_stencil.get_cpu(0));
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

	device::device(_details::com_ptr<ID3D12Device8> dev) : _device(std::move(dev)) {
		_rtv_descriptors.initialize(*_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, descriptor_heap_size);
		_dsv_descriptors.initialize(*_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, descriptor_heap_size);
		_srv_descriptors.initialize(*_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, descriptor_heap_size);
		_sampler_descriptors.initialize(*_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, descriptor_heap_size);
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
		adapter_properties result = uninitialized;
		DXGI_ADAPTER_DESC1 desc;
		_details::assert_dx(_adapter->GetDesc1(&desc));
		result.is_software = desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE;
		result.constant_buffer_alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
		result.name = system::platforms::windows::_details::wstring_to_u8string(
			desc.Description, std::allocator<char8_t>()
		);
		result.is_discrete = true; // TODO
		// TODO
		return result;
	}
}
