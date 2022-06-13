#include "lotus/renderer/context.h"

/// \file
/// Implementation of scene loading and rendering.

#include "lotus/logging.h"
#include "lotus/utils/stack_allocator.h"

namespace lotus::renderer {
	namespace _details {
		graphics::descriptor_type to_descriptor_type(image_binding_type type) {
			constexpr static enum_mapping<image_binding_type, graphics::descriptor_type> table{
				std::pair(image_binding_type::read_only,  graphics::descriptor_type::read_only_image ),
				std::pair(image_binding_type::read_write, graphics::descriptor_type::read_write_image),
			};
			return table[type];
		}
	}

	void context::pass::draw_instanced(
		std::vector<input_buffer_binding> inputs,
		std::uint32_t num_verts,
		index_buffer_binding indices,
		std::uint32_t num_indices,
		graphics::primitive_topology topology,
		all_resource_bindings resources,
		assets::owning_handle<assets::shader> vs,
		assets::owning_handle<assets::shader> ps,
		graphics_pipeline_state state,
		std::uint32_t num_insts
	) {
		_command.commands.emplace_back(
			std::in_place_type<pass_commands::draw_instanced>,
			num_insts,
			std::move(inputs), num_verts,
			std::move(indices), num_indices,
			topology,
			std::move(resources),
			std::move(vs), std::move(ps),
			std::move(state)
		);
	}

	void context::pass::draw_instanced(
		assets::owning_handle<assets::geometry> geom,
		assets::owning_handle<assets::material> mat,
		std::span<const input_buffer_binding> additional_inputs,
		graphics_pipeline_state state,
		std::uint32_t num_insts
	) {
		assert(false); // TODO not implemented
	}

	void context::pass::end() {
		_context->_commands.emplace_back<context_commands::render_pass>(std::move(_command));
		_context = nullptr;
	}


	context context::create(assets::manager &asset_man, graphics::context &ctx, graphics::command_queue &queue) {
		return context(asset_man, ctx, queue);
	}

	image2d_view context::request_surface2d(std::u8string_view name, cvec2s size, graphics::format fmt) {
		graphics::image_tiling tiling = graphics::image_tiling::optimal;
		graphics::image_usage::mask usage_mask = 
			graphics::image_usage::mask::read_only_texture |
			graphics::image_usage::mask::read_write_color_texture |
			(
				graphics::format_properties::get(fmt).has_depth_stencil() ?
				graphics::image_usage::mask::depth_stencil_render_target :
				graphics::image_usage::mask::color_render_target
			);
		auto *surf = new _details::surface2d(size, 1, fmt, tiling, usage_mask, std::u8string(name));
		auto surf_ptr = std::shared_ptr<_details::surface2d>(surf, _details::context_managed_deleter(*this));
		return image2d_view(std::move(surf_ptr), fmt);
	}

	swap_chain context::request_swap_chain(
		std::u8string_view name, system::window &wnd,
		std::uint32_t num_images, std::span<const graphics::format> formats
	) {
		auto *chain = new _details::swap_chain(
			wnd, num_images, { formats.begin(), formats.end() }, std::u8string(name)
		);
		auto surf_ptr = std::shared_ptr<_details::swap_chain>(chain, _details::context_managed_deleter(*this));
		return swap_chain(surf_ptr);
	}

	void context::run_compute_shader(
		assets::owning_handle<assets::shader> shader, cvec3<std::uint32_t> num_thread_groups,
		all_resource_bindings bindings
	) {
		_commands.emplace_back().emplace<context_commands::dispatch_compute>(
			std::move(bindings), std::move(shader), num_thread_groups
		);
	}

	context::pass context::begin_pass(
		std::vector<surface2d_color> color_rts, surface2d_depth_stencil ds_rt, cvec2s sz
	) {
		return context::pass(*this, std::move(color_rts), std::move(ds_rt), sz);
	}

	void context::present(swap_chain sc) {
		_commands.emplace_back().emplace<context_commands::present>(std::move(sc));
	}

	context::context(assets::manager &asset_man, graphics::context &ctx, graphics::command_queue &queue) :
		_assets(asset_man), _device(asset_man.get_device()), _context(ctx), _queue(queue),
		_cmd_alloc(_device.create_command_allocator()),
		_descriptor_pool(_device.create_descriptor_pool({
			graphics::descriptor_range::create(graphics::descriptor_type::acceleration_structure, 1024),
			graphics::descriptor_range::create(graphics::descriptor_type::constant_buffer, 1024),
			graphics::descriptor_range::create(graphics::descriptor_type::read_only_buffer, 1024),
			graphics::descriptor_range::create(graphics::descriptor_type::read_only_image, 1024),
			graphics::descriptor_range::create(graphics::descriptor_type::read_write_buffer, 1024),
			graphics::descriptor_range::create(graphics::descriptor_type::read_write_image, 1024),
			graphics::descriptor_range::create(graphics::descriptor_type::sampler, 1024),
		}, 1024)), // TODO proper numbers
		_thread(std::this_thread::get_id()) {
	}

	graphics::image2d_view &context::_request_image_view(const image2d_view &view) {
		if (!view._surface->image) {
			// create resource if it's not initialized
			view._surface->image = _device.create_committed_image2d(
				view._surface->size[0], view._surface->size[1], 1, 1,
				view._surface->original_format, view._surface->tiling, view._surface->usages
			);
			view._surface->current_usage = graphics::image_usage::initial;
		}
		auto mip_levels = graphics::mip_levels::only_highest(); // TODO
		return _resources.back().image_views.emplace_back(_device.create_image2d_view_from(
			view._surface->image, view._view_format, mip_levels
		));
	}

	graphics::image2d_view &context::_request_image_view(_execution_context &ectx, const swap_chain &chain) {
		auto &chain_data = *chain._swap_chain;

		if (chain_data.next_image_index == _details::swap_chain::invalid_image_index) {
			graphics::back_buffer_info back_buffer = nullptr;
			if (chain_data.chain) {
				back_buffer = _device.acquire_back_buffer(chain_data.chain);
			}
			cvec2s size = chain_data.window.get_size();
			if (size != chain_data.current_size || back_buffer.status != graphics::swap_chain_status::ok) {
				// wait until everything's done
				{
					auto cmd_list = ectx.take_command_list();
					auto fence = _device.create_fence(graphics::synchronization_state::unset);
					_queue.submit_command_lists({ &cmd_list }, &fence);
					_device.wait_for_fence(fence);
				}
				// create or resize the swap chain
				chain_data.current_size = size;
				if (chain_data.chain && back_buffer.status == graphics::swap_chain_status::ok) {
					_device.resize_swap_chain_buffers(
						chain_data.chain, chain_data.current_size
					);
				} else {
					chain_data.chain = nullptr;
					auto [new_chain, fmt] = _context.create_swap_chain_for_window(
						chain_data.window, _device, _queue,
						chain_data.num_images, chain_data.expected_formats
					);
					chain_data.chain = std::move(new_chain);
					chain_data.current_format = fmt;
				}
				// update fences
				for (auto &fence : chain_data.fences) {
					_device.reset_fence(fence);
				}
				while (chain_data.fences.size() < chain_data.num_images) {
					chain_data.fences.emplace_back(_device.create_fence(graphics::synchronization_state::unset));
				}
				chain_data.chain.update_synchronization_primitives(chain_data.fences);
				// re-acquire back buffer
				back_buffer = _device.acquire_back_buffer(chain_data.chain);
				assert(back_buffer.status == graphics::swap_chain_status::ok);
			}
			chain_data.next_image_index = back_buffer.index;
			chain_data.next_fence = back_buffer.on_presented;

			// TODO state transition
		}

		auto &img = _resources.back().images.emplace_back(
			chain_data.chain.get_image(chain_data.next_image_index)
		);
		auto &img_view = _resources.back().image_views.emplace_back(
			_device.create_image2d_view_from(img, chain_data.current_format, graphics::mip_levels::all())
		);
		return img_view;
	}

	void context::_create_descriptor_binding(
		_execution_context&, graphics::descriptor_set &set,
		const graphics::descriptor_set_layout &layout, std::uint32_t reg,
		const descriptor_resource::image2d &img
	) {
		auto &img_view = _request_image_view(img.view);
		graphics::image_usage target_usage = uninitialized;
		if (img.writable) {
			_device.write_descriptor_set_read_write_images(set, layout, reg, { &img_view });
			target_usage = graphics::image_usage::read_write_color_texture;
		} else {
			_device.write_descriptor_set_read_only_images(set, layout, reg, { &img_view });
			target_usage = graphics::image_usage::read_only_texture;
		}
		// TODO state transition
	}

	void context::_create_descriptor_binding(
		_execution_context &ectx, graphics::descriptor_set &set,
		const graphics::descriptor_set_layout &layout, std::uint32_t reg,
		const descriptor_resource::swap_chain_image &chain
	) {
		auto &img_view = _request_image_view(ectx, chain.image);
		_device.write_descriptor_set_read_write_images(set, layout, reg, { &img_view });
	}

	void context::_create_descriptor_binding(
		_execution_context &ectx, graphics::descriptor_set &set,
		const graphics::descriptor_set_layout &layout, std::uint32_t reg,
		const descriptor_resource::immediate_constant_buffer &cbuf
	) {
		auto &upload_buf = _resources.back().buffers.emplace_back(_device.create_committed_buffer(
			cbuf.data.size(), graphics::heap_type::upload, graphics::buffer_usage::mask::copy_source
		));
		{ // upload
			void *ptr = _device.map_buffer(upload_buf, 0, 0);
			std::memcpy(ptr, cbuf.data.data(), cbuf.data.size());
			_device.unmap_buffer(upload_buf, 0, cbuf.data.size());
		}

		auto &constant_buf = _resources.back().buffers.emplace_back(_device.create_committed_buffer(
			cbuf.data.size(), graphics::heap_type::device_only,
			graphics::buffer_usage::mask::copy_destination | graphics::buffer_usage::mask::read_only_buffer
		));
		ectx.get_command_list().copy_buffer(upload_buf, 0, constant_buf, 0, cbuf.data.size());
		// TODO state transition

		_device.write_descriptor_set_constant_buffers(
			set, layout, reg,
			{ graphics::constant_buffer_view::create(constant_buf, 0, cbuf.data.size()) }
		);
	}

	std::vector<context::_descriptor_set> context::_check_and_create_descriptor_set_bindings(
		_execution_context &ectx, const asset<assets::shader> &target,
		const all_resource_bindings &resources
	) {
		std::vector<_descriptor_set> result;

		const auto &target_sets = target.value.descriptor_bindings.sets;
		auto set_it = target_sets.begin();
		for (const auto &set_bindings : resources.sets) {
			while (set_it != target_sets.end() && set_it->space < set_bindings.space) {
				log().error<u8"Missing bindings for register space {}">(set_it->space);
				++set_it;
			}
			if (set_it == target_sets.end() || set_it->space != set_bindings.space) {
				log().error<u8"Specified inexistant bindings for register space {}">(set_bindings.space);
				continue;
			}

			auto &result_set = result.emplace_back(
				_device.create_descriptor_set(_descriptor_pool, set_it->layout), set_it->space
			);

			auto binding_it = set_it->ranges.begin();
			std::uint32_t next_offset_in_range = 0;
			for (const auto &binding : set_bindings.bindings) {
				while (
					binding_it != set_it->ranges.end() &&
					binding_it->get_last_register_index() < binding.register_index
				) {
					if (next_offset_in_range < binding_it->range.count) {
						log().error<u8"Missing bindings for register {} to {} in space {]">(
							next_offset_in_range, binding_it->range.count - 1, set_bindings.space
						);
					}
					++binding_it;
					next_offset_in_range = 0;
				}
				if (binding_it == set_it->ranges.end() || binding_it->register_index > binding.register_index) {
					log().error<u8"Specified inexistant binding for register index {}, space {}">(
						binding.register_index, set_bindings.space
					);
					continue;
				}

				std::visit([&, this](const auto &rsrc) {
					_create_descriptor_binding(ectx, result_set.set, set_it->layout, binding.register_index, rsrc);
				}, binding.resource);
			}
		}

		return result;
	}

	void context::_bind_descriptor_sets(
		_execution_context &ectx, const graphics::pipeline_resources &rsrc,
		const std::vector<_descriptor_set> &sets, _bind_point pt
	) {
		// organize descriptor sets by register space
		std::vector<const graphics::descriptor_set*> indexed_sets;
		for (const auto &set : sets) {
			if (set.space >= indexed_sets.size()) {
				indexed_sets.resize(set.space + 1);
			}
			indexed_sets[set.space] = &set.set;
		}
		switch (pt) {
		case _bind_point::graphics:
			ectx.get_command_list().bind_graphics_descriptor_sets(rsrc, 0, indexed_sets);
			break;
		case _bind_point::compute:
			ectx.get_command_list().bind_compute_descriptor_sets(rsrc, 0, indexed_sets);
			break;
		}
	}

	graphics::pipeline_resources context::_create_pipeline_resources(
		std::initializer_list<const asset<assets::shader>*> shaders
	) {
		std::vector<const shader_descriptor_bindings::set*> layouts;
		std::vector<const graphics::descriptor_set_layout*> sets;

		for (const auto *shader : shaders) {
			for (const auto &set : shader->value.descriptor_bindings.sets) {
				if (set.space >= sets.size()) {
					sets.resize(set.space + 1);
					layouts.resize(set.space + 1);
				}
				if (sets[set.space] == nullptr) {
					sets[set.space] = &set.layout;
					layouts[set.space] = &set;
				} else {
					if (set.ranges != layouts[set.space]->ranges) {
						log().error<u8"Mismatch descriptor set at space {}">(set.space);
					}
				}
			}
		}

		return _device.create_pipeline_resources(sets);
	}

	void context::_handle_pass_command(
		_execution_context &ectx,
		const graphics::frame_buffer_layout &fb_layout,
		const pass_commands::draw_instanced &cmd
	) {
		auto &cmd_list = ectx.get_command_list();

		std::vector<graphics::input_buffer_layout> input_layouts;
		for (const auto &input : cmd.inputs) {
			input_layouts.emplace_back(input.elements, input.stride, input.input_rate, input.buffer_index);
		}

		auto pipeline_resources = _create_pipeline_resources({ &cmd.vertex_shader.get(), &cmd.pixel_shader.get() });
		graphics::shader_set shaders(cmd.vertex_shader.get().value.binary, cmd.pixel_shader.get().value.binary);
		auto &pipeline_state = _resources.back().graphics_pipelines.emplace_back(
			_device.create_graphics_pipeline_state(
				pipeline_resources,
				shaders,
				cmd.state.blend_options,
				cmd.state.rasterizer_options,
				cmd.state.depth_stencil_options,
				input_layouts,
				cmd.topology,
				fb_layout
			)
		);

		std::vector<graphics::vertex_buffer> bufs;
		for (const auto &input : cmd.inputs) {
			if (input.buffer_index >= bufs.size()) {
				bufs.resize(input.buffer_index + 1, nullptr);
			}
			bufs[input.buffer_index] =
				graphics::vertex_buffer(input.buffer.get().value.data, input.offset, input.stride);
		}

		auto sets = _check_and_create_descriptor_set_bindings(ectx, cmd.vertex_shader.get(), cmd.resource_bindings);

		cmd_list.bind_pipeline_state(pipeline_state);
		_bind_descriptor_sets(ectx, pipeline_resources, sets, _bind_point::graphics);
		cmd_list.bind_vertex_buffers(0, bufs);
		if (cmd.index_buffer.buffer) {
			cmd_list.bind_index_buffer(
				cmd.index_buffer.buffer.get().value.data,
				cmd.index_buffer.offset,
				cmd.index_buffer.format
			);
			cmd_list.draw_indexed_instanced(0, cmd.index_count, 0, 0, cmd.instance_count);
		} else {
			cmd_list.draw_instanced(0, cmd.index_count, 0, cmd.instance_count);
		}
	}

	void context::_handle_command(_execution_context &ectx, const context_commands::dispatch_compute &cmd) {
		auto &cmd_list = ectx.get_command_list();

		// check that the descriptor bindings are correct
		auto sets = _check_and_create_descriptor_set_bindings(ectx, cmd.shader.get(), cmd.resources);
		auto pipeline_resources = _create_pipeline_resources({ &cmd.shader.get() });
		auto &pipeline = _resources.back().compute_pipelines.emplace_back(_device.create_compute_pipeline_state(
			pipeline_resources, cmd.shader.get().value.binary
		));

		cmd_list.bind_pipeline_state(pipeline);
		_bind_descriptor_sets(ectx, pipeline_resources, sets, _bind_point::compute);
		cmd_list.run_compute_shader(cmd.num_thread_groups[0], cmd.num_thread_groups[1], cmd.num_thread_groups[2]);
	}

	void context::_handle_command(_execution_context &ectx, const context_commands::render_pass &cmd) {
		auto &cmd_list = ectx.get_command_list();

		std::vector<graphics::color_render_target_access> color_rt_access;
		std::vector<graphics::image2d_view*> color_rts;
		std::vector<graphics::format> color_rt_formats;
		graphics::format ds_rt_format = graphics::format::none;

		// create frame buffer
		for (const auto &rt : cmd.color_render_targets) {
			if (std::holds_alternative<image2d_view>(rt.view)) {
				auto &img = std::get<image2d_view>(rt.view);
				assert(img._surface->size == cmd.render_target_size);
				color_rts.emplace_back(&_request_image_view(img));
				color_rt_formats.emplace_back(img._view_format);
			} else {
				auto &chain = std::get<swap_chain>(rt.view);
				assert(chain._swap_chain->current_size == cmd.render_target_size);
				color_rts.emplace_back(&_request_image_view(ectx, chain));
				color_rt_formats.emplace_back(chain._swap_chain->current_format);
			}
			color_rt_access.emplace_back(rt.access);
		}
		graphics::image2d_view *depth_stencil_rt = nullptr;
		if (cmd.depth_stencil_target.view) {
			assert(cmd.depth_stencil_target.view._surface->size == cmd.render_target_size);
			depth_stencil_rt = &_request_image_view(cmd.depth_stencil_target.view);
			ds_rt_format = cmd.depth_stencil_target.view._view_format;
		}
		auto &frame_buffer = _resources.back().frame_buffers.emplace_back(
			_device.create_frame_buffer(color_rts, depth_stencil_rt, cmd.render_target_size)
		);

		// frame buffer access
		graphics::frame_buffer_access access = nullptr;
		access.color_render_targets  = color_rt_access;
		access.depth_render_target   = cmd.depth_stencil_target.depth_access;
		access.stencil_render_target = cmd.depth_stencil_target.stencil_access;

		graphics::frame_buffer_layout fb_layout(color_rt_formats, ds_rt_format);

		// TODO state transition for everything in the pass

		cmd_list.begin_pass(frame_buffer, access);
		
		for (const auto &pass_cmd_val : cmd.commands) {
			std::visit([&](const auto &pass_cmd) {
				_handle_pass_command(ectx, fb_layout, pass_cmd);
			}, pass_cmd_val);
		}

		cmd_list.end_pass();
	}

	void context::_handle_command(_execution_context &ectx, const context_commands::present &cmd) {
		cmd.target._swap_chain->next_image_index = _details::swap_chain::invalid_image_index;
		if (auto list = ectx.maybe_take_command_list()) {
			_queue.submit_command_lists({ &list }, nullptr);
		}
		if (_queue.present(cmd.target._swap_chain->chain) != graphics::swap_chain_status::ok) {
			// TODO?
		}
	}

	void context::flush() {
		assert(std::this_thread::get_id() == _thread);

		_resources.emplace_back();
		_execution_context ectx(*this);

		auto cmds = std::exchange(_commands, {});
		for (const auto &cmd : cmds) {
			std::visit([&, this](const auto &cmd_val) {
				_handle_command(ectx, cmd_val);
			}, cmd);
		}

		auto cmd_list = ectx.take_command_list();
		_queue.submit_command_lists({ &cmd_list }, nullptr);
	}
}
