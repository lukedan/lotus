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


	all_resource_bindings all_resource_bindings::from_unsorted(std::vector<resource_set_binding> s) {
		all_resource_bindings result = nullptr;
		result.consolidate();
		return result;
	}

	void all_resource_bindings::consolidate() {
		// sort based on register space
		std::sort(
			sets.begin(), sets.end(),
			[](const resource_set_binding &lhs, const resource_set_binding &rhs) {
				return lhs.space < rhs.space;
			}
		);
		// deduplicate
		auto last = sets.begin();
		for (auto it = sets.begin(); it != sets.end(); ++it) {
			if (last->space == it->space) {
				if (last != it) {
					for (auto &b : it->bindings) {
						last->bindings.emplace_back(std::move(b));
					}
				}
			} else {
				++last;
				if (last != it) {
					*last = std::move(*it);
				}
			}
		}
		sets.erase(last + 1, sets.end());
		// sort each set
		for (auto &set : sets) {
			std::sort(
				set.bindings.begin(), set.bindings.end(),
				[](const resource_binding &lhs, const resource_binding &rhs) {
					return lhs.register_index < rhs.register_index;
				}
			);
			// check for any duplicate bindings
			for (std::size_t i = 1; i < set.bindings.size(); ++i) {
				if (set.bindings[i].register_index == set.bindings[i - 1].register_index) {
					log().error<
						u8"Duplicate bindings for set {} register {}"
					>(set.space, set.bindings[i].register_index);
				}
			}
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


	context::_execution_context context::_execution_context::create(context &ctx) {
		return _execution_context(ctx, ctx._all_resources.emplace_back());
	}

	graphics::command_list &context::_execution_context::get_command_list() {
		if (!_list) {
			_list = &record(_ctx._device.create_and_start_command_list(_ctx._cmd_alloc));
		}
		return *_list;
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
		_empty_layout(_device.create_descriptor_set_layout({}, graphics::shader_stage::all)),
		_batch_semaphore(_device.create_timeline_semaphore(0)),
		_thread(std::this_thread::get_id()) {
	}

	graphics::image2d_view &context::_request_image_view(_execution_context &ectx, const image2d_view &view) {
		if (!view._surface->image) {
			// create resource if it's not initialized
			view._surface->image = _device.create_committed_image2d(
				view._surface->size[0], view._surface->size[1], 1, 1,
				view._surface->original_format, view._surface->tiling, view._surface->usages
			);
			view._surface->current_usage = graphics::image_usage::initial;
		}
		auto mip_levels = graphics::mip_levels::only_highest(); // TODO
		return ectx.create_image2d_view(view._surface->image, view._view_format, mip_levels);
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
					auto fence = _device.create_fence(graphics::synchronization_state::unset);
					ectx.submit(_queue, &fence);
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

		auto &img = ectx.record(chain_data.chain.get_image(chain_data.next_image_index));
		auto &img_view = ectx.create_image2d_view(
			img, chain_data.current_format, graphics::mip_levels::only_highest() // TODO
		);
		return img_view;
	}

	void context::_create_descriptor_binding(
		_execution_context &ectx, graphics::descriptor_set &set,
		const graphics::descriptor_set_layout &layout, std::uint32_t reg,
		const descriptor_resource::image2d &img
	) {
		auto &img_view = _request_image_view(ectx, img.view);
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
		auto &upload_buf = ectx.create_buffer(
			cbuf.data.size(), graphics::heap_type::upload, graphics::buffer_usage::mask::copy_source
		);
		{ // upload
			void *ptr = _device.map_buffer(upload_buf, 0, 0);
			std::memcpy(ptr, cbuf.data.data(), cbuf.data.size());
			_device.unmap_buffer(upload_buf, 0, cbuf.data.size());
		}

		auto &constant_buf = ectx.create_buffer(
			cbuf.data.size(), graphics::heap_type::device_only,
			graphics::buffer_usage::mask::copy_destination | graphics::buffer_usage::mask::read_only_buffer
		);
		ectx.get_command_list().copy_buffer(upload_buf, 0, constant_buf, 0, cbuf.data.size());
		// TODO state transition

		_device.write_descriptor_set_constant_buffers(
			set, layout, reg,
			{ graphics::constant_buffer_view::create(constant_buf, 0, cbuf.data.size()) }
		);
	}

	void context::_create_descriptor_binding(
		_execution_context &ectx, graphics::descriptor_set &set,
		const graphics::descriptor_set_layout &layout, std::uint32_t reg,
		const descriptor_resource::sampler &samp
	) {
		auto &gfx_samp = ectx.record(_device.create_sampler(
			samp.minification, samp.magnification, samp.mipmapping,
			samp.mip_lod_bias, samp.min_lod, samp.max_lod, samp.max_anisotropy,
			samp.addressing_u, samp.addressing_v, samp.addressing_w, samp.border_color, samp.comparison
		));
		_device.write_descriptor_set_samplers(
			set, layout, reg, { &gfx_samp }
		);
	}

	std::vector<_details::descriptor_set_info> context::_check_and_create_descriptor_set_bindings(
		_execution_context &ectx, std::initializer_list<const asset<assets::shader>*> targets,
		const all_resource_bindings &resources
	) {
		std::vector<_details::descriptor_set_info> result;

		using _iter_t = std::vector<shader_descriptor_bindings::set>::const_iterator;
		std::vector<std::pair<_iter_t, _iter_t>> iters;
		for (const auto *t : targets) {
			iters.emplace_back(t->value.descriptor_bindings.sets.begin(), t->value.descriptor_bindings.sets.end());
		}
		for (const auto &set_bindings : resources.sets) {
			_details::descriptor_set_info *result_set = nullptr;
			_iter_t prev_iter;
			for (auto &layout : iters) {
				while (layout.first != layout.second && layout.first->space < set_bindings.space) {
					log().error<u8"Missing bindings for register space {}">(layout.first->space);
					++layout.first;
				}
				if (layout.first == layout.second || layout.first->space > set_bindings.space) {
					continue;
				}
				if (result_set) {
					if (layout.first->ranges != prev_iter->ranges) {
						log().error<
							u8"Conflicting bindings for set {} between shaders in the same pipeline"
						>(set_bindings.space);
					}
					continue;
				}

				result_set = &result.emplace_back(
					ectx.record(_device.create_descriptor_set(_descriptor_pool, layout.first->layout)),
					set_bindings.space
				);
				prev_iter = layout.first;

				auto binding_it = layout.first->ranges.begin();
				std::uint32_t next_offset_in_range = 0;
				for (const auto &binding : set_bindings.bindings) {
					while (
						binding_it != layout.first->ranges.end() &&
						binding_it->get_last_register_index() < binding.register_index
					) { // we're after this range
						if (next_offset_in_range < binding_it->range.count) {
							log().error<u8"Missing bindings for register {} to {} in space {}">(
								next_offset_in_range + binding_it->register_index,
								binding_it->get_last_register_index(),
								set_bindings.space
							);
						}
						++binding_it;
						next_offset_in_range = 0;
					}
					if (
						binding_it == layout.first->ranges.end() ||
						binding_it->register_index > binding.register_index
					) {
						log().error<u8"Specified inexistant binding for register index {}, space {}">(
							binding.register_index, set_bindings.space
						);
						continue;
					}
					std::uint32_t offset_in_range = binding.register_index - binding_it->register_index;
					if (offset_in_range > next_offset_in_range) {
						log().error<u8"Missing bindings for register {} to {} in space {}">(
							next_offset_in_range + binding_it->register_index,
							offset_in_range + binding_it->register_index - 1,
							set_bindings.space
						);
					}
					next_offset_in_range = offset_in_range + 1;

					std::visit([&, this](const auto &rsrc) {
						_create_descriptor_binding(
							ectx, *result_set->set, layout.first->layout, binding.register_index, rsrc
						);
					}, binding.resource);
				}
			}

			if (!result_set) {
				log().error<u8"Specified inexistant bindings for register space {}">(set_bindings.space);
			}
		}

		return result;
	}

	void context::_bind_descriptor_sets(
		_execution_context &ectx, const graphics::pipeline_resources &rsrc,
		std::vector<_details::descriptor_set_info> sets, _bind_point pt
	) {
		// organize descriptor sets by register space
		std::sort(
			sets.begin(), sets.end(),
			[](const _details::descriptor_set_info &lhs, const _details::descriptor_set_info &rhs) {
				return lhs.space < rhs.space;
			}
		);
		std::vector<graphics::descriptor_set*> set_ptrs;
		for (const auto &s : sets) {
			set_ptrs.emplace_back(s.set);
		}

		for (std::size_t i = 0; i < sets.size(); ) {
			std::size_t end = i + 1;
			for (; end < sets.size() && sets[end].space == sets[i].space + (end - i); ++end) {
			}

			switch (pt) {
			case _bind_point::graphics:
				ectx.get_command_list().bind_graphics_descriptor_sets(rsrc, sets[i].space, { set_ptrs.begin() + i, set_ptrs.begin() + end });
				break;
			case _bind_point::compute:
				ectx.get_command_list().bind_compute_descriptor_sets(rsrc, sets[i].space, { set_ptrs.begin() + i, set_ptrs.begin() + end });
				break;
			}

			i = end;
		}
	}

	graphics::pipeline_resources &context::_create_pipeline_resources(
		_execution_context &ectx, std::initializer_list<const asset<assets::shader>*> shaders
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

		for (const auto *&s : sets) {
			if (s == nullptr) {
				s = &_empty_layout;
			}
		}

		return ectx.record(_device.create_pipeline_resources(sets));
	}

	pass_commands::command_data context::_preprocess_command(
		_execution_context &ectx,
		const graphics::frame_buffer_layout &fb_layout,
		const pass_commands::draw_instanced &cmd
	) {
		pass_commands::command_data result = nullptr;
		result.resources = &_create_pipeline_resources(ectx, { &cmd.vertex_shader.get(), &cmd.pixel_shader.get() });

		graphics::shader_set shaders(cmd.vertex_shader.get().value.binary, cmd.pixel_shader.get().value.binary);
		std::vector<graphics::input_buffer_layout> input_layouts;
		for (const auto &input : cmd.inputs) {
			input_layouts.emplace_back(input.elements, input.stride, input.input_rate, input.buffer_index);
		}
		result.pipeline_state = &ectx.record(_device.create_graphics_pipeline_state(
			*result.resources,
			shaders,
			cmd.state.blend_options,
			cmd.state.rasterizer_options,
			cmd.state.depth_stencil_options,
			input_layouts,
			cmd.topology,
			fb_layout
		));

		result.descriptor_sets = _check_and_create_descriptor_set_bindings(
			ectx, { &cmd.vertex_shader.get(), &cmd.pixel_shader.get() }, cmd.resource_bindings
		);

		return result;
	}

	void context::_handle_pass_command(
		_execution_context &ectx,
		pass_commands::command_data data,
		const pass_commands::draw_instanced &cmd
	) {
		auto &cmd_list = ectx.get_command_list();

		std::vector<graphics::vertex_buffer> bufs;
		for (const auto &input : cmd.inputs) {
			if (input.buffer_index >= bufs.size()) {
				bufs.resize(input.buffer_index + 1, nullptr);
			}
			bufs[input.buffer_index] =
				graphics::vertex_buffer(input.buffer.get().value.data, input.offset, input.stride);
		}

		cmd_list.bind_pipeline_state(*data.pipeline_state);
		_bind_descriptor_sets(ectx, *data.resources, std::move(data.descriptor_sets), _bind_point::graphics);
		cmd_list.bind_vertex_buffers(0, bufs);
		if (cmd.index_buffer.buffer) {
			cmd_list.bind_index_buffer(
				cmd.index_buffer.buffer.get().value.data,
				cmd.index_buffer.offset,
				cmd.index_buffer.format
			);
			cmd_list.draw_indexed_instanced(0, cmd.index_count, 0, 0, cmd.instance_count);
		} else {
			cmd_list.draw_instanced(0, cmd.vertex_count, 0, cmd.instance_count);
		}
	}

	void context::_handle_command(_execution_context &ectx, const context_commands::dispatch_compute &cmd) {
		auto &cmd_list = ectx.get_command_list();

		// check that the descriptor bindings are correct
		auto sets = _check_and_create_descriptor_set_bindings(ectx, { &cmd.shader.get() }, cmd.resources);
		auto &pipeline_resources = _create_pipeline_resources(ectx, { &cmd.shader.get() });
		auto &pipeline = ectx.record(_device.create_compute_pipeline_state(
			pipeline_resources, cmd.shader.get().value.binary
		));

		cmd_list.bind_pipeline_state(pipeline);
		_bind_descriptor_sets(ectx, pipeline_resources, std::move(sets), _bind_point::compute);
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
				color_rts.emplace_back(&_request_image_view(ectx, img));
				color_rt_formats.emplace_back(img._view_format);
			} else {
				auto &chain = std::get<swap_chain>(rt.view);
				color_rts.emplace_back(&_request_image_view(ectx, chain));
				assert(chain._swap_chain->current_size == cmd.render_target_size);
				color_rt_formats.emplace_back(chain._swap_chain->current_format);
			}
			color_rt_access.emplace_back(rt.access);
		}
		graphics::image2d_view *depth_stencil_rt = nullptr;
		if (cmd.depth_stencil_target.view) {
			assert(cmd.depth_stencil_target.view._surface->size == cmd.render_target_size);
			depth_stencil_rt = &_request_image_view(ectx, cmd.depth_stencil_target.view);
			ds_rt_format = cmd.depth_stencil_target.view._view_format;
		}
		auto &frame_buffer = ectx.create_frame_buffer(color_rts, depth_stencil_rt, cmd.render_target_size);

		// frame buffer access
		graphics::frame_buffer_access access = nullptr;
		access.color_render_targets  = color_rt_access;
		access.depth_render_target   = cmd.depth_stencil_target.depth_access;
		access.stencil_render_target = cmd.depth_stencil_target.stencil_access;

		graphics::frame_buffer_layout fb_layout(color_rt_formats, ds_rt_format);

		// TODO state transition for everything in the pass
		std::vector<pass_commands::command_data> data;
		for (const auto &pass_cmd_val : cmd.commands) {
			std::visit([&](const auto &pass_cmd) {
				data.emplace_back(_preprocess_command(ectx, fb_layout, pass_cmd));
			}, pass_cmd_val);
		}

		cmd_list.begin_pass(frame_buffer, access);
		cmd_list.set_viewports({ graphics::viewport::create(
			aab2f::create_from_min_max(zero, cmd.render_target_size.into<float>()), 0.0f, 1.0f
		) });
		cmd_list.set_scissor_rectangles({ aab2i::create_from_min_max(zero, cmd.render_target_size.into<int>()) });
		
		for (std::size_t i = 0; i < cmd.commands.size(); ++i) {
			std::visit([&](const auto &pass_cmd) {
				_handle_pass_command(ectx, std::move(data[i]), pass_cmd);
			}, cmd.commands[i]);
		}

		cmd_list.end_pass();
	}

	void context::_handle_command(_execution_context &ectx, const context_commands::present &cmd) {
		// if we haven't written anything to the swap chain, just don't present
		if (cmd.target._swap_chain->chain) {
			cmd.target._swap_chain->next_image_index = _details::swap_chain::invalid_image_index;
			ectx.submit(_queue, nullptr);
			if (_queue.present(cmd.target._swap_chain->chain) != graphics::swap_chain_status::ok) {
				// TODO?
			}
		}
	}

	void context::flush() {
		assert(std::this_thread::get_id() == _thread);

		++_batch_index;
		auto ectx = _execution_context::create(*this);

		auto cmds = std::exchange(_commands, {});
		for (const auto &cmd : cmds) {
			std::visit([&, this](const auto &cmd_val) {
				_handle_command(ectx, cmd_val);
			}, cmd);
		}

		auto signal_semaphores = {
			graphics::timeline_semaphore_synchronization(_batch_semaphore, _batch_index)
		};
		ectx.submit(_queue, graphics::queue_synchronization(nullptr, {}, signal_semaphores));

		// cleanup
		std::uint32_t first_batch = _batch_index + 1 - _all_resources.size();
		std::uint64_t finished_batch = _device.query_timeline_semaphore(_batch_semaphore);
		while (first_batch <= finished_batch) {
			_all_resources.pop_front();
			++first_batch;
		}
	}
}
