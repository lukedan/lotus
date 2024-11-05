#include "lotus/renderer/context/execution/queue_pseudo_context.h"

/// \file
/// Implementation of the per-queue pseudo-execution context.

#include "lotus/utils/misc.h"
#include "lotus/renderer/context/context.h"

namespace lotus::renderer::execution {
	queue_pseudo_context::queue_pseudo_context(batch_context &bctx, queue_context &qctx, _details::queue_data &q) :
		_batch_ctx(bctx),
		_queue_ctx(qctx),
		_q(q),
		_cmd_ops(q.batch_commands.size()) {

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

	void queue_pseudo_context::process_dependency_acquisitions() {
		short_vector<queue_submission_index, 4> dep_acquired_batch(_get_num_queues(), queue_submission_index::invalid);
		short_vector<gpu::timeline_semaphore::value_type, 4> dep_acquired_prev(_get_num_queues(), 0);

		for (std::size_t i = 0; i < _q.batch_commands.size(); ++i) {
			auto &actions = _cmd_ops[i].acquire_dependencies;
			actions.resize(_get_num_queues(), std::nullopt);

			// go through all dependency events to figure out which single command we should acquire dependency from
			// in all queues
			for (const _dependency_acquisition &acq : _cmd_ops[i].acquire_dependency_requests) {
				auto &queue_action = actions[acq.queue_index];
				std::visit(overloaded{
					[&](queue_submission_index si) {
						// check if we've already acquired the dependency
						if (
							dep_acquired_batch[acq.queue_index] != queue_submission_index::invalid &&
							dep_acquired_batch[acq.queue_index] >= si
						) {
							return;
						}

						const queue_submission_index acquire_index =
							std::holds_alternative<queue_submission_index>(queue_action) ?
							// we've already acquired a dependency from a command in this batch
							// acquire from the later of the two commands
							std::max(std::get<queue_submission_index>(queue_action), si) :
							// either we've not acquired a dependency or we've acquired one from a previous batch
							// the dependency from this batch always takes priority
							si;
								
						queue_action.emplace<queue_submission_index>(acquire_index);
						dep_acquired_batch[acq.queue_index] = acquire_index;
					},
					[&](gpu::timeline_semaphore::value_type value) {
						// if we've acquired from this batch, we don't need to acquire from previous batches
						if (dep_acquired_batch[acq.queue_index] != queue_submission_index::invalid) {
							return;
						}
						// check if we've already acquired the dependency
						if (dep_acquired_prev[acq.queue_index] >= value) {
							return;
						}

						queue_action.emplace<gpu::timeline_semaphore::value_type>(value);
						dep_acquired_prev[acq.queue_index] = value;
					}
				}, acq.target);
			}

			// mark all commands that need to release a dependency
			for (std::uint32_t qi = 0; qi < _get_num_queues(); ++qi) {
				std::visit(overloaded{
					[&](queue_submission_index qsi) {
						auto &cmd = _batch_ctx.get_queue_pseudo_context(qi)._cmd_ops[std::to_underlying(qsi)];
						cmd.release_dependency = true;
					},
					[](auto&) {
						// either no dependency, or it's from a previous batch; ignored
					}
				}, actions[qi]);
			}
		}
	}

	void queue_pseudo_context::gather_semaphore_values() {
		gpu::timeline_semaphore::value_type &value = _q.semaphore_value;
		queue_context &qctx = _batch_ctx.get_queue_context(_get_queue_index());
		for (std::size_t i = 0; i < _q.batch_commands.size(); ++i) {
			if (_cmd_ops[i].release_dependency) {
				qctx._cmd_ops[i].release_dependency = ++value;
			}
		}
	}

	void queue_pseudo_context::finalize_dependency_processing() {
		for (std::size_t i = 0; i < _q.batch_commands.size(); ++i) {
			auto &batch_cmd_op = _batch_ctx.get_queue_context(_get_queue_index())._cmd_ops[i];
			batch_cmd_op.acquire_dependencies.resize(_get_num_queues(), 0);
			for (std::uint32_t qi = 0; qi < _get_num_queues(); ++qi) {
				std::visit(overloaded{
					[&](gpu::timeline_semaphore::value_type val) {
						batch_cmd_op.acquire_dependencies[qi] = val;
					},
					[&](queue_submission_index qsi) {
						const auto &src_cmd = _batch_ctx.get_queue_context(qi)._cmd_ops[std::to_underlying(qsi)];
						batch_cmd_op.acquire_dependencies[qi] = src_cmd.release_dependency.value();
					},
					[](std::nullopt_t) {
						// nothing to handle
					}
				}, _cmd_ops[i].acquire_dependencies[qi]);
			}
		}
	}

#pragma region _pseudo_execute()
	void queue_pseudo_context::_pseudo_execute(const commands::copy_buffer &cmd) {
		_pseudo_use_buffer(
			*cmd.source._ptr,
			gpu::synchronization_point_mask::copy,
			gpu::buffer_access_mask::copy_source,
			_pseudo_execution_current_command_range()
		);
		_pseudo_use_buffer(
			*cmd.destination._ptr,
			gpu::synchronization_point_mask::copy,
			gpu::buffer_access_mask::copy_destination,
			_pseudo_execution_current_command_range()
		);
	}

	void queue_pseudo_context::_pseudo_execute(const commands::copy_buffer_to_image &cmd) {
		_pseudo_use_buffer(
			*cmd.source._ptr,
			gpu::synchronization_point_mask::copy,
			gpu::buffer_access_mask::copy_source,
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
				gpu::synchronization_point_mask::acceleration_structure_build,
				gpu::buffer_access_mask::acceleration_structure_build_input,
				_pseudo_execution_current_command_range()
			);
			if (input.index_data) {
				_pseudo_use_buffer(
					*input.index_data._ptr,
					gpu::synchronization_point_mask::acceleration_structure_build,
					gpu::buffer_access_mask::acceleration_structure_build_input,
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
			gpu::synchronization_point_mask::acceleration_structure_build,
			gpu::buffer_access_mask::acceleration_structure_write,
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
				gpu::synchronization_point_mask::acceleration_structure_build,
				gpu::buffer_access_mask::acceleration_structure_build_input,
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
			gpu::synchronization_point_mask::acceleration_structure_build,
			gpu::buffer_access_mask::acceleration_structure_write,
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
						gpu::synchronization_point_mask::vertex_input,
						gpu::buffer_access_mask::vertex_buffer,
						scope
					);
				}
				if (draw_cmd.index_buffer.data) {
					_pseudo_use_buffer(
						*draw_cmd.index_buffer.data._ptr,
						gpu::synchronization_point_mask::index_input,
						gpu::buffer_access_mask::index_buffer,
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
	}

	void queue_pseudo_context::_pseudo_execute(const commands::acquire_dependency &cmd) {
		auto *dep = cmd.target._ptr;
		// pseudo execution should have guaranteed that this dependency has been released
		crash_if(!dep->release_event.has_value());
		if (dep->release_event->batch != _batch_ctx.get_batch_index()) {
			// explicitly acquire from a previous batch
			_batch_ctx.request_dependency_explicit(
				dep->release_event->queue,
				dep->release_value.value(),
				_get_queue_index(),
				_pseudo_cmd_index
			);
		} else {
			// acquire from the same batch
			_batch_ctx.request_dependency_from_this_batch(
				dep->release_event->queue,
				dep->release_event->command_index,
				_get_queue_index(),
				_pseudo_cmd_index
			);
		}
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
					*rsrc.resource._ptr, sync_points, gpu::buffer_access_mask::shader_read, scope
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
			_pseudo_use_buffer(*buf.buffer, sync_points, buf.access, scope);
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
		_pseudo_use_buffer(*buf.data._ptr, sync_points, gpu::buffer_access_mask::constant_buffer, scope);
	}

	void queue_pseudo_context::_pseudo_use_resource(
		const descriptor_resource::structured_buffer &buf,
		gpu::synchronization_point_mask sync_points,
		_queue_submission_range scope
	) {
		_pseudo_use_buffer(*buf.data._ptr, sync_points, _details::to_access_mask(buf.binding_type), scope);
	}

	void queue_pseudo_context::_pseudo_use_resource(
		const recorded_resources::tlas &buf,
		gpu::synchronization_point_mask sync_points,
		_queue_submission_range scope
	) {
		_pseudo_use_buffer(
			*buf._ptr->memory, sync_points, gpu::buffer_access_mask::acceleration_structure_read, scope
		);
	}

	void queue_pseudo_context::_pseudo_use_buffer(
		_details::buffer &buf,
		gpu::synchronization_point_mask sync_points,
		gpu::buffer_access_mask access,
		_queue_submission_range scope
	) {
		_q.ctx.execution_log(
			"    USE_BUFFER \"{}\", SYNC_POINTS {}, ACCESS {}",
			string::to_generic(buf.name),
			sync_points,
			access
		);

		_q.ctx._maybe_initialize_buffer(buf);

		const gpu::buffer_barrier barrier(
			buf.data,
			buf.previous_access.access.sync_points,
			buf.previous_access.access.access,
			_batch_ctx.get_queue_context(buf.previous_access.queue_index)._q.queue.get_family(),
			sync_points,
			access,
			_q.queue.get_family()
		);

		if (
			buf.previous_access.batch == batch_index::zero ||
			_get_queue_index() == buf.previous_access.queue_index
		) {
			// same queue - only needs transition
			_queue_ctx
				._cmd_ops[std::to_underlying(scope.begin)]
				.pre_buffer_transitions.emplace_back(barrier);
		} else {
			// needs both a dependency and a queue family transfer
			if (buf.previous_access.batch != _batch_ctx.get_batch_index()) {
				/*std::abort();*/ // TODO
			} else {
				_batch_ctx.request_dependency_from_this_batch(
					buf.previous_access.queue_index,
					buf.previous_access.queue_command_index,
					_get_queue_index(),
					scope.begin
				);

				// queue family ownership release
				_batch_ctx
					.get_queue_context(buf.previous_access.queue_index)
					._cmd_ops[std::to_underlying(buf.previous_access.queue_command_index)]
					.post_buffer_transitions.emplace_back(barrier);
				// queue family ownership acquire
				_queue_ctx._cmd_ops[std::to_underlying(scope.begin)].pre_buffer_transitions.emplace_back(barrier);
			}
		}

		// update access
		buf.previous_access.access              = _details::buffer_access(sync_points, access);
		buf.previous_access.queue_index         = _get_queue_index();
		buf.previous_access.batch               = _batch_ctx.get_batch_index();
		buf.previous_access.queue_command_index = scope.end;
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

		// update each mip and array slice individually
		for (std::uint32_t array_slice = 0; array_slice < 1; ++array_slice) { // TODO: array slices
			for (std::uint32_t mip = 0; mip < img.num_mips; ++mip) {
				_details::image_access_event &prev_access = img.previous_access[array_slice][mip];

				const gpu::image_barrier barrier(
					gpu::subresource_range(gpu::mip_levels::only(mip), array_slice, 1, gpu::image_aspect_mask::color), // TODO: correct aspect
					img.get_image(),
					prev_access.access.sync_points,
					prev_access.access.access,
					prev_access.access.layout,
					_batch_ctx.get_queue_context(prev_access.queue_index)._q.queue.get_family(),
					access.sync_points,
					access.access,
					access.layout,
					_q.queue.get_family()
				);

				if (prev_access.batch == batch_index::zero || prev_access.queue_index == _get_queue_index()) {
					_queue_ctx._cmd_ops[std::to_underlying(scope.begin)].pre_image_transitions.emplace_back(barrier);
				} else {
					if (prev_access.batch != _batch_ctx.get_batch_index()) {
						/*std::abort();*/ // TODO
					} else {
						_batch_ctx.request_dependency_from_this_batch(
							prev_access.queue_index,
							prev_access.queue_command_index,
							_get_queue_index(),
							scope.begin
						);

						// queue family ownership release
						_batch_ctx
							.get_queue_context(prev_access.queue_index)
							._cmd_ops[std::to_underlying(prev_access.queue_command_index)]
							.post_image_transitions.emplace_back(barrier);
						// queue family ownership acquire
						_queue_ctx
							._cmd_ops[std::to_underlying(scope.begin)]
							.pre_image_transitions.emplace_back(barrier);
					}
				}

				prev_access.access              = access;
				prev_access.queue_index         = _get_queue_index();
				prev_access.batch               = _batch_ctx.get_batch_index();
				prev_access.queue_command_index = scope.end;
			}
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

		// TODO properly stage transitions
		auto &back_buffer = chain.back_buffers[chain.next_image_index];
		_queue_ctx._cmd_ops[std::to_underlying(scope.begin)].pre_image_transitions.emplace_back(
			gpu::subresource_range::first_color(),
			back_buffer.image,
			back_buffer.current_usage.sync_points,
			back_buffer.current_usage.access,
			back_buffer.current_usage.layout,
			_q.queue.get_family(),
			access.sync_points,
			access.access,
			access.layout,
			_q.queue.get_family()
		);
		back_buffer.current_usage = access;
		/*std::abort();*/
	}

	std::uint32_t queue_pseudo_context::_maybe_insert_timestamp() {
		/*
		if (!_valid_timestamp) {
			_queue_ctx._timestamp_command_indices.emplace_back(_pseudo_cmd_index);
			_valid_timestamp = true;
		}
		return static_cast<std::uint32_t>(_queue_ctx._timestamp_command_indices.size() - 1);
		*/
		return 0;
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
