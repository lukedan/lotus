#include "lotus/graphics/backends/vulkan/device.h"

/// \file
/// Implementation of vulkan devices.

#include <format>

#include <spirv_reflect.h>

#include "lotus/logging.h"
#include "lotus/graphics/acceleration_structure.h"
#include "lotus/graphics/descriptors.h"
#include "lotus/graphics/frame_buffer.h"
#include "lotus/graphics/pipeline.h"
#include "lotus/graphics/synchronization.h"

namespace lotus::graphics::backends::vulkan {
	back_buffer_info device::acquire_back_buffer(swap_chain &swapchain) {
		auto &sync = swapchain._synchronization[swapchain._frame_counter];
		if (++swapchain._frame_counter == swapchain._synchronization.size()) {
			swapchain._frame_counter = 0;
		}
		// Vulkan requires that we have at least one synchronization primitive for the call. If there's none, then
		// either we're on the first frame, in which case we should copy from the next frame; or we just haven't
		// specified any which is an error
		if (sync.notify_fence == nullptr) {
			sync.notify_fence = sync.next_fence;
		}
		assert(sync.notify_fence);

		std::uint32_t frame_index;
		auto res = _device->acquireNextImageKHR(
			swapchain._swapchain.get(),
			std::numeric_limits<std::uint64_t>::max(),
			nullptr,
			sync.notify_fence ? static_cast<fence*>(sync.notify_fence)->_fence.get() : nullptr,
			&frame_index
		);
		swapchain._frame_index = static_cast<std::uint16_t>(frame_index);

		back_buffer_info result = uninitialized;
		result.index        = swapchain._frame_index;
		result.on_presented = sync.notify_fence;
		if (res == vk::Result::eErrorOutOfDateKHR || res == vk::Result::eErrorSurfaceLostKHR) {
			result.status = swap_chain_status::unavailable;
		} else if (res == vk::Result::eSuboptimalKHR) {
			result.status = swap_chain_status::suboptimal;
		} else {
			result.status = swap_chain_status::ok;
		}
		return result;
	}

	void device::resize_swap_chain_buffers(swap_chain &s, cvec2s size) {
		vk::UniqueSwapchainKHR old_swapchain = std::move(s._swapchain);
		vk::SwapchainCreateInfoKHR info;
		info
			.setSurface(s._surface.get())
			.setMinImageCount(static_cast<std::uint32_t>(s.get_image_count()))
			.setImageFormat(s._format.format)
			.setImageColorSpace(s._format.colorSpace)
			.setImageExtent(vk::Extent2D(static_cast<std::uint32_t>(size[0]), static_cast<std::uint32_t>(size[1])))
			.setImageArrayLayers(1)
			.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment)
			.setImageSharingMode(vk::SharingMode::eExclusive)
			.setPresentMode(vk::PresentModeKHR::eMailbox)
			.setClipped(true)
			.setOldSwapchain(old_swapchain.get());
		// TODO allocator
		s._swapchain = _details::unwrap(_device->createSwapchainKHRUnique(info));

		s._images = _details::unwrap(_device->getSwapchainImagesKHR(s._swapchain.get()));
		s._synchronization.resize(s._images.size(), nullptr);
	}

	command_queue device::create_command_queue() {
		command_queue result;
		result._queue = _device->getQueue(_graphics_compute_queue_family_index, 0);
		return result;
	}

	command_allocator device::create_command_allocator() {
		command_allocator alloc;

		vk::CommandPoolCreateInfo info;
		info
			.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
			.setQueueFamilyIndex(_graphics_compute_queue_family_index);

		// TODO allocator
		alloc._pool = _details::unwrap(_device->createCommandPoolUnique(info));

		return alloc;
	}

	command_list device::create_and_start_command_list(command_allocator &alloc) {
		command_list result = nullptr;

		vk::CommandBufferAllocateInfo info;
		info
			.setCommandPool(alloc._pool.get())
			.setLevel(vk::CommandBufferLevel::ePrimary)
			.setCommandBufferCount(1);
		auto buffers = _details::unwrap(_device->allocateCommandBuffers(info));
		assert(buffers.size() == 1);
		result._buffer = buffers[0];
		result._pool   = alloc._pool.get();
		result._device = this;

		vk::CommandBufferBeginInfo begin_info;
		_details::assert_vk(result._buffer.begin(begin_info));

		return result;
	}

	descriptor_pool device::create_descriptor_pool(
		std::span<const descriptor_range> capacity, std::size_t max_num_sets
	) {
		descriptor_pool result = nullptr;

		auto bookmark = stack_allocator::for_this_thread().bookmark();
		auto ranges = bookmark.create_reserved_vector_array<vk::DescriptorPoolSize>(capacity.size());
		for (const auto &range : capacity) {
			ranges.emplace_back()
				.setType(_details::conversions::to_descriptor_type(range.type))
				.setDescriptorCount(static_cast<std::uint32_t>(range.count));
		}

		vk::DescriptorPoolCreateInfo info;
		info
			.setFlags(
				vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet |
				vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind
			)
			.setMaxSets(static_cast<std::uint32_t>(max_num_sets))
			.setPoolSizeCount(static_cast<std::uint32_t>(capacity.size()))
			.setPoolSizes(ranges);
		// TODO allocator
		result._pool = _details::unwrap(_device->createDescriptorPoolUnique(info));

		return result;
	}

	descriptor_set device::create_descriptor_set(descriptor_pool &pool, const descriptor_set_layout &layout) {
		descriptor_set result = nullptr;

		vk::DescriptorSetAllocateInfo info;
		info
			.setDescriptorPool(pool._pool.get())
			.setSetLayouts(layout._layout.get());
		auto sets = _details::unwrap(_device->allocateDescriptorSetsUnique(info));
		assert(sets.size() == 1);
		result._set = std::move(sets[0]);
		result._variable_binding_index = layout._variable_binding_index;

		return result;
	}

	descriptor_set device::create_descriptor_set(
		descriptor_pool &pool, const descriptor_set_layout &layout, std::size_t dynamic_size
	) {
		descriptor_set result = nullptr;
		auto count = static_cast<std::uint32_t>(dynamic_size);
		vk::DescriptorSetVariableDescriptorCountAllocateInfo variable_count_info;
		variable_count_info
			.setDescriptorCounts(count);
		vk::DescriptorSetAllocateInfo info;
		info
			.setPNext(&variable_count_info)
			.setDescriptorPool(pool._pool.get())
			.setDescriptorSetCount(1)
			.setSetLayouts(layout._layout.get());
		auto sets = _details::unwrap(_device->allocateDescriptorSetsUnique(info));
		assert(sets.size() == 1);
		result._set = std::move(sets[0]);
		result._variable_binding_index = layout._variable_binding_index;

		return result;
	}

	/// Handles both when we're trying to write to a normal descriptor range and when we want to write to a variable
	/// count descriptor range.
	static void _set_write_descriptor_binding(
		vk::WriteDescriptorSet &info, std::size_t first_register, std::size_t variable_index
	) {
		if (first_register >= variable_index) {
			info
				.setDstBinding(static_cast<std::uint32_t>(variable_index))
				.setDstArrayElement(static_cast<std::uint32_t>(first_register - variable_index));
		} else {
			info
				.setDstBinding(static_cast<std::uint32_t>(first_register));
		}
	}

	void device::write_descriptor_set_read_only_images(
		descriptor_set &set, const descriptor_set_layout&,
		std::size_t first_register, std::span<const graphics::image_view *const> images
	) {
		auto bookmark = stack_allocator::for_this_thread().bookmark();
		auto imgs = bookmark.create_reserved_vector_array<vk::DescriptorImageInfo>(images.size());
		for (const auto *img : images) {
			imgs.emplace_back()
				.setImageView(img ? static_cast<const _details::image_view*>(img)->_view.get() : nullptr)
				.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
		}

		vk::WriteDescriptorSet info;
		info
			.setDstSet(set._set.get())
			.setDescriptorType(vk::DescriptorType::eSampledImage)
			.setImageInfo(imgs);
		_set_write_descriptor_binding(info, first_register, set._variable_binding_index);
		_device->updateDescriptorSets(info, {});
	}

	void device::write_descriptor_set_read_write_images(
		descriptor_set &set, const descriptor_set_layout&,
		std::size_t first_register, std::span<const image_view *const> images
	) {
		auto bookmark = stack_allocator::for_this_thread().bookmark();
		auto imgs = bookmark.create_reserved_vector_array<vk::DescriptorImageInfo>(images.size());
		for (const auto *img : images) {
			imgs.emplace_back()
				.setImageView(img ? static_cast<const _details::image_view*>(img)->_view.get() : nullptr)
				.setImageLayout(vk::ImageLayout::eGeneral);
		}

		vk::WriteDescriptorSet info;
		info
			.setDstSet(set._set.get())
			.setDescriptorType(vk::DescriptorType::eStorageImage)
			.setImageInfo(imgs);
		_set_write_descriptor_binding(info, first_register, set._variable_binding_index);
		_device->updateDescriptorSets(info, {});
	}

	void device::write_descriptor_set_read_only_structured_buffers(
		descriptor_set &set, const descriptor_set_layout&,
		std::size_t first_register, std::span<const structured_buffer_view> buffers
	) {
		auto bookmark = stack_allocator::for_this_thread().bookmark();
		auto bufs = bookmark.create_reserved_vector_array<vk::DescriptorBufferInfo>(buffers.size());
		for (const auto &buf : buffers) {
			bufs.emplace_back()
				.setBuffer(buf.data ? static_cast<buffer*>(buf.data)->_buffer : nullptr)
				.setOffset(buf.first * buf.stride)
				.setRange(buf.count * buf.stride);
		}

		vk::WriteDescriptorSet info;
		info
			.setDstSet(set._set.get())
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setBufferInfo(bufs);
		_set_write_descriptor_binding(info, first_register, set._variable_binding_index);
		_device->updateDescriptorSets(info, {});
	}

	void device::write_descriptor_set_constant_buffers(
		descriptor_set &set, const descriptor_set_layout&,
		std::size_t first_register, std::span<const constant_buffer_view> buffers
	) {
		auto bookmark = stack_allocator::for_this_thread().bookmark();
		auto bufs = bookmark.create_reserved_vector_array<vk::DescriptorBufferInfo>(buffers.size());
		for (const auto &buf : buffers) {
			bufs.emplace_back()
				.setBuffer(buf.data ? static_cast<const buffer*>(buf.data)->_buffer : nullptr)
				.setOffset(buf.offset)
				.setRange(buf.size);
		}

		vk::WriteDescriptorSet info;
		info
			.setDstSet(set._set.get())
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setBufferInfo(bufs);
		_set_write_descriptor_binding(info, first_register, set._variable_binding_index);
		_device->updateDescriptorSets(info, {});
	}

	void device::write_descriptor_set_samplers(
		descriptor_set &set, const descriptor_set_layout&,
		std::size_t first_register, std::span<const graphics::sampler *const> samplers
	) {
		auto bookmark = stack_allocator::for_this_thread().bookmark();
		auto smps = bookmark.create_reserved_vector_array<vk::DescriptorImageInfo>(samplers.size());
		for (const auto *smp : samplers) {
			smps.emplace_back()
				.setSampler(smp ? static_cast<const sampler*>(smp)->_sampler.get() : nullptr);
		}

		vk::WriteDescriptorSet info;
		info
			.setDstSet(set._set.get())
			.setDescriptorType(vk::DescriptorType::eSampler)
			.setImageInfo(smps);
		_set_write_descriptor_binding(info, first_register, set._variable_binding_index);
		_device->updateDescriptorSets(info, {});
	}

	shader_binary device::load_shader(std::span<const std::byte> data) {
		shader_binary result = nullptr;

		vk::ShaderModuleCreateInfo info;
		constexpr std::size_t word_size = sizeof(std::uint32_t);
		auto word_count = static_cast<std::uint32_t>((data.size() + word_size - 1) / word_size);
		info
			.setCode({ word_count, reinterpret_cast<const std::uint32_t*>(data.data()) });
		// TODO allocator
		result._module = _details::unwrap(_device->createShaderModuleUnique(info));

		result._reflection = spv_reflect::ShaderModule(data.size(), data.data());

		return result;
	}

	sampler device::create_sampler(
		filtering minification, filtering magnification, filtering mipmapping,
		float mip_lod_bias, float min_lod, float max_lod, std::optional<float> max_anisotropy,
		sampler_address_mode addressing_u, sampler_address_mode addressing_v, sampler_address_mode addressing_w,
		linear_rgba_f border_color, std::optional<comparison_function> comparison
	) {
		sampler result;

		vk::SamplerCustomBorderColorCreateInfoEXT border_color_info;
		std::array<float, 4> border_color_arr{ { border_color.r, border_color.g, border_color.b, border_color.a } };
		border_color_info
			.setCustomBorderColor(vk::ClearColorValue(border_color_arr))
			.setFormat(vk::Format::eUndefined);
		vk::SamplerCreateInfo info;
		info
			.setPNext(&border_color_info)
			.setMagFilter(_details::conversions::to_filter(magnification))
			.setMinFilter(_details::conversions::to_filter(minification))
			.setMipmapMode(_details::conversions::to_sampler_mipmap_mode(mipmapping))
			.setAddressModeU(_details::conversions::to_sampler_address_mode(addressing_u))
			.setAddressModeV(_details::conversions::to_sampler_address_mode(addressing_v))
			.setAddressModeW(_details::conversions::to_sampler_address_mode(addressing_w))
			.setMipLodBias(mip_lod_bias)
			.setAnisotropyEnable(max_anisotropy.has_value())
			.setMaxAnisotropy(max_anisotropy.value_or(0.0f))
			.setCompareEnable(comparison.has_value())
			.setCompareOp(_details::conversions::to_compare_op(comparison.value_or(comparison_function::always)))
			.setMinLod(min_lod)
			.setMaxLod(max_lod)
			.setBorderColor(vk::BorderColor::eFloatCustomEXT);
		// TODO allocator
		result._sampler = _details::unwrap(_device->createSamplerUnique(info));

		return result;
	}

	descriptor_set_layout device::create_descriptor_set_layout(
		std::span<const descriptor_range_binding> ranges, shader_stage visible_stages
	) {
		descriptor_set_layout result = nullptr;

		auto stages = _details::conversions::to_shader_stage_flags(visible_stages);

		auto bookmark = stack_allocator::for_this_thread().bookmark();
		auto arr = bookmark.create_vector_array<vk::DescriptorSetLayoutBinding>();
		auto flags_arr = bookmark.create_vector_array<vk::DescriptorBindingFlags>();
		for (const auto &rng : ranges) {
			vk::DescriptorSetLayoutBinding binding;
			binding
				.setDescriptorType(_details::conversions::to_descriptor_type(rng.range.type))
				.setStageFlags(stages);
			if (rng.range.count == descriptor_range::unbounded_count) {
				binding
					.setDescriptorCount(65536) // TODO what should I use here?
					.setBinding(static_cast<std::uint32_t>(rng.register_index));
				arr.emplace_back(binding);
				flags_arr.emplace_back(
					vk::DescriptorBindingFlagBits::eVariableDescriptorCount |
					vk::DescriptorBindingFlagBits::ePartiallyBound |
					vk::DescriptorBindingFlagBits::eUpdateAfterBind |
					vk::DescriptorBindingFlagBits::eUpdateUnusedWhilePending
				);
				result._variable_binding_index = rng.register_index;
			} else {
				binding.setDescriptorCount(1);
				for (std::size_t i = 0; i < rng.range.count; ++i) {
					binding.setBinding(static_cast<std::uint32_t>(rng.register_index + i));
					arr.emplace_back(binding);
					flags_arr.emplace_back(vk::DescriptorBindingFlags());
				}
			}
		}

		vk::DescriptorSetLayoutBindingFlagsCreateInfo variable_info;
		variable_info
			.setBindingFlags(flags_arr);

		vk::DescriptorSetLayoutCreateInfo info;
		info
			.setPNext(&variable_info)
			.setFlags(vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool)
			.setBindings(arr);
		// TODO allocator
		result._layout = _details::unwrap(_device->createDescriptorSetLayoutUnique(info));

		return result;
	}

	pipeline_resources device::create_pipeline_resources(
		std::span<const graphics::descriptor_set_layout *const> layouts
	) {
		pipeline_resources result = nullptr;

		auto bookmark = stack_allocator::for_this_thread().bookmark();
		auto arr = bookmark.create_reserved_vector_array<vk::DescriptorSetLayout>(layouts.size());
		for (const auto *layout : layouts) {
			arr.emplace_back(static_cast<const descriptor_set_layout*>(layout)->_layout.get());
		}

		vk::PipelineLayoutCreateInfo info;
		info.setSetLayouts(arr);
		// TODO allocator
		result._layout = _details::unwrap(_device->createPipelineLayoutUnique(info));

		return result;
	}

	graphics_pipeline_state device::create_graphics_pipeline_state(
		const pipeline_resources &resources,
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
		std::size_t num_viewports
	) {
		graphics_pipeline_state result = nullptr;

		auto bookmark = stack_allocator::for_this_thread().bookmark();

		std::size_t count = 0;
		std::array<vk::PipelineShaderStageCreateInfo, 5> stages;
		auto add_shader = [&](const shader_binary *s, vk::ShaderStageFlagBits stage) {
			if (s) {
				const char *entry_point = nullptr;
				// TODO this is bad
				for (std::uint32_t i = 0; i < s->_reflection.GetEntryPointCount(); ++i) {
					if (
						static_cast<std::uint32_t>(s->_reflection.GetEntryPointShaderStage(i)) ==
						static_cast<std::uint32_t>(stage)
					) {
						entry_point = s->_reflection.GetEntryPointName(i);
						break;
					}
				}
				stages[count]
					.setStage(stage)
					.setModule(s->_module.get())
					.setPName(entry_point);
				++count;
			}
		};
		add_shader(vs, vk::ShaderStageFlagBits::eVertex);
		add_shader(ps, vk::ShaderStageFlagBits::eFragment);
		add_shader(hs, vk::ShaderStageFlagBits::eTessellationControl);
		add_shader(ds, vk::ShaderStageFlagBits::eTessellationEvaluation);
		add_shader(gs, vk::ShaderStageFlagBits::eGeometry);

		auto input_bindings =
			bookmark.create_reserved_vector_array<vk::VertexInputBindingDescription>(input_buffers.size());
		auto attribute_descriptions = bookmark.create_vector_array<vk::VertexInputAttributeDescription>();
		{
			for (const auto &buf : input_buffers) {
				input_bindings.emplace_back()
					.setBinding(static_cast<std::uint32_t>(buf.buffer_index))
					.setStride(static_cast<std::uint32_t>(buf.stride))
					.setInputRate(_details::conversions::to_vertex_input_rate(buf.input_rate));
				for (const auto &attr : buf.elements) {
					SpvReflectResult spv_result;
					const SpvReflectInterfaceVariable *input = nullptr;
					if (attr.semantic_index == 0) {
						input = vs->_reflection.GetInputVariableBySemantic(
							reinterpret_cast<const char*>(attr.semantic_name), &spv_result
						);
						if (spv_result == SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND) {
							input = nullptr;
						} else {
							_details::assert_spv_reflect(spv_result);
						}
					}
					if (!input) {
						char sem_buf[30];
						auto fmt_result = std::format_to_n(
							std::begin(sem_buf), std::size(sem_buf) - 1,
							"{}{}", reinterpret_cast<const char*>(attr.semantic_name), attr.semantic_index
						);
						assert(static_cast<std::size_t>(fmt_result.size) + 1 < std::size(sem_buf));
						sem_buf[fmt_result.size] = '\0';
						input = vs->_reflection.GetInputVariableBySemantic(sem_buf, &spv_result);
						_details::assert_spv_reflect(spv_result);
					}
					attribute_descriptions.emplace_back()
						.setLocation(input->location)
						.setBinding(static_cast<std::uint32_t>(buf.buffer_index))
						.setFormat(_details::conversions::to_format(attr.element_format))
						.setOffset(static_cast<std::uint32_t>(attr.byte_offset));
				}
			}
		}

		vk::PipelineVertexInputStateCreateInfo vertex_input;
		vertex_input
			.setVertexBindingDescriptions(input_bindings)
			.setVertexAttributeDescriptions(attribute_descriptions);

		vk::PipelineInputAssemblyStateCreateInfo input_assembly;
		input_assembly
			.setTopology(_details::conversions::to_primitive_topology(topology));

		vk::PipelineViewportStateCreateInfo viewport;
		viewport
			.setViewportCount(static_cast<std::uint32_t>(num_viewports))
			.setScissorCount(static_cast<std::uint32_t>(num_viewports));

		vk::PipelineRasterizationStateCreateInfo rasterization;
		rasterization
			.setPolygonMode(rasterizer.is_wireframe ? vk::PolygonMode::eLine : vk::PolygonMode::eFill)
			.setCullMode(_details::conversions::to_cull_mode_flags(rasterizer.culling))
			.setFrontFace(_details::conversions::to_front_face(rasterizer.front_facing))
			.setLineWidth(1.0f);
		if (rasterizer.depth_bias.bias > 0.0f && rasterizer.depth_bias.slope_scaled_bias > 0.0f) {
			rasterization
				.setDepthBiasEnable(true)
				.setDepthBiasConstantFactor(rasterizer.depth_bias.bias)
				.setDepthBiasClamp(rasterizer.depth_bias.clamp)
				.setDepthBiasSlopeFactor(rasterizer.depth_bias.slope_scaled_bias);
		}

		vk::PipelineMultisampleStateCreateInfo multisample;

		vk::PipelineDepthStencilStateCreateInfo depth_stencil_info;
		depth_stencil_info
			.setDepthTestEnable(depth_stencil.enable_depth_testing)
			.setDepthWriteEnable(depth_stencil.write_depth)
			.setDepthCompareOp(_details::conversions::to_compare_op(depth_stencil.depth_comparison))
			.setStencilTestEnable(depth_stencil.enable_stencil_testing)
			.setFront(_details::conversions::to_stencil_op_state(
				depth_stencil.stencil_front_face, depth_stencil.stencil_read_mask, depth_stencil.stencil_write_mask
			))
			.setBack(_details::conversions::to_stencil_op_state(
				depth_stencil.stencil_back_face, depth_stencil.stencil_read_mask, depth_stencil.stencil_write_mask
			));

		auto rt_blends =
			bookmark.create_reserved_vector_array<vk::PipelineColorBlendAttachmentState>(blend.size());
		for (const auto &op : blend) {
			rt_blends.emplace_back()
				.setBlendEnable(op.enabled)
				.setSrcColorBlendFactor(_details::conversions::to_blend_factor(op.source_color))
				.setDstColorBlendFactor(_details::conversions::to_blend_factor(op.destination_color))
				.setColorBlendOp(_details::conversions::to_blend_op(op.color_operation))
				.setSrcAlphaBlendFactor(_details::conversions::to_blend_factor(op.source_alpha))
				.setDstAlphaBlendFactor(_details::conversions::to_blend_factor(op.destination_alpha))
				.setAlphaBlendOp(_details::conversions::to_blend_op(op.alpha_operation))
				.setColorWriteMask(_details::conversions::to_color_component_flags(op.write_mask));
		}

		vk::PipelineColorBlendStateCreateInfo color_blend;
		color_blend.setAttachments(rt_blends);

		std::array dynamic_states{
			vk::DynamicState::eViewport,
			vk::DynamicState::eScissor,
		};
		vk::PipelineDynamicStateCreateInfo dynamic;
		dynamic.setDynamicStates(dynamic_states);

		auto color_rt_formats = bookmark.create_reserved_vector_array<vk::Format>(
			fb_layout.color_render_target_formats.size()
		);
		for (const auto &f : fb_layout.color_render_target_formats) {
			color_rt_formats.emplace_back(_details::conversions::to_format(f));
		}

		vk::Format ds_rt_format = _details::conversions::to_format(fb_layout.depth_stencil_render_target_format);

		vk::PipelineRenderingCreateInfo fb_info;
		fb_info
			.setColorAttachmentFormats(color_rt_formats)
			.setDepthAttachmentFormat(ds_rt_format)
			.setStencilAttachmentFormat(ds_rt_format);

		vk::GraphicsPipelineCreateInfo info;
		info
			.setPNext(&fb_info)
			.setStages({ static_cast<std::uint32_t>(count), stages.data() })
			.setPVertexInputState(&vertex_input)
			.setPInputAssemblyState(&input_assembly)
			.setPViewportState(&viewport)
			.setPRasterizationState(&rasterization)
			.setPMultisampleState(&multisample)
			.setPDepthStencilState(&depth_stencil_info)
			.setPColorBlendState(&color_blend)
			.setPDynamicState(&dynamic)
			.setLayout(resources._layout.get())
			.setSubpass(0);
		// TODO allocator
		result._pipeline = _details::unwrap(_device->createGraphicsPipelineUnique(nullptr, info));

		return result;
	}

	compute_pipeline_state device::create_compute_pipeline_state(
		const pipeline_resources &rsrc, const shader_binary &cs
	) {
		compute_pipeline_state result = nullptr;

		vk::ComputePipelineCreateInfo info;
		info.stage
			.setStage(vk::ShaderStageFlagBits::eCompute)
			.setModule(cs._module.get());
		info
			.setLayout(rsrc._layout.get());
		// TODO allocator
		result._pipeline = _details::unwrap(_device->createComputePipelineUnique(nullptr, info));

		return result;
	}

	memory_block device::allocate_memory(std::size_t size, memory_type_index mem_id) {
		memory_block result;

		vk::MemoryAllocateInfo info;
		info
			.setAllocationSize(size)
			.setMemoryTypeIndex(static_cast<std::uint32_t>(mem_id));
		// TODO allocator
		result._memory = _details::unwrap(_device->allocateMemoryUnique(info));

		return result;
	}

	buffer device::create_committed_buffer(
		std::size_t size, memory_type_index mem_id, buffer_usage::mask allowed_usage
	) {
		buffer result = nullptr;
		result._device = _device.get();

		vk::BufferCreateInfo buf_info;
		buf_info
			.setSize(size)
			.setUsage(
				_details::conversions::to_buffer_usage_flags(allowed_usage) |
				vk::BufferUsageFlagBits::eShaderDeviceAddress
			);
		// TODO allocator
		result._buffer = _details::unwrap(_device->createBuffer(buf_info));

		auto req = _device->getBufferMemoryRequirements(result._buffer);

		vk::MemoryDedicatedAllocateInfo dedicated_info;
		dedicated_info
			.setBuffer(result._buffer);

		vk::MemoryAllocateFlagsInfo flags_info;
		flags_info
			.setPNext(&dedicated_info)
			.setFlags(vk::MemoryAllocateFlagBits::eDeviceAddress);

		vk::MemoryAllocateInfo info;
		info
			.setPNext(&flags_info)
			.setAllocationSize(req.size)
			.setMemoryTypeIndex(static_cast<std::uint32_t>(mem_id));
		// TODO allocator
		result._memory = _details::unwrap(_device->allocateMemory(info));
		_details::assert_vk(_device->bindBufferMemory(result._buffer, result._memory, 0));

		return result;
	}

	image2d device::create_committed_image2d(
		std::size_t width, std::size_t height, std::size_t array_slices, std::size_t mip_levels,
		format fmt, image_tiling tiling, image_usage::mask allowed_usage
	) {
		image2d result = nullptr;
		result._device = _device.get();

		vk::ImageCreateInfo img_info;
		img_info
			.setImageType(vk::ImageType::e2D)
			.setFormat(_details::conversions::to_format(fmt))
			.setExtent(vk::Extent3D(static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), 1))
			.setMipLevels(static_cast<std::uint32_t>(mip_levels))
			.setArrayLayers(static_cast<std::uint32_t>(array_slices))
			.setSamples(vk::SampleCountFlagBits::e1)
			.setTiling(_details::conversions::to_image_tiling(tiling))
			.setUsage(_details::conversions::to_image_usage_flags(allowed_usage))
			.setInitialLayout(vk::ImageLayout::eUndefined);
		// TODO allocator
		result._image = _details::unwrap(_device->createImage(img_info));

		auto req = _device->getImageMemoryRequirements(result._image);

		vk::MemoryDedicatedAllocateInfo dedicated_info;
		dedicated_info
			.setImage(result._image);

		vk::MemoryAllocateInfo info;
		info
			.setPNext(&dedicated_info)
			.setAllocationSize(_device->getImageMemoryRequirements(result._image).size)
			.setMemoryTypeIndex(_find_memory_type_index(
				req.memoryTypeBits,
				vk::MemoryPropertyFlagBits::eDeviceLocal, vk::MemoryPropertyFlags(),
				vk::MemoryPropertyFlags(), vk::MemoryPropertyFlags()
			));
		// TODO allocator
		result._memory = _details::unwrap(_device->allocateMemory(info));
		_details::assert_vk(_device->bindImageMemory(result._image, result._memory, 0));

		return result;
	}

	std::tuple<buffer, staging_buffer_pitch, std::size_t> device::create_committed_staging_buffer(
		std::size_t width, std::size_t height, format fmt, memory_type_index mem_id,
		buffer_usage::mask allowed_usage
	) {
		vk::SubresourceLayout layout;
		{
			vk::ImageCreateInfo img_info;
			img_info
				.setImageType(vk::ImageType::e2D)
				.setFormat(_details::conversions::to_format(fmt))
				.setExtent(vk::Extent3D(static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), 1))
				.setMipLevels(1)
				.setArrayLayers(1)
				.setSamples(vk::SampleCountFlagBits::e1)
				.setTiling(vk::ImageTiling::eLinear)
				.setUsage(vk::ImageUsageFlagBits::eSampled);
			auto img = _details::unwrap(_device->createImageUnique(img_info));
			vk::ImageSubresource subresource;
			const auto &format_props = format_properties::get(fmt);
			subresource
				.setAspectMask(
					// we can only have one thing at a time
					// stencil can be emulated using int textures
					format_props.depth_bits > 0 ?
					vk::ImageAspectFlagBits::eDepth :
					vk::ImageAspectFlagBits::eColor
				)
				.setArrayLayer(0)
				.setMipLevel(0);
			layout = _device->getImageSubresourceLayout(img.get(), subresource);
		}

		buffer result_buf = create_committed_buffer(layout.size, mem_id, allowed_usage);
		staging_buffer_pitch result_pitch = uninitialized;
		result_pitch._pixels = static_cast<std::uint32_t>(width);
		result_pitch._bytes = static_cast<std::uint32_t>(width * format_properties::get(fmt).bytes_per_pixel());

		return std::make_tuple(std::move(result_buf), result_pitch, result_pitch._bytes * height);
	}

	void *device::map_buffer(buffer &buf, std::size_t begin, std::size_t length) {
		return _map_memory(buf._memory, begin, length);
	}

	void device::unmap_buffer(buffer &buf, std::size_t begin, std::size_t length) {
		_unmap_memory(buf._memory, begin, length);
	}

	void *device::map_image2d(
		image2d &img, subresource_index i, std::size_t begin, std::size_t length
	) {
		if (length > 0) {
			begin += _device->getImageSubresourceLayout(
				img._image, _details::conversions::to_image_subresource(i)
			).offset;
		}
		return _map_memory(img._memory, begin, length);
	}

	void device::unmap_image2d(
		image2d &img, subresource_index i, std::size_t begin, std::size_t length
	) {
		if (length > 0) {
			begin += _device->getImageSubresourceLayout(
				img._image, _details::conversions::to_image_subresource(i)
			).offset;
		}
		_unmap_memory(img._memory, begin, length);
	}

	image2d_view device::create_image2d_view_from(const image2d &img, format f, mip_levels mip) {
		image2d_view result = nullptr;

		vk::ImageAspectFlags aspects =
			format_properties::get(f).depth_bits > 0 ?
			vk::ImageAspectFlagBits::eDepth :
			vk::ImageAspectFlagBits::eColor;

		vk::ImageViewCreateInfo info;
		info
			.setImage(img._image)
			.setViewType(vk::ImageViewType::e2D)
			.setFormat(_details::conversions::to_format(f))
			.setComponents(vk::ComponentMapping())
			.setSubresourceRange(_details::conversions::to_image_subresource_range(mip, aspects));
		// TODO allocator
		result._view = _details::unwrap(_device->createImageViewUnique(info));

		return result;
	}

	frame_buffer device::create_frame_buffer(
		std::span<const graphics::image2d_view *const> color, const image2d_view *ds, cvec2s size
	) {
		frame_buffer result = nullptr;
		result._color_views.reserve(color.size());
		for (const auto *c : color) {
			result._color_views.emplace_back(c->_view.get());
		}
		if (ds) {
			result._depth_stencil_view = ds->_view.get();
		}
		result._size = size;
		return result;
	}

	fence device::create_fence(synchronization_state state) {
		fence result = nullptr;

		vk::FenceCreateInfo info;
		info
			.setFlags(
				state == synchronization_state::set ?
				vk::FenceCreateFlagBits::eSignaled :
				vk::FenceCreateFlags()
			);
		// TODO allocator
		result._fence = _details::unwrap(_device->createFenceUnique(info));

		return result;
	}

	timeline_semaphore device::create_timeline_semaphore(std::uint64_t value) {
		timeline_semaphore result = nullptr;

		vk::SemaphoreTypeCreateInfo type_info;
		type_info
			.setSemaphoreType(vk::SemaphoreType::eTimeline)
			.setInitialValue(value);
		vk::SemaphoreCreateInfo info;
		info
			.setPNext(&type_info);
		// TODO allocator
		result._semaphore = _details::unwrap(_device->createSemaphoreUnique(info));

		return result;
	}

	void device::reset_fence(fence &f) {
		_details::assert_vk(_device->resetFences(f._fence.get()));
	}

	void device::wait_for_fence(fence &f) {
		_details::assert_vk(_device->waitForFences(f._fence.get(), true, std::numeric_limits<std::uint64_t>::max()));
	}

	void device::signal_timeline_semaphore(timeline_semaphore &sem, std::uint64_t val) {
		vk::SemaphoreSignalInfo info;
		info
			.setSemaphore(sem._semaphore.get())
			.setValue(val);
		_details::assert_vk(_device->signalSemaphore(info));
	}

	std::uint64_t device::query_timeline_semaphore(timeline_semaphore &sem) {
		return _details::unwrap(_device->getSemaphoreCounterValue(sem._semaphore.get()));
	}

	void device::wait_for_timeline_semaphore(timeline_semaphore &sem, std::uint64_t val) {
		vk::SemaphoreWaitInfo info;
		info
			.setSemaphores(sem._semaphore.get())
			.setValues(val);
		_details::assert_vk(_device->waitSemaphores(info, std::numeric_limits<std::uint64_t>::max()));
	}

	void device::set_debug_name(buffer &buf, const char8_t *name) {
		vk::DebugMarkerObjectNameInfoEXT info;
		info
			.setObjectType(vk::DebugReportObjectTypeEXT::eBuffer)
			.setObject(reinterpret_cast<std::uint64_t>(static_cast<VkBuffer>(buf._buffer)))
			.setPObjectName(reinterpret_cast<const char*>(name));
		_details::assert_vk(_device->debugMarkerSetObjectNameEXT(info, *_dispatch_loader));
	}

	void device::set_debug_name(image &img, const char8_t *name) {
		vk::DebugMarkerObjectNameInfoEXT info;
		info
			.setObjectType(vk::DebugReportObjectTypeEXT::eBuffer)
			.setObject(reinterpret_cast<std::uint64_t>(static_cast<VkImage>(
				static_cast<_details::image&>(img)._image
			)))
			.setPObjectName(reinterpret_cast<const char*>(name));
		_details::assert_vk(_device->debugMarkerSetObjectNameEXT(info, *_dispatch_loader));
	}

	bottom_level_acceleration_structure_geometry device::create_bottom_level_acceleration_structure_geometry(
		std::span<const std::pair<vertex_buffer_view, index_buffer_view>> data
	) {
		bottom_level_acceleration_structure_geometry result;
		result._geometries.reserve(data.size());
		result._pimitive_counts.reserve(data.size());
		for (const auto &[vert, index] : data) {
			auto &geom = result._geometries.emplace_back();
			geom
				.setGeometryType(vk::GeometryTypeKHR::eTriangles)
				.setFlags(vk::GeometryFlagsKHR());
			geom.geometry.triangles
				.setVertexFormat(_details::conversions::to_format(vert.vertex_format))
				.setVertexData(_device->getBufferAddress(vert.data->_buffer) + vert.offset)
				.setVertexStride(vert.stride)
				.setMaxVertex(static_cast<std::uint32_t>(vert.count))
				.setTransformData(nullptr);
			if (index.data) {
				geom.geometry.triangles
					.setIndexType(_details::conversions::to_index_type(index.element_format))
					.setIndexData(_device->getBufferAddress(index.data->_buffer) + index.offset);
				result._pimitive_counts.emplace_back(static_cast<std::uint32_t>(index.count / 3));
			} else {
				geom.geometry.triangles
					.setIndexType(vk::IndexType::eNoneKHR)
					.setIndexData(nullptr);
				result._pimitive_counts.emplace_back(static_cast<std::uint32_t>(vert.count / 3));
			}
		}
		result._build_info
			.setType(vk::AccelerationStructureTypeKHR::eBottomLevel)
			.setFlags(vk::BuildAccelerationStructureFlagsKHR())
			.setMode(vk::BuildAccelerationStructureModeKHR::eBuild)
			.setSrcAccelerationStructure(nullptr)
			.setDstAccelerationStructure(nullptr)
			.setGeometries(result._geometries)
			.setScratchData(nullptr);
		return result;
	}

	instance_description device::get_bottom_level_acceleration_structure_description(
		bottom_level_acceleration_structure &as,
		mat44f trans, std::uint32_t id, std::uint8_t mask, std::uint32_t hit_group_offset
	) const {
		instance_description result = uninitialized;
		result._instance
			.setInstanceCustomIndex(id)
			.setMask(mask)
			.setInstanceShaderBindingTableRecordOffset(hit_group_offset)
			.setFlags(vk::GeometryInstanceFlagsKHR())
			.setAccelerationStructureReference(_device->getAccelerationStructureAddressKHR(
				as._acceleration_structure.get(), *_dispatch_loader
			));
		for (std::size_t row = 0; row < 3; ++row) {
			for (std::size_t col = 0; col < 4; ++col) {
				result._instance.transform.matrix[row][col] = trans(row, col);
			}
		}
		return result;
	}

	acceleration_structure_build_sizes device::get_bottom_level_acceleration_structure_build_sizes(
		const bottom_level_acceleration_structure_geometry &geom
	) {
		auto vk_result = _device->getAccelerationStructureBuildSizesKHR(
			vk::AccelerationStructureBuildTypeKHR::eDevice, geom._build_info, geom._pimitive_counts,
			*_dispatch_loader
		);
		acceleration_structure_build_sizes result = uninitialized;
		result.acceleration_structure_size = vk_result.accelerationStructureSize;
		result.build_scratch_size          = vk_result.buildScratchSize;
		result.update_scratch_size         = vk_result.updateScratchSize;
		return result;
	}

	acceleration_structure_build_sizes device::get_top_level_acceleration_structure_build_sizes(
		const buffer &top_level_buf, std::size_t offset, std::size_t count
	) {
		vk::AccelerationStructureGeometryInstancesDataKHR instances;
		instances
			.setArrayOfPointers(false)
			.setData(_device->getBufferAddress(top_level_buf._buffer) + offset);
		vk::AccelerationStructureGeometryKHR geom;
		geom
			.setGeometryType(vk::GeometryTypeKHR::eInstances)
			.setFlags(vk::GeometryFlagsKHR());
		geom.geometry.setInstances(instances);
		vk::AccelerationStructureBuildGeometryInfoKHR build_info;
		build_info
			.setType(vk::AccelerationStructureTypeKHR::eTopLevel)
			.setFlags(vk::BuildAccelerationStructureFlagsKHR())
			.setMode(vk::BuildAccelerationStructureModeKHR::eBuild)
			.setSrcAccelerationStructure(nullptr)
			.setDstAccelerationStructure(nullptr)
			.setGeometries(geom);
		auto instance_count = static_cast<std::uint32_t>(count);
		auto vk_result = _device->getAccelerationStructureBuildSizesKHR(
			vk::AccelerationStructureBuildTypeKHR::eDevice, build_info, instance_count, *_dispatch_loader
		);

		acceleration_structure_build_sizes result = uninitialized;
		result.acceleration_structure_size = vk_result.accelerationStructureSize;
		result.build_scratch_size          = vk_result.buildScratchSize;
		result.update_scratch_size         = vk_result.updateScratchSize;
		return result;
	}

	bottom_level_acceleration_structure device::create_bottom_level_acceleration_structure(
		buffer &buf, std::size_t offset, std::size_t size
	) {
		bottom_level_acceleration_structure result = nullptr;
		vk::AccelerationStructureCreateInfoKHR create_info;
		create_info
			.setCreateFlags(vk::AccelerationStructureCreateFlagsKHR())
			.setBuffer(buf._buffer)
			.setOffset(offset)
			.setSize(size)
			.setType(vk::AccelerationStructureTypeKHR::eBottomLevel);
		// TODO allocator
		result._acceleration_structure = _details::unwrap(_device->createAccelerationStructureKHRUnique(
			create_info, nullptr, *_dispatch_loader
		));
		return result;
	}

	top_level_acceleration_structure device::create_top_level_acceleration_structure(
		buffer &buf, std::size_t offset, std::size_t size
	) {
		top_level_acceleration_structure result = nullptr;
		vk::AccelerationStructureCreateInfoKHR create_info;
		create_info
			.setCreateFlags(vk::AccelerationStructureCreateFlagsKHR())
			.setBuffer(buf._buffer)
			.setOffset(offset)
			.setSize(size)
			.setType(vk::AccelerationStructureTypeKHR::eTopLevel);
		// TODO allocator
		result._acceleration_structure = _details::unwrap(_device->createAccelerationStructureKHRUnique(
			create_info, nullptr, *_dispatch_loader
		));
		return result;
	}

	void device::write_descriptor_set_acceleration_structures(
		descriptor_set &set, const descriptor_set_layout&, std::size_t first_register,
		std::span<graphics::top_level_acceleration_structure *const> acceleration_structures
	) {
		auto bookmark = stack_allocator::for_this_thread().bookmark();
		auto as_ptrs = bookmark.create_reserved_vector_array<vk::AccelerationStructureKHR>(
			acceleration_structures.size()
		);
		for (const auto *as : acceleration_structures) {
			as_ptrs.emplace_back(as->_acceleration_structure.get());
		}

		vk::WriteDescriptorSetAccelerationStructureKHR as_writes;
		as_writes
			.setAccelerationStructures(as_ptrs);

		vk::WriteDescriptorSet info;
		info
			.setPNext(&as_writes)
			.setDstSet(set._set.get())
			.setDstBinding(static_cast<std::uint32_t>(first_register))
			.setDescriptorCount(static_cast<std::uint32_t>(acceleration_structures.size()))
			.setDescriptorType(vk::DescriptorType::eAccelerationStructureKHR);
		_device->updateDescriptorSets(info, {});
	}

	shader_group_handle device::get_shader_group_handle(
		raytracing_pipeline_state &pipeline, std::size_t index
	) {
		shader_group_handle result = uninitialized;
		result._data.resize(_raytracing_properties.shaderGroupHandleSize);
		_details::assert_vk(_device->getRayTracingShaderGroupHandlesKHR(
			pipeline._pipeline.get(), static_cast<std::uint32_t>(index), 1,
			result._data.size(), result._data.data(), *_dispatch_loader
		));
		return result;
	}

	raytracing_pipeline_state device::create_raytracing_pipeline_state(
		std::span<const shader_function> hit_group_shaders, std::span<const hit_shader_group> hit_groups,
		std::span<const shader_function> general_shaders, std::size_t max_recursion_depth, std::size_t, std::size_t,
		pipeline_resources &rsrc
	) {
		auto bookmark = stack_allocator::for_this_thread().bookmark();

		auto stages = bookmark.create_reserved_vector_array<vk::PipelineShaderStageCreateInfo>(
			hit_group_shaders.size() + general_shaders.size()
		);
		auto groups = bookmark.create_reserved_vector_array<vk::RayTracingShaderGroupCreateInfoKHR>(
			hit_groups.size() + general_shaders.size()
		);
		for (const auto &func : hit_group_shaders) {
			stages.emplace_back()
				.setStage(static_cast<vk::ShaderStageFlagBits>(
					static_cast<std::underlying_type_t<vk::ShaderStageFlagBits>>(
						_details::conversions::to_shader_stage_flags(func.stage)
					)
				))
				.setModule(func.code->_module.get())
				.setPName(reinterpret_cast<const char*>(func.entry_point));
		}
		for (const auto &group : hit_groups) {
			groups.emplace_back()
				.setType(vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup)
				.setGeneralShader(VK_SHADER_UNUSED_KHR)
				.setClosestHitShader(
					group.closest_hit_shader_index == hit_shader_group::no_shader ?
					VK_SHADER_UNUSED_KHR : static_cast<std::uint32_t>(group.closest_hit_shader_index)
				)
				.setAnyHitShader(
					group.any_hit_shader_index == hit_shader_group::no_shader ?
					VK_SHADER_UNUSED_KHR : static_cast<std::uint32_t>(group.any_hit_shader_index)
				)
				.setIntersectionShader(VK_SHADER_UNUSED_KHR);
		}
		for (const auto &general : general_shaders) {
			groups.emplace_back()
				.setType(vk::RayTracingShaderGroupTypeKHR::eGeneral)
				.setGeneralShader(static_cast<std::uint32_t>(stages.size()))
				.setClosestHitShader(VK_SHADER_UNUSED_KHR)
				.setAnyHitShader(VK_SHADER_UNUSED_KHR)
				.setIntersectionShader(VK_SHADER_UNUSED_KHR);
			stages.emplace_back()
				.setStage(static_cast<vk::ShaderStageFlagBits>(
					static_cast<std::underlying_type_t<vk::ShaderStageFlagBits>>(
						_details::conversions::to_shader_stage_flags(general.stage)
					)
				))
				.setModule(general.code->_module.get())
				.setPName(reinterpret_cast<const char*>(general.entry_point));
		}

		vk::RayTracingPipelineCreateInfoKHR info;
		info
			.setFlags(vk::PipelineCreateFlags())
			.setStages(stages)
			.setGroups(groups)
			.setMaxPipelineRayRecursionDepth(static_cast<std::uint32_t>(max_recursion_depth))
			.setLayout(rsrc._layout.get());
		raytracing_pipeline_state result = nullptr;
		result._pipeline = _details::unwrap(_device->createRayTracingPipelineKHRUnique(
			nullptr, nullptr, info, nullptr, *_dispatch_loader
		));
		return result;
	}

	std::uint32_t device::_find_memory_type_index(
		std::uint32_t requirements,
		vk::MemoryPropertyFlags required_on, vk::MemoryPropertyFlags required_off,
		vk::MemoryPropertyFlags optional_on, vk::MemoryPropertyFlags optional_off
	) const {
		std::uint32_t best = 32;
		int best_count = std::numeric_limits<int>::min();
		while (requirements != 0) {
			auto index = static_cast<std::uint32_t>(std::countr_zero(requirements));
			auto flags = _memory_properties.memoryTypes[index].propertyFlags;
			if ((flags & required_on) == required_on && (flags & required_off) == vk::MemoryPropertyFlags()) {
				int count = 0;
				count += std::popcount(static_cast<vk::MemoryPropertyFlags::MaskType>(flags & optional_on));
				count -= std::popcount(static_cast<vk::MemoryPropertyFlags::MaskType>(flags & optional_off));
				if (count > best_count) {
					best_count = count;
					best = index;
				}
			}
			requirements &= ~(1u << index);
		}
		assert(best < 32);
		return best;
	}

	void *device::_map_memory(vk::DeviceMemory mem, std::size_t beg, std::size_t len) {
		// TODO reference counting
		void *result = _details::unwrap(_device->mapMemory(mem, 0, VK_WHOLE_SIZE));
		if (len > 0) {
			vk::DeviceSize align = _device_limits.nonCoherentAtomSize;
			std::size_t end = beg + len;
			beg = align * (beg / align);
			end = align * ((end + align - 1) / align);
			len = end - beg;
			vk::MappedMemoryRange range;
			range
				.setMemory(mem)
				.setOffset(beg)
				.setSize(len);
			_details::assert_vk(_device->invalidateMappedMemoryRanges(range));
		}
		return result;
	}

	void device::_unmap_memory(vk::DeviceMemory mem, std::size_t beg, std::size_t len) {
		// TODO reference counting
		if (len > 0) {
			vk::DeviceSize align = _device_limits.nonCoherentAtomSize;
			std::size_t end = beg + len;
			beg = align * (beg / align);
			end = align * ((end + align - 1) / align);
			len = end - beg;
			vk::MappedMemoryRange range;
			range
				.setMemory(mem)
				.setOffset(beg)
				.setSize(len);
			_details::assert_vk(_device->flushMappedMemoryRanges(range));
		}
		_device->unmapMemory(mem);
	}


	device adapter::create_device() {
		device result = nullptr;

		result._physical_device = _device;
		result._dispatch_loader = _dispatch_loader;

		auto bookmark = stack_allocator::for_this_thread().bookmark();
		auto queue_familly_allocator = bookmark.create_std_allocator<vk::QueueFamilyProperties>();
		auto families = _device.getQueueFamilyProperties(queue_familly_allocator);
		result._graphics_compute_queue_family_index = result._compute_queue_family_index =
			static_cast<std::uint32_t>(families.size());
		constexpr vk::QueueFlags graphics_compute_flags = vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute;
		for (std::uint32_t i = 0; i < families.size(); ++i) {
			vk::QueueFlags flags = families[i].queueFlags;
			if ((flags & graphics_compute_flags) == graphics_compute_flags) {
				result._graphics_compute_queue_family_index = i;
			} else if (flags & vk::QueueFlagBits::eCompute) {
				result._compute_queue_family_index = i;
			}
		}
		assert(result._graphics_compute_queue_family_index < families.size());
		if (result._compute_queue_family_index >= families.size()) {
			result._compute_queue_family_index = result._graphics_compute_queue_family_index;
		}

		result._memory_properties = _device.getMemoryProperties();
		result._device_limits = _device.getProperties().limits;

		float priority = 0.5f;
		vk::DeviceQueueCreateInfo queue_info;
		queue_info
			.setQueueFamilyIndex(result._graphics_compute_queue_family_index)
			.setQueueCount(1)
			.setQueuePriorities(priority);

		std::array extensions{
			VK_KHR_SWAPCHAIN_EXTENSION_NAME,
			VK_EXT_CUSTOM_BORDER_COLOR_EXTENSION_NAME,
			VK_EXT_DEBUG_MARKER_EXTENSION_NAME,
			/*VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
			VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
			VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,*/
			/*VK_GOOGLE_HLSL_FUNCTIONALITY_1_EXTENSION_NAME,
			VK_GOOGLE_USER_TYPE_EXTENSION_NAME,*/
		};

		auto extension_allocator = bookmark.create_std_allocator<vk::ExtensionProperties>();
		auto available_extensions = _details::unwrap(_device.enumerateDeviceExtensionProperties(
			nullptr, extension_allocator, *_dispatch_loader
		));
		for (auto *ext_required : extensions) {
			bool found = false;
			for (const auto &ext : available_extensions) {
				if (std::strncmp(ext_required, ext.extensionName.data(), ext.extensionName.size()) == 0) {
					found = true;
					break;
				}
			}
			if (!found) {
				log().error<u8"Extension {} not found. Device creation may fail.">(ext_required);
			}
		}

		vk::PhysicalDeviceRobustness2FeaturesEXT robustness_features;
		robustness_features
			.setNullDescriptor(true);

		vk::PhysicalDeviceCustomBorderColorFeaturesEXT border_color_features;
		border_color_features
			.setPNext(&robustness_features)
			.setCustomBorderColors(true)
			.setCustomBorderColorWithoutFormat(true);

		vk::PhysicalDeviceRayTracingPipelineFeaturesKHR raytracing_features;
		raytracing_features
			.setPNext(&border_color_features)
			.setRayTracingPipeline(true);

		vk::PhysicalDeviceAccelerationStructureFeaturesKHR acceleration_structure_features;
		acceleration_structure_features
			.setPNext(&raytracing_features)
			.setAccelerationStructure(true);

		vk::PhysicalDeviceVulkan13Features vk13_features;
		vk13_features
			.setPNext(&acceleration_structure_features)
			.setDynamicRendering(true);

		vk::PhysicalDeviceVulkan12Features vk12_features;
		vk12_features
			.setPNext(&vk13_features)
			.setBufferDeviceAddress(true)
			.setDescriptorBindingPartiallyBound(true)
			.setDescriptorBindingUpdateUnusedWhilePending(true)
			.setDescriptorBindingSampledImageUpdateAfterBind(true)
			.setDescriptorBindingStorageBufferUpdateAfterBind(true)
			.setDescriptorBindingStorageImageUpdateAfterBind(true)
			.setDescriptorBindingStorageTexelBufferUpdateAfterBind(true)
			.setDescriptorBindingUniformBufferUpdateAfterBind(true)
			.setDescriptorBindingUniformTexelBufferUpdateAfterBind(true)
			.setDescriptorBindingVariableDescriptorCount(true)
			.setRuntimeDescriptorArray(true)
			.setTimelineSemaphore(true);

		vk::PhysicalDeviceFeatures2 features;
		features.setPNext(&vk12_features);
		features.features
			.setSamplerAnisotropy(true)
			.setShaderInt64(true);

		vk::DeviceCreateInfo info;
		info
			.setPNext(&features)
			.setQueueCreateInfos(queue_info)
			.setPEnabledExtensionNames(extensions);

		// TODO allocator
		result._device = _details::unwrap(_device.createDeviceUnique(info));

		auto all_props = _device.getProperties2<
			vk::PhysicalDeviceProperties2,
			vk::PhysicalDeviceRayTracingPipelinePropertiesKHR
		>();
		result._raytracing_properties = all_props.get<vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();

		// collect memory types
		// TODO allocator
		result._memory_properties_list.reserve(result._memory_properties.memoryTypeCount);
		for (std::uint32_t i = 0; i < result._memory_properties.memoryTypeCount; ++i) {
			const auto &type = result._memory_properties.memoryTypes[i];
			result._memory_properties_list.emplace_back(
				static_cast<memory_type_index>(i),
				_details::conversions::back_to_memory_properties(type.propertyFlags)
			);
		}

		return result;
	}

	adapter_properties adapter::get_properties() const {
		adapter_properties result = uninitialized;
		const auto objs = _device.getProperties2<
			vk::PhysicalDeviceProperties2,
			vk::PhysicalDeviceRayTracingPipelinePropertiesKHR,
			vk::PhysicalDeviceAccelerationStructurePropertiesKHR
		>();
		const auto &&[props, ray_tracing_props, acceleration_structure_props] = objs.get<
			vk::PhysicalDeviceProperties2,
			vk::PhysicalDeviceRayTracingPipelinePropertiesKHR,
			vk::PhysicalDeviceAccelerationStructurePropertiesKHR
		>();

		result.is_software = props.properties.deviceType == vk::PhysicalDeviceType::eCpu;
		result.is_discrete = props.properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu;
		result.name        = reinterpret_cast<const char8_t*>(props.properties.deviceName.data());

		result.constant_buffer_alignment           = props.properties.limits.minUniformBufferOffsetAlignment;
		result.acceleration_structure_alignment    = props.properties.limits.minStorageBufferOffsetAlignment; // TODO what should this be?
		result.shader_group_handle_size            = ray_tracing_props.shaderGroupHandleSize;
		result.shader_group_handle_alignment       = ray_tracing_props.shaderGroupHandleAlignment;
		result.shader_group_handle_table_alignment = ray_tracing_props.shaderGroupBaseAlignment;

		return result;
	}
}
