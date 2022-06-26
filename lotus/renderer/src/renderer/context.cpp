#include "lotus/renderer/context.h"

/// \file
/// Implementation of scene loading and rendering.

#include "lotus/logging.h"
#include "lotus/utils/stack_allocator.h"

namespace lotus::renderer {
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
		std::uint32_t num_insts,
		std::u8string_view description
	) {
		_command.commands.emplace_back(
			description,
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
		std::uint32_t num_insts,
		std::u8string_view description
	) {
		assert(false); // TODO not implemented
	}

	void context::pass::end() {
		_context->_commands.emplace_back(std::move(_description), std::move(_command));
		_context = nullptr;
	}


	context::_execution_context context::_execution_context::create(context &ctx) {
		auto &resources = ctx._all_resources.emplace_back(std::exchange(ctx._deferred_delete_resources, {}));
		return _execution_context(ctx, resources);
	}

	graphics::command_list &context::_execution_context::get_command_list() {
		if (!_list) {
			_list = &record(_ctx._device.create_and_start_command_list(_ctx._cmd_alloc));
		}
		return *_list;
	}

	void context::_execution_context::flush_transitions() {
		std::vector<graphics::image_barrier> image_barriers;

		{ // handle surface transitions
			auto surf_transitions = std::exchange(_surface_transitions, {});
			std::sort(
				surf_transitions.begin(), surf_transitions.end(),
				[](const _surface2d_transition_info &lhs, const _surface2d_transition_info &rhs) {
					if (lhs.surface == rhs.surface) {
						return lhs.mip_levels.minimum < rhs.mip_levels.minimum;
					}
					assert(lhs.surface->id != rhs.surface->id);
					return lhs.surface->id < rhs.surface->id;
				}
			);
			for (auto it = surf_transitions.begin(); it != surf_transitions.end(); ) {
				// transition resources
				std::size_t first_barrier = image_barriers.size();
				auto *surf = it->surface;
				for (auto first = it; it != surf_transitions.end() && it->surface == first->surface; ++it) {
					std::uint16_t max_mip =
						it->mip_levels.is_all() ?
						it->surface->num_mips :
						it->mip_levels.minimum + it->mip_levels.num_levels;
					for (std::uint16_t mip = it->mip_levels.minimum; mip < max_mip; ++mip) {
						image_barriers.emplace_back(
							graphics::subresource_index::create_color(mip, 0),
							it->surface->image,
							it->surface->current_usages[mip],
							it->usage
						);
					}
				}
				// deduplicate & warn about any conflicts
				std::sort(
					image_barriers.begin() + first_barrier, image_barriers.end(),
					[](const graphics::image_barrier &lhs, const graphics::image_barrier &rhs) {
						if (lhs.subresource.aspects == rhs.subresource.aspects) {
							if (lhs.subresource.array_slice == rhs.subresource.array_slice) {
								return lhs.subresource.mip_level < rhs.subresource.mip_level;
							}
							return lhs.subresource.array_slice < rhs.subresource.array_slice;
						}
						return lhs.subresource.aspects < rhs.subresource.aspects;
					}
				);
				if (image_barriers.size() > first_barrier) {
					auto last = image_barriers.begin() + first_barrier;
					for (auto cur = last; cur != image_barriers.end(); ++cur) {
						if (cur->subresource == last->subresource) {
							if (cur->to_state != last->to_state) {
								log().error<
									u8"Multiple transition targets for image resource {} slice {} mip {}. "
									u8"Maybe a flush_transitions() call is missing?"
								>(
									string::to_generic(surf->name),
									last->subresource.array_slice, last->subresource.mip_level
								);
							}
						} else {
							*++last = *cur;
						}
					}
					image_barriers.erase(last + 1, image_barriers.end());
					for (auto cur = image_barriers.begin() + first_barrier; cur != image_barriers.end(); ++cur) {
						surf->current_usages[cur->subresource.mip_level] = cur->to_state;
					}
				}
			}
		}

		{ // handle swap chain image transitions
			auto swap_chain_transitions = std::exchange(_swap_chain_transitions, {});
			// TODO check for conflicts
			for (const auto &trans : swap_chain_transitions) {
				image_barriers.emplace_back(
					graphics::subresource_index::first_color(),
					trans.chain->images[trans.chain->next_image_index],
					trans.chain->current_usages[trans.chain->next_image_index],
					trans.usage
				);
				trans.chain->current_usages[trans.chain->next_image_index] = trans.usage;
			}
		}

		if (image_barriers.size() > 0) {
			get_command_list().resource_barrier(image_barriers, {});
		}
	}


	context context::create(assets::manager &asset_man, graphics::context &ctx, graphics::command_queue &queue) {
		return context(asset_man, ctx, queue);
	}

	context::~context() {
		wait_idle();
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
		auto *surf = new _details::surface2d(
			size, 1, fmt, tiling, usage_mask, _resource_index++, std::u8string(name)
		);
		auto surf_ptr = std::shared_ptr<_details::surface2d>(surf, _details::context_managed_deleter(*this));
		return image2d_view(std::move(surf_ptr), fmt, graphics::mip_levels::all());
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
		all_resource_bindings bindings, std::u8string_view description
	) {
		_commands.emplace_back(description).value.emplace<context_commands::dispatch_compute>(
			std::move(bindings), std::move(shader), num_thread_groups
		);
	}

	context::pass context::begin_pass(
		std::vector<surface2d_color> color_rts, surface2d_depth_stencil ds_rt, cvec2s sz,
		std::u8string_view description
	) {
		return context::pass(*this, std::move(color_rts), std::move(ds_rt), sz, description);
	}

	void context::present(swap_chain sc, std::u8string_view description) {
		_commands.emplace_back(description).value.emplace<context_commands::present>(std::move(sc));
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

	graphics::image2d_view &context::_request_image_view(
		_execution_context &ectx, const recorded_resources::image2d_view &view
	) {
		if (!view._surface->image) {
			// create resource if it's not initialized
			view._surface->image = _device.create_committed_image2d(
				view._surface->size[0], view._surface->size[1], 1, view._surface->num_mips,
				view._surface->format, view._surface->tiling, view._surface->usages
			);
			if constexpr (register_debug_names) {
				_device.set_debug_name(view._surface->image, view._surface->name);
			}
		}
		return ectx.create_image2d_view(view._surface->image, view._view_format, view._mip_levels);
	}

	graphics::image2d_view &context::_request_image_view(
		_execution_context &ectx, const recorded_resources::swap_chain &chain
	) {
		auto &chain_data = *chain._swap_chain;

		if (chain_data.next_image_index == _details::swap_chain::invalid_image_index) {
			graphics::back_buffer_info back_buffer = nullptr;
			if (chain_data.chain) {
				back_buffer = _device.acquire_back_buffer(chain_data.chain);
				// wait until the buffer becomes available
				// we must always immediate wait after acquiring to make vulkan happy
				if (back_buffer.on_presented) {
					_device.wait_for_fence(*back_buffer.on_presented);
					_device.reset_fence(*back_buffer.on_presented);
				}
			}
			if (chain_data.desired_size != chain_data.current_size || back_buffer.status != graphics::swap_chain_status::ok) {
				// wait until everything's done
				{
					auto fence = _device.create_fence(graphics::synchronization_state::unset);
					ectx.submit(_queue, &fence);
					_device.wait_for_fence(fence);
				}
				chain_data.images.clear();
				// create or resize the swap chain
				chain_data.current_size = chain_data.desired_size;
				if (chain_data.chain && back_buffer.status == graphics::swap_chain_status::ok) {
					_device.resize_swap_chain_buffers(
						chain_data.chain, chain_data.current_size
					);
				} else {
					ectx.record(std::move(chain_data.chain));
					auto [new_chain, fmt] = _context.create_swap_chain_for_window(
						chain_data.window, _device, _queue,
						chain_data.num_images, chain_data.expected_formats
					);
					chain_data.chain = std::move(new_chain);
					chain_data.current_format = fmt;
				}
				// update chain images
				for (std::size_t i = 0; i < chain_data.num_images; ++i) {
					chain_data.images.emplace_back(chain_data.chain.get_image(i));
				}
				chain_data.current_usages = std::vector<graphics::image_usage>(
					chain_data.num_images, graphics::image_usage::initial
				);
				// update fences
				for (auto &fence : chain_data.fences) {
					ectx.record(std::move(fence));
				}
				chain_data.fences.clear();
				for (std::size_t i = 0; i < chain_data.num_images; ++i) {
					chain_data.fences.emplace_back(_device.create_fence(graphics::synchronization_state::unset));
				}
				chain_data.chain.update_synchronization_primitives(chain_data.fences);
				// re-acquire back buffer
				back_buffer = _device.acquire_back_buffer(chain_data.chain);
				// wait until the buffer becomes available
				if (back_buffer.on_presented) {
					_device.wait_for_fence(*back_buffer.on_presented);
					_device.reset_fence(*back_buffer.on_presented);
				}
				assert(back_buffer.status == graphics::swap_chain_status::ok);
			}
			chain_data.next_image_index = back_buffer.index;
		}

		return ectx.create_image2d_view(
			chain_data.images[chain_data.next_image_index],
			chain_data.current_format,
			graphics::mip_levels::only_highest()
		);
	}

	void context::_create_descriptor_binding(
		_execution_context &ectx, graphics::descriptor_set &set,
		const graphics::descriptor_set_layout &layout, std::uint32_t reg,
		const descriptor_resource::image2d &img
	) {
		auto &img_view = _request_image_view(ectx, img.view);
		graphics::image_usage target_usage = uninitialized;
		switch (img.binding_type) {
		case image_binding_type::read_only:
			_device.write_descriptor_set_read_only_images(set, layout, reg, { &img_view });
			target_usage = graphics::image_usage::read_only_texture;
			break;
		case image_binding_type::read_write:
			_device.write_descriptor_set_read_write_images(set, layout, reg, { &img_view });
			target_usage = graphics::image_usage::read_write_color_texture;
			break;
		}
		ectx.stage_transition(*img.view._surface, img.view._mip_levels, target_usage);
	}

	void context::_create_descriptor_binding(
		_execution_context &ectx, graphics::descriptor_set &set,
		const graphics::descriptor_set_layout &layout, std::uint32_t reg,
		const descriptor_resource::swap_chain_image &chain
	) {
		auto &img_view = _request_image_view(ectx, chain.image);
		_device.write_descriptor_set_read_write_images(set, layout, reg, { &img_view });
		ectx.stage_transition(*chain.image._swap_chain, graphics::image_usage::read_write_color_texture);
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

		// TODO use barriers provided by the execution context
		ectx.get_command_list().resource_barrier({}, {
			graphics::buffer_barrier(constant_buf, graphics::buffer_usage::copy_destination, graphics::buffer_usage::read_only_buffer),
		});

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

	std::vector<context::_descriptor_set_info> context::_check_and_create_descriptor_set_bindings(
		_execution_context &ectx, std::initializer_list<const asset<assets::shader>*> targets,
		const all_resource_bindings &resources
	) {
		std::vector<_descriptor_set_info> result;

		using _iter_t = std::vector<shader_descriptor_bindings::set>::const_iterator;
		std::vector<std::pair<_iter_t, _iter_t>> iters;
		for (const auto *t : targets) {
			iters.emplace_back(t->value.descriptor_bindings.sets.begin(), t->value.descriptor_bindings.sets.end());
		}
		for (std::size_t set_index = 0; set_index < resources.sets.size(); ++set_index) {
			const auto &set_bindings = resources.sets[set_index];
			_descriptor_set_info *result_set = nullptr;
			_iter_t prev_iter;
			for (auto &layout : iters) {
				while (layout.first != layout.second && layout.first->space < set_bindings.space) {
					if (set_index > 0 && layout.first->space != resources.sets[set_index - 1].space) {
						log().error<u8"Missing bindings for register space {}">(layout.first->space);
					}
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
						// the compiler can optimize out unused descriptors
						/*log().error<u8"Specified inexistant binding for register index {}, space {}">(
							binding.register_index, set_bindings.space
						);*/
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
		std::vector<_descriptor_set_info> sets, _bind_point pt
	) {
		// organize descriptor sets by register space
		std::sort(
			sets.begin(), sets.end(),
			[](const _descriptor_set_info &lhs, const _descriptor_set_info &rhs) {
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
				ectx.get_command_list().bind_graphics_descriptor_sets(
					rsrc, sets[i].space, { set_ptrs.begin() + i, set_ptrs.begin() + end }
				);
				break;
			case _bind_point::compute:
				ectx.get_command_list().bind_compute_descriptor_sets(
					rsrc, sets[i].space, { set_ptrs.begin() + i, set_ptrs.begin() + end }
				);
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

	context::_pass_command_data context::_preprocess_command(
		_execution_context &ectx,
		const graphics::frame_buffer_layout &fb_layout,
		const pass_commands::draw_instanced &cmd
	) {
		context::_pass_command_data result = nullptr;
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
		_execution_context &ectx, _pass_command_data data, const pass_commands::draw_instanced &cmd
	) {
		std::vector<graphics::vertex_buffer> bufs;
		for (const auto &input : cmd.inputs) {
			if (input.buffer_index >= bufs.size()) {
				bufs.resize(input.buffer_index + 1, nullptr);
			}
			bufs[input.buffer_index] =
				graphics::vertex_buffer(input.buffer.get().value.data, input.offset, input.stride);
		}

		auto &cmd_list = ectx.get_command_list();
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
		// create descriptor bindings
		auto sets = _check_and_create_descriptor_set_bindings(ectx, { &cmd.shader.get() }, cmd.resources);
		auto &pipeline_resources = _create_pipeline_resources(ectx, { &cmd.shader.get() });
		auto &pipeline = ectx.record(_device.create_compute_pipeline_state(
			pipeline_resources, cmd.shader.get().value.binary
		));
		ectx.flush_transitions();

		auto &cmd_list = ectx.get_command_list();
		cmd_list.bind_pipeline_state(pipeline);
		_bind_descriptor_sets(ectx, pipeline_resources, std::move(sets), _bind_point::compute);
		cmd_list.run_compute_shader(cmd.num_thread_groups[0], cmd.num_thread_groups[1], cmd.num_thread_groups[2]);
	}

	void context::_handle_command(_execution_context &ectx, const context_commands::render_pass &cmd) {
		std::vector<graphics::color_render_target_access> color_rt_access;
		std::vector<graphics::image2d_view*> color_rts;
		std::vector<graphics::format> color_rt_formats;
		graphics::format ds_rt_format = graphics::format::none;

		// create frame buffer
		for (const auto &rt : cmd.color_render_targets) {
			if (std::holds_alternative<recorded_resources::image2d_view>(rt.view)) {
				auto img = std::get<recorded_resources::image2d_view>(rt.view);
				// make sure we're only using one mip
				if (img._mip_levels.num_levels != 1) {
					if (img._surface->num_mips - img._mip_levels.minimum > 1) {
						std::uint16_t num_levels =
							img._mip_levels.is_all() ?
							img._surface->num_mips - img._mip_levels.minimum :
							img._mip_levels.num_levels;
						log().error<u8"More than one ({}) mip specified for render target for texture {}">(
							num_levels, string::to_generic(img._surface->name)
						);
					}
					img._mip_levels.num_levels = 1;
				}
				color_rts.emplace_back(&_request_image_view(ectx, img));
				color_rt_formats.emplace_back(img._view_format);
				ectx.stage_transition(*img._surface, img._mip_levels, graphics::image_usage::color_render_target);
				assert(img._surface->size == cmd.render_target_size);
			} else if (std::holds_alternative<recorded_resources::swap_chain>(rt.view)) {
				auto &chain = std::get<recorded_resources::swap_chain>(rt.view);
				color_rts.emplace_back(&_request_image_view(ectx, chain));
				color_rt_formats.emplace_back(chain._swap_chain->current_format);
				ectx.stage_transition(*chain._swap_chain, graphics::image_usage::color_render_target);
				assert(chain._swap_chain->current_size == cmd.render_target_size);
			} else {
				assert(false); // unhandled
			}
			color_rt_access.emplace_back(rt.access);
		}
		graphics::image2d_view *depth_stencil_rt = nullptr;
		if (cmd.depth_stencil_target.view) {
			assert(cmd.depth_stencil_target.view._surface->size == cmd.render_target_size);
			depth_stencil_rt = &_request_image_view(ectx, cmd.depth_stencil_target.view);
			ds_rt_format = cmd.depth_stencil_target.view._view_format;
			ectx.stage_transition(
				*cmd.depth_stencil_target.view._surface,
				cmd.depth_stencil_target.view._mip_levels,
				graphics::image_usage::depth_stencil_render_target
			);
		}
		auto &frame_buffer = ectx.create_frame_buffer(color_rts, depth_stencil_rt, cmd.render_target_size);

		// frame buffer access
		graphics::frame_buffer_access access = nullptr;
		access.color_render_targets  = color_rt_access;
		access.depth_render_target   = cmd.depth_stencil_target.depth_access;
		access.stencil_render_target = cmd.depth_stencil_target.stencil_access;

		graphics::frame_buffer_layout fb_layout(color_rt_formats, ds_rt_format);

		// preprocess all commands in the pass and transition resources
		std::vector<_pass_command_data> data;
		for (const auto &pass_cmd_val : cmd.commands) {
			std::visit([&](const auto &pass_cmd) {
				data.emplace_back(_preprocess_command(ectx, fb_layout, pass_cmd));
			}, pass_cmd_val.value);
		}
		ectx.flush_transitions();

		auto &cmd_list = ectx.get_command_list();
		cmd_list.begin_pass(frame_buffer, access);
		cmd_list.set_viewports({ graphics::viewport::create(
			aab2f::create_from_min_max(zero, cmd.render_target_size.into<float>()), 0.0f, 1.0f
		) });
		cmd_list.set_scissor_rectangles({ aab2i::create_from_min_max(zero, cmd.render_target_size.into<int>()) });
		
		for (std::size_t i = 0; i < cmd.commands.size(); ++i) {
			std::visit([&](const auto &pass_cmd) {
				_handle_pass_command(ectx, std::move(data[i]), pass_cmd);
			}, cmd.commands[i].value);
		}

		cmd_list.end_pass();
	}

	void context::_handle_command(_execution_context &ectx, const context_commands::present &cmd) {
		// if we haven't written anything to the swap chain, just don't present
		if (cmd.target._swap_chain->chain) {
			ectx.stage_transition(*cmd.target._swap_chain, graphics::image_usage::present);
			ectx.flush_transitions();

			ectx.submit(_queue, nullptr);
			if (_queue.present(cmd.target._swap_chain->chain) != graphics::swap_chain_status::ok) {
				// TODO?
			}
		}
		// do this last, since handling transitions needs the index of the image
		cmd.target._swap_chain->next_image_index = _details::swap_chain::invalid_image_index;
	}

	void context::_cleanup() {
		std::uint32_t first_batch = _batch_index + 1 - _all_resources.size();
		std::uint64_t finished_batch = _device.query_timeline_semaphore(_batch_semaphore);
		while (first_batch <= finished_batch) {
			_all_resources.pop_front();
			++first_batch;
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
			}, cmd.value);
		}
		ectx.flush_transitions();

		auto signal_semaphores = {
			graphics::timeline_semaphore_synchronization(_batch_semaphore, _batch_index)
		};
		ectx.submit(_queue, graphics::queue_synchronization(nullptr, {}, signal_semaphores));

		_cleanup();
	}

	void context::wait_idle() {
		_device.wait_for_timeline_semaphore(_batch_semaphore, _batch_index);
		_cleanup();
	}
}
