#include "lotus/renderer/context.h"

/// \file
/// Implementation of scene loading and rendering.

#include <unordered_map>

#include "lotus/logging.h"
#include "lotus/utils/stack_allocator.h"

namespace lotus::renderer {
	void context::pass::draw_instanced(
		std::vector<input_buffer_binding> inputs,
		std::uint32_t num_verts,
		index_buffer_binding indices,
		std::uint32_t num_indices,
		gpu::primitive_topology topology,
		all_resource_bindings resources,
		assets::handle<assets::shader> vs,
		assets::handle<assets::shader> ps,
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
		assets::handle<assets::geometry> geom_asset,
		assets::handle<assets::material> mat_asset,
		pass_context &pass_ctx,
		std::span<const input_buffer_binding> additional_inputs,
		all_resource_bindings additional_resources,
		graphics_pipeline_state state,
		std::uint32_t num_insts,
		std::u8string_view description
	) {
		const auto &geom = geom_asset.get().value;
		const auto &mat = mat_asset.get().value;
		auto &&[vs, inputs] = pass_ctx.get_vertex_shader(*_context, *mat.data, geom);
		auto ps = pass_ctx.get_pixel_shader(*_context, *mat.data);
		for (const auto &in : additional_inputs) {
			inputs.emplace_back(in);
		}
		auto resource_bindings = mat.data->create_resource_bindings();
		resource_bindings.sets.reserve(resource_bindings.sets.size() + additional_resources.sets.size());
		for (auto &s : additional_resources.sets) {
			resource_bindings.sets.emplace_back(std::move(s));
		}
		resource_bindings.consolidate();
		_command.commands.emplace_back(
			description,
			std::in_place_type<pass_commands::draw_instanced>,
			num_insts,
			std::move(inputs), geom.num_vertices,
			geom.get_index_buffer_binding(), geom.num_indices,
			geom.topology,
			std::move(resource_bindings),
			std::move(vs), std::move(ps),
			std::move(state)
		);
	}

	void context::pass::end() {
		_context->_commands.emplace_back(std::move(_description), std::move(_command));
		_context = nullptr;
	}


	context::_execution_context context::_execution_context::create(context &ctx) {
		auto &resources = ctx._all_resources.emplace_back(std::exchange(ctx._deferred_delete_resources, {}));
		return _execution_context(ctx, resources);
	}

	gpu::command_list &context::_execution_context::get_command_list() {
		if (!_list) {
			_list = &record(_ctx._device.create_and_start_command_list(_ctx._cmd_alloc));
		}
		return *_list;
	}

	void context::_execution_context::stage_transition(
		_details::surface2d &surf, gpu::mip_levels mips, gpu::image_usage usage
	) {
		_surface_transitions.emplace_back(surf, mips, usage);
		for (const auto &ref : surf.array_references) {
			const auto &img = ref.arr->images[ref.index].image;
			assert(img._surface == &surf);
			if (!gpu::mip_levels::intersection(img._mip_levels, mips).is_empty()) {
				ref.arr->staged_transitions.emplace_back(ref.index);
			}
		}
	}

	void context::_execution_context::stage_all_transitions(_details::descriptor_array &arr) {
		auto transitions = std::exchange(arr.staged_transitions, {});
		std::sort(transitions.begin(), transitions.end());
		transitions.erase(std::unique(transitions.begin(), transitions.end()), transitions.end());
		auto usage = gpu::to_image_usage(arr.type);
		assert(usage != gpu::image_usage::num_enumerators);
		for (auto index : transitions) {
			const auto &img = arr.images[index];
			_surface_transitions.emplace_back(*img.image._surface, img.image._mip_levels, usage);
		}
	}

	void context::_execution_context::flush_transitions() {
		std::vector<gpu::image_barrier> image_barriers;
		std::vector<gpu::buffer_barrier> buffer_barriers;

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
					auto max_mip =
						it->mip_levels.is_all() ?
						it->surface->num_mips :
						it->mip_levels.minimum + it->mip_levels.num_levels;
					for (std::uint16_t mip = it->mip_levels.minimum; mip < max_mip; ++mip) {
						// check if a transition is really necessary
						gpu::image_usage current_usage = it->surface->current_usages[mip];
						if (current_usage == it->usage) {
							if (current_usage != gpu::image_usage::read_write_color_texture) {
								continue;
							}
						}

						gpu::subresource_index sub_index(mip, 0, gpu::image_aspect_mask::none);
						const auto &fmt_prop = gpu::format_properties::get(it->surface->format);
						// TODO more intelligent aspect mask?
						if (fmt_prop.has_color()) {
							sub_index.aspects |= gpu::image_aspect_mask::color;
						}
						if (fmt_prop.depth_bits > 0) {
							sub_index.aspects |= gpu::image_aspect_mask::depth;
						}
						if (fmt_prop.stencil_bits > 0) {
							sub_index.aspects |= gpu::image_aspect_mask::stencil;
						}
						image_barriers.emplace_back(sub_index, it->surface->image, current_usage, it->usage);
					}
				}
				// deduplicate & warn about any conflicts
				std::sort(
					image_barriers.begin() + first_barrier, image_barriers.end(),
					[](const gpu::image_barrier &lhs, const gpu::image_barrier &rhs) {
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
					gpu::subresource_index::first_color(),
					trans.chain->images[trans.chain->next_image_index],
					trans.chain->current_usages[trans.chain->next_image_index],
					trans.usage
				);
				trans.chain->current_usages[trans.chain->next_image_index] = trans.usage;
			}
		}

		{ // handle raw buffer barriers
			auto buffer_transitions = std::exchange(_raw_buffer_transitions, {});
			for (auto trans : buffer_transitions) {
				buffer_barriers.emplace_back(*trans.first, trans.second.first, trans.second.second);
			}
		}

		if (image_barriers.size() > 0) {
			constexpr bool _separate_barriers = false;
			if constexpr (is_debugging && _separate_barriers) {
				for (const auto &b : image_barriers) {
					get_command_list().resource_barrier({ b }, {});
				}
				for (const auto &b : buffer_barriers) {
					get_command_list().resource_barrier({}, { b });
				}
			} else {
				get_command_list().resource_barrier(image_barriers, buffer_barriers);
			}
		}
	}

	gpu::constant_buffer_view context::_execution_context::stage_immediate_constant_buffer(
		std::span<const std::byte> data
	) {
		if (_immediate_constant_buffer_used + data.size() > immediate_constant_buffer_cache_size) {
			flush_immediate_constant_buffers();
		}

		if (!_immediate_constant_device_buffer) {
			_immediate_constant_device_buffer = _ctx._device.create_committed_buffer(
				immediate_constant_buffer_cache_size,
				_ctx._device_memory_index,
				gpu::buffer_usage::mask::copy_destination | gpu::buffer_usage::mask::read_only_buffer
			);
			_immediate_constant_upload_buffer = _ctx._device.create_committed_buffer(
				immediate_constant_buffer_cache_size,
				_ctx._upload_memory_index,
				gpu::buffer_usage::mask::copy_source
			);
			_immediate_constant_upload_buffer_ptr = static_cast<std::byte*>(
				_ctx._device.map_buffer(_immediate_constant_upload_buffer, 0, 0)
			);
		}

		gpu::constant_buffer_view result(
			_immediate_constant_device_buffer, _immediate_constant_buffer_used, data.size()
		);

		assert(_immediate_constant_buffer_used + data.size() <= immediate_constant_buffer_cache_size);
		std::memcpy(
			_immediate_constant_upload_buffer_ptr + _immediate_constant_buffer_used, data.data(), data.size()
		);
		_immediate_constant_buffer_used = memory::align_size(
			_immediate_constant_buffer_used + data.size(), _ctx._adapter_properties.constant_buffer_alignment
		);

		return result;
	}

	void context::_execution_context::flush_immediate_constant_buffers() {
		if (!_immediate_constant_upload_buffer) {
			return;
		}
		_ctx._device.unmap_buffer(_immediate_constant_upload_buffer, 0, _immediate_constant_buffer_used);

		// TODO we need to immediately submit the command list because other commands have already been recorded
		auto &cmd_list = _resources.record(_ctx._device.create_and_start_command_list(_ctx._transient_cmd_alloc));
		cmd_list.copy_buffer(
			_immediate_constant_upload_buffer, 0,
			_immediate_constant_device_buffer, 0,
			_immediate_constant_buffer_used
		);
		cmd_list.resource_barrier({}, {
			gpu::buffer_barrier(
				_immediate_constant_device_buffer,
				gpu::buffer_usage::copy_destination,
				gpu::buffer_usage::read_only_buffer
			),
		});
		cmd_list.finish();
		_ctx._queue.submit_command_lists({ &cmd_list }, nullptr);

		_resources.record(std::exchange(_immediate_constant_device_buffer, nullptr));
		_resources.record(std::exchange(_immediate_constant_upload_buffer, nullptr));
		_immediate_constant_upload_buffer = 0;
		_immediate_constant_upload_buffer_ptr = nullptr;
	}


	context context::create(
		gpu::context &ctx,
		const gpu::adapter_properties &adap_prop,
		gpu::device &dev,
		gpu::command_queue &queue
	) {
		return context(ctx, adap_prop, dev, queue);
	}

	context::~context() {
		wait_idle();
	}

	image2d_view context::request_image2d(
		std::u8string_view name, cvec2s size, std::uint32_t num_mips, gpu::format fmt,
		gpu::image_usage::mask usages
	) {
		gpu::image_tiling tiling = gpu::image_tiling::optimal;
		auto *surf = new _details::surface2d(
			size, num_mips, fmt, tiling, usages, _resource_index++, name
		);
		auto surf_ptr = std::shared_ptr<_details::surface2d>(surf, _details::context_managed_deleter(*this));
		return image2d_view(std::move(surf_ptr), fmt, gpu::mip_levels::all());
	}

	buffer context::request_buffer(
		std::u8string_view name, std::uint32_t size_bytes, gpu::buffer_usage::mask usages
	) {
		auto *buf = new _details::buffer(size_bytes, usages, name);
		auto buf_ptr = std::shared_ptr<_details::buffer>(buf, _details::context_managed_deleter(*this));
		return buffer(std::move(buf_ptr));
	}

	swap_chain context::request_swap_chain(
		std::u8string_view name, system::window &wnd,
		std::uint32_t num_images, std::span<const gpu::format> formats
	) {
		auto *chain = new _details::swap_chain(
			wnd, num_images, { formats.begin(), formats.end() }, name
		);
		auto chain_ptr = std::shared_ptr<_details::swap_chain>(chain, _details::context_managed_deleter(*this));
		return swap_chain(std::move(chain_ptr));
	}

	descriptor_array context::request_descriptor_array(
		std::u8string_view name, gpu::descriptor_type type, std::uint32_t capacity
	) {
		auto *arr = new _details::descriptor_array(type, capacity, name);
		auto arr_ptr = std::shared_ptr<_details::descriptor_array>(arr, _details::context_managed_deleter(*this));
		return descriptor_array(std::move(arr_ptr));
	}

	void context::upload_image(const image2d_view &target, const void *data, std::u8string_view description) {
		cvec2s image_size = target.get_size();
		std::uint32_t mip_index = target._mip_levels.minimum;
		image_size[0] >>= mip_index;
		image_size[1] >>= mip_index;

		const auto &format_props = gpu::format_properties::get(target.get_original_format());
		std::uint32_t bytes_per_pixel = format_props.bytes_per_pixel();

		auto staging_buffer = _device.create_committed_staging_buffer(
			image_size[0], image_size[1], target.get_original_format(),
			_upload_memory_index, gpu::buffer_usage::mask::copy_source
		);

		// copy to device
		auto *src = static_cast<const std::byte*>(data);
		auto *dst = static_cast<std::byte*>(_device.map_buffer(staging_buffer.data, 0, 0));
		for (std::uint32_t y = 0; y < image_size[1]; ++y) {
			std::memcpy(
				dst + y * staging_buffer.row_pitch.get_pitch_in_bytes(),
				src + y * image_size[0] * bytes_per_pixel,
				image_size[0] * bytes_per_pixel
			);
		}
		_device.unmap_buffer(staging_buffer.data, 0, staging_buffer.total_size);

		_commands.emplace_back(description).value.emplace<context_commands::upload_image>(
			std::move(staging_buffer), target
		);
	}

	void context::run_compute_shader(
		assets::handle<assets::shader> shader, cvec3<std::uint32_t> num_thread_groups,
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
			gpu::timeline_semaphore_synchronization(_batch_semaphore, _batch_index)
		};
		ectx.submit(_queue, gpu::queue_synchronization(nullptr, {}, signal_semaphores));

		_cleanup();
	}

	void context::wait_idle() {
		_device.wait_for_timeline_semaphore(_batch_semaphore, _batch_index);
		_cleanup();
	}

	void context::write_image_descriptors(
		descriptor_array &arr_handle, std::uint32_t first_index, std::span<const image2d_view> imgs
	) {
		auto &arr = *arr_handle._array;

		_maybe_initialize_descriptor_array(arr);

		for (std::size_t i = 0; i < imgs.size(); ++i) {
			auto descriptor_index = static_cast<std::uint32_t>(i + first_index);
			auto &cur_ref = arr.images[descriptor_index];
			if (auto *surf = cur_ref.image._surface) {
				auto old_index = cur_ref.reference_index;
				surf->array_references[old_index] = surf->array_references.back();
				surf->array_references.pop_back();
				auto new_ref = surf->array_references[old_index];
				new_ref.arr->images[new_ref.index].reference_index = old_index;
				cur_ref = nullptr;
				arr.has_descriptor_overwrites = true;
			}
			// update recorded image
			cur_ref.image = imgs[i];
			auto &new_surf = *cur_ref.image._surface;
			cur_ref.reference_index = static_cast<std::uint32_t>(new_surf.array_references.size());
			auto &new_ref = new_surf.array_references.emplace_back(nullptr);
			new_ref.arr = &arr;
			new_ref.index = descriptor_index;
			// stage the write
			arr.staged_transitions.emplace_back(descriptor_index);
			arr.staged_writes.emplace_back(descriptor_index);
		}
	}

	context::context(
		gpu::context &ctx,
		const gpu::adapter_properties &adap_prop,
		gpu::device &dev,
		gpu::command_queue &queue
	) :
		_device(dev), _context(ctx), _queue(queue),
		_cmd_alloc(_device.create_command_allocator()),
		_transient_cmd_alloc(_device.create_command_allocator()),
		_descriptor_pool(_device.create_descriptor_pool({
			gpu::descriptor_range::create(gpu::descriptor_type::acceleration_structure, 1024),
			gpu::descriptor_range::create(gpu::descriptor_type::constant_buffer, 1024),
			gpu::descriptor_range::create(gpu::descriptor_type::read_only_buffer, 1024),
			gpu::descriptor_range::create(gpu::descriptor_type::read_only_image, 1024),
			gpu::descriptor_range::create(gpu::descriptor_type::read_write_buffer, 1024),
			gpu::descriptor_range::create(gpu::descriptor_type::read_write_image, 1024),
			gpu::descriptor_range::create(gpu::descriptor_type::sampler, 1024),
		}, 1024)), // TODO proper numbers
		_empty_layout(_device.create_descriptor_set_layout({}, gpu::shader_stage::all)),
		_batch_semaphore(_device.create_timeline_semaphore(0)),
		_adapter_properties(adap_prop),
		_cache(_device),
		_thread(std::this_thread::get_id()) {

		const auto &memory_types = _device.enumerate_memory_types();
		for (const auto &type : memory_types) {
			if (
				_device_memory_index == gpu::memory_type_index::invalid &&
				!is_empty(type.second & gpu::memory_properties::device_local)
			) {
				_device_memory_index = type.first;
			}
			if (
				_upload_memory_index == gpu::memory_type_index::invalid &&
				!is_empty(type.second & gpu::memory_properties::host_visible)
			) {
				_upload_memory_index = type.first;
			}
		}
	}

	void context::_maybe_create_image(_details::surface2d &surf) {
		if (!surf.image) {
			// create resource if it's not initialized
			surf.image = _device.create_committed_image2d(
				surf.size[0], surf.size[1], 1, surf.num_mips,
				surf.format, surf.tiling, surf.usages
			);
			if constexpr (should_register_debug_names) {
				_device.set_debug_name(surf.image, surf.name);
			}
		}
	}

	gpu::image2d_view context::_create_image_view(const recorded_resources::image2d_view &view) {
		_maybe_create_image(*view._surface);
		return _device.create_image2d_view_from(view._surface->image, view._view_format, view._mip_levels);
	}

	gpu::image2d_view &context::_request_image_view(
		_execution_context &ectx, const recorded_resources::image2d_view &view
	) {
		return ectx.record(_create_image_view(view));
	}

	gpu::image2d_view &context::_request_image_view(
		_execution_context &ectx, const recorded_resources::swap_chain &chain
	) {
		auto &chain_data = *chain._swap_chain;

		if (chain_data.next_image_index == _details::swap_chain::invalid_image_index) {
			gpu::back_buffer_info back_buffer = nullptr;
			if (chain_data.chain) {
				back_buffer = _device.acquire_back_buffer(chain_data.chain);
				// wait until the buffer becomes available
				// we must always immediate wait after acquiring to make vulkan happy
				if (back_buffer.on_presented) {
					_device.wait_for_fence(*back_buffer.on_presented);
					_device.reset_fence(*back_buffer.on_presented);
				}
			}
			if (chain_data.desired_size != chain_data.current_size || back_buffer.status != gpu::swap_chain_status::ok) {
				// wait until everything's done
				{
					auto fence = _device.create_fence(gpu::synchronization_state::unset);
					ectx.submit(_queue, &fence);
					_device.wait_for_fence(fence);
				}
				chain_data.images.clear();
				// create or resize the swap chain
				chain_data.current_size = chain_data.desired_size;
				if (chain_data.chain && back_buffer.status == gpu::swap_chain_status::ok) {
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
				chain_data.current_usages = std::vector<gpu::image_usage>(
					chain_data.num_images, gpu::image_usage::initial
				);
				// update fences
				for (auto &fence : chain_data.fences) {
					ectx.record(std::move(fence));
				}
				chain_data.fences.clear();
				for (std::size_t i = 0; i < chain_data.num_images; ++i) {
					chain_data.fences.emplace_back(_device.create_fence(gpu::synchronization_state::unset));
				}
				chain_data.chain.update_synchronization_primitives(chain_data.fences);
				// re-acquire back buffer
				back_buffer = _device.acquire_back_buffer(chain_data.chain);
				// wait until the buffer becomes available
				if (back_buffer.on_presented) {
					_device.wait_for_fence(*back_buffer.on_presented);
					_device.reset_fence(*back_buffer.on_presented);
				}
				assert(back_buffer.status == gpu::swap_chain_status::ok);
			}
			chain_data.next_image_index = static_cast<std::uint32_t>(back_buffer.index);
		}

		return ectx.record(_device.create_image2d_view_from(
			chain_data.images[chain_data.next_image_index],
			chain_data.current_format,
			gpu::mip_levels::only_highest()
		));
	}

	void context::_maybe_initialize_descriptor_array(_details::descriptor_array &arr) {
		if (!arr.set) {
			const auto &layout = _cache.get_descriptor_set_layout(arr.get_layout_key());
			arr.set = _device.create_descriptor_set(_descriptor_pool, layout, arr.capacity);
			arr.images.resize(arr.capacity, nullptr);
		}
	}

	void context::_create_descriptor_binding(
		_execution_context &ectx, gpu::descriptor_set &set,
		const gpu::descriptor_set_layout &layout, std::uint32_t reg,
		const descriptor_resource::image2d &img
	) {
		auto &img_view = _request_image_view(ectx, img.view);
		gpu::image_usage target_usage = uninitialized;
		switch (img.binding_type) {
		case image_binding_type::read_only:
			_device.write_descriptor_set_read_only_images(set, layout, reg, { &img_view });
			target_usage = gpu::image_usage::read_only_texture;
			break;
		case image_binding_type::read_write:
			_device.write_descriptor_set_read_write_images(set, layout, reg, { &img_view });
			target_usage = gpu::image_usage::read_write_color_texture;
			break;
		}
		ectx.stage_transition(*img.view._surface, img.view._mip_levels, target_usage);
	}

	void context::_create_descriptor_binding(
		_execution_context &ectx, gpu::descriptor_set &set,
		const gpu::descriptor_set_layout &layout, std::uint32_t reg,
		const descriptor_resource::swap_chain_image &chain
	) {
		auto &img_view = _request_image_view(ectx, chain.image);
		_device.write_descriptor_set_read_write_images(set, layout, reg, { &img_view });
		ectx.stage_transition(*chain.image._swap_chain, gpu::image_usage::read_write_color_texture);
	}

	void context::_create_descriptor_binding(
		_execution_context &ectx, gpu::descriptor_set &set,
		const gpu::descriptor_set_layout &layout, std::uint32_t reg,
		const descriptor_resource::immediate_constant_buffer &cbuf
	) {
		_device.write_descriptor_set_constant_buffers(
			set, layout, reg, { ectx.stage_immediate_constant_buffer(cbuf.data), }
		);
	}

	void context::_create_descriptor_binding(
		_execution_context &ectx, gpu::descriptor_set &set,
		const gpu::descriptor_set_layout &layout, std::uint32_t reg,
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

	std::tuple<
		cache_keys::descriptor_set_layout,
		const gpu::descriptor_set_layout&,
		gpu::descriptor_set&
	> context::_use_descriptor_set(
		_execution_context &ectx, const resource_set_binding::descriptor_bindings &bindings
	) {
		cache_keys::descriptor_set_layout key = nullptr;
		for (const auto &b : bindings.bindings) {
			auto type = std::visit([this](const auto &rsrc) {
				return _get_descriptor_type(rsrc);
			}, b.resource);
			key.ranges.emplace_back(gpu::descriptor_range_binding::create(type, 1, b.register_index));
		}
		key.consolidate();

		auto &layout = _cache.get_descriptor_set_layout(key);
		auto &set = ectx.record(_device.create_descriptor_set(_descriptor_pool, layout));
		for (const auto &b : bindings.bindings) {
			std::visit([&, this](const auto &rsrc) {
				_create_descriptor_binding(ectx, set, layout, b.register_index, rsrc);
			}, b.resource);
		}
		return { std::move(key), layout, set };
	}

	std::tuple<
		cache_keys::descriptor_set_layout,
		const gpu::descriptor_set_layout&,
		gpu::descriptor_set&
	> context::_use_descriptor_set(
		_execution_context &ectx, const recorded_resources::descriptor_array &arr
	) {
		auto key = arr._array->get_layout_key();
		auto &layout = _cache.get_descriptor_set_layout(key);

		ectx.stage_all_transitions(*arr._array);
		// write descriptors
		if (arr._array->has_descriptor_overwrites) {
			// TODO wait until we've finished using this descriptor
			arr._array->has_descriptor_overwrites = false;
		}
		if (!arr._array->staged_writes.empty()) {
			auto writes = std::exchange(arr._array->staged_writes, {});
			std::sort(writes.begin(), writes.end());
			writes.erase(std::unique(writes.begin(), writes.end()), writes.end());
			auto write_func = gpu::device::get_write_image_descriptor_function(arr._array->type);
			assert(write_func);
			for (auto index : writes) {
				const auto &img = arr._array->images[index].image;
				// TODO batch writes
				gpu::image2d_view *view = nullptr;
				if (img) {
					view = &_request_image_view(ectx, img);
				}
				std::initializer_list<const gpu::image_view*> views = { view };
				(_device.*write_func)(arr._array->set, layout, index, views);
			}
		}

		return { std::move(key), layout, arr._array->set };
	}

	std::tuple<
		cache_keys::pipeline_resources,
		const gpu::pipeline_resources&,
		std::vector<context::_descriptor_set_info>
	> context::_check_and_create_descriptor_set_bindings(
		_execution_context &ectx, const all_resource_bindings &resources
	) {
		std::vector<_descriptor_set_info> sets;
		cache_keys::pipeline_resources key;

		for (const auto &set : resources.sets) {
			auto &&[layout_key, layout, desc_set] = std::visit([&, this](const auto &bindings) {
				return _use_descriptor_set(ectx, bindings);
			}, set.bindings);

			sets.emplace_back(desc_set, set.space);
			key.sets.emplace_back(std::move(layout_key), set.space);
		}
		key.sort();
		auto &rsrc = _cache.get_pipeline_resources(key);

		return { std::move(key), rsrc, sets };
	}

	void context::_bind_descriptor_sets(
		_execution_context &ectx, const gpu::pipeline_resources &rsrc,
		std::vector<_descriptor_set_info> sets, _bind_point pt
	) {
		// organize descriptor sets by register space
		std::sort(
			sets.begin(), sets.end(),
			[](const _descriptor_set_info &lhs, const _descriptor_set_info &rhs) {
				return lhs.space < rhs.space;
			}
		);
		std::vector<gpu::descriptor_set*> set_ptrs;
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

	context::_pass_command_data context::_preprocess_command(
		_execution_context &ectx,
		const gpu::frame_buffer_layout &fb_layout,
		const pass_commands::draw_instanced &cmd
	) {
		context::_pass_command_data result = nullptr;
		auto &&[rsrc_key, rsrc, sets] = _check_and_create_descriptor_set_bindings(ectx, cmd.resource_bindings);
		result.descriptor_sets = std::move(sets);
		result.resources = &rsrc;

		std::vector<cache_keys::graphics_pipeline::input_buffer_layout> input_layouts;
		for (const auto &input : cmd.inputs) {
			input_layouts.emplace_back(input.elements, input.stride, input.buffer_index, input.input_rate);
		}

		cache_keys::graphics_pipeline key = nullptr;
		key.pipeline_resources      = std::move(rsrc_key);
		key.input_buffers           = std::move(input_layouts); // TODO;
		key.color_rt_formats        =
			{ fb_layout.color_render_target_formats.begin(), fb_layout.color_render_target_formats.end() };
		key.dpeth_stencil_rt_format = fb_layout.depth_stencil_render_target_format;
		key.vertex_shader           = cmd.vertex_shader;
		key.pixel_shader            = cmd.pixel_shader;
		key.pipeline_state          = cmd.state;
		key.topology                = cmd.topology;

		result.pipeline_state = &_cache.get_graphics_pipeline_state(key);
		return result;
	}

	void context::_handle_pass_command(
		_execution_context &ectx, _pass_command_data data, const pass_commands::draw_instanced &cmd
	) {
		std::vector<gpu::vertex_buffer> bufs;
		for (const auto &input : cmd.inputs) {
			if (input.buffer_index >= bufs.size()) {
				bufs.resize(input.buffer_index + 1, nullptr);
			}
			bufs[input.buffer_index] =
				gpu::vertex_buffer(input.buffer.get().value.data, input.offset, input.stride);
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

	void context::_handle_command(_execution_context &ectx, const context_commands::upload_image &img) {
		recorded_resources::image2d_view dest = img.destination;
		dest._mip_levels.num_levels = 1;
		_maybe_create_image(*dest._surface);

		cvec2s size = dest._surface->size;
		auto mip_level = dest._mip_levels.minimum;
		size[0] >>= mip_level;
		size[1] >>= mip_level;
		ectx.stage_transition(*dest._surface, dest._mip_levels, gpu::image_usage::copy_destination);
		ectx.flush_transitions();
		ectx.get_command_list().copy_buffer_to_image(
			img.source.data, 0, img.source.row_pitch, aab2s::create_from_min_max(zero, size),
			dest._surface->image, gpu::subresource_index::create_color(mip_level, 0), zero
		);
	}

	void context::_handle_command(_execution_context &ectx, const context_commands::dispatch_compute &cmd) {
		// create descriptor bindings
		auto &&[rsrc_key, pipeline_resources, sets] = _check_and_create_descriptor_set_bindings(ectx, cmd.resources);
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
		std::vector<gpu::color_render_target_access> color_rt_access;
		std::vector<gpu::image2d_view*> color_rts;
		std::vector<gpu::format> color_rt_formats;
		gpu::format ds_rt_format = gpu::format::none;

		// create frame buffer
		for (const auto &rt : cmd.color_render_targets) {
			if (std::holds_alternative<recorded_resources::image2d_view>(rt.view)) {
				auto img = std::get<recorded_resources::image2d_view>(rt.view).highest_mip_with_warning();
				color_rts.emplace_back(&_request_image_view(ectx, img));
				color_rt_formats.emplace_back(img._view_format);
				ectx.stage_transition(*img._surface, img._mip_levels, gpu::image_usage::color_render_target);
				assert(img._surface->size == cmd.render_target_size);
			} else if (std::holds_alternative<recorded_resources::swap_chain>(rt.view)) {
				auto &chain = std::get<recorded_resources::swap_chain>(rt.view);
				color_rts.emplace_back(&_request_image_view(ectx, chain));
				color_rt_formats.emplace_back(chain._swap_chain->current_format);
				ectx.stage_transition(*chain._swap_chain, gpu::image_usage::color_render_target);
				assert(chain._swap_chain->current_size == cmd.render_target_size);
			} else {
				assert(false); // unhandled
			}
			color_rt_access.emplace_back(rt.access);
		}
		gpu::image2d_view *depth_stencil_rt = nullptr;
		if (cmd.depth_stencil_target.view) {
			assert(cmd.depth_stencil_target.view._surface->size == cmd.render_target_size);
			auto img = cmd.depth_stencil_target.view.highest_mip_with_warning();
			depth_stencil_rt = &_request_image_view(ectx, img);
			ds_rt_format = img._view_format;
			ectx.stage_transition(
				*img._surface, img._mip_levels, gpu::image_usage::depth_stencil_render_target
			);
		}
		auto &frame_buffer = ectx.create_frame_buffer(color_rts, depth_stencil_rt, cmd.render_target_size);

		// frame buffer access
		gpu::frame_buffer_access access = nullptr;
		access.color_render_targets  = color_rt_access;
		access.depth_render_target   = cmd.depth_stencil_target.depth_access;
		access.stencil_render_target = cmd.depth_stencil_target.stencil_access;

		gpu::frame_buffer_layout fb_layout(color_rt_formats, ds_rt_format);

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
		cmd_list.set_viewports({ gpu::viewport::create(
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
			ectx.stage_transition(*cmd.target._swap_chain, gpu::image_usage::present);
			ectx.flush_transitions();

			ectx.submit(_queue, nullptr);
			if (_queue.present(cmd.target._swap_chain->chain) != gpu::swap_chain_status::ok) {
				// TODO?
			}
		}
		// do this last, since handling transitions needs the index of the image
		cmd.target._swap_chain->next_image_index = _details::swap_chain::invalid_image_index;
	}

	void context::_cleanup() {
		auto first_batch = static_cast<std::uint32_t>(_batch_index + 1 - _all_resources.size());
		std::uint64_t finished_batch = _device.query_timeline_semaphore(_batch_semaphore);
		while (first_batch <= finished_batch) {
			const auto &batch = _all_resources.front();
			for (const auto &surf : batch.surface2d_meta) {
				for (const auto &array_ref : surf->array_references) {
					if (array_ref.arr->set) {
						std::initializer_list<const gpu::image_view*> images = { nullptr };
						(_device.*_device.get_write_image_descriptor_function(array_ref.arr->type))(
							array_ref.arr->set,
							_cache.get_descriptor_set_layout(array_ref.arr->get_layout_key()),
							array_ref.index,
							images
						);
					}
				}
			}
			_all_resources.pop_front();
			++first_batch;
		}
	}
}
