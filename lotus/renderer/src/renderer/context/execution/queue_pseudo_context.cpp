#include "lotus/renderer/context/execution/queue_pseudo_context.h"

/// \file
/// Implementation of the per-queue pseudo-execution context.

#include "lotus/renderer/context/context.h"

namespace lotus::renderer::execution {
	queue_pseudo_context::queue_pseudo_context(batch_context &bctx, queue_context &qctx, _details::queue_data &q) :
		_batch_ctx(bctx),
		_queue_ctx(qctx),
		_q(q),
		_pseudo_acquired_dependencies(q.ctx.get_num_queues(), queue_submission_index::invalid) {

		// initialize queue data
		_get_queue_resolve_data().timers.resize(_q.num_timers, nullptr);
	}

	const command &queue_pseudo_context::get_next_pseudo_execution_command() const {
		return _q.batch_commands[std::to_underlying(_pseudo_cmd_index)];
	}

	void queue_pseudo_context::pseudo_execute_next_command() {
		std::visit(
			[&](const auto &cmd) {
				_q.ctx.execution_log(
					"PSEUDO_EXEC QUEUE[{}].COMMAND[{}]: {}(\"{}\")",
					_q.queue.get_index(), std::to_underlying(_pseudo_cmd_index), typeid(cmd).name(),
					string::to_generic(get_next_pseudo_execution_command().description.value_or(u8" - "))
				);
				_pseudo_execute(cmd);
				if (bit_mask::contains<commands::flags::advances_timer>(cmd.get_flags())) {
					_valid_timestamp = false;
				}
			},
			get_next_pseudo_execution_command().value
		);
		_pseudo_cmd_index = index::next(_pseudo_cmd_index);
	}

	bool queue_pseudo_context::is_pseudo_execution_blocked() const {
		const auto &cmd = get_next_pseudo_execution_command();
		if (std::holds_alternative<commands::acquire_dependency>(cmd.value)) {
			const auto *dep = std::get<commands::acquire_dependency>(cmd.value).target._ptr;
			return !dep->release_event.has_value();
		}
		return false;
	}

	bool queue_pseudo_context::is_pseudo_execution_finished() const {
		return std::to_underlying(_pseudo_cmd_index) >= _q.batch_commands.size();
	}

	void queue_pseudo_context::process_pseudo_release_events() {
		// sort and deduplicate release events
		std::sort(
			_pseudo_release_dependency_events.begin(),
			_pseudo_release_dependency_events.end(),
			[](
				release_dependency_event<_details::dependency*> lhs,
				release_dependency_event<_details::dependency*> rhs
			) {
				return lhs.command_index < rhs.command_index;
			}
		);

		// gather all release events
		for (std::size_t event_i = 0; event_i < _pseudo_release_dependency_events.size(); ++event_i) {
			const auto [cur_idx, cur_dep] = _pseudo_release_dependency_events[event_i];
			// check if we can discard this event
			bool can_discard = false;
			if (event_i > 0) {
				can_discard = true;
				const queue_submission_index prev_idx = _pseudo_release_dependency_events[event_i - 1].command_index;
				for (auto cmd_i = std::to_underlying(prev_idx); cmd_i < std::to_underlying(cur_idx); ++cmd_i) {
					const auto flags = _q.batch_commands[cmd_i].get_flags();
					if (!bit_mask::contains<commands::flags::dependency_release_unordered>(flags)) {
						can_discard = false;
						break;
					}
				}
			}
			if (!can_discard) {
				++_q.semaphore_value;
			}
			// set semaphore value for the dependency, if any
			if (cur_dep) {
				crash_if(!cur_dep->release_event.has_value());
				crash_if(cur_dep->release_value.has_value());
				cur_dep->release_value.emplace(_q.semaphore_value);
			}
			// schedule the release event
			if (!can_discard) {
				_queue_ctx._release_dependency_events.emplace_back(cur_idx, _q.semaphore_value);
			}
		}
	}

	void queue_pseudo_context::process_pseudo_acquire_events() {
		// push all command indices towards the back
		for (std::size_t event_i = 0; event_i < _pseudo_acquire_dependency_events.size(); ++event_i) {
			auto &event = _pseudo_acquire_dependency_events[event_i];
			// check that the indices are monotonically non-decreasing
			crash_if(
				event_i + 1 < _pseudo_acquire_dependency_events.size() &&
				_pseudo_acquire_dependency_events[event_i + 1].command_index < event.command_index
			);
			if (event_i > 0) { // reuse results from previous event
				event.command_index = std::max(
					event.command_index,
					_pseudo_acquire_dependency_events[event_i - 1].command_index
				);
			}
			// group together consecutive acquire events
			while (
				std::to_underlying(event.command_index) + 1 < _q.batch_commands.size() &&
				std::holds_alternative<commands::acquire_dependency>(
					_q.batch_commands[std::to_underlying(event.command_index)].value
				)
			) {
				event.command_index = index::next(event.command_index);
			}
		}

		// fetch actual semaphore values
		short_vector<std::size_t, 4> next_event_indices(_q.ctx.get_num_queues(), 0);
		for (const auto &event : _pseudo_acquire_dependency_events) {
			gpu::timeline_semaphore::value_type sem_value;
			if (event.data == queue_submission_index::zero) {
				// acquire from the end of the previous batch
				sem_value = _batch_ctx.get_batch_resolve_data().queues[event.queue_index].begin_of_batch;
			} else {
				const queue_context &other_qctx = _batch_ctx.get_queue_context(event.queue_index);
				const auto &other_queue_release_events = other_qctx._release_dependency_events;
				auto &next_event_index = next_event_indices[event.queue_index];
				// look for the first release event after the given command index
				while (
					next_event_index < other_queue_release_events.size() &&
					event.data > other_queue_release_events[next_event_index].command_index
				) {
					++next_event_index;
				}
				// we need to acquire the *previous* event
				crash_if(next_event_index == 0);
				sem_value = other_queue_release_events[next_event_index - 1].data;
			}
			_queue_ctx._acquire_dependency_events.emplace_back(event.queue_index, sem_value, event.command_index);
		}
	}

#pragma region _pseudo_execute()
	void queue_pseudo_context::_pseudo_execute(const commands::copy_buffer &cmd) {
		_pseudo_use_buffer(
			*cmd.source._ptr,
			_details::buffer_access(gpu::synchronization_point_mask::copy, gpu::buffer_access_mask::copy_source),
			_pseudo_execution_current_command_range()
		);
		_pseudo_use_buffer(
			*cmd.destination._ptr,
			_details::buffer_access(
				gpu::synchronization_point_mask::copy, gpu::buffer_access_mask::copy_destination
			),
			_pseudo_execution_current_command_range()
		);
	}

	void queue_pseudo_context::_pseudo_execute(const commands::copy_buffer_to_image &cmd) {
		_pseudo_use_buffer(
			*cmd.source._ptr,
			_details::buffer_access(gpu::synchronization_point_mask::copy, gpu::buffer_access_mask::copy_source),
			_pseudo_execution_current_command_range()
		);
		_pseudo_use_image(
			*cmd.destination._ptr,
			_details::image_access(
				gpu::subresource_range::nonarray_color(cmd.destination._mip_levels),
				gpu::synchronization_point_mask::copy,
				gpu::image_access_mask::copy_destination,
				gpu::image_layout::copy_destination
			),
			_pseudo_execution_current_command_range()
		);
	}

	void queue_pseudo_context::_pseudo_execute(const commands::build_blas &cmd) {
		std::vector<gpu::raytracing_geometry_view> gpu_geoms; // TODO allocator

		for (auto &input : cmd.geometry) {
			// mark input buffer memory usage
			_pseudo_use_buffer(
				*input.vertex_data._ptr,
				_details::buffer_access(
					gpu::synchronization_point_mask::acceleration_structure_build,
					gpu::buffer_access_mask::acceleration_structure_build_input
				),
				_pseudo_execution_current_command_range()
			);
			if (input.index_data) {
				_pseudo_use_buffer(
					*input.index_data._ptr,
					_details::buffer_access(
						gpu::synchronization_point_mask::acceleration_structure_build,
						gpu::buffer_access_mask::acceleration_structure_build_input
					),
					_pseudo_execution_current_command_range()
				);
			}

			// collect geometry without actual buffers
			gpu_geoms.emplace_back(
				batch_context::get_vertex_buffer_view(input),
				batch_context::get_index_buffer_view(input),
				input.flags
			);
		}

		// initialize memory for the BLAS
		auto baked_gpu_geoms = _q.ctx._device.create_bottom_level_acceleration_structure_geometry(gpu_geoms);
		const auto build_sizes = _q.ctx._device.get_bottom_level_acceleration_structure_build_sizes(baked_gpu_geoms);
		cmd.target._ptr->memory = _q.ctx._request_buffer_raw(
			u8"BLAS memory", // TODO more descriptive name
			build_sizes.acceleration_structure_size,
			gpu::buffer_usage_mask::acceleration_structure,
			cmd.target._ptr->memory_pool
		);
		_pseudo_use_buffer(
			*cmd.target._ptr->memory,
			_details::buffer_access(
				gpu::synchronization_point_mask::acceleration_structure_build,
				gpu::buffer_access_mask::acceleration_structure_write
			),
			_pseudo_execution_current_command_range()
		);

		// initialize BLAS
		cmd.target._ptr->handle = _q.ctx._device.create_bottom_level_acceleration_structure(
			cmd.target._ptr->memory->data, 0, build_sizes.acceleration_structure_size
		);
	}

	void queue_pseudo_context::_pseudo_execute(const commands::build_tlas &cmd) {
		for (const auto &input : cmd.instances) {
			_pseudo_use_buffer(
				*input.acceleration_structure._ptr->memory,
				_details::buffer_access(
					gpu::synchronization_point_mask::acceleration_structure_build,
					gpu::buffer_access_mask::acceleration_structure_build_input
				),
				_pseudo_execution_current_command_range()
			);
		}

		// initialize memory for the TLAS
		const auto build_sizes =
			_q.ctx._device.get_top_level_acceleration_structure_build_sizes(cmd.instances.size());
		cmd.target._ptr->memory = _q.ctx._request_buffer_raw(
			u8"TLAS memory", // TODO more descriptive name
			build_sizes.acceleration_structure_size,
			gpu::buffer_usage_mask::acceleration_structure,
			cmd.target._ptr->memory_pool
		);
		_pseudo_use_buffer(
			*cmd.target._ptr->memory,
			_details::buffer_access(
				gpu::synchronization_point_mask::acceleration_structure_build,
				gpu::buffer_access_mask::acceleration_structure_write
			),
			_pseudo_execution_current_command_range()
		);

		// initialize TLAS
		cmd.target._ptr->handle = _q.ctx._device.create_top_level_acceleration_structure(
			cmd.target._ptr->memory->data, 0, build_sizes.acceleration_structure_size
		);
	}

	void queue_pseudo_context::_pseudo_execute(const commands::begin_pass &cmd) {
		// figure out the range of commands in this pass
		_queue_submission_range scope(_pseudo_cmd_index, index::next(_pseudo_cmd_index));
		while (!std::holds_alternative<commands::end_pass>(_q.batch_commands[std::to_underlying(scope.end)].value)) {
			scope.end = index::next(scope.end);
			crash_if(std::to_underlying(scope.end) >= _q.batch_commands.size());
		}

		// process the render targets
		for (const auto &rt : cmd.color_render_targets) {
			if (std::holds_alternative<recorded_resources::image2d_view>(rt.view)) {
				const auto &img = std::get<recorded_resources::image2d_view>(rt.view);
				_pseudo_use_image(
					*img._ptr,
					_details::image_access(
						gpu::subresource_range::nonarray_color(img._mip_levels),
						gpu::synchronization_point_mask::all_graphics,
						gpu::image_access_mask::color_render_target,
						gpu::image_layout::color_render_target
					),
					scope
				);
			} else if (std::holds_alternative<recorded_resources::swap_chain>(rt.view)) {
				const auto &chain = std::get<recorded_resources::swap_chain>(rt.view);
				_pseudo_use_swap_chain(
					*chain._ptr,
					_details::image_access(
						gpu::subresource_range::first_color(),
						gpu::synchronization_point_mask::all_graphics,
						gpu::image_access_mask::color_render_target,
						gpu::image_layout::color_render_target
					),
					scope
				);
			} else {
				std::abort(); // not handled
			}
		}
		if (cmd.depth_stencil_target.view) {
			_pseudo_use_image(
				*cmd.depth_stencil_target.view._ptr,
				_details::image_access(
					gpu::subresource_range::nonarray_depth_stencil(cmd.depth_stencil_target.view._mip_levels),
					gpu::synchronization_point_mask::all_graphics,
					gpu::image_access_mask::depth_stencil_read_write, // TODO: use read_only for these two when possible
					gpu::image_layout::depth_stencil_read_write
				),
				scope
			);
		}

		// process draw commands
		for (auto cmd_i = index::next(scope.begin); cmd_i != scope.end; cmd_i = index::next(cmd_i)) {
			auto &pass_cmd = _q.batch_commands[std::to_underlying(cmd_i)];
			auto &pass_cmd_union = pass_cmd.value;
			if (std::holds_alternative<commands::draw_instanced>(pass_cmd_union)) {
				_q.ctx.execution_log(
					"  USE_DRAW_CMD_RESOURCES \"{}\"", string::to_generic(pass_cmd.description.value_or(u8" - "))
				);

				auto &draw_cmd = std::get<commands::draw_instanced>(pass_cmd_union);
				for (const auto &input : draw_cmd.inputs) {
					_pseudo_use_buffer(
						*input.data._ptr,
						_details::buffer_access(
							gpu::synchronization_point_mask::vertex_input, gpu::buffer_access_mask::vertex_buffer
						),
						scope
					);
				}
				if (draw_cmd.index_buffer.data) {
					_pseudo_use_buffer(
						*draw_cmd.index_buffer.data._ptr,
						_details::buffer_access(
							gpu::synchronization_point_mask::index_input, gpu::buffer_access_mask::index_buffer
						),
						scope
					);
				}
				_pseudo_use_resource_sets(
					draw_cmd.resource_bindings,
					// TODO maybe narrow this down?
					gpu::synchronization_point_mask::vertex_shader | gpu::synchronization_point_mask::pixel_shader,
					scope
				);
			} else {
				// ensure that all command types are handled
				crash_if(!(
					std::holds_alternative<commands::start_timer>(pass_cmd_union) ||
					std::holds_alternative<commands::end_timer>(pass_cmd_union)   ||
					std::holds_alternative<commands::pause_for_debugging>(pass_cmd_union)
				));
			}
		}

		// set next command index - will be incremented in pseudo_execute_next_command()
		_pseudo_cmd_index = scope.end;
	}

	void queue_pseudo_context::_pseudo_execute(const commands::dispatch_compute &cmd) {
		_pseudo_use_resource_sets(
			cmd.resources, gpu::synchronization_point_mask::compute_shader, _pseudo_execution_current_command_range()
		);
	}
	
	void queue_pseudo_context::_pseudo_execute(const commands::trace_rays &cmd) {
		_pseudo_use_resource_sets(
			cmd.resource_bindings, gpu::synchronization_point_mask::raytracing, _pseudo_execution_current_command_range()
		);
	}

	void queue_pseudo_context::_pseudo_execute(const commands::present &cmd) {
		_pseudo_use_swap_chain(
			*cmd.target._ptr,
			_details::image_access(
				gpu::subresource_range::first_color(),
				gpu::synchronization_point_mask::none,
				gpu::image_access_mask::none,
				gpu::image_layout::present
			),
			_pseudo_execution_current_command_range()
		);
	}

	void queue_pseudo_context::_pseudo_execute(const commands::release_dependency &cmd) {
		auto *dep = cmd.target._ptr;
		crash_if(dep->release_event.has_value()); // cannot release multiple times
		dep->release_event.emplace(_get_queue_index(), _batch_ctx.get_batch_index(), _pseudo_cmd_index);
		// explicit in case this dependency is acquired in a later batch
		_pseudo_release_dependency_events.emplace_back(_pseudo_cmd_index, dep);
	}

	void queue_pseudo_context::_pseudo_execute(const commands::acquire_dependency &cmd) {
		auto *dep = cmd.target._ptr;
		// pseudo execution should have guaranteed that this dependency has been released
		crash_if(!dep->release_event.has_value());
		_request_dependency_from(
			dep->release_event->queue,
			dep->release_event->batch,
			dep->release_event->command_index,
			_pseudo_cmd_index
		);
	}

	void queue_pseudo_context::_pseudo_execute(const commands::start_timer &cmd) {
		_get_queue_resolve_data().timers[std::to_underlying(cmd.index)].first_timestamp = _maybe_insert_timestamp();
	}

	void queue_pseudo_context::_pseudo_execute(const commands::end_timer &cmd) {
		_get_queue_resolve_data().timers[std::to_underlying(cmd.index)].second_timestamp = _maybe_insert_timestamp();
	}
#pragma endregion

	void queue_pseudo_context::_pseudo_use_resource_sets(
		_details::numbered_bindings_view sets,
		gpu::synchronization_point_mask sync_points,
		_queue_submission_range scope
	) {
		for (const auto &set : sets) {
			std::visit(
				[&](auto &&set) {
					_pseudo_use_resource_set(set, sync_points, scope);
				},
				set.value
			);
		}
	}

	void queue_pseudo_context::_pseudo_use_resource_set(
		std::span<const all_resource_bindings::numbered_binding> bindings,
		gpu::synchronization_point_mask sync_points,
		_queue_submission_range scope
	) {
		_q.ctx.execution_log("  USE_BINDINGS");

		for (const auto &binding : bindings) {
			std::visit(
				[&](auto &&binding) {
					_pseudo_use_resource(binding, sync_points, scope);
				},
				binding.value
			);
		}
	}

	/// Checks that the resource references inside a descriptor array are valid.
	template <typename RecordedResource, typename View> static void _maybe_validate_descriptor_array(
		const _details::descriptor_array<RecordedResource, View> &arr
	) {
		constexpr bool _validate_descriptor_array = false;
		if constexpr (_validate_descriptor_array) {
			for (std::size_t i = 0; i < arr.resources.size(); ++i) {
				const auto &rsrc = arr.resources[i];
				if (rsrc.resource) {
					crash_if(rsrc.resource._ptr->array_references.size() <= rsrc.reference_index);
					crash_if(rsrc.resource._ptr->array_references[rsrc.reference_index].array != &arr);
					crash_if(rsrc.resource._ptr->array_references[rsrc.reference_index].index != i);
				}
			}
		}
	}

	void queue_pseudo_context::_pseudo_use_resource_set(
		recorded_resources::image_descriptor_array arr,
		gpu::synchronization_point_mask sync_points,
		_queue_submission_range scope
	) {
		_q.ctx.execution_log("  USE_IMG_DSCRPTR_ARRAY \"{}\"", string::to_generic(arr._ptr->name));

		_maybe_validate_descriptor_array(*arr._ptr);
		_q.ctx._maybe_initialize_descriptor_array(*arr._ptr);
		_q.ctx._flush_descriptor_array_writes(*arr._ptr);

		for (const auto &rsrc : arr._ptr->resources) {
			if (rsrc.resource) {
				_pseudo_use_image(
					*rsrc.resource._ptr,
					_details::image_access(
						gpu::subresource_range::nonarray_color(rsrc.resource._mip_levels),
						sync_points,
						gpu::image_access_mask::shader_read,
						gpu::image_layout::shader_read_only
					),
					scope
				);
			}
		}
	}

	void queue_pseudo_context::_pseudo_use_resource_set(
		recorded_resources::buffer_descriptor_array arr,
		gpu::synchronization_point_mask sync_points,
		_queue_submission_range scope
	) {
		_q.ctx.execution_log("  USE_BUF_DSCRPTR_ARRAY \"{}\"", string::to_generic(arr._ptr->name));

		_maybe_validate_descriptor_array(*arr._ptr);
		_q.ctx._maybe_initialize_descriptor_array(*arr._ptr);
		_q.ctx._flush_descriptor_array_writes(*arr._ptr);

		for (const auto &rsrc : arr._ptr->resources) {
			if (rsrc.resource) {
				_pseudo_use_buffer(
					*rsrc.resource._ptr,
					_details::buffer_access(sync_points, gpu::buffer_access_mask::shader_read),
					scope
				);
			}
		}
	}

	void queue_pseudo_context::_pseudo_use_resource_set(
		recorded_resources::cached_descriptor_set set,
		gpu::synchronization_point_mask sync_points,
		_queue_submission_range scope
	) {
		_q.ctx.execution_log("  USE_CACHED_DSCRPTR_SET \"{}\"", string::to_generic(set._ptr->name));

		_q.ctx._maybe_initialize_cached_descriptor_set(*set._ptr);

		for (const auto &img : set._ptr->used_image2ds) {
			_pseudo_use_image(*img.image, img.get_image_access(sync_points), scope);
		}
		for (const auto &img : set._ptr->used_image3ds) {
			_pseudo_use_image(*img.image, img.get_image_access(sync_points), scope);
		}
		for (const auto &buf : set._ptr->used_buffers) {
			_pseudo_use_buffer(*buf.buffer, buf.get_buffer_access(sync_points), scope);
		}
	}

	void queue_pseudo_context::_pseudo_use_resource(
		const descriptor_resource::image2d &img,
		gpu::synchronization_point_mask sync_points,
		_queue_submission_range scope
	) {
		_pseudo_use_image(
			*img.view._ptr,
			_details::image_access::from_binding_type(
				gpu::subresource_range::nonarray_color(img.view._mip_levels), sync_points, img.binding_type
			),
			scope
		);
	}

	void queue_pseudo_context::_pseudo_use_resource(
		const descriptor_resource::image3d &img,
		gpu::synchronization_point_mask sync_points,
		_queue_submission_range scope
	) {
		_pseudo_use_image(
			*img.view._ptr,
			_details::image_access::from_binding_type(
				gpu::subresource_range::nonarray_color(img.view._mip_levels), sync_points, img.binding_type
			),
			scope
		);
	}

	void queue_pseudo_context::_pseudo_use_resource(
		const descriptor_resource::swap_chain &chain,
		gpu::synchronization_point_mask sync_points,
		_queue_submission_range scope
	) {
		_pseudo_use_swap_chain(
			*chain.chain._ptr,
			_details::image_access::from_binding_type(
				gpu::subresource_range::first_color(), sync_points, chain.binding_type
			),
			scope
		);
	}

	void queue_pseudo_context::_pseudo_use_resource(
		const descriptor_resource::constant_buffer &buf,
		gpu::synchronization_point_mask sync_points,
		_queue_submission_range scope
	) {
		_pseudo_use_buffer(
			*buf.data._ptr, _details::buffer_access(sync_points, gpu::buffer_access_mask::constant_buffer), scope
		);
	}

	void queue_pseudo_context::_pseudo_use_resource(
		const descriptor_resource::structured_buffer &buf,
		gpu::synchronization_point_mask sync_points,
		_queue_submission_range scope
	) {
		_pseudo_use_buffer(
			*buf.data._ptr, _details::buffer_access::from_binding_type(sync_points, buf.binding_type), scope
		);
	}

	void queue_pseudo_context::_pseudo_use_resource(
		const recorded_resources::tlas &buf,
		gpu::synchronization_point_mask sync_points,
		_queue_submission_range scope
	) {
		_pseudo_use_buffer(
			*buf._ptr->memory,
			_details::buffer_access(sync_points, gpu::buffer_access_mask::acceleration_structure_read),
			scope
		);
	}

	void queue_pseudo_context::_transition_buffer_here(
		_details::buffer &target, _details::buffer_access from, _details::buffer_access to
	) {
		_queue_ctx._buffer_transitions[std::to_underlying(_pseudo_cmd_index)].emplace_back(
			target.data, from.sync_points, from.access, to.sync_points, to.access
		);
	}

	void queue_pseudo_context::_pseudo_use_buffer(
		_details::buffer &buf, _details::buffer_access new_access, _queue_submission_range scope
	) {
		_q.ctx.execution_log(
			"    USE_BUFFER \"{}\", SYNC_POINTS {}, ACCESS {}",
			string::to_generic(buf.name),
			new_access.sync_points,
			new_access.access
		);

		_q.ctx._maybe_initialize_buffer(buf);

		const _details::buffer_access_event event(new_access, _batch_ctx.get_batch_index(), scope.end);

		if (bit_mask::contains_any(new_access.access, gpu::buffer_access_mask::write_bits)) {
			// write access - wait until all previous reads have finished
			for (std::uint32_t i = 0; i < _get_num_queues(); ++i) {
				const auto &access = buf.previous_queue_access[i];
				if (access) {
					if (i == _get_queue_index()) { // same queue - only need transition
						_transition_buffer_here(buf, access->access, new_access);
					} else { // insert dependency
						_request_dependency_from(i, access->batch, access->queue_index, scope.begin);
					}
				}
			}

			// also record as last write
			buf.previous_modification = { _get_queue_index(), event };
		} else { // read-only access - only need to wait for previous write
			const auto &[queue_idx, write_event] = buf.previous_modification;
			const bool is_cpu_write = write_event.access.access == gpu::buffer_access_mask::cpu_write;

			// must have been written to before
			crash_if(write_event.access.access == gpu::buffer_access_mask::none);
			// CPU write cannot coexist with other read or write access
			crash_if(
				!is_cpu_write &&
				bit_mask::contains<gpu::buffer_access_mask::cpu_write>(write_event.access.access)
			);

			if (is_cpu_write || queue_idx == _get_queue_index()) { // only need transition
				const auto &prev_read = buf.previous_queue_access[_get_queue_index()];
				if (prev_read && prev_read->queue_index > write_event.queue_index) {
					// we've read on the same queue before
					// TODO: is a transition still necessary?
					_transition_buffer_here(buf, prev_read->access, new_access);
				} else {
					_transition_buffer_here(buf, write_event.access, new_access);
				}
			} else { // need dependency
				_request_dependency_from(queue_idx, write_event.batch, write_event.queue_index, scope.begin);
			}
		}
		buf.previous_queue_access[_get_queue_index()] = event;
	}

	void queue_pseudo_context::_pseudo_use_image(
		_details::image2d &img, _details::image_access access, _queue_submission_range scope
	) {
		_q.ctx._maybe_initialize_image(img);
		_pseudo_use_image_impl(img, access, scope);
	}

	void queue_pseudo_context::_pseudo_use_image(
		_details::image3d &img, _details::image_access access, _queue_submission_range scope
	) {
		_q.ctx._maybe_initialize_image(img);
		_pseudo_use_image_impl(img, access, scope);
	}

	void queue_pseudo_context::_pseudo_use_image_impl(
		_details::image_base &img, _details::image_access access, _queue_submission_range scope
	) {
		_q.ctx.execution_log(
			"    USE_IMAGE SYNC_POINTS {}, ACCESS {}, LAYOUT {}, "
			"MIPS [{}, +{}), ARRAY_SLICES [{}, +{}), ASPECTS {}, \"{}\"",
			access.sync_points,
			access.access,
			access.layout,
			access.subresource_range.mips.first_level,
			access.subresource_range.mips.num_levels,
			access.subresource_range.first_array_slice,
			access.subresource_range.num_array_slices,
			access.subresource_range.aspects,
			string::to_generic(img.name)
		);

		const _details::image_access_event event(access, _batch_ctx.get_batch_index(), scope.end);

		const bool is_write_access = bit_mask::contains_any(access.access, gpu::image_access_mask::write_bits);
		for (std::uint32_t i = 0; i < _get_num_queues(); ++i) {

		}
		if (is_write_access) {
			// write access - wait until all previous reads have finished
		} else { // read-only access
			// TODO: layout changes?
		}
	}

	void queue_pseudo_context::_pseudo_use_swap_chain(
		_details::swap_chain &chain, _details::image_access access, _queue_submission_range scope
	) {
		_q.ctx.execution_log(
			"    USE_SWAP_CHAIN SYNC_POINTS {}, ACCESS {}, LAYOUT {}, \"{}\"",
			access.sync_points,
			access.access,
			access.layout,
			string::to_generic(chain.name)
		);

		if (access.layout == gpu::image_layout::present) {
			crash_if(chain.next_image_index == _details::swap_chain::invalid_image_index); // must be acquired
			crash_if(_q.queue.get_index() != chain.queue_index); // must be presented on the specified queue
			crash_if(chain.previous_present == _q.ctx._batch_index); // cannot present twice in the same batch
			// update batch index
			chain.previous_present = _q.ctx._batch_index;
			_batch_ctx.mark_swap_chain_presented(chain);
		} else {
			_q.ctx._maybe_update_swap_chain(chain);
		}

		// TODO stage transitions
		/*std::abort();*/
	}

	void queue_pseudo_context::_request_dependency_from(
		std::uint32_t release_queue, batch_index release_batch, queue_submission_index release_after,
		queue_submission_index acquire_before
	) {
		_batch_ctx.request_dependency(
			release_queue, release_batch, release_after, _get_queue_index(), acquire_before
		);
	}

	std::uint32_t queue_pseudo_context::_maybe_insert_timestamp() {
		if (!_valid_timestamp) {
			_queue_ctx._timestamp_command_indices.emplace_back(_pseudo_cmd_index);
			_valid_timestamp = true;
		}
		return static_cast<std::uint32_t>(_queue_ctx._timestamp_command_indices.size() - 1);
	}

	std::uint32_t queue_pseudo_context::_get_num_queues() const {
		return _q.ctx.get_num_queues();
	}

	std::uint32_t queue_pseudo_context::_get_queue_index() const {
		return _q.queue.get_index();
	}

	batch_resolve_data::queue_data &queue_pseudo_context::_get_queue_resolve_data() {
		return _batch_ctx.get_batch_resolve_data().queues[_get_queue_index()];
	}
}
