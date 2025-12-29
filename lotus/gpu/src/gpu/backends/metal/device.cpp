#include "lotus/gpu/backends/metal/device.h"

/// \file
/// Implementation of Metal devices.

#include <array>

#include "lotus/gpu/acceleration_structure.h"
#include "lotus/gpu/pipeline.h"
#include "lotus/gpu/resources.h"
#include "lotus/gpu/backends/common/dxc.h"

namespace lotus::gpu::backends::metal {
	std::tuple<image2d, fence*, swap_chain_status> device::acquire_back_buffer(swap_chain &chain) {
		chain._drawable = NS::RetainPtr(chain._layer->nextDrawable());
		if (!chain._drawable) {
			return { image2d(nullptr), nullptr, swap_chain_status::unavailable };
		}
		return { image2d({ NS::RetainPtr(chain._drawable->texture()), nullptr }), nullptr, swap_chain_status::ok };
	}

	command_allocator device::create_command_allocator(command_queue &q) {
		return command_allocator(q._q.get());
	}

	command_list device::create_and_start_command_list(command_allocator &alloc) {
		return command_list(NS::RetainPtr(alloc._q->commandBuffer()));
	}

	/// Default resource options for argument buffers. Assumes that we don't need to read from the argument buffer.
	constexpr static MTL::ResourceOptions _arg_buffer_options =
		MTL::ResourceOptionCPUCacheModeWriteCombined |
		MTL::ResourceStorageModeShared |
		MTL::ResourceHazardTrackingModeUntracked;

	descriptor_pool device::create_descriptor_pool(
		std::span<const descriptor_range> capacity, u32 // max_num_sets is not important
	) {
		usize total_resources = 0;
		for (const descriptor_range &range : capacity) {
			total_resources += range.count;
		}
		auto heap_desc = NS::TransferPtr(MTL::HeapDescriptor::alloc()->init());
		heap_desc->setType(MTL::HeapTypeAutomatic);
		heap_desc->setResourceOptions(_arg_buffer_options);
		heap_desc->setSize(total_resources * sizeof(IRDescriptorTableEntry));
		auto heap = NS::TransferPtr(_dev->newHeap(heap_desc.get()));
		return descriptor_pool({ std::move(heap), _residency_set.get() });
	}

	descriptor_set device::create_descriptor_set(descriptor_pool &pool, const descriptor_set_layout &layout) {
		u32 max_slot_index = 0;
		for (const descriptor_range_binding &range : layout._bindings) {
			max_slot_index = std::max(max_slot_index, range.get_last_register_index());
		}
		const NS::UInteger size_bytes = (max_slot_index + 1) * sizeof(IRDescriptorTableEntry);
		auto buf_ptr = NS::TransferPtr(pool._heap->newBuffer(size_bytes, _arg_buffer_options));
		crash_if(!buf_ptr);
		_maybe_set_descriptor_set_name(buf_ptr.get(), layout);
		return descriptor_set({ std::move(buf_ptr), _residency_set.get() });
	}

	descriptor_set device::create_descriptor_set(
		descriptor_pool &pool, const descriptor_set_layout &layout, u32 dynamic_size
	) {
		u32 max_slot_index = 0;
		for (const descriptor_range_binding &range : layout._bindings) {
			if (range.range.count == descriptor_range::unbounded_count) {
				max_slot_index = range.register_index + dynamic_size - 1;
			} else {
				max_slot_index = std::max(max_slot_index, range.get_last_register_index());
			}
		}
		const NS::UInteger size_bytes = (max_slot_index + 1) * sizeof(IRDescriptorTableEntry);
		auto buf_ptr = NS::TransferPtr(pool._heap->newBuffer(size_bytes, _arg_buffer_options));
		crash_if(!buf_ptr);
		_maybe_set_descriptor_set_name(buf_ptr.get(), layout);
		return descriptor_set({ std::move(buf_ptr), _residency_set.get() });
	}

	void device::write_descriptor_set_read_only_images(
		descriptor_set &set,
		const descriptor_set_layout &layout,
		u32 first_register,
		std::span<const image_view_base *const> images
	) {
		_validate_descriptor_set_bindings(
			layout, first_register, static_cast<u32>(images.size()), descriptor_type::read_only_image
		);
		_write_descriptor_set_images(set, first_register, images);
	}

	void device::write_descriptor_set_read_write_images(
		descriptor_set &set,
		const descriptor_set_layout &layout,
		u32 first_register,
		std::span<const image_view_base *const> images
	) {
		_validate_descriptor_set_bindings(
			layout, first_register, static_cast<u32>(images.size()), descriptor_type::read_write_image
		);
		_write_descriptor_set_images(set, first_register, images);
	}

	void device::write_descriptor_set_read_only_structured_buffers(
		descriptor_set &set,
		const descriptor_set_layout &layout,
		u32 first_regsiter,
		std::span<const structured_buffer_view> buffers
	) {
		_validate_descriptor_set_bindings(
			layout, first_regsiter, static_cast<u32>(buffers.size()), descriptor_type::read_only_buffer
		);
		_write_descriptor_set_structured_buffers(set, first_regsiter, buffers);
	}

	void device::write_descriptor_set_read_write_structured_buffers(
		descriptor_set &set,
		const descriptor_set_layout &layout,
		u32 first_regsiter,
		std::span<const structured_buffer_view> buffers
	) {
		_validate_descriptor_set_bindings(
			layout, first_regsiter, static_cast<u32>(buffers.size()), descriptor_type::read_write_buffer
		);
		_write_descriptor_set_structured_buffers(set, first_regsiter, buffers);
	}

	void device::write_descriptor_set_constant_buffers(
		descriptor_set &set,
		const descriptor_set_layout &layout,
		u32 first_register,
		std::span<const constant_buffer_view> buffers
	) {
		_validate_descriptor_set_bindings(
			layout, first_register, static_cast<u32>(buffers.size()), descriptor_type::constant_buffer
		);

		auto *arr = static_cast<IRDescriptorTableEntry*>(set._arg_buffer->contents());
		for (usize i = 0; i < buffers.size(); ++i) {
			const u64 base_addr = buffers[i].data->_buf->gpuAddress();
			IRDescriptorTableSetBuffer(&arr[first_register + i], base_addr + buffers[i].offset, 0);
		}
	}

	void device::write_descriptor_set_samplers(
		descriptor_set &set,
		const descriptor_set_layout &layout,
		u32 first_register,
		std::span<const gpu::sampler *const> samplers
	) {
		_validate_descriptor_set_bindings(
			layout, first_register, static_cast<u32>(samplers.size()), descriptor_type::sampler
		);

		auto *arr = static_cast<IRDescriptorTableEntry*>(set._arg_buffer->contents());
		for (usize i = 0; i < samplers.size(); ++i) {
			IRDescriptorTableSetSampler(
				&arr[first_register + i],
				samplers[i]->_smp.get(),
				samplers[i]->_mip_lod_bias
			);
		}
	}

	shader_binary device::load_shader(std::span<const std::byte> data) {
		shader_binary result = nullptr;

		// load reflection from DXIL
		common::dxc_compiler compiler = nullptr;
		common::_details::com_ptr<IUnknown> raw_refl;
		compiler.load_shader_reflection(data, IID_PPV_ARGS(&raw_refl));

		_details::shader::ir_conversion_result result_ir = nullptr;

		common::_details::com_ptr<ID3D12ShaderReflection> refl_shader;
		const HRESULT shader_res = raw_refl->QueryInterface(IID_PPV_ARGS(&refl_shader));
		if (shader_res != E_NOINTERFACE) { // regular shader
			common::_details::assert_dx(shader_res);

			// load compute shader reflection data
			UINT x, y, z;
			refl_shader->GetThreadGroupSize(&x, &y, &z);
			result._thread_group_size = cvec3u32(x, y, z);

			// convert
			result_ir = _details::shader::convert_to_metal_ir(
				data, _details::shader::create_root_signature_for_shader_reflection(refl_shader).get()
			);

			// load vertex shader reflection data
			auto shader_refl = _details::ir_make_unique(IRShaderReflectionCreate());
			crash_if(!IRObjectGetReflection(
				result_ir.object.get(),
				IRObjectGetMetalIRShaderStage(result_ir.object.get()),
				shader_refl.get()
			));
			if (
				IRVersionedVSInfo vsinfo;
				IRShaderReflectionCopyVertexInfo(shader_refl.get(), IRReflectionVersion_1_0, &vsinfo)
			) {
				for (usize i = 0; i < vsinfo.info_1_0.num_vertex_inputs; ++i) {
					const IRVertexInputInfo_1_0 &input = vsinfo.info_1_0.vertex_inputs[i];
					result._vs_input_attributes.emplace_back(
						std::u8string(string::assume_utf8(input.name)),
						input.attributeIndex
					);
				}
				IRShaderReflectionReleaseVertexInfo(&vsinfo);
			}
		} else { // shader library
			common::_details::com_ptr<ID3D12LibraryReflection> refl_lib;
			common::_details::assert_dx(raw_refl->QueryInterface(IID_PPV_ARGS(&refl_lib)));
			D3D12_LIBRARY_DESC lib_desc = {};
			common::_details::assert_dx(refl_lib->GetDesc(&lib_desc));

			_details::ir_unique_ptr<IRRootSignature> root_signature;

			{ // create root signature for all combined shaders
				std::vector<D3D12_SHADER_INPUT_BIND_DESC> all_bindings;
				for (UINT func_i = 0; func_i < lib_desc.FunctionCount; ++func_i) {
					ID3D12FunctionReflection *refl_func = refl_lib->GetFunctionByIndex(static_cast<INT>(func_i));
					D3D12_FUNCTION_DESC func_desc = {};
					common::_details::assert_dx(refl_func->GetDesc(&func_desc));
					for (UINT binding_i = 0; binding_i < func_desc.BoundResources; ++binding_i) {
						common::_details::assert_dx(
							refl_func->GetResourceBindingDesc(binding_i, &all_bindings.emplace_back())
						);
					}
				}

				// sort and merge/deduplicate all resource bindings
				if (!all_bindings.empty()) {
					std::sort(
						all_bindings.begin(),
						all_bindings.end(),
						[](const D3D12_SHADER_INPUT_BIND_DESC &lhs, const D3D12_SHADER_INPUT_BIND_DESC &rhs) {
							return
								lhs.Space == rhs.Space ?
								lhs.BindPoint < rhs.BindPoint :
								lhs.Space < rhs.Space;
						}
					);
					u32 num_final_bindings = 1;
					for (usize i = 1; i < all_bindings.size(); ++i) {
						D3D12_SHADER_INPUT_BIND_DESC &prev_binding = all_bindings[num_final_bindings - 1];
						const D3D12_SHADER_INPUT_BIND_DESC &cur_binding = all_bindings[i];
						// check if this binding will be merged with the previous one
						const bool overlaps_with_prev =
							cur_binding.Space == prev_binding.Space && (
								prev_binding.BindCount == 0 ||
								cur_binding.BindPoint < prev_binding.BindPoint + prev_binding.BindCount
							);
						if (overlaps_with_prev) {
							crash_if(cur_binding.Type != prev_binding.Type);
							// TODO check other fields?
							if (cur_binding.BindCount == 0 || prev_binding.BindCount == 0) {
								prev_binding.BindCount = 0;
							} else {
								prev_binding.BindCount = std::max(
									prev_binding.BindCount,
									cur_binding.BindPoint + cur_binding.BindCount - prev_binding.BindPoint
								);
							}
							continue;
						}
						// otherwise, add this binding to the end of the list
						all_bindings[num_final_bindings] = cur_binding;
						++num_final_bindings;
					}
					all_bindings.erase(
						all_bindings.begin() + static_cast<std::ptrdiff_t>(num_final_bindings), all_bindings.end()
					);
				}

				std::vector<IRDescriptorRange1> all_ranges;
				std::vector<IRRootParameter1> root_params;
				if (!all_bindings.empty()) { // create root signature
					all_ranges.reserve(all_bindings.size());
					{ // reserve space for all descriptor tables upfront
						IRRootParameter1 empty_root_param = {};
						empty_root_param.ParameterType                       = IRRootParameterTypeDescriptorTable;
						empty_root_param.DescriptorTable.pDescriptorRanges   = nullptr;
						empty_root_param.DescriptorTable.NumDescriptorRanges = 0;
						empty_root_param.ShaderVisibility                    = IRShaderVisibilityAll;
						root_params.resize(all_bindings.back().Space + 1, empty_root_param);
					}
					auto add_root_table = [&](usize start, usize past_end) {
						const u32 space = all_ranges[start].RegisterSpace;
						root_params[space].DescriptorTable.pDescriptorRanges   = &all_ranges[start];
						root_params[space].DescriptorTable.NumDescriptorRanges = static_cast<u32>(past_end - start);
					};
					usize start_of_space = 0;
					for (usize i = 0; i < all_bindings.size(); ++i) {
						all_ranges.emplace_back(
							_details::conversions::d3d12_shader_input_bind_desc_to_ir_descriptor_range(
								all_bindings[i]
							)
						);
						if (all_ranges[i].RegisterSpace != all_ranges[start_of_space].RegisterSpace) {
							add_root_table(start_of_space, i);
							start_of_space = i;
						}
					}
					add_root_table(start_of_space, all_bindings.size()); // fill in the last range
				}

				root_signature = _details::shader::create_root_signature_for_bindings(root_params);
			}

			result_ir = _details::shader::convert_to_metal_ir(data, root_signature.get());
		}

		// create shader library
		NS::Error *err = nullptr;
		result._lib = NS::TransferPtr(_dev->newLibrary(result_ir.data, &err));
		if (err) {
			log().error("{}", err->localizedDescription()->utf8String());
			std::abort();
		}

		return result;
	}

	sampler device::create_sampler(
		filtering minification,
		filtering magnification,
		filtering mipmapping,
		f32 mip_lod_bias,
		f32 min_lod,
		f32 max_lod,
		std::optional<f32> max_anisotropy,
		sampler_address_mode addressing_u,
		sampler_address_mode addressing_v,
		sampler_address_mode addressing_w,
		linear_rgba_f32, // border color not supported
		comparison_function comparison
	) {
		auto smp = NS::TransferPtr(MTL::SamplerDescriptor::alloc()->init());
		smp->setSAddressMode(_details::conversions::to_sampler_address_mode(addressing_u));
		smp->setTAddressMode(_details::conversions::to_sampler_address_mode(addressing_v));
		smp->setRAddressMode(_details::conversions::to_sampler_address_mode(addressing_w));
		// TODO border color?
		smp->setMinFilter(_details::conversions::to_sampler_min_mag_filter(minification));
		smp->setMagFilter(_details::conversions::to_sampler_min_mag_filter(magnification));
		smp->setMipFilter(_details::conversions::to_sampler_mip_filter(mipmapping));
		smp->setLodMinClamp(min_lod);
		smp->setLodMaxClamp(max_lod);
		smp->setMaxAnisotropy(static_cast<NS::UInteger>(max_anisotropy.value_or(1.0f)));
		smp->setCompareFunction(_details::conversions::to_compare_function(comparison));
		smp->setSupportArgumentBuffers(true);
		return sampler(NS::TransferPtr(_dev->newSamplerState(smp.get())), mip_lod_bias);
	}

	descriptor_set_layout device::create_descriptor_set_layout(
		std::span<const descriptor_range_binding> bindings, shader_stage stage
	) {
		descriptor_set_layout result = nullptr;
		result._bindings.insert(result._bindings.end(), bindings.begin(), bindings.end());
		result._stage = stage;
		std::sort(
			result._bindings.begin(),
			result._bindings.end(),
			[](descriptor_range_binding lhs, descriptor_range_binding rhs) {
				return lhs.register_index < rhs.register_index;
			}
		);
		// verify that there are no overlapping ranges
		for (usize i = 1; i < result._bindings.size(); ++i) {
			crash_if(result._bindings[i].register_index <= result._bindings[i - 1].get_last_register_index());
		}
		return result;
	}

	pipeline_resources device::create_pipeline_resources(std::span<const gpu::descriptor_set_layout *const>) {
		// TODO
		return nullptr;
	}

	graphics_pipeline_state device::create_graphics_pipeline_state(
		const pipeline_resources&,
		const shader_binary *vs,
		const shader_binary *ps,
		const shader_binary *ds,
		const shader_binary *hs,
		const shader_binary *gs,
		std::span<const render_target_blend_options> blend,
		const rasterizer_options &rasterizer,
		const depth_stencil_options &depth_stencil,
		std::span<const input_buffer_layout> input_buffers,
		primitive_topology topology,
		const frame_buffer_layout &fb_layout,
		[[maybe_unused]] u32 num_viewports
	) {
		auto vert_descriptor = NS::TransferPtr(MTL::VertexDescriptor::alloc()->init());
		for (const input_buffer_layout &input_layout : input_buffers) {
			const NS::UInteger buffer_index = kIRVertexBufferBindPoint + input_layout.buffer_index;
			MTL::VertexBufferLayoutDescriptor *buffer_layout = vert_descriptor->layouts()->object(buffer_index);
			buffer_layout->setStride(input_layout.stride);
			buffer_layout->setStepFunction(_details::conversions::to_vertex_step_function(input_layout.input_rate));
			for (const input_buffer_element &input_elem: input_layout.elements) {
				// find the index of this attribute
				std::string semantic = std::format(
					"{}{}", string::to_generic(input_elem.semantic_name), input_elem.semantic_index
				);
				for (char &c : semantic) {
					c = static_cast<char>(std::tolower(c));
				}
				NS::UInteger elem_index = std::numeric_limits<NS::UInteger>::max();
				for (usize i = 0; i < vs->_vs_input_attributes.size(); ++i) {
					if (vs->_vs_input_attributes[i].name == string::assume_utf8(semantic)) {
						crash_if(elem_index != std::numeric_limits<NS::UInteger>::max()); // duplicate semantic
						elem_index = i;
					}
				}
				crash_if(elem_index == std::numeric_limits<NS::UInteger>::max());
				elem_index += kIRStageInAttributeStartIndex;

				// add vertex attribute
				MTL::VertexAttributeDescriptor *attr = vert_descriptor->attributes()->object(elem_index);
				attr->setFormat(_details::conversions::to_vertex_format(input_elem.element_format));
				attr->setOffset(input_elem.byte_offset);
				attr->setBufferIndex(buffer_index);
			}
		}

		auto descriptor = NS::TransferPtr(MTL::RenderPipelineDescriptor::alloc()->init());
		descriptor->setVertexFunction(_details::get_single_shader_function(vs->_lib.get()).get());
		descriptor->setFragmentFunction(_details::get_single_shader_function(ps->_lib.get()).get());
		// TODO tessellation shaders?
		descriptor->setVertexDescriptor(vert_descriptor.get());
		for (usize i = 0; i < fb_layout.color_render_target_formats.size(); ++i) {
			const render_target_blend_options &blend_opts = blend[i];
			MTL::RenderPipelineColorAttachmentDescriptor *attachment = descriptor->colorAttachments()->object(i);
			attachment->setPixelFormat(_details::conversions::to_pixel_format(
				fb_layout.color_render_target_formats[i]
			));
			attachment->setWriteMask(_details::conversions::to_color_write_mask(blend_opts.write_mask));
			attachment->setBlendingEnabled(blend_opts.enabled);
			attachment->setAlphaBlendOperation(
				_details::conversions::to_blend_operation(blend_opts.alpha_operation)
			);
			attachment->setRgbBlendOperation(
				_details::conversions::to_blend_operation(blend_opts.color_operation)
			);
			attachment->setDestinationAlphaBlendFactor(
				_details::conversions::to_blend_factor(blend_opts.destination_alpha)
			);
			attachment->setDestinationRGBBlendFactor(
				_details::conversions::to_blend_factor(blend_opts.destination_color)
			);
			attachment->setSourceAlphaBlendFactor(
				_details::conversions::to_blend_factor(blend_opts.source_alpha)
			);
			attachment->setSourceRGBBlendFactor(
				_details::conversions::to_blend_factor(blend_opts.source_color)
			);
		}
		{
			const auto &fmt_props = format_properties::get(fb_layout.depth_stencil_render_target_format);
			const MTL::PixelFormat ds_format =
				_details::conversions::to_pixel_format(fb_layout.depth_stencil_render_target_format);
			if (fmt_props.has_depth()) {
				descriptor->setDepthAttachmentPixelFormat(ds_format);
			}
			if (fmt_props.has_stencil()) {
				descriptor->setStencilAttachmentPixelFormat(ds_format);
			}
		}
		descriptor->setInputPrimitiveTopology(_details::conversions::to_primitive_topology_class(topology));
		// TODO multisample and tessellation settings
		descriptor->setSupportIndirectCommandBuffers(true);
		descriptor->setShaderValidation(_details::conversions::to_shader_validation(_context_opts));

		auto ds_descriptor = NS::TransferPtr(MTL::DepthStencilDescriptor::alloc()->init());
		if (depth_stencil.enable_depth_testing) {
			ds_descriptor->setDepthCompareFunction(
				_details::conversions::to_compare_function(depth_stencil.depth_comparison)
			);
			ds_descriptor->setDepthWriteEnabled(depth_stencil.write_depth);
		}
		if (depth_stencil.enable_stencil_testing) {
			ds_descriptor->setBackFaceStencil(_details::conversions::to_stencil_descriptor(
				depth_stencil.stencil_back_face, depth_stencil.stencil_read_mask, depth_stencil.stencil_write_mask
			).get());
			ds_descriptor->setFrontFaceStencil(_details::conversions::to_stencil_descriptor(
				depth_stencil.stencil_front_face, depth_stencil.stencil_read_mask, depth_stencil.stencil_write_mask
			).get());
		}

		NS::Error *error = nullptr;
		auto pipeline_state = NS::TransferPtr(_dev->newRenderPipelineState(descriptor.get(), &error));
		if (error) {
			log().error("{}", error->localizedDescription()->utf8String());
			std::abort();
		}
		auto depth_stencil_state = NS::TransferPtr(_dev->newDepthStencilState(ds_descriptor.get()));
		// TODO num viewports?
		return graphics_pipeline_state(
			std::move(pipeline_state), std::move(depth_stencil_state), rasterizer, topology
		);
	}

	compute_pipeline_state device::create_compute_pipeline_state(
		const pipeline_resources&, const shader_binary &shader
	) {
		NS::Error *error = nullptr;
		auto pipeline = NS::TransferPtr(_dev->newComputePipelineState(
			_details::get_single_shader_function(shader._lib.get()).get(), &error
		));
		if (error) {
			log().error("{}", error->localizedDescription()->utf8String());
			std::abort();
		}
		return compute_pipeline_state(std::move(pipeline), shader._thread_group_size);
	}

	std::span<const std::pair<memory_type_index, memory_properties>> device::enumerate_memory_types() const {
		constexpr static std::array _memory_types{
			std::pair(static_cast<memory_type_index>(_details::memory_type_index::shared_cpu_cached),   memory_properties::host_visible | memory_properties::host_cached),
			std::pair(static_cast<memory_type_index>(_details::memory_type_index::shared_cpu_uncached), memory_properties::host_visible                                 ),
			std::pair(static_cast<memory_type_index>(_details::memory_type_index::device_private),      memory_properties::device_local                                 ),
		};
		static_assert(
			std::size(_memory_types) == std::to_underlying(_details::memory_type_index::num_enumerators),
			"Missing memory types"
		);
		return _memory_types;
	}

	memory_block device::allocate_memory(usize size, memory_type_index type) {
		auto heap_descriptor = NS::TransferPtr(MTL::HeapDescriptor::alloc()->init());
		heap_descriptor->setType(MTL::HeapTypePlacement);
		heap_descriptor->setResourceOptions(_details::conversions::to_resource_options(type));
		heap_descriptor->setHazardTrackingMode(MTL::HazardTrackingModeUntracked);
		heap_descriptor->setSize(size);
		auto heap = NS::TransferPtr(_dev->newHeap(heap_descriptor.get()));
		return memory_block({ std::move(heap), _residency_set.get() });
	}

	buffer device::create_committed_buffer(usize size, memory_type_index type, buffer_usage_mask) {
		auto buf = NS::TransferPtr(_dev->newBuffer(size, _details::conversions::to_resource_options(type)));
		return buffer({ std::move(buf), _residency_set.get() });
	}

	image2d device::create_committed_image2d(
		cvec2u32 size,
		u32 mip_levels,
		format fmt,
		image_tiling, // no support
		image_usage_mask usages
	) {
		auto descriptor = _details::create_texture_descriptor(
			MTL::TextureType2DArray, // need to use array type for Metal-DXIR interop
			fmt,
			cvec3u32(size, 1u),
			mip_levels,
			MTL::ResourceOptionCPUCacheModeWriteCombined | MTL::ResourceStorageModePrivate,
			usages
		);
		auto img = NS::TransferPtr(_dev->newTexture(descriptor.get()));
		return image2d({ std::move(img), _residency_set.get() });
	}

	image3d device::create_committed_image3d(
		cvec3u32 size,
		u32 mip_levels,
		format fmt,
		image_tiling, // no support
		image_usage_mask usages
	) {
		auto descriptor = _details::create_texture_descriptor(
			MTL::TextureType3D,
			fmt,
			size,
			mip_levels,
			MTL::ResourceOptionCPUCacheModeWriteCombined | MTL::ResourceStorageModePrivate,
			usages
		);
		auto img = NS::TransferPtr(_dev->newTexture(descriptor.get()));
		return image3d({ std::move(img), _residency_set.get() });
	}

	std::tuple<buffer, staging_buffer_metadata, usize> device::create_committed_staging_buffer(
		cvec2u32 size, format fmt, memory_type_index mem_type, buffer_usage_mask usages
	) {
		// the buffer is tightly packed
		const auto &format_props = format_properties::get(fmt);
		const usize bytes_per_row = size[0] * format_props.bytes_per_fragment;
		const usize buf_size = bytes_per_row * size[1];
		buffer buf = create_committed_buffer(buf_size, mem_type, usages);
		staging_buffer_metadata result = uninitialized;
		result.image_size         = size;
		result.row_pitch_in_bytes = static_cast<u32>(bytes_per_row);
		result.pixel_format       = fmt;
		return { std::move(buf), result, buf_size };
	}

	memory::size_alignment device::get_image2d_memory_requirements(
		cvec2u32 size,
		u32 mip_levels,
		format fmt,
		image_tiling, // no support
		image_usage_mask usages
	) {
		auto descriptor = _details::create_texture_descriptor(
			MTL::TextureType2DArray,
			fmt,
			cvec3u32(size, 1u),
			mip_levels,
			0, // TODO what resource options should be used? does it affect anything?
			usages
		);
		return _details::conversions::back_to_size_alignment(_dev->heapTextureSizeAndAlign(descriptor.get()));
	}

	memory::size_alignment device::get_image3d_memory_requirements(
		cvec3u32 size,
		u32 mip_levels,
		format fmt,
		image_tiling, // no support
		image_usage_mask usages
	) {
		auto descriptor = _details::create_texture_descriptor(
			MTL::TextureType3D,
			fmt,
			size,
			mip_levels,
			0, // TODO what resource options should be used? does it affect anything?
			usages
		);
		return _details::conversions::back_to_size_alignment(_dev->heapTextureSizeAndAlign(descriptor.get()));
	}

	memory::size_alignment device::get_buffer_memory_requirements(usize size, buffer_usage_mask usages) {
		if (bit_mask::contains<buffer_usage_mask::acceleration_structure>(usages)) {
			return _details::conversions::back_to_size_alignment(_dev->heapAccelerationStructureSizeAndAlign(size));
		}
		// TODO what resource options should be used? does it affect anything?
		return _details::conversions::back_to_size_alignment(_dev->heapBufferSizeAndAlign(size, 0));
	}

	buffer device::create_placed_buffer(usize size, buffer_usage_mask, const memory_block &mem, usize offset) {
		auto ptr = NS::TransferPtr(mem._heap->newBuffer(size, mem._heap->resourceOptions(), offset));
		return buffer({ std::move(ptr), nullptr });
	}

	image2d device::create_placed_image2d(
		cvec2u32 size,
		u32 mip_levels,
		format fmt,
		image_tiling, // no support
		image_usage_mask usages,
		const memory_block &mem,
		usize offset
	) {
		auto descriptor = _details::create_texture_descriptor(
			MTL::TextureType2DArray,
			fmt,
			cvec3u32(size, 1u),
			mip_levels,
			mem._heap->resourceOptions(),
			usages
		);
		auto ptr = NS::TransferPtr(mem._heap->newTexture(descriptor.get(), offset));
		return image2d({ std::move(ptr), nullptr });
	}

	image3d device::create_placed_image3d(
		cvec3u32 size,
		u32 mip_levels,
		format fmt,
		image_tiling, // no support
		image_usage_mask usages,
		const memory_block &mem,
		usize offset
	) {
		auto descriptor = _details::create_texture_descriptor(
			MTL::TextureType3D,
			fmt,
			size,
			mip_levels,
			mem._heap->resourceOptions(),
			usages
		);
		auto ptr = NS::TransferPtr(mem._heap->newTexture(descriptor.get(), offset));
		return image3d({ std::move(ptr), nullptr });
	}

	std::byte *device::map_buffer(buffer &buf) {
		return static_cast<std::byte*>(buf._buf->contents());
	}

	void device::unmap_buffer(buffer&) {
	}

	void device::flush_mapped_buffer_to_host(buffer&, usize, usize) {
	}

	void device::flush_mapped_buffer_to_device(buffer&, usize, usize) {
	}

	image2d_view device::create_image2d_view_from(const image2d &img, format fmt, mip_levels mips) {
		if (img._tex->framebufferOnly()) {
			// cannot create views of framebuffer only textures
			// TODO check that the formats etc. match
			crash_if(_details::conversions::to_pixel_format(fmt) != img._tex->pixelFormat());
			return image2d_view(img._tex.get_ptr());
		}

		return image2d_view(NS::TransferPtr(img._tex->newTextureView(
			_details::conversions::to_pixel_format(fmt),
			MTL::TextureType2DArray,
			_details::conversions::to_range(mips, img._tex.get()),
			NS::Range(0, 1)
		)));
	}

	image3d_view device::create_image3d_view_from(const image3d &img, format fmt, mip_levels mips) {
		return image3d_view(NS::TransferPtr(img._tex->newTextureView(
			_details::conversions::to_pixel_format(fmt),
			MTL::TextureType3D,
			_details::conversions::to_range(mips, img._tex.get()),
			NS::Range(0, 1)
		)));
	}

	frame_buffer device::create_frame_buffer(
		std::span<const gpu::image2d_view *const> color_rts, const image2d_view *depth_stencil_rt, cvec2u32 size
	) {
		frame_buffer result = nullptr;
		result._color_rts.reserve(color_rts.size());
		for (const gpu::image2d_view *rt : color_rts) {
			result._color_rts.emplace_back(rt->_tex.get());
		}
		if (depth_stencil_rt) {
			result._depth_stencil_rt = depth_stencil_rt->_tex.get();
		}
		result._size = size;
		return result;
	}

	fence device::create_fence(synchronization_state st) {
		auto event = NS::TransferPtr(_dev->newSharedEvent());
		event->setSignaledValue(st == synchronization_state::set ? 1 : 0);
		return fence(std::move(event));
	}

	timeline_semaphore device::create_timeline_semaphore(gpu::_details::timeline_semaphore_value_type val) {
		auto event = NS::TransferPtr(_dev->newSharedEvent());
		event->setSignaledValue(val);
		return timeline_semaphore(std::move(event));
	}

	void device::reset_fence(fence &f) {
		f._event->setSignaledValue(0);
	}

	void device::wait_for_fence(fence &f) {
		f._event->waitUntilSignaledValue(1, std::numeric_limits<u64>::max());
	}

	void device::signal_timeline_semaphore(
		timeline_semaphore &sem, gpu::_details::timeline_semaphore_value_type val
	) {
		sem._event->setSignaledValue(val);
	}

	gpu::_details::timeline_semaphore_value_type device::query_timeline_semaphore(timeline_semaphore &sem) {
		return sem._event->signaledValue();
	}

	void device::wait_for_timeline_semaphore(
		timeline_semaphore &sem, gpu::_details::timeline_semaphore_value_type val
	) {
		sem._event->waitUntilSignaledValue(val, std::numeric_limits<u64>::max());
	}

	timestamp_query_heap device::create_timestamp_query_heap(u32 size) {
		auto descriptor = NS::TransferPtr(MTL::CounterSampleBufferDescriptor::alloc()->init());
		descriptor->setCounterSet(_timestamp_counter_set);
		descriptor->setStorageMode(MTL::StorageModeShared);
		descriptor->setSampleCount(size);
		NS::Error *err = nullptr;
		auto heap = NS::TransferPtr(_dev->newCounterSampleBuffer(descriptor.get(), &err));
		if (err) {
			log().error("Failed to create counter sample buffer: {}", err->localizedDescription()->utf8String());
			std::abort();
		}
		return timestamp_query_heap(std::move(heap));
	}

	void device::fetch_query_results(
		timestamp_query_heap &heap, u32 first, std::span<u64> timestamps
	) {
		NS::SharedPtr<NS::Data> data =
			NS::RetainPtr(heap._buf->resolveCounterRange(NS::Range(first, timestamps.size())));
		auto *result = static_cast<const MTL::CounterResultTimestamp*>(data->mutableBytes());
		crash_if(data->length() < timestamps.size() * sizeof(MTL::CounterResultTimestamp));
		for (usize i = 0; i < timestamps.size(); i++) {
			timestamps[i] = result[i].timestamp;
		}
	}

	void device::set_debug_name(buffer &buf, const char8_t *name) {
		buf._buf->setLabel(_details::conversions::to_string(name).get());
	}

	void device::set_debug_name(image_base &img, const char8_t *name) {
		static_cast<_details::basic_image_base&>(img)._tex->setLabel(_details::conversions::to_string(name).get());
	}

	void device::set_debug_name(image_view_base &img, const char8_t *name) {
		static_cast<_details::basic_image_view_base&>(img)._tex->setLabel(
			_details::conversions::to_string(name).get()
		);
	}

	bottom_level_acceleration_structure_geometry device::create_bottom_level_acceleration_structure_geometry(
		std::span<const raytracing_geometry_view> geoms
	) {
		std::vector<NS::SharedPtr<MTL::AccelerationStructureTriangleGeometryDescriptor>> descs_ptrs;
		std::vector<MTL::AccelerationStructureGeometryDescriptor*> descs;
		descs.reserve(geoms.size());
		for (usize i = 0; i < geoms.size(); ++i) {
			const raytracing_geometry_view &geom = geoms[i];
			auto desc = NS::TransferPtr(MTL::AccelerationStructureTriangleGeometryDescriptor::alloc()->init());
			desc->setOpaque(bit_mask::contains<raytracing_geometry_flags::opaque>(geom.flags));
			desc->setAllowDuplicateIntersectionFunctionInvocation(
				!bit_mask::contains<raytracing_geometry_flags::no_duplicate_any_hit_invocation>(geom.flags)
			);
			desc->setTriangleCount(geom.index_buffer.count / 3);
			desc->setIndexBuffer(geom.index_buffer.data->_buf.get());
			desc->setIndexType(_details::conversions::to_index_type(geom.index_buffer.element_format));
			desc->setIndexBufferOffset(geom.index_buffer.offset);
			desc->setVertexBuffer(geom.vertex_buffer.data->_buf.get());
			desc->setVertexBufferOffset(geom.vertex_buffer.offset);
			desc->setVertexStride(geom.vertex_buffer.stride);
			desc->setVertexFormat(_details::conversions::to_attribute_format(geom.vertex_buffer.vertex_format));
			descs.emplace_back(desc.get());
			descs_ptrs.emplace_back(std::move(desc));
		}
		const CFArrayRef cfarray = CFArrayCreate(
			kCFAllocatorDefault,
			const_cast<const void**>(reinterpret_cast<void**>(descs.data())),
			static_cast<CFIndex>(descs.size()),
			&kCFTypeArrayCallBacks
		);
		auto as_desc = NS::TransferPtr(MTL::PrimitiveAccelerationStructureDescriptor::alloc()->init());
		as_desc->setGeometryDescriptors(reinterpret_cast<const NS::Array*>(cfarray));
		CFRelease(cfarray);
		return bottom_level_acceleration_structure_geometry(std::move(as_desc));
	}

	instance_description device::get_bottom_level_acceleration_structure_description(
		bottom_level_acceleration_structure &blas,
		mat44f32 trans,
		u32 id,
		u8 mask,
		u32 hit_group_offset,
		raytracing_instance_flags flags
	) const {
		instance_description result = uninitialized;
		result._descriptor.transformationMatrix            =
			_details::conversions::to_packed_float4x3(trans.block<3, 4>(0, 0));
		result._descriptor.options                         =
			_details::conversions::to_acceleration_structure_instance_options(flags);
		result._descriptor.mask                            = mask;
		result._descriptor.intersectionFunctionTableOffset = hit_group_offset;
		result._descriptor.userID                          = id;
		result._descriptor.accelerationStructureID         = blas._as->gpuResourceID();
		return result;
	}

	acceleration_structure_build_sizes device::get_bottom_level_acceleration_structure_build_sizes(
		const bottom_level_acceleration_structure_geometry &geom
	) {
		return _details::conversions::back_to_acceleration_structure_build_sizes(
			_dev->accelerationStructureSizes(geom._descriptor.get())
		);
	}

	acceleration_structure_build_sizes device::get_top_level_acceleration_structure_build_sizes(
		usize instance_count
	) {
		auto desc = NS::TransferPtr(MTL::IndirectInstanceAccelerationStructureDescriptor::alloc()->init());
		desc->setInstanceDescriptorType(MTL::AccelerationStructureInstanceDescriptorTypeIndirect);
		desc->setMaxInstanceCount(instance_count);
		return _details::conversions::back_to_acceleration_structure_build_sizes(
			_dev->accelerationStructureSizes(desc.get())
		);
	}

	bottom_level_acceleration_structure device::create_bottom_level_acceleration_structure(
		buffer &buf, usize offset, usize size
	) {
		auto blas = _create_acceleration_structure(buf, offset, size);
		return bottom_level_acceleration_structure(std::move(blas));
	}

	top_level_acceleration_structure device::create_top_level_acceleration_structure(
		buffer &buf, usize offset, usize size
	) {
		// a conservative estimate of how many instances there are in the acceleration structure
		const usize instance_count = (size / sizeof(MTL::IndirectAccelerationStructureInstanceDescriptor)) + 1;

		// create acceleration structure
		auto as = _create_acceleration_structure(buf, offset, size);

		// header buffer
		auto header = NS::TransferPtr(_dev->newBuffer(
			sizeof(IRRaytracingAccelerationStructureGPUHeader) + sizeof(u32) * instance_count,
			MTL::ResourceCPUCacheModeWriteCombined |
				MTL::ResourceStorageModeShared            |
				MTL::ResourceHazardTrackingModeUntracked
		));
		auto *const header_data = static_cast<u8*>(header->contents());
		std::vector<u32> instance_offsets(instance_count, 0);
		IRRaytracingSetAccelerationStructure(
			header_data,
			as->gpuResourceID(),
			header_data + sizeof(IRRaytracingAccelerationStructureGPUHeader),
			instance_offsets.data(),
			instance_count
		);
		return top_level_acceleration_structure(std::move(as), { std::move(header), _residency_set.get() });
	}

	void device::write_descriptor_set_acceleration_structures(
		descriptor_set &set,
		const descriptor_set_layout &layout,
		u32 first_register,
		std::span<gpu::top_level_acceleration_structure *const> as
	) {
		_validate_descriptor_set_bindings(
			layout, first_register, static_cast<u32>(as.size()), descriptor_type::acceleration_structure
		);

		auto *arr = static_cast<IRDescriptorTableEntry*>(set._arg_buffer->contents());
		for (usize i = 0; i < as.size(); ++i) {
			IRDescriptorTableSetAccelerationStructure(&arr[first_register + i], as[i]->_header->gpuAddress());
		}
	}

	shader_group_handle device::get_shader_group_handle(const raytracing_pipeline_state&, usize index) {
		// TODO
	}

	raytracing_pipeline_state device::create_raytracing_pipeline_state(
		std::span<const shader_function> hit_group_shaders,
		std::span<const hit_shader_group> hit_groups,
		std::span<const shader_function> general_shaders,
		usize max_recursion_depth,
		usize max_payload_size,
		usize max_attribute_size,
		const pipeline_resources&
	) {
		// collect the list of linked functions
		auto linked_funcs = NS::TransferPtr(MTL::LinkedFunctions::alloc()->init());
		{
			auto bookmark = get_scratch_bookmark();
			auto shader_funcs = bookmark.create_vector_array<MTL::Function*>();
			for (const shader_function &group_func : hit_group_shaders) {
				MTL::Function *func = group_func.code->_lib->newFunction(NS::String::string(
					reinterpret_cast<const char*>(group_func.entry_point), NS::UTF8StringEncoding
				));
				crash_if(!func);
				shader_funcs.emplace_back(func);
			}
			CFArrayRef functions = CFArrayCreate(
				kCFAllocatorDefault,
				const_cast<const void**>(reinterpret_cast<void**>(shader_funcs.data())),
				static_cast<CFIndex>(shader_funcs.size()),
				&kCFTypeArrayCallBacks
			);
			linked_funcs->setFunctions(reinterpret_cast<const NS::Array*>(functions));
			CFRelease(functions);
		}

		auto compute_pipeline = NS::TransferPtr(MTL::ComputePipelineDescriptor::alloc()->init());
		compute_pipeline->setComputeFunction(_maybe_create_raygen_shader());
		compute_pipeline->setMaxCallStackDepth(max_recursion_depth); // TODO is this correct?
		compute_pipeline->setLabel(NS::String::string("Raytracing Pipeline", NS::UTF8StringEncoding));
		compute_pipeline->setLinkedFunctions(linked_funcs.get());
		// TODO
	}

	device::device(NS::SharedPtr<MTL::Device> dev, NS::SharedPtr<MTL::ResidencySet> set, context_options opts) :
		_dev(std::move(dev)), _residency_set(std::move(set)), _context_opts(opts) {

		// find the timestamp counter set
		NS::Array *counter_sets = _dev->counterSets();
		for (NS::UInteger i = 0; i < counter_sets->count(); ++i) {
			auto *counter_set = counter_sets->object<MTL::CounterSet>(i);
			if (counter_set->name()->isEqualToString(MTL::CommonCounterSetTimestamp)) {
				_timestamp_counter_set = counter_set;
				break;
			}
		}
	}

	_details::residency_ptr<MTL::AccelerationStructure> device::_create_acceleration_structure(
		buffer &buf, usize offset, usize size
	) {
		NS::SharedPtr<MTL::AccelerationStructure> result;
		MTL::ResidencySet *set = nullptr;
		if (buf._buf->heap()) {
			result = NS::TransferPtr(buf._buf->heap()->newAccelerationStructure(
				size, buf._buf->heapOffset() + offset
			));
		} else {
			// TODO metal does not support creating acceleration structures directly inside buffers
			result = NS::TransferPtr(buf._buf->device()->newAccelerationStructure(size));
			set = _residency_set.get();
		}
		crash_if(!result);
		return { std::move(result), set };
	}

	void device::_maybe_set_descriptor_set_name(MTL::Buffer *buf, const descriptor_set_layout &layout) {
		constexpr static enums::sequential_mapping<descriptor_type, char8_t> _ids{
			std::pair(descriptor_type::sampler,                u8's'),
			std::pair(descriptor_type::read_only_image,        u8'i'),
			std::pair(descriptor_type::read_write_image,       u8'I'),
			std::pair(descriptor_type::read_only_buffer,       u8'b'),
			std::pair(descriptor_type::read_write_buffer,      u8'B'),
			std::pair(descriptor_type::constant_buffer,        u8'c'),
			std::pair(descriptor_type::acceleration_structure, u8'a'),
		};

		if (bit_mask::contains<context_options::enable_debug_info>(_context_opts)) {
			std::string name = "DescriptorTable";
			for (const descriptor_range_binding &binding : layout._bindings) {
				const std::string count =
					binding.range.count == descriptor_range::unbounded_count ?
					"+" :
					std::format("{}", binding.range.count);
				name += std::format(
					"_{}[{},{}]", static_cast<char>(_ids[binding.range.type]), binding.register_index, count
				);
			}
			buf->setLabel(_details::conversions::to_string(reinterpret_cast<const char8_t*>(name.c_str())).get());
		}
	}

	MTL::Function *device::_maybe_create_raygen_shader() {
		if (_raygen_shader) {
			return _raygen_shader.get();
		}

		const auto compiler = _details::ir_make_unique(IRCompilerCreate());
		auto metal_lib = _details::ir_make_unique(IRMetalLibBinaryCreate());
		crash_if(!IRMetalLibSynthesizeIndirectRayDispatchFunction(compiler.get(), metal_lib.get()));
		const dispatch_data_t data = IRMetalLibGetBytecodeData(metal_lib.get());

		NS::Error *err = nullptr;
		auto library = NS::TransferPtr(_dev->newLibrary(data, &err));
		if (err) {
			log().error("{}", err->localizedDescription()->utf8String());
			std::abort();
		}
		_raygen_shader = _details::get_single_shader_function(library.get());

		return _raygen_shader.get();
	}

	void device::_validate_descriptor_set_bindings(
		const descriptor_set_layout &layout, u32 first_register, u32 count, descriptor_type type
	) {
		const u32 past_last_register = first_register + count;
		for (const descriptor_range_binding &binding : layout._bindings) {
			if (past_last_register <= binding.register_index) {
				continue;
			}
			if (!binding.range.is_unbounded() && first_register > binding.get_last_register_index()) {
				continue;
			}
			crash_if(binding.range.type != type);
			// TODO check that the entire range is covered
		}
	}

	void device::_write_descriptor_set_images(
		descriptor_set &set, u32 first_register, std::span<const image_view_base *const> images
	) {
		auto *arr = static_cast<IRDescriptorTableEntry*>(set._arg_buffer->contents());
		for (usize i = 0; i < images.size(); ++i) {
			const auto *const img = static_cast<const _details::basic_image_view_base*>(images[i]);
			IRDescriptorTableSetTexture(&arr[first_register + i], img->_tex.get(), 0.0f, 0);
		}
	}

	void device::_write_descriptor_set_structured_buffers(
		descriptor_set &set, u32 first_register, std::span<const structured_buffer_view> buffers
	) {
		auto *arr = static_cast<IRDescriptorTableEntry*>(set._arg_buffer->contents());
		for (usize i = 0; i < buffers.size(); ++i) {
			// TODO metal does not support custom strides?
			IRBufferView view = {};
			view.buffer       = buffers[i].data->_buf.get();
			view.bufferOffset = buffers[i].first * buffers[i].stride;
			view.bufferSize   = buffers[i].count * buffers[i].stride;
			view.typedBuffer  = true;
			IRDescriptorTableSetBufferView(&arr[first_register + i], &view);
		}
	}


	std::pair<device, std::vector<command_queue>> adapter::create_device(
		std::span<const queue_family> families
	) {
		// create residency set
		auto residency_set_desc = NS::TransferPtr(MTL::ResidencySetDescriptor::alloc()->init());
		NS::Error *error = nullptr;
		auto residency_set = NS::TransferPtr(_dev->newResidencySet(residency_set_desc.get(), &error));
		if (error) {
			log().error(
				"Failed to create residency set: {}",
				error->localizedDescription()->cString(NS::UTF8StringEncoding)
			);
			std::abort();
		}

		// create command queues
		std::vector<command_queue> queues;
		queues.reserve(families.size());
		for (usize i = 0; i < families.size(); ++i) {
			// Metal does not distinguish between different types of queues
			auto ptr = NS::TransferPtr(_dev->newCommandQueue());
			ptr->addResidencySet(residency_set.get());
			queues.emplace_back(command_queue(std::move(ptr)));
		}

		return { device(_dev, std::move(residency_set), _context_opts), std::move(queues) };
	}

	adapter_properties adapter::get_properties() const {
		adapter_properties result = uninitialized;
		result.name        = _details::conversions::back_to_string(_dev->name());
		result.is_software = false;
		result.is_discrete = _dev->location() != MTL::DeviceLocationBuiltIn;
		// TODO no way to query these?
		// https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf
		result.constant_buffer_alignment           = 32;
		result.acceleration_structure_alignment    = 1; // TODO
		result.shader_group_handle_size            = 1; // TODO
		result.shader_group_handle_alignment       = 1; // TODO
		result.shader_group_handle_table_alignment = 1; // TODO
		return result;
	}
}
