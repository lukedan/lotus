#include "lotus/renderer/context/context.h"

/// \file
/// Implementation of scene loading and rendering.

#include <unordered_map>
#include <queue>

#include "lotus/logging.h"
#include "lotus/memory/stack_allocator.h"
#include "lotus/renderer/context/execution/descriptors.h"
#include "lotus/renderer/context/execution/queue_context.h"
#include "lotus/renderer/context/execution/queue_pseudo_context.h"
#include "lotus/renderer/context/execution/batch_context.h"
#include "lotus/renderer/mipmap.h"

namespace lotus::renderer {
	gpu::device &context::device_access::get(context &ctx) {
		return ctx._device;
	}


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
		_details::bindings_builder builder;
		auto reflections = { &vs->reflection, &ps->reflection };
		builder.add(std::move(resources), reflections);
		_q->add_command<commands::draw_instanced>(
			description,
			num_insts,
			std::move(inputs), num_verts,
			std::move(indices), num_indices,
			topology,
			builder.sort_and_take(),
			std::move(vs), std::move(ps),
			std::move(state)
		);
	}

	void context::pass::draw_instanced(
		assets::handle<assets::geometry> geom,
		assets::handle<assets::material> mat,
		pass_context &pass_ctx,
		std::span<const input_buffer_binding> additional_inputs,
		all_resource_bindings additional_resources,
		constant_uploader &uploader,
		std::uint32_t num_insts,
		std::u8string_view description
	) {
		instance_render_details details = pass_ctx.get_render_details(_q->ctx, *mat->data, geom.get().value);

		draw_instanced(
			std::move(geom),
			std::move(mat),
			details,
			additional_inputs,
			std::move(additional_resources),
			uploader,
			num_insts,
			description
		);
	}

	void context::pass::draw_instanced(
		assets::handle<assets::geometry> geom,
		assets::handle<assets::material> mat,
		const instance_render_details &details,
		std::span<const input_buffer_binding> additional_inputs,
		all_resource_bindings additional_resources,
		constant_uploader &uploader,
		std::uint32_t num_insts,
		std::u8string_view description
	) {
		std::vector<input_buffer_binding> inputs;
		inputs.reserve(details.input_buffers.size() + additional_inputs.size());
		inputs.insert(inputs.end(), details.input_buffers.begin(), details.input_buffers.end());
		inputs.insert(inputs.end(), additional_inputs.begin(), additional_inputs.end());

		_details::bindings_builder builder;
		auto reflections = { &details.vertex_shader->reflection, &details.pixel_shader->reflection };
		builder.add(mat->data->create_resource_bindings(uploader), reflections);
		builder.add(std::move(additional_resources), reflections);

		_q->add_command<commands::draw_instanced>(
			description,
			num_insts,
			std::move(inputs), geom->num_vertices,
			geom->get_index_buffer_binding(), geom->num_indices,
			geom->topology,
			builder.sort_and_take(),
			details.vertex_shader, details.pixel_shader,
			details.pipeline
		);
	}

	void context::pass::end() {
		if (_q) {
			_q->add_command<commands::end_pass>(u8"End Pass");
			_q->within_pass = false;
			_q = nullptr;
		}
	}


	void context::queue::copy_buffer(
		const buffer &source, const buffer &target,
		std::uint32_t src_offset, std::uint32_t dst_offset, std::uint32_t sz,
		std::u8string_view description
	) {
		_q->add_command<commands::copy_buffer>(
			description, source, target, src_offset, dst_offset, sz
		);
	}

	void context::queue::copy_buffer_to_image(
		const buffer &source, const image2d_view &target,
		gpu::staging_buffer::metadata meta, std::uint32_t src_offset, cvec2u32 dst_offset,
		std::u8string_view description
	) {
		_q->add_command<commands::copy_buffer_to_image>(
			description, source, target, std::move(meta), src_offset, dst_offset
		);
	}

	void context::queue::copy_buffer_to_image(
		const staging_buffer &source, const image2d_view &target,
		std::uint32_t src_offset, cvec2u32 dst_offset,
		std::u8string_view description
	) {
		copy_buffer_to_image(source.data, target, source.meta, src_offset, dst_offset, description);
	}

	void context::queue::build_blas(
		blas &b, std::span<const geometry_buffers_view> geom_buffers, std::u8string_view description
	) {
		_q->add_command<commands::build_blas>(description, b, std::vector(geom_buffers.begin(), geom_buffers.end()));
	}

	void context::queue::build_tlas(
		tlas &t, std::span<const blas_instance> instances, std::u8string_view description
	) {
		_q->add_command<commands::build_tlas>(description, t, std::vector(instances.begin(), instances.end()));
	}

	void context::queue::trace_rays(
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
		_details::bindings_builder builder;
		std::vector<gpu::shader_reflection> reflections;
		for (const auto &sh : hit_group_shaders) {
			reflections.emplace_back(sh.shader_library->reflection.find_shader(sh.entry_point, sh.stage));
		}
		for (const auto &sh : general_shaders) {
			reflections.emplace_back(sh.shader_library->reflection.find_shader(sh.entry_point, sh.stage));
		}
		std::vector<const gpu::shader_reflection*> reflection_ptrs;
		for (const auto &r : reflections) {
			reflection_ptrs.emplace_back(&r);
		}
		builder.add(std::move(resources), reflection_ptrs);

		_q->add_command<commands::trace_rays>(
			description,
			builder.sort_and_take(),
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

	void context::queue::release_dependency(dependency dep, std::u8string_view description) {
		_q->add_command<commands::release_dependency>(description, dep);
	}

	void context::queue::acquire_dependency(dependency dep, std::u8string_view description) {
		_q->add_command<commands::acquire_dependency>(description, dep);
	}

	void context::queue::run_compute_shader(
		assets::handle<assets::shader> shader, cvec3<std::uint32_t> num_thread_groups,
		all_resource_bindings resources, std::u8string_view description
	) {
		_details::bindings_builder builder;
		builder.add(std::move(resources), { &shader->reflection });

		_q->add_command<commands::dispatch_compute>(
			description, builder.sort_and_take(), std::move(shader), num_thread_groups
		);
	}

	void context::queue::run_compute_shader_with_thread_dimensions(
		assets::handle<assets::shader> shader, cvec3<std::uint32_t> num_threads, all_resource_bindings resources,
		std::u8string_view description
	) {
		auto thread_group_size = shader->reflection.get_thread_group_size().into<std::uint32_t>();
		cvec3u32 groups = mat::memberwise_divide(
			num_threads + thread_group_size - cvec3u32(1, 1, 1), thread_group_size
		);
		run_compute_shader(std::move(shader), groups, std::move(resources), description);
	}

	context::pass context::queue::begin_pass(
		std::vector<image2d_color> color_rts, image2d_depth_stencil ds_rt, cvec2u32 sz,
		std::u8string_view description
	) {
		_q->add_command<commands::begin_pass>(description, std::move(color_rts), ds_rt, sz);
		_q->within_pass = true;
		return context::pass(*_q);
	}

	void context::queue::present(swap_chain sc, std::u8string_view description) {
		_q->add_command<commands::present>(description, std::move(sc));
	}

	context::timer context::queue::start_timer(std::u8string name) {
		auto index = static_cast<commands::timer_index>(_q->num_timers++);
		_q->add_command<commands::start_timer>(u8"Start Timer", std::move(name), index);
		return timer(*_q, index);
	}

	void context::queue::pause_for_debugging(std::u8string_view description) {
		_q->add_command<commands::pause_for_debugging>(description);
	}


	context context::create(
		gpu::context &ctx,
		const gpu::adapter_properties &adap_prop,
		gpu::device &dev,
		std::span<const gpu::command_queue> queues
	) {
		return context(ctx, adap_prop, dev, queues);
	}

	context::~context() {
		wait_idle();
	}

	pool context::request_pool(
		std::u8string_view name, gpu::memory_type_index memory_type, std::uint32_t chunk_size
	) {
		if (memory_type == gpu::memory_type_index::invalid) {
			memory_type = _device_memory_index;
		}

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
		std::u8string_view name,
		cvec2u32 size, std::uint32_t num_mips, gpu::format fmt, gpu::image_usage_mask usages, const pool &p
	) {
		gpu::image_tiling tiling = gpu::image_tiling::optimal;
		auto *surf = new _details::image2d(
			size, num_mips, fmt, tiling, usages,
			p._ptr, static_cast<std::uint32_t>(_queues.size()),
			_allocate_resource_id(), name
		);
		auto surf_ptr = std::shared_ptr<_details::image2d>(surf, _details::context_managed_deleter(*this));
		return image2d_view(std::move(surf_ptr), fmt, gpu::mip_levels::all());
	}

	image3d_view context::request_image3d(
		std::u8string_view name,
		cvec3u32 size, std::uint32_t num_mips, gpu::format fmt, gpu::image_usage_mask usages, const pool &p
	) {
		gpu::image_tiling tiling = gpu::image_tiling::optimal;
		auto *surf = new _details::image3d(
			size, num_mips, fmt, tiling, usages,
			p._ptr, static_cast<std::uint32_t>(_queues.size()),
			_allocate_resource_id(), name
		);
		auto surf_ptr = std::shared_ptr<_details::image3d>(surf, _details::context_managed_deleter(*this));
		return image3d_view(std::move(surf_ptr), fmt, gpu::mip_levels::all());
	}

	buffer context::request_buffer(
		std::u8string_view name, std::uint32_t size_bytes, gpu::buffer_usage_mask usages, const pool &p
	) {
		return buffer(_request_buffer_raw(name, size_bytes, usages, p._ptr));
	}

	staging_buffer context::request_staging_buffer(std::u8string_view name, cvec2u32 size, gpu::format fmt) {
		const static auto usages = gpu::buffer_usage_mask::copy_source;
		const auto memory_index = get_upload_memory_type_index();

		// create staging buffer
		auto staging_buf = _device.create_committed_staging_buffer(size, fmt, memory_index, usages);

		// wrap it in a buffer
		auto *buf_ptr = new _details::buffer(
			staging_buf.total_size, usages, nullptr, _queues.size(), _allocate_resource_id(), name
		);
		buf_ptr->data = std::move(staging_buf.data);
		if constexpr (should_register_debug_names) {
			_device.set_debug_name(buf_ptr->data, buf_ptr->name);
		}
		auto res_buf = std::shared_ptr<_details::buffer>(buf_ptr, _details::context_managed_deleter(*this));

		return staging_buffer(buffer(std::move(res_buf)), std::move(staging_buf.meta), staging_buf.total_size);
	}

	staging_buffer context::request_staging_buffer_for(std::u8string_view name, const image2d_view &img) {
		const auto mip = recorded_resources::image2d_view(img).highest_mip_with_warning()._mip_levels.first_level;
		const cvec2u32 mip_size = mipmap::get_size(img.get_size(), mip);
		return request_staging_buffer(name, mip_size, img.get_original_format());
	}

	swap_chain context::request_swap_chain(
		std::u8string_view name, system::window &wnd, queue &q,
		std::uint32_t num_images, std::span<const gpu::format> formats
	) {
		auto *chain = new _details::swap_chain(
			wnd, q._q->queue.get_index(),
			num_images, { formats.begin(), formats.end() }, _allocate_resource_id(), name
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

	blas context::request_blas(std::u8string_view name, const pool &p) {
		auto *blas_ptr = new _details::blas(p._ptr, _allocate_resource_id(), name);
		auto ptr = std::shared_ptr<_details::blas>(blas_ptr, _details::context_managed_deleter(*this));
		return blas(std::move(ptr));
	}

	tlas context::request_tlas(std::u8string_view name, const pool &p) {
		auto *tlas_ptr = new _details::tlas(p._ptr, _allocate_resource_id(), name);
		auto ptr = std::shared_ptr<_details::tlas>(tlas_ptr, _details::context_managed_deleter(*this));
		return tlas(std::move(ptr));
	}

	cached_descriptor_set context::request_cached_descriptor_set(
		std::u8string_view name, std::span<const all_resource_bindings::numbered_binding> bindings
	) {
		auto key = execution::descriptor_set_builder::get_descriptor_set_layout_key(bindings);
		auto &layout = _cache.get_descriptor_set_layout(key);
		auto set = _device.create_descriptor_set(_descriptor_pool, layout);
		auto *set_ptr = new _details::cached_descriptor_set(
			std::move(set), std::move(key.ranges), layout, _allocate_resource_id(), name
		);
		std::abort(); // TODO: create descriptor bindings
		/*
		for (const auto &binding : bindings) {
			std::visit([&](const auto &rsrc) {
				_create_descriptor_binding_cached(*set_ptr, binding.register_index, rsrc);
			}, binding.value);
		}
		*/
		return cached_descriptor_set(std::shared_ptr<_details::cached_descriptor_set>(
			set_ptr, _details::context_managed_deleter(*this)
		));
	}

	dependency context::request_dependency(std::u8string_view name) {
		auto *dep = new _details::dependency(_allocate_resource_id(), name);
		return dependency(std::shared_ptr<_details::dependency>(dep, _details::context_managed_deleter(*this)));
	}

	std::vector<batch_statistics_early> context::execute_all() {
		constexpr bool _insert_pass_markers = decltype(command::description)::is_enabled;

		crash_if(std::this_thread::get_id() != _thread);

		++_batch_index;
		auto &batch_data = _batch_data.emplace_back();
		batch_data.resources = std::exchange(_deferred_delete_resources, execution::batch_resources());
		batch_data.resolve_data.first_command = _first_batch_command_index;
		batch_data.resolve_data.queues.resize(_queues.size());
		for (std::uint32_t i = 0; i < get_num_queues(); ++i) {
			batch_data.resolve_data.queues[i].begin_of_batch = _queues[i].semaphore_value;
		}

		execution::batch_context bctx(*this);

		// step 1: pseudo-execute all commands, gather resource transitions and barriers
		while (true) {
			std::size_t next_queue_index = _queues.size();
			{ // find a queue that can be executed
				bool has_commands = false;
				auto next_global_sub_index = global_submission_index::max;
				for (std::size_t queue_i = 0; queue_i < _queues.size(); ++queue_i) {
					auto &qctx = bctx.get_queue_pseudo_context(queue_i);
					if (qctx.is_pseudo_execution_finished()) {
						continue;
					}
					has_commands = true;
					if (qctx.is_pseudo_execution_blocked()) {
						continue;
					}
					auto &cmd = qctx.get_next_pseudo_execution_command();
					if (cmd.index < next_global_sub_index) {
						next_global_sub_index = cmd.index;
						next_queue_index = queue_i;
					}
				}
				if (next_queue_index >= _queues.size()) {
					if (has_commands) {
						std::abort(); // no command can be executed; most likely loop in the dependency graph
					}
					break; // otherwise, we've finished pseudo-execution
				}
			}
			bctx.get_queue_pseudo_context(next_queue_index).pseudo_execute_next_command();
		}

		// step 2: process all barriers
		for (std::uint32_t i = 0; i < get_num_queues(); ++i) {
			bctx.get_queue_pseudo_context(i).process_pseudo_release_events();
		}
		for (std::uint32_t i = 0; i < get_num_queues(); ++i) {
			bctx.get_queue_pseudo_context(i).process_pseudo_acquire_events();
		}

		// step 3: execute all commands, respecting previously gathered dependencies and transitions
		// this is parallelizable
		for (std::uint32_t i = 0; i < get_num_queues(); ++i) {
			auto &qctx = bctx.get_queue_context(i);
			qctx.start_execution();
			while (!qctx.is_finished()) {
				qctx.execute_next_command();
			}
			qctx.finish_execution();
		}

		// finalize & clean up
		_first_batch_command_index = _sub_index;
		for (std::size_t i = 0; i < _queues.size(); ++i) {
			_queues[i].reset_batch();
			batch_data.resolve_data.queues[i].end_of_batch = _queues[i].semaphore_value;
		}

		_cleanup();

		std::vector<batch_statistics_early> stats;
		for (std::size_t i = 0; i < _queues.size(); ++i) {
			stats.emplace_back(std::move(bctx.get_queue_context(static_cast<std::uint32_t>(i)).early_statistics));
		}
		return stats;
	}

	void context::wait_idle() {
		for (auto &q : _queues) {
			_device.wait_for_timeline_semaphore(q.semaphore, q.semaphore_value);
		}
		_cleanup();
	}

	std::byte *context::map_buffer(buffer &buf) {
		_maybe_initialize_buffer(*buf._ptr);
		return _device.map_buffer(buf._ptr->data);
	}

	void context::unmap_buffer(buffer &buf) {
		_device.unmap_buffer(buf._ptr->data);
	}

	void context::flush_mapped_buffer_to_device(buffer &buf, std::size_t begin, std::size_t length) {
		// TODO check that this buffer is not being used by the device?
		_device.flush_mapped_buffer_to_device(buf._ptr->data, begin, length);
		// mark this buffer as CPU written
		buf._ptr->previous_modification = {
			0,
			_details::buffer_access_event(
				_details::buffer_access(
					gpu::synchronization_point_mask::cpu_access,
					gpu::buffer_access_mask::cpu_write
				),
				_sub_index,
				queue_submission_index::zero
			)
		};
	}

	void context::flush_mapped_buffer_to_host(buffer &buf, std::size_t begin, std::size_t length) {
		// TODO check that this buffer is not being written to from the device?
		_device.flush_mapped_buffer_to_host(buf._ptr->data, begin, length);
	}

	void context::write_data_to_buffer(buffer &buf, std::span<const std::byte> data) {
		crash_if(data.size() != buf.get_size_in_bytes());
		write_data_to_buffer_custom(
			buf,
			[&](std::byte *dst) {
				std::memcpy(dst, data.data(), data.size());
			}
		);
	}

	void context::write_image_data_to_buffer_tight(
		buffer &buf, const gpu::staging_buffer::metadata &meta, std::span<const std::byte> data
	) {
		write_data_to_buffer_custom(
			buf,
			[&](std::byte *dst) {
				const auto &format_props = gpu::format_properties::get(meta.get_format());
				const auto frag_size = format_props.fragment_size.into<std::uint32_t>();
				const auto num_frags = vec::memberwise_divide(
					meta.get_size() + frag_size - cvec2u32(1, 1), frag_size
				);
				const auto frag_bytes = format_props.bytes_per_fragment;
				const auto row_bytes = num_frags[0] * frag_bytes;
				crash_if(data.size() != row_bytes * num_frags[1]);

				const std::byte *cur_src = data.data();
				std::byte *cur_dst = dst;
				for (std::uint32_t y = 0; y < num_frags[1]; ++y) {
					std::memcpy(cur_dst, cur_src, row_bytes);
					cur_dst += meta.get_pitch_in_bytes();
					cur_src += row_bytes;
				}
			}
		);
	}

	void context::write_image_descriptors(
		image_descriptor_array &arr_handle, std::uint32_t first_index, std::span<const image2d_view> imgs
	) {
		for (std::size_t i = 0; i < imgs.size(); ++i) {
			_write_one_descriptor_array_element(
				*arr_handle._ptr,
				recorded_resources::image2d_view(imgs[i]),
				static_cast<std::uint32_t>(i + first_index)
			);
		}
	}

	void context::write_buffer_descriptors(
		buffer_descriptor_array &arr_handle, std::uint32_t first_index, std::span<const structured_buffer_view> bufs
	) {
		for (std::size_t i = 0; i < bufs.size(); ++i) {
			_write_one_descriptor_array_element(
				*arr_handle._ptr,
				recorded_resources::structured_buffer_view(bufs[i]),
				static_cast<std::uint32_t>(i + first_index)
			);
		}
	}

	context::context(
		gpu::context &ctx,
		const gpu::adapter_properties &adap_prop,
		gpu::device &dev,
		std::span<const gpu::command_queue> queues
	) :
		_device(dev), _context(ctx),
		_descriptor_pool(_device.create_descriptor_pool({
			gpu::descriptor_range::create(gpu::descriptor_type::acceleration_structure, 10240),
			gpu::descriptor_range::create(gpu::descriptor_type::constant_buffer, 10240),
			gpu::descriptor_range::create(gpu::descriptor_type::read_only_buffer, 10240),
			gpu::descriptor_range::create(gpu::descriptor_type::read_only_image, 10240),
			gpu::descriptor_range::create(gpu::descriptor_type::read_write_buffer, 10240),
			gpu::descriptor_range::create(gpu::descriptor_type::read_write_image, 10240),
			gpu::descriptor_range::create(gpu::descriptor_type::sampler, 1024),
		}, 10240)), // TODO proper numbers
		_adapter_properties(adap_prop),
		_cache(_device),
		_thread(std::this_thread::get_id()) {

		// initialize queues
		_queues.reserve(queues.size());
		for (auto &&q : queues) {
			_queues.emplace_back(*this, q, _device.create_timeline_semaphore(0));
		}

		const auto &memory_types = _device.enumerate_memory_types();
		for (const auto &type : memory_types) {
			if (
				_device_memory_index == gpu::memory_type_index::invalid &&
				bit_mask::contains<gpu::memory_properties::device_local>(type.second)
			) {
				_device_memory_index = type.first;
			}
			if (
				_upload_memory_index == gpu::memory_type_index::invalid &&
				bit_mask::contains<gpu::memory_properties::host_visible>(type.second)
			) {
				_upload_memory_index = type.first;
			}
			constexpr gpu::memory_properties _readback_properties =
				gpu::memory_properties::host_visible | gpu::memory_properties::host_cached;
			if (
				_readback_memory_index == gpu::memory_type_index::invalid &&
				bit_mask::contains_all(type.second, _readback_properties)
			) {
				_readback_memory_index = type.first;
			}
		}
	}

	std::shared_ptr<_details::buffer> context::_request_buffer_raw(
		std::u8string_view name,
		std::uint32_t size_bytes,
		gpu::buffer_usage_mask usages,
		const std::shared_ptr<_details::pool> &pool
	) {
		auto *buf = new _details::buffer(size_bytes, usages, pool, _queues.size(), _allocate_resource_id(), name);
		return std::shared_ptr<_details::buffer>(buf, _details::context_managed_deleter(*this));
	}

	void context::_maybe_initialize_image(_details::image2d &img) {
		if (!img.image) {
			if (img.memory_pool) {
				const memory::size_alignment size_align = _device.get_image2d_memory_requirements(
					img.size, img.num_mips, img.format, img.tiling, img.usages
				);
				img.memory = img.memory_pool->allocate(size_align);
				auto &&[blk, off] = img.memory_pool->get_memory_and_offset(img.memory);
				img.image = _device.create_placed_image2d(
					img.size, img.num_mips, img.format, img.tiling, img.usages, blk, off
				);
			} else {
				img.image = _device.create_committed_image2d(
					img.size, img.num_mips, img.format, img.tiling, img.usages
				);
			}

			if constexpr (should_register_debug_names) {
				_device.set_debug_name(img.image, img.name);
			}
		}
	}

	void context::_maybe_initialize_image(_details::image3d &img) {
		if (!img.image) {
			if (img.memory_pool) {
				const memory::size_alignment size_align = _device.get_image3d_memory_requirements(
					img.size, img.num_mips, img.format, img.tiling, img.usages
				);
				img.memory = img.memory_pool->allocate(size_align);
				auto &&[blk, off] = img.memory_pool->get_memory_and_offset(img.memory);
				img.image = _device.create_placed_image3d(
					img.size, img.num_mips, img.format, img.tiling, img.usages, blk, off
				);
			} else {
				img.image = _device.create_committed_image3d(
					img.size, img.num_mips, img.format, img.tiling, img.usages
				);
			}

			if constexpr (should_register_debug_names) {
				_device.set_debug_name(img.image, img.name);
			}
		}
	}

	void context::_maybe_initialize_buffer(_details::buffer &buf) {
		if (!buf.data) {
			if (buf.memory_pool) {
				const memory::size_alignment size_align =
					_device.get_buffer_memory_requirements(buf.size, buf.usages);
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

	void context::_flush_descriptor_array_writes(_details::image_descriptor_array &arr) {
		if (!arr.staged_writes.empty()) {
			if (arr.has_descriptor_overwrites) {
				arr.has_descriptor_overwrites = false;
			}

			auto writes = std::exchange(arr.staged_writes, {});
			std::sort(writes.begin(), writes.end());
			writes.erase(std::unique(writes.begin(), writes.end()), writes.end());
			auto write_func = gpu::device::get_write_image_descriptor_function(arr.type);
			crash_if(write_func == nullptr);
			for (auto index : writes) {
				auto &rsrc = arr.resources[index];
				if (rsrc.view.value) {
					// technically this is last batch's resource
					_batch_data.back().resources.record(std::exchange(rsrc.view.value, nullptr));
				}
				// TODO batch writes
				if (rsrc.resource) {
					rsrc.view.value = _device.create_image2d_view_from(
						rsrc.resource._ptr->image, rsrc.resource._view_format, rsrc.resource._mip_levels
					);
					_device.set_debug_name(rsrc.view.value, rsrc.resource._ptr->name);
				}
				std::initializer_list<const gpu::image_view_base*> views = { &rsrc.view.value };
				(_device.*write_func)(arr.set, *arr.layout, index, views);
			}
		}
	}

	void context::_flush_descriptor_array_writes(_details::buffer_descriptor_array &arr) {
		if (!arr.staged_writes.empty()) {
			if (arr.has_descriptor_overwrites) {
				arr.has_descriptor_overwrites = false;
			}

			auto writes = std::exchange(arr.staged_writes, {});
			std::sort(writes.begin(), writes.end());
			writes.erase(std::unique(writes.begin(), writes.end()), writes.end());
			auto write_func = gpu::device::get_write_structured_buffer_descriptor_function(arr.type);
			crash_if(write_func == nullptr);
			for (auto index : writes) {
				const auto &buf = arr.resources[index].resource;
				// TODO batch writes
				gpu::structured_buffer_view view = nullptr;
				if (buf) {
					crash_if((buf._first + buf._count) * buf._stride > buf._ptr->size);
					view = gpu::structured_buffer_view(buf._ptr->data, buf._first, buf._count, buf._stride);
				}
				std::initializer_list<gpu::structured_buffer_view> views = { view };
				(_device.*write_func)(arr.set, *arr.layout, index, views);
			}
		}
	}

	gpu::image2d_view context::_create_image_view(const recorded_resources::image2d_view &view) {
		auto result = _device.create_image2d_view_from(view._ptr->image, view._view_format, view._mip_levels);
		if constexpr (should_register_debug_names) {
			_device.set_debug_name(result, view._ptr->name);
		}
		return result;
	}

	gpu::image3d_view context::_create_image_view(const recorded_resources::image3d_view &view) {
		auto result = _device.create_image3d_view_from(view._ptr->image, view._view_format, view._mip_levels);
		if constexpr (should_register_debug_names) {
			_device.set_debug_name(result, view._ptr->name);
		}
		return result;
	}

	gpu::image2d_view &context::_request_image_view(const recorded_resources::image2d_view &view) {
		return _batch_data.back().resources.record(_create_image_view(view));
	}

	gpu::image3d_view &context::_request_image_view(const recorded_resources::image3d_view &view) {
		return _batch_data.back().resources.record(_create_image_view(view));
	}

	gpu::image2d_view &context::_request_image_view(const recorded_resources::swap_chain &chain) {
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
					/*auto fence = _device.create_fence(gpu::synchronization_state::unset);
					ectx.submit(&fence);
					_device.wait_for_fence(fence);*/
					std::abort(); // TODO
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
					_batch_data.back().resources.record(std::move(chain_data.chain));
					auto [new_chain, fmt] = _context.create_swap_chain_for_window(
						chain_data.window, _device, _queues[chain_data.queue_index].queue,
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
					_batch_data.back().resources.record(std::move(fence));
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

		return _batch_data.back().resources.record(_device.create_image2d_view_from(
			chain_data.images[chain_data.next_image_index],
			chain_data.current_format,
			gpu::mip_levels::only_top()
		));
	}

	void context::_cleanup() {
		std::vector<gpu::timeline_semaphore::value_type> semaphore_values;
		for (auto &q : _queues) {
			semaphore_values.emplace_back(_device.query_timeline_semaphore(q.semaphore));
		}

		while (!_batch_data.empty()) {
			auto &batch = _batch_data.front();
			bool batch_ongoing = false;
			for (std::size_t i = 0; i < _queues.size(); ++i) {
				if (semaphore_values[i] < batch.resolve_data.queues[0].end_of_batch) {
					batch_ongoing = true;
					break;
				}
			}
			if (batch_ongoing) {
				break;
			}
			
			for (const auto &img : batch.resources.image2d_meta) {
				while (!img->array_references.empty()) {
					_write_one_descriptor_array_element(
						*img->array_references.back().array,
						recorded_resources::image2d_view(nullptr),
						img->array_references.back().index
					);
				}
				if (img->memory) {
					img->memory_pool->free(img->memory);
				}
			}

			for (const auto &img : batch.resources.image3d_meta) {
				if (img->memory) {
					img->memory_pool->free(img->memory);
				}
			}

			for (const auto &buf : batch.resources.buffer_meta) {
				while (!buf->array_references.empty()) {
					_write_one_descriptor_array_element(
						*buf->array_references.back().array,
						recorded_resources::structured_buffer_view(nullptr),
						buf->array_references.back().index
					);
				}
				if (buf->memory) {
					buf->memory_pool->free(buf->memory);
				}
			}

			constexpr bool _debug_resource_disposal = false;
			if constexpr (_debug_resource_disposal) {
				auto rsrc = std::move(_batch_data.front());
				for (auto &img : rsrc.resources.image2d_views) {
					img = nullptr;
				}
			}

			if (on_batch_statistics_available) {
				batch_statistics_late result = zero;
				
				for (std::size_t iq = 0; iq < _queues.size(); ++iq) { // read back timer information
					auto &queue_data = batch.resolve_data.queues[iq];
					double frequency = _queues[iq].queue.get_timestamp_frequency();

					auto &queue_res = result.timer_results.emplace_back();
					queue_res.reserve(queue_data.timers.size());

					if (queue_data.timestamp_heap) {
						std::vector<std::uint64_t> raw_results(static_cast<std::size_t>(queue_data.num_timestamps));
						_device.fetch_query_results(*queue_data.timestamp_heap, 0, raw_results);

						for (auto &tmr : queue_data.timers) {
							std::uint64_t ticks = raw_results[tmr.second_timestamp] - raw_results[tmr.first_timestamp];
							auto &res = queue_res.emplace_back(nullptr);
							/*res.name = std::move(tmr.name);*/
							res.duration_ms = static_cast<float>((ticks * 1000) / frequency);
						}
					}
				}

				const auto cur_batch_index = _batch_index - static_cast<std::uint32_t>(_batch_data.size());
				on_batch_statistics_available(cur_batch_index, std::move(result));
			}

			_batch_data.pop_front();
		}
	}
}
