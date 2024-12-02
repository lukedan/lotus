#include "lotus/renderer/context/execution/descriptors.h"

/// \file
/// Implementation of descriptor utilities.

#include "lotus/renderer/context/context.h"

namespace lotus::renderer::execution {
	descriptor_set_builder::descriptor_set_builder(
		renderer::context &ctx,
		const gpu::descriptor_set_layout &layout,
		gpu::descriptor_pool &pool
	) : _ctx(ctx), _layout(layout), _result(_ctx._device.create_descriptor_set(pool, layout)) {
	}

	void descriptor_set_builder::create_binding(std::uint32_t reg, const descriptor_resource::image2d &img) {
		const auto func = gpu::device::get_write_image_descriptor_function(to_descriptor_type(img.binding_type));
		std::initializer_list<const gpu::image_view_base*> img_views = { &_ctx._request_image_view(img.view) };
		(_ctx._device.*func)(_result, _layout, reg, img_views);
	}

	void descriptor_set_builder::create_binding(std::uint32_t reg, const descriptor_resource::image3d &img) {
		const auto func = gpu::device::get_write_image_descriptor_function(to_descriptor_type(img.binding_type));
		std::initializer_list<const gpu::image_view_base*> img_view = { &_ctx._request_image_view(img.view) };
		(_ctx._device.*func)(_result, _layout, reg, img_view);
	}

	void descriptor_set_builder::create_binding(std::uint32_t reg, const descriptor_resource::swap_chain &chain) {
		const auto func = gpu::device::get_write_image_descriptor_function(to_descriptor_type(chain.binding_type));
		std::initializer_list<const gpu::image_view_base*> img_view =
			{ &_ctx._request_swap_chain_view(chain.chain) };
		(_ctx._device.*func)(_result, _layout, reg, img_view);
	}

	void descriptor_set_builder::create_binding(
		std::uint32_t reg, const descriptor_resource::constant_buffer &buf
	) {
		_ctx._device.write_descriptor_set_constant_buffers(_result, _layout, reg, {
			gpu::constant_buffer_view(buf.data._ptr->data, buf.offset, buf.size)
		});
	}

	void descriptor_set_builder::create_binding(
		std::uint32_t reg, const descriptor_resource::structured_buffer &buf
	) {
		const gpu::structured_buffer_view buf_view(
			buf.data._ptr->data, buf.data._first, buf.data._count, buf.data._stride
		);
		switch (buf.binding_type) {
		case buffer_binding_type::read_only:
			_ctx._device.write_descriptor_set_read_only_structured_buffers(_result, _layout, reg, { buf_view });
			break;
		case buffer_binding_type::read_write:
			_ctx._device.write_descriptor_set_read_write_structured_buffers(_result, _layout, reg, { buf_view });
			break;
		case buffer_binding_type::num_enumerators:
			std::unreachable();
		}
	}

	void descriptor_set_builder::create_binding(
		std::uint32_t reg, const recorded_resources::tlas &as
	) {
		_ctx._device.write_descriptor_set_acceleration_structures(_result, _layout, reg, { &as._ptr->handle });
	}

	void descriptor_set_builder::create_binding(
		std::uint32_t reg, const sampler_state &samp
	) {
		_ctx._device.write_descriptor_set_samplers(_result, _layout, reg, { &_ctx._cache.get_sampler(samp) });
	}

	void descriptor_set_builder::create_bindings(std::span<const all_resource_bindings::numbered_binding> bindings) {
		for (auto &binding : bindings) {
			std::visit(
				[&](auto &&resource) {
					create_binding(binding.register_index, resource);
				},
				binding.value
			);
		}
	}

	execution::cache_keys::descriptor_set_layout descriptor_set_builder::get_descriptor_set_layout_key(
		std::span<const all_resource_bindings::numbered_binding> bindings
	) {
		cache_keys::descriptor_set_layout key = nullptr;
		for (const auto &b : bindings) {
			auto type = std::visit(
				[](const auto &rsrc) {
					return get_descriptor_type(rsrc);
				},
				b.value
			);
			key.ranges.emplace_back(gpu::descriptor_range_binding::create(type, 1, b.register_index));
		}
		key.consolidate();
		return key;
	}
}
