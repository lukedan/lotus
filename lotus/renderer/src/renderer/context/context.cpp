#include "lotus/renderer/context/context.h"

/// \file
/// Implementation of scene loading and rendering.

#include <unordered_map>

#include "lotus/logging.h"
#include "lotus/memory/stack_allocator.h"

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
		resources.maybe_consolidate();
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
		auto resource_bindings = all_resource_bindings::merge(
			mat.data->create_resource_bindings(), additional_resources
		);
		resource_bindings.maybe_consolidate();
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


	gpu::device &context::device_access::get(context &ctx) {
		return ctx._device;
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

	pool context::request_pool(
		std::u8string_view name, gpu::memory_type_index memory_type, std::uint32_t chunk_size
	) {
		auto *p = new _details::pool(
			[dev = &_device, memory_type](std::size_t sz) {
				return dev->allocate_memory(sz, memory_type);
			},
			chunk_size, _allocate_resource_id(), name
		);
		auto pool_ptr = std::shared_ptr<_details::pool>(p, _details::context_managed_deleter(*this));
		return pool(std::move(pool_ptr));
	}

	image2d_view context::request_image2d(
		std::u8string_view name, cvec2s size, std::uint32_t num_mips, gpu::format fmt,
		gpu::image_usage_mask usages, const pool &p
	) {
		gpu::image_tiling tiling = gpu::image_tiling::optimal;
		auto *surf = new _details::image2d(
			size, num_mips, fmt, tiling, usages, p._ptr, _allocate_resource_id(), name
		);
		auto surf_ptr = std::shared_ptr<_details::image2d>(surf, _details::context_managed_deleter(*this));
		return image2d_view(std::move(surf_ptr), fmt, gpu::mip_levels::all());
	}

	buffer context::request_buffer(
		std::u8string_view name, std::uint32_t size_bytes, gpu::buffer_usage_mask usages, const pool &p
	) {
		auto *buf = new _details::buffer(size_bytes, usages, p._ptr, _allocate_resource_id(), name);
		auto buf_ptr = std::shared_ptr<_details::buffer>(buf, _details::context_managed_deleter(*this));
		return buffer(std::move(buf_ptr));
	}

	swap_chain context::request_swap_chain(
		std::u8string_view name, system::window &wnd,
		std::uint32_t num_images, std::span<const gpu::format> formats
	) {
		auto *chain = new _details::swap_chain(
			wnd, num_images, { formats.begin(), formats.end() }, _allocate_resource_id(), name
		);
		auto chain_ptr = std::shared_ptr<_details::swap_chain>(chain, _details::context_managed_deleter(*this));
		return swap_chain(std::move(chain_ptr));
	}

	image_descriptor_array context::request_image_descriptor_array(
		std::u8string_view name, gpu::descriptor_type type, std::uint32_t capacity
	) {
		auto *arr = new _details::image_descriptor_array(type, capacity, _allocate_resource_id(), name);
		auto arr_ptr = std::shared_ptr<_details::image_descriptor_array>(
			arr, _details::context_managed_deleter(*this)
		);
		return image_descriptor_array(std::move(arr_ptr));
	}

	buffer_descriptor_array context::request_buffer_descriptor_array(
		std::u8string_view name, gpu::descriptor_type type, std::uint32_t capacity
	) {
		auto *arr = new _details::buffer_descriptor_array(type, capacity, _allocate_resource_id(), name);
		auto arr_ptr = std::shared_ptr<_details::buffer_descriptor_array>(
			arr, _details::context_managed_deleter(*this)
		);
		return buffer_descriptor_array(std::move(arr_ptr));
	}

	blas context::request_blas(
		std::u8string_view name, std::span<const geometry_buffers_view> geometries, const pool &p
	) {
		auto *blas_ptr = new _details::blas(
			std::vector(geometries.begin(), geometries.end()), p._ptr, _allocate_resource_id(), name
		);
		auto ptr = std::shared_ptr<_details::blas>(blas_ptr, _details::context_managed_deleter(*this));
		return blas(std::move(ptr));
	}

	tlas context::request_tlas(std::u8string_view name, std::span<const blas_reference> blases, const pool &p) {
		std::vector<gpu::instance_description> instances;
		std::vector<std::shared_ptr<_details::blas>> references;
		for (const auto &b : blases) {
			_maybe_initialize_blas(*b.acceleration_structure._ptr);
			instances.emplace_back(_device.get_bottom_level_acceleration_structure_description(
				b.acceleration_structure._ptr->handle, b.transform, b.id, b.mask, b.hit_group_offset, b.flags
			));
			references.emplace_back(b.acceleration_structure._ptr);
		}
		auto *tlas_ptr = new _details::tlas(
			std::move(instances), std::move(references), p._ptr, _allocate_resource_id(), name
		);
		auto ptr = std::shared_ptr<_details::tlas>(tlas_ptr, _details::context_managed_deleter(*this));
		return tlas(std::move(ptr));
	}

	cached_descriptor_set context::create_cached_descriptor_set(
		std::u8string_view name, const resource_set_binding::descriptors &bindings
	) {
		auto key = _get_descriptor_set_layout_key(bindings);
		auto &layout = _cache.get_descriptor_set_layout(key);
		auto set = _device.create_descriptor_set(_descriptor_pool, layout);
		auto *set_ptr = new _details::cached_descriptor_set(
			std::move(set), std::move(key.ranges), layout, _allocate_resource_id(), name
		);
		for (const auto &binding : bindings.bindings) {
			std::visit([&](const auto &rsrc) {
				_create_descriptor_binding_cached(*set_ptr, binding.register_index, rsrc);
			}, binding.resource);
		}
		return cached_descriptor_set(std::shared_ptr<_details::cached_descriptor_set>(
			set_ptr, _details::context_managed_deleter(*this)
		));
	}

	void context::upload_image(const image2d_view &target, const std::byte *data, std::u8string_view description) {
		cvec2s image_size = target.get_size();
		std::uint32_t mip_index = target._mip_levels.minimum;
		image_size[0] >>= mip_index;
		image_size[1] >>= mip_index;

		const auto &format_props = gpu::format_properties::get(target.get_original_format());
		std::uint32_t bytes_per_fragment = format_props.bytes_per_fragment;

		auto staging_buffer = _device.create_committed_staging_buffer(
			image_size[0], image_size[1], target.get_original_format(),
			_upload_memory_index, gpu::buffer_usage_mask::copy_source
		);
		cvec2s frag_size = format_props.fragment_size.into<std::size_t>();
		// TODO we can't do this - Vulkan doesn't play well with it
		cvec2s num_fragments = mat::memberwise_divide(image_size + frag_size - cvec2s(1, 1), frag_size);

		// copy to device
		auto *src = data;
		auto *dst = _device.map_buffer(staging_buffer.data, 0, 0);
		for (std::uint32_t y = 0; y < num_fragments[1]; ++y) {
			std::memcpy(
				dst + y * staging_buffer.meta.get_pitch_in_bytes(),
				src + y * num_fragments[0] * bytes_per_fragment,
				num_fragments[0] * bytes_per_fragment
			);
		}
		_device.unmap_buffer(staging_buffer.data, 0, staging_buffer.total_size);

		_commands.emplace_back(description).value.emplace<context_commands::upload_image>(
			std::move(staging_buffer), target
		);
	}

	void context::upload_buffer(
		const buffer &target, std::span<const std::byte> data, std::uint32_t offset,
		std::u8string_view description
	) {
		if (!_uploads) {
			_uploads = execution::upload_buffers(*this, [this](std::size_t size) -> gpu::buffer& {
				return _deferred_delete_resources.buffers.emplace_back(_device.create_committed_buffer(
					size, _upload_memory_index, gpu::buffer_usage_mask::copy_source
				));
			});
		}
		auto staged = _uploads.stage(data, 1);
		_commands.emplace_back(description).value.emplace<context_commands::upload_buffer>(
			*staged.buffer, staged.offset, staged.type, target, offset, static_cast<std::uint32_t>(data.size())
		);
	}

	void context::build_blas(blas &b, std::u8string_view description) {
		_commands.emplace_back(description).value.emplace<context_commands::build_blas>(b);
	}

	void context::build_tlas(tlas &t, std::u8string_view description) {
		_commands.emplace_back(description).value.emplace<context_commands::build_tlas>(t);
	}

	void context::trace_rays(
		std::span<const shader_function> hit_group_shaders,
		std::span<const gpu::hit_shader_group> hit_groups,
		std::span<const shader_function> general_shaders,
		std::uint32_t raygen_shader_index,
		std::span<const std::uint32_t> miss_shader_indices,
		std::span<const std::uint32_t> shader_groups,
		std::uint32_t max_recursion_depth,
		std::uint32_t max_payload_size,
		std::uint32_t max_attribute_size,
		cvec3u32 num_threads,
		all_resource_bindings resources,
		std::u8string_view description
	) {
		resources.maybe_consolidate();
		_commands.emplace_back(description).value.emplace<context_commands::trace_rays>(
			std::move(resources),
			std::vector(hit_group_shaders.begin(), hit_group_shaders.end()),
			std::vector(hit_groups.begin(), hit_groups.end()),
			std::vector(general_shaders.begin(), general_shaders.end()),
			raygen_shader_index,
			std::vector(miss_shader_indices.begin(), miss_shader_indices.end()),
			std::vector(shader_groups.begin(), shader_groups.end()),
			max_recursion_depth,
			max_payload_size,
			max_attribute_size,
			num_threads
		);
	}

	void context::run_compute_shader(
		assets::handle<assets::shader> shader, cvec3<std::uint32_t> num_thread_groups,
		all_resource_bindings resources, std::u8string_view description
	) {
		resources.maybe_consolidate();
		_commands.emplace_back(description).value.emplace<context_commands::dispatch_compute>(
			std::move(resources), std::move(shader), num_thread_groups
		);
	}

	void context::run_compute_shader_with_thread_dimensions(
		assets::handle<assets::shader> shader, cvec3<std::uint32_t> num_threads, all_resource_bindings resources,
		std::u8string_view description
	) {
		auto thread_group_size = shader->reflection.get_thread_group_size().into<std::uint32_t>();
		cvec3u32 groups = mat::memberwise_divide(
			num_threads + thread_group_size - cvec3u32(1, 1, 1), thread_group_size
		);
		context::run_compute_shader(std::move(shader), groups, std::move(resources), description);
	}

	context::pass context::begin_pass(
		std::vector<image2d_color> color_rts, image2d_depth_stencil ds_rt, cvec2s sz,
		std::u8string_view description
	) {
		return context::pass(*this, std::move(color_rts), std::move(ds_rt), sz, description);
	}

	void context::present(swap_chain sc, std::u8string_view description) {
		_commands.emplace_back(description).value.emplace<context_commands::present>(std::move(sc));
	}

	void context::flush() {
		crash_if(std::this_thread::get_id() != _thread);

		++_batch_index;
		_all_resources.emplace_back(std::exchange(_deferred_delete_resources, {}));

		if (_uploads) {
			_uploads.flush();
		}

		{
			auto ectx = execution::context(*this, _all_resources.back());

			auto cmds = std::exchange(_commands, {});
			for (auto &cmd : cmds) {
				std::visit([&, this](auto &cmd_val) {
					_handle_command(ectx, cmd_val);
				}, cmd.value);

				constexpr bool _debug_separate_commands = false;
				if constexpr (_debug_separate_commands) {
					gpu::fence f = _device.create_fence(gpu::synchronization_state::unset);
					ectx.submit(_queue, &f);
					_device.wait_for_fence(f);
				}
			}
			ectx.flush_transitions();

			auto signal_semaphores = {
				gpu::timeline_semaphore_synchronization(_batch_semaphore, _batch_index)
			};
			ectx.submit(_queue, gpu::queue_synchronization(nullptr, {}, signal_semaphores));
		}

		_cleanup();
	}

	void context::wait_idle() {
		_device.wait_for_timeline_semaphore(_batch_semaphore, _batch_index);
		_cleanup();
	}

	void context::write_image_descriptors(
		image_descriptor_array &arr_handle, std::uint32_t first_index, std::span<const image2d_view> imgs
	) {
		auto &arr = *arr_handle._ptr;
		_maybe_initialize_descriptor_array(arr);
		for (std::size_t i = 0; i < imgs.size(); ++i) {
			_write_one_descriptor_array_element<&recorded_resources::image2d_view::_image>(
				arr, recorded_resources::image2d_view(imgs[i]), static_cast<std::uint32_t>(i + first_index)
			);
		}
	}

	void context::write_buffer_descriptors(
		buffer_descriptor_array &arr_handle, std::uint32_t first_index, std::span<const structured_buffer_view> bufs
	) {
		auto &arr = *arr_handle._ptr;
		_maybe_initialize_descriptor_array(arr);
		for (std::size_t i = 0; i < bufs.size(); ++i) {
			_write_one_descriptor_array_element<&recorded_resources::structured_buffer_view::_buffer>(
				arr, recorded_resources::structured_buffer_view(bufs[i]), static_cast<std::uint32_t>(i + first_index)
			);
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
			gpu::descriptor_range::create(gpu::descriptor_type::acceleration_structure, 10240),
			gpu::descriptor_range::create(gpu::descriptor_type::constant_buffer, 10240),
			gpu::descriptor_range::create(gpu::descriptor_type::read_only_buffer, 10240),
			gpu::descriptor_range::create(gpu::descriptor_type::read_only_image, 10240),
			gpu::descriptor_range::create(gpu::descriptor_type::read_write_buffer, 10240),
			gpu::descriptor_range::create(gpu::descriptor_type::read_write_image, 10240),
			gpu::descriptor_range::create(gpu::descriptor_type::sampler, 1024),
		}, 10240)), // TODO proper numbers
		_batch_semaphore(_device.create_timeline_semaphore(0)),
		_adapter_properties(adap_prop),
		_cache(_device),
		_thread(std::this_thread::get_id()),
		_uploads(nullptr) {

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

	void context::_maybe_create_image(_details::image2d &surf) {
		if (!surf.image) {
			// create resource if it's not initialized
			if (surf.memory_pool) {
				memory::size_alignment size_align = _device.get_image_memory_requirements(
					surf.size[0], surf.size[1], 1, surf.num_mips, surf.format, surf.tiling, surf.usages
				);
				surf.memory = surf.memory_pool->allocate(size_align);
				auto &&[blk, off] = surf.memory_pool->get_memory_and_offset(surf.memory);
				surf.image = _device.create_placed_image2d(
					surf.size[0], surf.size[1], 1, surf.num_mips, surf.format, surf.tiling, surf.usages, blk, off
				);
			} else {
				surf.image = _device.create_committed_image2d(
					surf.size[0], surf.size[1], 1, surf.num_mips,
					surf.format, surf.tiling, surf.usages
				);
			}

			if constexpr (should_register_debug_names) {
				_device.set_debug_name(surf.image, surf.name);
			}
		}
	}

	void context::_maybe_create_buffer(_details::buffer &buf) {
		if (!buf.data) {
			if (buf.memory_pool) {
				memory::size_alignment size_align = _device.get_buffer_memory_requirements(buf.size, buf.usages);
				buf.memory = buf.memory_pool->allocate(size_align);
				auto &&[blk, off] = buf.memory_pool->get_memory_and_offset(buf.memory);
				buf.data = _device.create_placed_buffer(buf.size, buf.usages, blk, off);
			} else {
				buf.data = _device.create_committed_buffer(buf.size, _device_memory_index, buf.usages);
			}

			if constexpr (should_register_debug_names) {
				_device.set_debug_name(buf.data, buf.name);
			}
		}
	}

	gpu::image2d_view context::_create_image_view(const recorded_resources::image2d_view &view) {
		_maybe_create_image(*view._image);
		auto result = _device.create_image2d_view_from(view._image->image, view._view_format, view._mip_levels);
		_device.set_debug_name(result, view._image->name);
		return result;
	}

	gpu::image2d_view &context::_request_image_view(
		execution::context &ectx, const recorded_resources::image2d_view &view
	) {
		return ectx.record(_create_image_view(view));
	}

	gpu::image2d_view &context::_request_image_view(
		execution::context &ectx, const recorded_resources::swap_chain &chain
	) {
		auto &chain_data = *chain._ptr;

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
				// clean up any references to the old swap chain images
				_cleanup();
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
				chain_data.current_usages = std::vector<_details::image_access>(
					chain_data.num_images, _details::image_access::initial()
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
				crash_if(back_buffer.status != gpu::swap_chain_status::ok);
			}
			chain_data.next_image_index = static_cast<std::uint32_t>(back_buffer.index);
		}

		return ectx.record(_device.create_image2d_view_from(
			chain_data.images[chain_data.next_image_index],
			chain_data.current_format,
			gpu::mip_levels::only_highest()
		));
	}

	void context::_maybe_initialize_blas(_details::blas &b) {
		if (!b.handle) {
			std::vector<gpu::raytracing_geometry_view> input_geom;
			for (const auto &geom : b.input) {
				_maybe_create_buffer(*geom.vertex_data._ptr);
				if (geom.index_data._ptr) {
					_maybe_create_buffer(*geom.index_data._ptr);
				}
				input_geom.emplace_back(
					gpu::vertex_buffer_view(
						geom.vertex_data._ptr->data,
						geom.vertex_format, geom.vertex_offset, geom.vertex_stride, geom.vertex_count
					),
					geom.index_data ? gpu::index_buffer_view(
						geom.index_data._ptr->data,
						geom.index_format, geom.index_offset, geom.index_count
					) : nullptr,
					geom.flags
				);
			}
			b.geometry    = _device.create_bottom_level_acceleration_structure_geometry(input_geom);
			b.build_sizes = _device.get_bottom_level_acceleration_structure_build_sizes(b.geometry);
			// TODO better name
			b.memory      = request_buffer(
				u8"BLAS memory", static_cast<std::uint32_t>(b.build_sizes.acceleration_structure_size),
				gpu::buffer_usage_mask::acceleration_structure, pool(b.memory_pool)
			)._ptr;
			_maybe_create_buffer(*b.memory);
			b.handle      = _device.create_bottom_level_acceleration_structure(
				b.memory->data, 0, b.build_sizes.acceleration_structure_size
			);
		}
	}

	void context::_maybe_initialize_tlas(_details::tlas &t) {
		if (!t.handle) {
			t.input_data = _device.create_committed_buffer(
				sizeof(gpu::instance_description) * t.input.size(), _device_memory_index,
				gpu::buffer_usage_mask::copy_destination | gpu::buffer_usage_mask::acceleration_structure_build_input
			);
			t.build_sizes = _device.get_top_level_acceleration_structure_build_sizes(
				t.input_data, 0, t.input.size()
			);
			// TODO better name
			t.memory = request_buffer(
				t.name, static_cast<std::uint32_t>(t.build_sizes.acceleration_structure_size),
				gpu::buffer_usage_mask::acceleration_structure, pool(t.memory_pool)
			)._ptr;
			_maybe_create_buffer(*t.memory);
			t.handle = _device.create_top_level_acceleration_structure(
				t.memory->data, 0, t.build_sizes.acceleration_structure_size
			);
		}
	}

	void context::_maybe_copy_tlas_build_input(execution::context &ectx, _details::tlas &t) {
		_maybe_initialize_tlas(t);

		if (!t.input_copied) { // upload data
			std::size_t input_size = sizeof(gpu::instance_description) * t.input.size();

			if (!t.input_data) { // the input buffer has been manually freed - allocate another
				t.input_data = _device.create_committed_buffer(
					input_size, _device_memory_index,
					gpu::buffer_usage_mask::copy_destination |
					gpu::buffer_usage_mask::acceleration_structure_build_input
				);
			}

			auto &upload_buf = ectx.create_buffer(
				input_size, _upload_memory_index, gpu::buffer_usage_mask::copy_source
			);
			{
				std::byte *ptr = _device.map_buffer(upload_buf, 0, 0);
				std::memcpy(ptr, t.input.data(), input_size);
				_device.unmap_buffer(upload_buf, 0, input_size);
			}
			ectx.transitions.stage_transition(
				upload_buf,
				{ gpu::synchronization_point_mask::cpu_access, gpu::buffer_access_mask::cpu_write },
				{ gpu::synchronization_point_mask::copy, gpu::buffer_access_mask::copy_source }
			);
			ectx.flush_transitions();
			ectx.get_command_list().copy_buffer(upload_buf, 0, t.input_data, 0, input_size);
			ectx.transitions.stage_transition(
				t.input_data,
				{ gpu::synchronization_point_mask::copy, gpu::buffer_access_mask::copy_destination },
				{
					gpu::synchronization_point_mask::acceleration_structure_build,
					gpu::buffer_access_mask::acceleration_structure_build_input
				}
			);
			t.input_copied = true;
		}
	}

	void context::_create_descriptor_binding_impl(
		execution::transition_buffer &transitions, gpu::descriptor_set &set,
		const gpu::descriptor_set_layout &layout, std::uint32_t reg,
		const descriptor_resource::image2d &img, const gpu::image2d_view &img_view
	) {
		_details::image_access target_access = uninitialized;
		switch (img.binding_type) {
		case image_binding_type::read_only:
			_device.write_descriptor_set_read_only_images(set, layout, reg, { &img_view });
			target_access = _details::image_access(
				gpu::synchronization_point_mask::all,
				gpu::image_access_mask::shader_read,
				gpu::image_layout::shader_read_only
			);
			break;
		case image_binding_type::read_write:
			_device.write_descriptor_set_read_write_images(set, layout, reg, { &img_view });
			target_access = _details::image_access(
				gpu::synchronization_point_mask::all,
				gpu::image_access_mask::shader_read | gpu::image_access_mask::shader_write,
				gpu::image_layout::shader_read_write
			);
			break;
		}
		transitions.stage_transition(*img.view._image, img.view._mip_levels, target_access);
	}

	void context::_create_descriptor_binding_impl(
		execution::transition_buffer &transitions, gpu::descriptor_set &set,
		const gpu::descriptor_set_layout &layout, std::uint32_t reg,
		const descriptor_resource::swap_chain_image &chain, const gpu::image2d_view &img_view
	) {
		_device.write_descriptor_set_read_write_images(set, layout, reg, { &img_view });
		transitions.stage_transition(
			*chain.image._ptr,
			_details::image_access(
				gpu::synchronization_point_mask::all,
				gpu::image_access_mask::shader_read,
				gpu::image_layout::shader_read_only
			)
		);
	}

	void context::_create_descriptor_binding_impl(
		execution::transition_buffer &transitions, gpu::descriptor_set &set,
		const gpu::descriptor_set_layout &layout, std::uint32_t reg,
		const descriptor_resource::structured_buffer &buf
	) {
		_maybe_create_buffer(*buf.data._buffer);
		switch (buf.binding_type) {
		case buffer_binding_type::read_only:
			_device.write_descriptor_set_read_only_structured_buffers(
				set, layout, reg,
				{
					gpu::structured_buffer_view(
						buf.data._buffer->data, buf.data._first, buf.data._count, buf.data._stride
					)
				}
			);
			transitions.stage_transition(
				*buf.data._buffer,
				_details::buffer_access(
					gpu::synchronization_point_mask::all, gpu::buffer_access_mask::shader_read
				)
			);
			break;
		case buffer_binding_type::read_write:
			_device.write_descriptor_set_read_write_structured_buffers(
				set, layout, reg,
				{
					gpu::structured_buffer_view(
						buf.data._buffer->data, buf.data._first, buf.data._count, buf.data._stride
					)
				}
			);
			transitions.stage_transition(
				*buf.data._buffer,
				_details::buffer_access(
					gpu::synchronization_point_mask::all,
					gpu::buffer_access_mask::shader_read | gpu::buffer_access_mask::shader_write
				)
			);
			break;
		}
	}

	void context::_create_descriptor_binding_impl(
		execution::transition_buffer &transitions, gpu::descriptor_set &set,
		const gpu::descriptor_set_layout &layout, std::uint32_t reg,
		const descriptor_resource::tlas &as
	) {
		_maybe_initialize_tlas(*as.acceleration_structure._ptr);
		_device.write_descriptor_set_acceleration_structures(
			set, layout, reg, { &as.acceleration_structure._ptr->handle }
		);
		transitions.stage_transition(
			*as.acceleration_structure._ptr->memory,
			_details::buffer_access(
				gpu::synchronization_point_mask::all,
				gpu::buffer_access_mask::acceleration_structure_read
			)
		);
		for (const auto &b : as.acceleration_structure._ptr->input_references) {
			transitions.stage_transition(
				*b->memory,
				_details::buffer_access(
					gpu::synchronization_point_mask::all,
					gpu::buffer_access_mask::acceleration_structure_read
				)
			);
		}
	}

	void context::_create_descriptor_binding_impl(
		gpu::descriptor_set &set,
		const gpu::descriptor_set_layout &layout, std::uint32_t reg,
		const descriptor_resource::sampler &samp
	) {
		_device.write_descriptor_set_samplers(set, layout, reg, { &_cache.get_sampler(samp) });
	}

	void context::_create_descriptor_binding(
		execution::context &ectx, gpu::descriptor_set &set,
		const gpu::descriptor_set_layout &layout, std::uint32_t reg,
		const descriptor_resource::immediate_constant_buffer &cbuf
	) {
		_device.write_descriptor_set_constant_buffers(
			set, layout, reg, { ectx.stage_immediate_constant_buffer(cbuf.data), }
		);
	}

	cache_keys::descriptor_set_layout context::_get_descriptor_set_layout_key(
		const resource_set_binding::descriptors &bindings
	) {
		cache_keys::descriptor_set_layout key = nullptr;
		for (const auto &b : bindings.bindings) {
			auto type = std::visit([this](const auto &rsrc) {
				return _get_descriptor_type(rsrc);
			}, b.resource);
			key.ranges.emplace_back(gpu::descriptor_range_binding::create(type, 1, b.register_index));
		}
		key.consolidate();
		return key;
	}

	std::tuple<
		cache_keys::descriptor_set_layout,
		const gpu::descriptor_set_layout&,
		gpu::descriptor_set&
	> context::_use_descriptor_set(
		execution::context &ectx, const resource_set_binding::descriptors &bindings
	) {
		auto key = _get_descriptor_set_layout_key(bindings);
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
	> context::_use_descriptor_set(execution::context&, const recorded_resources::cached_descriptor_set &set) {
		return { cache_keys::descriptor_set_layout(set._ptr->ranges), *set._ptr->layout, set._ptr->set };
	}

	std::tuple<
		cache_keys::pipeline_resources,
		const gpu::pipeline_resources&,
		std::vector<context::_descriptor_set_info>
	> context::_check_and_create_descriptor_set_bindings(
		execution::context &ectx, const all_resource_bindings &resources
	) {
		crash_if(!resources.is_consolidated());

		std::vector<_descriptor_set_info> sets;
		cache_keys::pipeline_resources key;

		for (const auto &set : resources.get_sets()) {
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
		execution::context &ectx, const gpu::pipeline_resources &rsrc,
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
			case _bind_point::raytracing:
				ectx.get_command_list().bind_ray_tracing_descriptor_sets(
					rsrc, sets[i].space, { set_ptrs.begin() + i, set_ptrs.begin() + end }
				);
			}

			i = end;
		}
	}

	context::_pass_command_data context::_preprocess_command(
		execution::context &ectx,
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
			ectx.transitions.stage_transition(
				*input.data._ptr,
				_details::buffer_access(
					gpu::synchronization_point_mask::vertex_input,
					gpu::buffer_access_mask::vertex_buffer
				)
			);
		}
		if (cmd.index_buffer.data) {
			ectx.transitions.stage_transition(
				*cmd.index_buffer.data._ptr,
				_details::buffer_access(
					gpu::synchronization_point_mask::index_input,
					gpu::buffer_access_mask::index_buffer
				)
			);
		}

		cache_keys::graphics_pipeline key = nullptr;
		key.pipeline_rsrc           = std::move(rsrc_key);
		key.input_buffers           = std::move(input_layouts);
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
		execution::context &ectx, _pass_command_data data, const pass_commands::draw_instanced &cmd
	) {
		std::vector<gpu::vertex_buffer> bufs;
		for (const auto &input : cmd.inputs) {
			if (input.buffer_index >= bufs.size()) {
				bufs.resize(input.buffer_index + 1, nullptr);
			}
			bufs[input.buffer_index] = gpu::vertex_buffer(input.data._ptr->data, input.offset, input.stride);
		}

		auto &cmd_list = ectx.get_command_list();
		cmd_list.bind_pipeline_state(*data.pipeline_state);
		_bind_descriptor_sets(ectx, *data.resources, std::move(data.descriptor_sets), _bind_point::graphics);
		cmd_list.bind_vertex_buffers(0, bufs);
		if (cmd.index_buffer.data) {
			cmd_list.bind_index_buffer(
				cmd.index_buffer.data._ptr->data,
				cmd.index_buffer.offset,
				cmd.index_buffer.format
			);
			cmd_list.draw_indexed_instanced(0, cmd.index_count, 0, 0, cmd.instance_count);
		} else {
			cmd_list.draw_instanced(0, cmd.vertex_count, 0, cmd.instance_count);
		}
	}

	void context::_handle_command(execution::context &ectx, context_commands::upload_image &img) {
		auto dest = img.destination;
		dest._mip_levels = gpu::mip_levels::only(dest._mip_levels.minimum);
		_maybe_create_image(*dest._image);

		auto mip_level = dest._mip_levels.minimum;

		cvec2s size = dest._image->size;
		size[0] = std::max<std::size_t>(1, size[0] >> mip_level);
		size[1] = std::max<std::size_t>(1, size[1] >> mip_level);

		ectx.transitions.stage_transition(
			img.source.data,
			{ gpu::synchronization_point_mask::cpu_access, gpu::buffer_access_mask::cpu_write },
			{ gpu::synchronization_point_mask::copy, gpu::buffer_access_mask::copy_source }
		);
		ectx.transitions.stage_transition(
			*dest._image,
			dest._mip_levels,
			_details::image_access(
				gpu::synchronization_point_mask::copy,
				gpu::image_access_mask::copy_destination,
				gpu::image_layout::copy_destination
			)
		);
		ectx.flush_transitions();
		ectx.get_command_list().copy_buffer_to_image(
			img.source.data, 0, img.source.meta,
			dest._image->image, gpu::subresource_index::create_color(mip_level, 0), zero
		);
	}

	void context::_handle_command(execution::context &ectx, context_commands::upload_buffer &buf) {
		auto dest = buf.destination;
		_maybe_create_buffer(*dest._ptr);

		if (buf.type != execution::upload_buffers::allocation_type::same_buffer) {
			ectx.transitions.stage_transition(
				*buf.source,
				{ gpu::synchronization_point_mask::cpu_access, gpu::buffer_access_mask::cpu_write },
				{ gpu::synchronization_point_mask::copy, gpu::buffer_access_mask::copy_source }
			);
		}
		ectx.transitions.stage_transition(
			*dest._ptr, { gpu::synchronization_point_mask::copy, gpu::buffer_access_mask::copy_destination }
		);
		ectx.flush_transitions();
		ectx.get_command_list().copy_buffer(
			*buf.source, buf.source_offset, dest._ptr->data, buf.destination_offset, buf.size
		);
	}

	void context::_handle_command(execution::context &ectx, const context_commands::dispatch_compute &cmd) {
		// create descriptor bindings
		auto &&[rsrc_key, pipeline_resources, sets] = _check_and_create_descriptor_set_bindings(ectx, cmd.resources);
		auto &pipeline = ectx.record(_device.create_compute_pipeline_state(
			pipeline_resources, cmd.shader->binary
		));
		ectx.flush_transitions();

		auto &cmd_list = ectx.get_command_list();
		cmd_list.bind_pipeline_state(pipeline);
		_bind_descriptor_sets(ectx, pipeline_resources, std::move(sets), _bind_point::compute);
		cmd_list.run_compute_shader(cmd.num_thread_groups[0], cmd.num_thread_groups[1], cmd.num_thread_groups[2]);
	}

	void context::_handle_command(execution::context &ectx, const context_commands::render_pass &cmd) {
		std::vector<gpu::color_render_target_access> color_rt_access;
		std::vector<gpu::image2d_view*> color_rts;
		std::vector<gpu::format> color_rt_formats;
		gpu::format ds_rt_format = gpu::format::none;

		// create frame buffer
		for (const auto &rt : cmd.color_render_targets) {
			constexpr auto access = _details::image_access(
				gpu::synchronization_point_mask::render_target_read_write,
				gpu::image_access_mask::color_render_target,
				gpu::image_layout::color_render_target
			);
			if (std::holds_alternative<recorded_resources::image2d_view>(rt.view)) {
				auto img = std::get<recorded_resources::image2d_view>(rt.view).highest_mip_with_warning();
				color_rts.emplace_back(&_request_image_view(ectx, img));
				color_rt_formats.emplace_back(img._view_format);
				ectx.transitions.stage_transition(*img._image, img._mip_levels, access);
				assert(img._image->size == cmd.render_target_size);
			} else if (std::holds_alternative<recorded_resources::swap_chain>(rt.view)) {
				auto &chain = std::get<recorded_resources::swap_chain>(rt.view);
				color_rts.emplace_back(&_request_image_view(ectx, chain));
				color_rt_formats.emplace_back(chain._ptr->current_format);
				ectx.transitions.stage_transition(*chain._ptr, access);
				assert(chain._ptr->current_size == cmd.render_target_size);
			} else {
				std::abort(); // unhandled
			}
			color_rt_access.emplace_back(rt.access);
		}
		gpu::image2d_view *depth_stencil_rt = nullptr;
		if (cmd.depth_stencil_target.view) {
			assert(cmd.depth_stencil_target.view._image->size == cmd.render_target_size);
			auto img = cmd.depth_stencil_target.view.highest_mip_with_warning();
			depth_stencil_rt = &_request_image_view(ectx, img);
			ds_rt_format = img._view_format;
			ectx.transitions.stage_transition(
				*img._image,
				img._mip_levels,
				// TODO use depth_stencil_read_only if none of the draw calls write them
				_details::image_access(
					gpu::synchronization_point_mask::depth_stencil_read_write,
					gpu::image_access_mask::depth_stencil_read_write,
					gpu::image_layout::depth_stencil_read_write
				)
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

	void context::_handle_command(execution::context &ectx, const context_commands::build_blas &cmd) {
		auto *blas_ptr = cmd.target._ptr;
		_maybe_initialize_blas(*blas_ptr);

		// handle transitions
		for (const auto &geom : blas_ptr->input) {
			constexpr auto access = _details::buffer_access(
				gpu::synchronization_point_mask::acceleration_structure_build,
				gpu::buffer_access_mask::acceleration_structure_build_input
			);
			ectx.transitions.stage_transition(*geom.vertex_data._ptr, access);
			if (geom.index_data._ptr) {
				ectx.transitions.stage_transition(*geom.index_data._ptr, access);
			}
		}
		ectx.transitions.stage_transition(
			*blas_ptr->memory,
			_details::buffer_access(
				gpu::synchronization_point_mask::acceleration_structure_build,
				gpu::buffer_access_mask::acceleration_structure_write
			)
		);
		ectx.flush_transitions();

		auto &scratch = ectx.create_buffer(
			blas_ptr->build_sizes.build_scratch_size,
			_device_memory_index,
			gpu::buffer_usage_mask::shader_read | gpu::buffer_usage_mask::shader_write
		);
		ectx.get_command_list().build_acceleration_structure(blas_ptr->geometry, blas_ptr->handle, scratch, 0);
	}

	void context::_handle_command(execution::context &ectx, const context_commands::build_tlas &cmd) {
		auto *tlas_ptr = cmd.target._ptr;
		_maybe_copy_tlas_build_input(ectx, *tlas_ptr);

		ectx.transitions.stage_transition(
			*tlas_ptr->memory,
			_details::buffer_access(
				gpu::synchronization_point_mask::acceleration_structure_build,
				gpu::buffer_access_mask::acceleration_structure_write
			)
		);
		for (const auto &ref : tlas_ptr->input_references) {
			ectx.transitions.stage_transition(
				*ref->memory,
				_details::buffer_access(
					gpu::synchronization_point_mask::acceleration_structure_build,
					gpu::buffer_access_mask::acceleration_structure_read
				)
			);
		}
		ectx.flush_transitions();

		auto &scratch = ectx.create_buffer(
			tlas_ptr->build_sizes.build_scratch_size,
			_device_memory_index,
			gpu::buffer_usage_mask::shader_read | gpu::buffer_usage_mask::shader_write
		);
		ectx.get_command_list().build_acceleration_structure(
			tlas_ptr->input_data, 0, tlas_ptr->input.size(), tlas_ptr->handle, scratch, 0
		);
	}

	void context::_handle_command(execution::context &ectx, const context_commands::trace_rays &cmd) {
		auto &&[rsrc_key, rsrc, sets] = _check_and_create_descriptor_set_bindings(ectx, cmd.resource_bindings);

		cache_keys::raytracing_pipeline pipeline_key = nullptr;
		pipeline_key.pipeline_rsrc       = std::move(rsrc_key);
		pipeline_key.hit_group_shaders   = cmd.hit_group_shaders;
		pipeline_key.hit_groups          = cmd.hit_groups;
		pipeline_key.general_shaders     = cmd.general_shaders;
		pipeline_key.max_recursion_depth = cmd.max_recursion_depth;
		pipeline_key.max_payload_size    = cmd.max_payload_size;
		pipeline_key.max_attribute_size  = cmd.max_attribute_size;
		auto &state = _cache.get_raytracing_pipeline_state(pipeline_key);

		// create shader group buffers
		std::size_t record_size = memory::align_up(
			_adapter_properties.shader_group_handle_size,
			_adapter_properties.shader_group_handle_alignment
		);

		auto [raygen_cbuf, raygen_data] = ectx.stage_immediate_constant_buffer(memory::size_alignment(
			record_size, _adapter_properties.shader_group_handle_table_alignment
		));
		{
			auto rec = _device.get_shader_group_handle(state, cmd.raygen_shader_group_index);
			std::memcpy(raygen_data, rec.data().data(), rec.data().size());
		}

		auto [miss_cbuf, miss_data] = ectx.stage_immediate_constant_buffer(memory::size_alignment(
			record_size * cmd.miss_group_indices.size(), _adapter_properties.shader_group_handle_table_alignment
		));
		{
			auto *ptr = miss_data;
			for (const auto &id : cmd.miss_group_indices) {
				auto rec = _device.get_shader_group_handle(state, id);
				std::memcpy(ptr, rec.data().data(), rec.data().size());
				ptr += record_size;
			}
		}

		auto [hit_cbuf, hit_data] = ectx.stage_immediate_constant_buffer(memory::size_alignment(
			record_size * cmd.hit_group_indices.size(), _adapter_properties.shader_group_handle_table_alignment
		));
		{
			auto *ptr = hit_data;
			for (const auto &id : cmd.hit_group_indices) {
				auto rec = _device.get_shader_group_handle(state, id);
				std::memcpy(ptr, rec.data().data(), rec.data().size());
				ptr += record_size;
			}
		}

		ectx.get_command_list().bind_pipeline_state(state);
		_bind_descriptor_sets(ectx, rsrc, sets, _bind_point::raytracing);
		ectx.flush_transitions();
		ectx.get_command_list().trace_rays(
			raygen_cbuf,
			gpu::shader_record_view(*miss_cbuf.data, miss_cbuf.offset, cmd.miss_group_indices.size(), record_size),
			gpu::shader_record_view(*hit_cbuf.data, hit_cbuf.offset, cmd.hit_group_indices.size(), record_size),
			cmd.num_threads[0], cmd.num_threads[1], cmd.num_threads[2]
		);
	}

	void context::_handle_command(execution::context &ectx, const context_commands::present &cmd) {
		// if we haven't written anything to the swap chain, just don't present
		if (cmd.target._ptr->chain) {
			ectx.transitions.stage_transition(
				*cmd.target._ptr,
				_details::image_access(
					gpu::synchronization_point_mask::none,
					gpu::image_access_mask::none,
					gpu::image_layout::present
				)
			);
			ectx.flush_transitions();

			ectx.submit(_queue, nullptr);
			if (_queue.present(cmd.target._ptr->chain) != gpu::swap_chain_status::ok) {
				// TODO?
			}
		}
		// do this last, since handling transitions needs the index of the image
		cmd.target._ptr->next_image_index = _details::swap_chain::invalid_image_index;
	}

	void context::_cleanup() {
		auto first_batch = static_cast<std::uint32_t>(_batch_index + 1 - _all_resources.size());
		std::uint64_t finished_batch = _device.query_timeline_semaphore(_batch_semaphore);
		while (first_batch <= finished_batch) {
			const auto &batch = _all_resources.front();
			
			for (const auto &surf : batch.image2d_meta) {
				for (const auto &array_ref : surf->array_references) {
					if (array_ref.array->set) {
						std::initializer_list<const gpu::image_view*> images = { nullptr };
						(_device.*_device.get_write_image_descriptor_function(array_ref.array->type))(
							array_ref.array->set,
							_cache.get_descriptor_set_layout(
								cache_keys::descriptor_set_layout::for_descriptor_array(array_ref.array->type)
							),
							array_ref.index,
							images
						);
					}
				}
				if (surf->memory) {
					surf->memory_pool->free(surf->memory);
				}
			}

			for (const auto &buf : batch.buffer_meta) {
				if (buf->memory) {
					buf->memory_pool->free(buf->memory);
				}
			}

			constexpr bool _debug_resource_disposal = false;
			if constexpr (_debug_resource_disposal) {
				auto rsrc = std::move(_all_resources.front());
				for (auto &img : rsrc.image_views) {
					img = nullptr;
				}
			}

			_all_resources.pop_front();
			++first_batch;
		}
	}
}
