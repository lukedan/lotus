#include "lotus/graphics/backends/vulkan/device.h"

/// \file
/// Implementation of vulkan devices.

#include <spirv_reflect.h>

#include "lotus/graphics/descriptors.h"
#include "lotus/graphics/frame_buffer.h"
#include "lotus/graphics/pass.h"
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

		swapchain._frame_index = static_cast<std::uint16_t>(_details::unwrap(_device->acquireNextImageKHR(
			swapchain._swapchain.get(),
			std::numeric_limits<std::uint64_t>::max(),
			nullptr,
			sync.notify_fence ? static_cast<fence*>(sync.notify_fence)->_fence.get() : nullptr
		)));

		back_buffer_info result = uninitialized;
		result.index        = swapchain._frame_index;
		result.on_presented = sync.notify_fence;
		return result;
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
		auto buffers = _details::unwrap(_device->allocateCommandBuffersUnique(info));
		assert(buffers.size() == 1);
		result._buffer = std::move(buffers[0]);

		vk::CommandBufferBeginInfo begin_info;
		_details::assert_vk(result._buffer->begin(begin_info));

		return result;
	}

	descriptor_pool device::create_descriptor_pool(
		std::span<const descriptor_range> capacity, std::size_t max_num_sets
	) {
		descriptor_pool result;

		auto bookmark = stack_allocator::scoped_bookmark::create();
		auto ranges =
			stack_allocator::for_this_thread().create_reserved_vector_array<vk::DescriptorPoolSize>(capacity.size());
		for (const auto &range : capacity) {
			ranges.emplace_back()
				.setType(_details::conversions::to_descriptor_type(range.type))
				.setDescriptorCount(static_cast<std::uint32_t>(range.count));
		}

		vk::DescriptorPoolCreateInfo info;
		info
			.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
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
			.setDescriptorSetCount(1)
			.setSetLayouts(layout._layout.get());
		auto sets = _details::unwrap(_device->allocateDescriptorSetsUnique(info));
		assert(sets.size() == 1);
		result._set = std::move(sets[0]);

		return result;
	}

	void device::write_descriptor_set_images(
		descriptor_set &set, const descriptor_set_layout&,
		std::size_t first_register, std::span<const graphics::image_view *const> images
	) {
		auto bookmark = stack_allocator::scoped_bookmark::create();
		auto imgs =
			stack_allocator::for_this_thread().create_reserved_vector_array<vk::DescriptorImageInfo>(images.size());
		for (const auto *img : images) {
			imgs.emplace_back()
				.setImageView(static_cast<const _details::image_view*>(img)->_view.get())
				.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal); // TODO RW?
		}

		vk::WriteDescriptorSet info;
		info
			.setDstSet(set._set.get())
			.setDstBinding(static_cast<std::uint32_t>(first_register))
			.setDescriptorType(vk::DescriptorType::eSampledImage)
			.setImageInfo(imgs);
		_device->updateDescriptorSets(info, {});
	}

	void device::write_descriptor_set_buffers(
		descriptor_set &set, const descriptor_set_layout&,
		std::size_t first_register, std::span<const buffer_view> buffers
	) {
		auto bookmark = stack_allocator::scoped_bookmark::create();
		auto bufs = stack_allocator::for_this_thread().create_reserved_vector_array<vk::DescriptorBufferInfo>(
			buffers.size()
		);
		for (const auto &buf : buffers) {
			bufs.emplace_back()
				.setBuffer(static_cast<buffer*>(buf.data)->_buffer)
				.setOffset(buf.first * buf.stride)
				.setRange(buf.size * buf.stride);
		}

		vk::WriteDescriptorSet info;
		info
			.setDstSet(set._set.get())
			.setDstBinding(static_cast<std::uint32_t>(first_register))
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setBufferInfo(bufs);
		_device->updateDescriptorSets(info, {});
	}

	void device::write_descriptor_set_constant_buffers(
		descriptor_set &set, const descriptor_set_layout&,
		std::size_t first_register, std::span<const constant_buffer_view> buffers
	) {
		auto bookmark = stack_allocator::scoped_bookmark::create();
		auto bufs = stack_allocator::for_this_thread().create_reserved_vector_array<vk::DescriptorBufferInfo>(
			buffers.size()
		);
		for (const auto &buf : buffers) {
			bufs.emplace_back()
				.setBuffer(static_cast<buffer*>(buf.data)->_buffer)
				.setOffset(buf.offset)
				.setRange(buf.size);
		}

		vk::WriteDescriptorSet info;
		info
			.setDstSet(set._set.get())
			.setDstBinding(static_cast<std::uint32_t>(first_register))
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setBufferInfo(bufs);
		_device->updateDescriptorSets(info, {});
	}

	void device::write_descriptor_set_samplers(
		descriptor_set &set, const descriptor_set_layout&,
		std::size_t first_register, std::span<const graphics::sampler *const> samplers
	) {
		auto bookmark = stack_allocator::scoped_bookmark::create();
		auto smps = stack_allocator::for_this_thread().create_reserved_vector_array<vk::DescriptorImageInfo>(
			samplers.size()
		);
		for (const auto *smp : samplers) {
			smps.emplace_back()
				.setSampler(static_cast<const sampler*>(smp)->_sampler.get());
		}

		vk::WriteDescriptorSet info;
		info
			.setDstSet(set._set.get())
			.setDstBinding(static_cast<std::uint32_t>(first_register))
			.setDescriptorType(vk::DescriptorType::eSampler)
			.setImageInfo(smps);
		_device->updateDescriptorSets(info, {});
	}

	shader device::load_shader(std::span<const std::byte> data) {
		shader result = nullptr;

		vk::ShaderModuleCreateInfo info;
		constexpr std::size_t word_size = sizeof(std::uint32_t);
		auto word_count = static_cast<std::uint32_t>((data.size() + word_size - 1) / word_size);
		info
			.setCode({ word_count, reinterpret_cast<const std::uint32_t*>(data.data()) });
		// TODO allocator
		result._module = _details::unwrap(_device->createShaderModuleUnique(info));

		result._reflection = load_shader_reflection(data);

		return result;
	}

	shader_reflection device::load_shader_reflection(std::span<const std::byte> code) {
		shader_reflection result = nullptr;
		_details::assert_spv_reflect(spvReflectCreateShaderModule(
			code.size(), code.data(), &result._reflection.emplace()
		));
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

		auto bookmark = stack_allocator::scoped_bookmark::create();
		auto arr = stack_allocator::for_this_thread().create_vector_array<vk::DescriptorSetLayoutBinding>();
		auto stages = _details::conversions::to_shader_stage_flags(visible_stages);
		for (const auto &rng : ranges) {
			vk::DescriptorSetLayoutBinding binding;
			binding
				.setDescriptorType(_details::conversions::to_descriptor_type(rng.range.type))
				.setDescriptorCount(1)
				.setStageFlags(stages);
			for (std::size_t i = 0; i < rng.range.count; ++i) {
				binding.setBinding(static_cast<std::uint32_t>(rng.register_index + i));
				arr.emplace_back(binding);
			}
		}

		vk::DescriptorSetLayoutCreateInfo info;
		info
			.setBindings(arr);
		// TODO allocator
		result._layout = _details::unwrap(_device->createDescriptorSetLayoutUnique(info));

		return result;
	}

	pipeline_resources device::create_pipeline_resources(
		std::span<const graphics::descriptor_set_layout *const> layouts
	) {
		pipeline_resources result;

		auto bookmark = stack_allocator::scoped_bookmark::create();
		auto arr = stack_allocator::for_this_thread().create_reserved_vector_array<vk::DescriptorSetLayout>(
			layouts.size()
		);
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
		const shader *vs,
		const shader *ps,
		const shader *ds,
		const shader *hs,
		const shader *gs,
		std::span<const render_target_blend_options> blend,
		const rasterizer_options &rasterizer,
		const depth_stencil_options &depth_stencil,
		std::span<const input_buffer_layout> input_buffers,
		primitive_topology topology,
		const pass_resources &environment,
		std::size_t num_viewports
	) {
		graphics_pipeline_state result = nullptr;

		auto bookmark = stack_allocator::scoped_bookmark::create();

		std::size_t count = 0;
		std::array<vk::PipelineShaderStageCreateInfo, 5> stages;
		auto add_shader = [&](const shader *s, vk::ShaderStageFlagBits stage) {
			if (s) {
				const char *entry_point = nullptr;
				// TODO this is bad
				const auto &refl = s->_reflection._reflection.value();
				for (std::uint32_t i = 0; i < refl.entry_point_count; ++i) {
					if (
						static_cast<std::uint32_t>(refl.entry_points[i].shader_stage) ==
						static_cast<std::uint32_t>(stage)
					) {
						entry_point = refl.entry_points[i].name;
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
			stack_allocator::for_this_thread().create_reserved_vector_array<vk::VertexInputBindingDescription>(
				input_buffers.size()
			);
		auto attribute_descriptions =
			stack_allocator::for_this_thread().create_vector_array<vk::VertexInputAttributeDescription>();
		{
			const auto &refl = vs->_reflection._reflection.value();
			for (const auto &buf : input_buffers) {
				input_bindings.emplace_back()
					.setBinding(static_cast<std::uint32_t>(buf.buffer_index))
					.setStride(static_cast<std::uint32_t>(buf.stride))
					.setInputRate(_details::conversions::to_vertex_input_rate(buf.input_rate));
				for (const auto &attr : buf.elements) {
					auto location = std::numeric_limits<std::uint32_t>::max();
					std::string_view expected_semantic(reinterpret_cast<const char*>(attr.semantic_name));
					for (std::uint32_t i = 0; i < refl.input_variable_count; ++i) {
						const auto &input = refl.input_variables[i];
						std::string_view semantic(input->semantic);
						if (semantic.starts_with(reinterpret_cast<const char*>(attr.semantic_name))) {
							std::uint32_t index = 0;
							if (semantic.size() > expected_semantic.size()) {
								auto res = std::from_chars(
									semantic.data() + expected_semantic.size(),
									semantic.data() + semantic.size(),
									index
								);
								assert(res.ec == std::errc() && res.ptr == semantic.data() + semantic.size());
							}
							if (index == attr.semantic_index) {
								location = input->location;
								break;
							}
						}
					}
					assert(location < std::numeric_limits<std::uint32_t>::max());
					attribute_descriptions.emplace_back()
						.setLocation(location)
						.setBinding(static_cast<std::uint32_t>(buf.buffer_index))
						.setFormat(_details::conversions::for_format(attr.element_format))
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
			stack_allocator::for_this_thread().create_reserved_vector_array<vk::PipelineColorBlendAttachmentState>(
				blend.size()
			);
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

		vk::GraphicsPipelineCreateInfo info;
		info
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
			.setRenderPass(environment._pass.get())
			.setSubpass(0);
		// TODO allocator
		result._pipeline = _details::unwrap(_device->createGraphicsPipelineUnique(nullptr, info));

		return result;
	}

	compute_pipeline_state device::create_compute_pipeline_state(
		const pipeline_resources &rsrc, const shader &cs
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

	pass_resources device::create_pass_resources(
		std::span<const render_target_pass_options> color,
		depth_stencil_pass_options ds
	) {
		pass_resources result;

		auto bookmark = stack_allocator::scoped_bookmark::create();

		bool has_depth_stencil = ds.pixel_format != format::none;
		bool has_stencil = format_properties::get(ds.pixel_format).stencil_bits > 0;
		vk::ImageLayout ds_layout =
			has_stencil ?
			vk::ImageLayout::eDepthStencilAttachmentOptimal :
			vk::ImageLayout::eDepthAttachmentOptimal;
		auto attachments =
			stack_allocator::for_this_thread().create_reserved_vector_array<vk::AttachmentDescription>(
				color.size() + has_depth_stencil ? 1 : 0
			);
		for (const auto &opt : color) {
			attachments.emplace_back()
				.setFormat(_details::conversions::for_format(opt.pixel_format))
				.setLoadOp(_details::conversions::to_attachment_load_op(opt.load_operation))
				.setStoreOp(_details::conversions::to_attachment_store_op(opt.store_operation))
				.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
				.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal);
		}
		if (has_depth_stencil) {
			attachments.emplace_back()
				.setFormat(_details::conversions::for_format(ds.pixel_format))
				.setLoadOp(_details::conversions::to_attachment_load_op(ds.depth_load_operation))
				.setStoreOp(_details::conversions::to_attachment_store_op(ds.depth_store_operation))
				.setStencilLoadOp(_details::conversions::to_attachment_load_op(ds.stencil_load_operation))
				.setStencilStoreOp(_details::conversions::to_attachment_store_op(ds.stencil_store_operation))
				.setInitialLayout(ds_layout)
				.setFinalLayout(ds_layout);
		}

		auto attachment_ref =
			stack_allocator::for_this_thread().create_reserved_vector_array<vk::AttachmentReference>(color.size());
		for (std::size_t i = 0; i < color.size(); ++i) {
			attachment_ref.emplace_back()
				.setAttachment(static_cast<std::uint32_t>(i))
				.setLayout(vk::ImageLayout::eColorAttachmentOptimal);
		}
		vk::AttachmentReference ds_ref;
		ds_ref
			.setAttachment(static_cast<std::uint32_t>(color.size()))
			.setLayout(ds_layout);

		vk::SubpassDescription subpass;
		subpass
			.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
			.setColorAttachments(attachment_ref)
			.setPDepthStencilAttachment(has_depth_stencil ? &ds_ref : nullptr);

		vk::RenderPassCreateInfo info;
		info
			.setAttachments(attachments)
			.setSubpasses(subpass);
		// TODO allocator
		result._pass = _details::unwrap(_device->createRenderPassUnique(info));

		return result;
	}

	device_heap device::create_device_heap(std::size_t size, heap_type type) {
		device_heap result;

		vk::MemoryAllocateInfo info;
		info
			.setAllocationSize(size)
			.setMemoryTypeIndex(0);
		// TODO allocator
		result._memory = _details::unwrap(_device->allocateMemoryUnique(info));

		return result;
	}

	buffer device::create_committed_buffer(
		std::size_t size, heap_type type, buffer_usage::mask allowed_usage
	) {
		buffer result = nullptr;
		result._device = _device.get();

		vk::BufferCreateInfo buf_info;
		buf_info
			.setSize(size)
			.setUsage(_details::conversions::to_buffer_usage_flags(allowed_usage));
		// TODO allocator
		result._buffer = _details::unwrap(_device->createBuffer(buf_info));

		auto req = _device->getBufferMemoryRequirements(result._buffer);

		vk::MemoryDedicatedAllocateInfo dedicated_info;
		dedicated_info
			.setBuffer(result._buffer);

		vk::MemoryAllocateInfo info;
		info
			.setPNext(&dedicated_info)
			.setAllocationSize(req.size)
			.setMemoryTypeIndex(_find_memory_type_index(req.memoryTypeBits, type));
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
			.setFormat(_details::conversions::for_format(fmt))
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
			.setMemoryTypeIndex(_find_memory_type_index(req.memoryTypeBits, heap_type::device_only));
		// TODO allocator
		result._memory = _details::unwrap(_device->allocateMemory(info));
		_details::assert_vk(_device->bindImageMemory(result._image, result._memory, 0));

		return result;
	}

	std::tuple<buffer, staging_buffer_pitch, std::size_t> device::create_committed_buffer_as_image2d(
		std::size_t width, std::size_t height, format fmt, heap_type committed_heap_type,
		buffer_usage::mask allowed_usage
	) {
		vk::SubresourceLayout layout;
		{
			vk::ImageCreateInfo img_info;
			img_info
				.setImageType(vk::ImageType::e2D)
				.setFormat(_details::conversions::for_format(fmt))
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

		buffer result_buf = create_committed_buffer(layout.size, committed_heap_type, allowed_usage);
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
			.setFormat(_details::conversions::for_format(f))
			.setComponents(vk::ComponentMapping())
			.setSubresourceRange(_details::conversions::to_image_subresource_range(mip, aspects));
		// TODO allocator
		result._view = _details::unwrap(_device->createImageViewUnique(info));

		return result;
	}

	frame_buffer device::create_frame_buffer(
		std::span<const graphics::image2d_view *const> color, const image2d_view *ds,
		const cvec2s &size, const pass_resources &pass
	) {
		frame_buffer result = nullptr;

		auto bookmark = stack_allocator::scoped_bookmark::create();
		auto attachments = stack_allocator::for_this_thread().create_reserved_vector_array<vk::ImageView>(
			color.size() + ds ? 1 : 0
		);
		for (const auto *view : color) {
			attachments.emplace_back(view->_view.get());
		}
		if (ds) {
			attachments.emplace_back(ds->_view.get());
		}

		vk::FramebufferCreateInfo info;
		info
			.setRenderPass(pass._pass.get())
			.setAttachments(attachments)
			.setWidth(static_cast<std::uint32_t>(size[0]))
			.setHeight(static_cast<std::uint32_t>(size[1]))
			.setLayers(1);
		// TODO allocator
		result._framebuffer = _details::unwrap(_device->createFramebufferUnique(info));
		result._size = vk::Extent2D(static_cast<std::uint32_t>(size[0]), static_cast<std::uint32_t>(size[1]));

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

	void device::reset_fence(fence &f) {
		_details::assert_vk(_device->resetFences(f._fence.get()));
	}

	void device::wait_for_fence(fence &f) {
		_details::assert_vk(_device->waitForFences(f._fence.get(), true, std::numeric_limits<std::uint64_t>::max()));
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

	std::uint32_t device::_find_memory_type_index(std::uint32_t requirements, heap_type ty) const {
		vk::MemoryPropertyFlags req_on, req_off, opt_on, opt_off;
		switch (ty) {
		case heap_type::device_only:
			req_on = vk::MemoryPropertyFlagBits::eDeviceLocal;
			opt_off = vk::MemoryPropertyFlagBits::eHostVisible;
			break;
		case heap_type::upload:
			req_on = vk::MemoryPropertyFlagBits::eHostVisible;
			break;
		case heap_type::readback:
			req_on = vk::MemoryPropertyFlagBits::eHostVisible;
			opt_on = vk::MemoryPropertyFlagBits::eHostCached;
			break;
		}
		return _find_memory_type_index(requirements, req_on, req_off, opt_on, opt_off);
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

		auto bookmark = stack_allocator::scoped_bookmark::create();
		auto allocator = stack_allocator::for_this_thread().create_std_allocator<vk::QueueFamilyProperties>();
		auto families = _device.getQueueFamilyProperties(allocator);
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
			/*VK_GOOGLE_HLSL_FUNCTIONALITY1_EXTENSION_NAME,
			VK_GOOGLE_USER_TYPE_EXTENSION_NAME,*/
		};

		vk::PhysicalDeviceCustomBorderColorFeaturesEXT border_color_features;
		border_color_features
			.setCustomBorderColors(true)
			.setCustomBorderColorWithoutFormat(true);

		vk::PhysicalDeviceFeatures2 features;
		features.setPNext(&border_color_features);
		features.features
			.setSamplerAnisotropy(true);

		vk::DeviceCreateInfo info;
		info
			.setPNext(&features)
			.setQueueCreateInfos(queue_info)
			.setPEnabledExtensionNames(extensions);

		// TODO allocator
		result._device = _details::unwrap(_device.createDeviceUnique(info));

		return result;
	}

	adapter_properties adapter::get_properties() const {
		adapter_properties result = uninitialized;
		auto props = _device.getProperties();
		result.constant_buffer_alignment = props.limits.minUniformBufferOffsetAlignment;
		result.is_software               = props.deviceType == vk::PhysicalDeviceType::eCpu;
		result.is_discrete               = props.deviceType == vk::PhysicalDeviceType::eDiscreteGpu;
		result.name                      = reinterpret_cast<const char8_t*>(props.deviceName.data());
		return result;
	}
}
