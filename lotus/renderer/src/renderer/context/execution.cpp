#include "lotus/renderer/context/execution.h"

/// \file
/// Implementation of command execution related functions.

#include "lotus/renderer/context/context.h"

namespace lotus::renderer {
	upload_buffers::result upload_buffers::stage(std::span<const std::byte> data, std::size_t alignment) {
		auto &dev = renderer::context::device_access::get(*_context);
		if (data.size() > _buffer_size) {
			auto &buf = allocate_buffer(data.size());
			auto *ptr = dev.map_buffer(buf, 0, 0);
			std::memcpy(ptr, data.data(), data.size());
			dev.unmap_buffer(buf, 0, data.size());
			return result(buf, 0, allocation_type::individual_buffer);
		}

		crash_if(!_current && _current_used != 0);

		allocation_type type = allocation_type::same_buffer;
		std::size_t alloc_start = memory::align_up(_current_used, alignment);
		std::size_t new_used = alloc_start + data.size();
		if (new_used > _buffer_size) { // flush
			dev.unmap_buffer(*_current, 0, _current_used);
			_current = nullptr;
			_current_used = 0;
			alloc_start = 0;
			new_used = data.size();
		}
		if (!_current) {
			_current = &allocate_buffer(_buffer_size);
			_current_ptr = dev.map_buffer(*_current, 0, 0);
			type = allocation_type::new_buffer;
		}
		std::memcpy(_current_ptr + alloc_start, data.data(), data.size());
		_current_used = new_used;
		return result(*_current, static_cast<std::uint32_t>(alloc_start), type);
	}

	void upload_buffers::flush() {
		if (_current) {
			renderer::context::device_access::get(*_context).unmap_buffer(*_current, 0, _current_used);
			_current = nullptr;
			_current_used = 0;
		}
	}
}


namespace lotus::renderer::execution {
	void transition_buffer::stage_transition(
		_details::image2d &img, gpu::mip_levels mips, _details::image_access access
	) {
		_image2d_transitions.emplace_back(img, mips, access);
		for (const auto &ref : img.array_references) {
			const auto &arr_img = ref.array->resources[ref.index].resource;
			crash_if(arr_img._ptr != &img);
			if (!gpu::mip_levels::intersection(arr_img._mip_levels, mips).is_empty()) {
				ref.array->staged_transitions.emplace_back(ref.index);
			}
		}
	}

	void transition_buffer::stage_transition(
		_details::image3d &img, gpu::mip_levels mips, _details::image_access access
	) {
		_image3d_transitions.emplace_back(img, mips, access);
	}

	void transition_buffer::stage_transition(_details::buffer &buf, _details::buffer_access access) {
		_buffer_transitions.emplace_back(buf, access);
		for (const auto &ref : buf.array_references) {
			crash_if(ref.array->resources[ref.index].resource._ptr != &buf);
			ref.array->staged_transitions.emplace_back(ref.index);
		}
	}

	void transition_buffer::stage_all_transitions_for(_details::image_descriptor_array &arr) {
		auto transitions = std::exchange(arr.staged_transitions, {});
		std::sort(transitions.begin(), transitions.end());
		transitions.erase(std::unique(transitions.begin(), transitions.end()), transitions.end());
		auto access = gpu::to_image_access_mask(arr.type);
		auto layout = gpu::to_image_layout(arr.type);
		crash_if(access == gpu::image_access_mask::none);
		crash_if(layout == gpu::image_layout::undefined);
		for (auto index : transitions) {
			const auto &img = arr.resources[index].resource;
			if (img._ptr) {
				_image2d_transitions.emplace_back(
					*img._ptr, img._mip_levels,
					_details::image_access(gpu::synchronization_point_mask::all, access, layout)
				);
			}
		}
	}

	void transition_buffer::stage_all_transitions_for(_details::buffer_descriptor_array &arr) {
		auto transitions = std::exchange(arr.staged_transitions, {});
		std::sort(transitions.begin(), transitions.end());
		transitions.erase(std::unique(transitions.begin(), transitions.end()), transitions.end());
		auto access = gpu::to_buffer_access_mask(arr.type);
		crash_if(access == gpu::buffer_access_mask::none);
		for (auto index : transitions) {
			const auto &buf = arr.resources[index].resource;
			if (buf._ptr) {
				_buffer_transitions.emplace_back(
					*buf._ptr, _details::buffer_access(gpu::synchronization_point_mask::all, access)
				);
			}
		}
	}

	/// Used for sorting image transitions.
	template <gpu::image_type Type> [[nodiscard]] static bool _image_transition_compare(
		const transition_records::basic_image<Type> &lhs, const transition_records::basic_image<Type> &rhs
	) {
		if (lhs.target == rhs.target) {
			return lhs.mip_levels.minimum < rhs.mip_levels.minimum;
		}
		return lhs.target->id < rhs.target->id;
	}
	/// Collects image transitions.
	///
	/// \return The number of new barriers added.
	template <gpu::image_type Type> static std::size_t _collect_image_transitions(
		std::vector<gpu::image_barrier> &barriers, std::span<const transition_records::basic_image<Type>> transitions
	) {
		std::size_t very_first = barriers.size();
		for (auto it = transitions.begin(); it != transitions.end(); ) {
			// transition resources
			std::size_t first_barrier = barriers.size();
			auto *surf = it->target;
			for (auto first = it; it != transitions.end() && it->target == first->target; ++it) {
				auto max_mip = std::min(it->target->num_mips, it->mip_levels.maximum);
				for (std::uint32_t mip = it->mip_levels.minimum; mip < max_mip; ++mip) {
					// check if a transition is really necessary
					auto &current_access = it->target->current_usages[mip];
					// when these accesses are enabled, force a barrier
					constexpr auto force_sync_bits =
						gpu::image_access_mask::shader_write |
						gpu::image_access_mask::copy_destination;
					if (
						(current_access.access | it->access.access) == current_access.access &&
						current_access.layout == it->access.layout &&
						bit_mask::contains_none(current_access.access, force_sync_bits)
					) {
						current_access.sync_points |= it->access.sync_points;
						continue;
					}

					gpu::subresource_range sub_index(mip, 1, 0, 1, gpu::image_aspect_mask::none);
					const auto &fmt_prop = gpu::format_properties::get(it->target->format);
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
					barriers.emplace_back(
						sub_index,
						it->target->image,
						current_access.sync_points,
						current_access.access,
						current_access.layout,
						it->access.sync_points,
						it->access.access,
						it->access.layout
					);
				}
			}
			// deduplicate & warn about any conflicts
			std::sort(
				barriers.begin() + first_barrier, barriers.end(),
				[](const gpu::image_barrier &lhs, const gpu::image_barrier &rhs) {
					if (lhs.subresources.first_array_slice == rhs.subresources.first_array_slice) {
						if (lhs.subresources.first_mip_level == rhs.subresources.first_mip_level) {
							return lhs.subresources.aspects < rhs.subresources.aspects;
						}
						return lhs.subresources.first_mip_level < rhs.subresources.first_mip_level;
					}
					return lhs.subresources.first_array_slice < rhs.subresources.first_array_slice;
				}
			);
			if (barriers.size() > first_barrier) {
				// TODO deduplicate
				auto last = barriers.begin() + first_barrier;
				for (auto cur = last; cur != barriers.end(); ++cur) {
					if (cur->subresources == last->subresources) {
						if (cur->to_access != last->to_access) {
							log().error(
								"Multiple transition targets for image resource {} slice {} mip {}. "
								"Maybe a flush_transitions() call is missing?",
								string::to_generic(surf->name),
								last->subresources.first_array_slice, last->subresources.first_mip_level
							);
						}
					} else {
						*++last = *cur;
					}
				}
				barriers.erase(last + 1, barriers.end());
				for (auto cur = barriers.begin() + first_barrier; cur != barriers.end(); ++cur) {
					surf->current_usages[cur->subresources.first_mip_level] = _details::image_access(
						cur->to_point, cur->to_access, cur->to_layout
					);
				}
			}
		}
		return barriers.size() - very_first;
	}
	std::tuple<
		std::vector<gpu::image_barrier>,
		std::vector<gpu::buffer_barrier>,
		statistics::transitions
	> transition_buffer::collect_transitions() && {
		statistics::transitions result_stats = _stats;
		result_stats.requested_image2d_transitions = static_cast<std::uint32_t>(_image2d_transitions.size());
		result_stats.requested_image3d_transitions = static_cast<std::uint32_t>(_image3d_transitions.size());
		result_stats.requested_buffer_transitions  = static_cast<std::uint32_t>(_buffer_transitions.size());

		// sort image transitions
		std::sort(
			_image2d_transitions.begin(), _image2d_transitions.end(),
			_image_transition_compare<gpu::image_type::type_2d>
		);
		std::sort(
			_image3d_transitions.begin(), _image3d_transitions.end(),
			_image_transition_compare<gpu::image_type::type_3d>
		);

		// sort buffer transitions
		std::sort(
			_buffer_transitions.begin(), _buffer_transitions.end(),
			[](const transition_records::buffer &lhs, const transition_records::buffer &rhs) {
				if (lhs.target == rhs.target) {
					return lhs.access < rhs.access;
				}
				return lhs.target->id < rhs.target->id;
			}
		);
		// deduplicate
		_buffer_transitions.erase(
			std::unique(_buffer_transitions.begin(), _buffer_transitions.end()), _buffer_transitions.end()
		);


		std::vector<gpu::image_barrier> image_barriers;
		std::vector<gpu::buffer_barrier> buffer_barriers;

		// handle image transitions
		result_stats.submitted_image2d_transitions = static_cast<std::uint32_t>(
			_collect_image_transitions<gpu::image_type::type_2d>(image_barriers, _image2d_transitions)
		);
		result_stats.submitted_image3d_transitions = static_cast<std::uint32_t>(
			_collect_image_transitions<gpu::image_type::type_3d>(image_barriers, _image3d_transitions)
		);

		{ // handle swap chain image transitions
			// TODO check for conflicts
			for (const auto &trans : _swap_chain_transitions) {
				auto cur_usage = trans.target->current_usages[trans.target->next_image_index];
				image_barriers.emplace_back(
					gpu::subresource_range::first_color(),
					trans.target->images[trans.target->next_image_index],
					cur_usage.sync_points,
					cur_usage.access,
					cur_usage.layout,
					trans.access.sync_points,
					trans.access.access,
					trans.access.layout
				);
				trans.target->current_usages[trans.target->next_image_index] = trans.access;
			}
		}

		{ // handle buffer transitions
			std::size_t first = buffer_barriers.size();
			_details::buffer *prev = nullptr;
			for (auto trans : _buffer_transitions) {
				if (trans.target == prev) {
					log().error(
						"Multiple transitions staged for buffer {}", string::to_generic(trans.target->name)
					);
					continue;
				}
				prev = trans.target;

				if (bit_mask::contains_any(trans.target->usage_hint, trans.access.access)) {
					trans.access.access |= trans.target->usage_hint;
				}

				// check if a transition is necessary
				// for any of these accesses, we want to insert barriers even if the usage does not change
				// basically all write accesses
				constexpr auto force_sync_bits =
					gpu::buffer_access_mask::shader_write                 |
					gpu::buffer_access_mask::acceleration_structure_write |
					gpu::buffer_access_mask::copy_destination;
				if ( 
					(trans.access.access | trans.target->access.access) == trans.target->access.access &&
					bit_mask::contains_none(trans.target->access.access, force_sync_bits)
				) {
					// record the extra sync points
					trans.target->access.sync_points |= trans.access.sync_points;
					continue;
				}

				buffer_barriers.emplace_back(
					trans.target->data,
					trans.target->access.sync_points,
					trans.target->access.access,
					trans.access.sync_points,
					trans.access.access
				);
				trans.target->access = trans.access;
			}
			result_stats.submitted_buffer_transitions = static_cast<std::uint32_t>(buffer_barriers.size() - first);
		}

		result_stats.submitted_raw_buffer_transitions = static_cast<std::uint32_t>(_raw_buffer_transitions.size());
		{ // handle raw buffer barriers
			for (auto trans : _raw_buffer_transitions) {
				buffer_barriers.emplace_back(
					*trans.first,
					trans.second.first.sync_points,
					trans.second.first.access,
					trans.second.second.sync_points,
					trans.second.second.access
				);
			}
		}

		return { std::move(image_barriers), std::move(buffer_barriers), result_stats };
	}


	context::context(_details::queue_data &q, batch_resources &rsrc) :
		transitions(nullptr),
		early_statistics(zero),
		_q(q), _resources(rsrc),
		_immediate_constant_device_buffer(nullptr),
		_immediate_constant_upload_buffer(nullptr),
		_immediate_constant_buffer_stats(zero) {

		// initialize queue data
		_get_queue_data().timers.resize(_q.num_timers, nullptr);
	}

	gpu::command_queue &context::get_command_queue() {
		return _q.queue;
	}

	gpu::command_list &context::get_command_list() {
		if (!_list) {
			if (!_cmd_alloc) {
				_cmd_alloc = &record(_get_device().create_command_allocator(_q.queue.get_type()));
			}
			_list = &record(_get_device().create_and_start_command_list(*_cmd_alloc));
		}
		return *_list;
	}

	bool context::submit(gpu::queue_synchronization sync) {
		flush_immediate_constant_buffers();

		if (!_list) {
			get_command_queue().submit_command_lists({}, std::move(sync));
			return false;
		}

		_list->finish();
		get_command_queue().submit_command_lists({ _list }, std::move(sync));
		_list = nullptr;
		return true;
	}

	gpu::buffer &context::create_buffer(
		std::size_t size, gpu::memory_type_index mem_type, gpu::buffer_usage_mask usage
	) {
		return _resources.buffers.emplace_back(_get_device().create_committed_buffer(size, mem_type, usage));
	}

	gpu::frame_buffer &context::create_frame_buffer(
		std::span<const gpu::image2d_view *const> color_rts,
		const gpu::image2d_view *ds_rt,
		cvec2u32 size
	) {
		return _resources.frame_buffers.emplace_back(
			_get_device().create_frame_buffer(color_rts, ds_rt, size)
		);
	}

	gpu::constant_buffer_view context::stage_immediate_constant_buffer(
		memory::size_alignment size_align, static_function<void(std::span<std::byte>)> fill_data
	) {
		if (_immediate_constant_device_buffer) {
			_immediate_constant_buffer_used = memory::align_up(_immediate_constant_buffer_used, size_align.alignment);
			if (_immediate_constant_buffer_used + size_align.size > immediate_constant_buffer_cache_size) {
				flush_immediate_constant_buffers();
			}
		}

		if (!_immediate_constant_device_buffer) {
			_immediate_constant_device_buffer = _get_device().create_committed_buffer(
				immediate_constant_buffer_cache_size,
				_get_context()._device_memory_index,
				gpu::buffer_usage_mask::copy_destination |
				gpu::buffer_usage_mask::shader_read      |
				gpu::buffer_usage_mask::shader_record_table
			);
			_immediate_constant_upload_buffer = _get_device().create_committed_buffer(
				immediate_constant_buffer_cache_size,
				_get_context()._upload_memory_index,
				gpu::buffer_usage_mask::copy_source
			);
			_immediate_constant_upload_buffer_ptr = _get_device().map_buffer(_immediate_constant_upload_buffer, 0, 0);
		}

		gpu::constant_buffer_view result(
			_immediate_constant_device_buffer, _immediate_constant_buffer_used, size_align.size
		);

		crash_if(_immediate_constant_buffer_used + size_align.size > immediate_constant_buffer_cache_size);
		std::byte *data_addr = _immediate_constant_upload_buffer_ptr + _immediate_constant_buffer_used;
		_immediate_constant_buffer_used += static_cast<std::uint32_t>(size_align.size);

		_immediate_constant_buffer_stats.immediate_constant_buffer_size_no_padding +=
			static_cast<std::uint32_t>(size_align.size);
		++_immediate_constant_buffer_stats.num_immediate_constant_buffers;

		auto span = std::span(data_addr, data_addr + size_align.size);
		fill_data(span);

		if constexpr (collect_constant_buffer_signature) {
			statistics::constant_buffer_signature sig = zero;
			sig.hash = fnv1a::hash_bytes(span);
			sig.size = static_cast<std::uint32_t>(size_align.size);
			++early_statistics.constant_buffer_counts[sig];
		}

		return result;
	}

	[[nodiscard]] gpu::constant_buffer_view context::stage_immediate_constant_buffer(
		std::span<const std::byte> data, std::size_t alignment
	) {
		if (alignment == 0) {
			alignment = _get_context()._adapter_properties.constant_buffer_alignment;
		}
		return stage_immediate_constant_buffer(
			memory::size_alignment(data.size(), alignment),
			[data_ptr = data.data()](std::span<std::byte> dst) {
				std::memcpy(dst.data(), data_ptr, dst.size());
			}
		);
	}

	void context::flush_immediate_constant_buffers() {
		if (!_immediate_constant_upload_buffer) {
			return;
		}

		_immediate_constant_buffer_stats.immediate_constant_buffer_size =
			static_cast<std::uint32_t>(_immediate_constant_buffer_used);
		early_statistics.immediate_constant_buffers.emplace_back(std::exchange(_immediate_constant_buffer_stats, zero));

		_get_device().unmap_buffer(_immediate_constant_upload_buffer, 0, _immediate_constant_buffer_used);

		{
			// we need to immediately submit the command list because other commands have already been recorded that
			// use these constant buffers
			// alternatively, we can scan all commands for immediate constant buffers
			auto &cmd_list = _resources.record(
				_get_device().create_and_start_command_list(_get_transient_command_allocator())
			);
			cmd_list.insert_marker(u8"Flush immediate constant buffers", linear_rgba_u8(255, 255, 0, 255));
			cmd_list.copy_buffer(
				_immediate_constant_upload_buffer, 0,
				_immediate_constant_device_buffer, 0,
				_immediate_constant_buffer_used
			);
			cmd_list.resource_barrier({}, {
				gpu::buffer_barrier(
					_immediate_constant_device_buffer,
					gpu::synchronization_point_mask::cpu_access,
					gpu::buffer_access_mask::cpu_write,
					gpu::synchronization_point_mask::all,
					gpu::buffer_access_mask::shader_read
				),
			});
			cmd_list.finish();
			get_command_queue().submit_command_lists({ &cmd_list }, nullptr);
		}

		_resources.record(std::exchange(_immediate_constant_device_buffer, nullptr));
		_resources.record(std::exchange(_immediate_constant_upload_buffer, nullptr));
		_immediate_constant_buffer_used = 0;
		_immediate_constant_upload_buffer_ptr = nullptr;
	}

	void context::flush_descriptor_array_writes(
		_details::image_descriptor_array &arr, const gpu::descriptor_set_layout &layout
	) {
		if (!arr.staged_writes.empty()) {
			// TODO wait more intelligently
			auto fence = _get_device().create_fence(gpu::synchronization_state::unset);
			submit(&fence);
			_get_device().wait_for_fence(fence);

			if (arr.has_descriptor_overwrites) {
				arr.has_descriptor_overwrites = false;
			}

			auto writes = std::exchange(arr.staged_writes, {});
			std::sort(writes.begin(), writes.end());
			writes.erase(std::unique(writes.begin(), writes.end()), writes.end());
			auto write_func = gpu::device::get_write_image_descriptor_function(arr.type);
			assert(write_func);
			for (auto index : writes) {
				auto &rsrc = arr.resources[index];
				if (rsrc.view.value) {
					record(std::exchange(rsrc.view.value, nullptr));
				}
				// TODO batch writes
				if (rsrc.resource) {
					rsrc.view.value = _get_device().create_image2d_view_from(
						rsrc.resource._ptr->image, rsrc.resource._view_format, rsrc.resource._mip_levels
					);
					_get_device().set_debug_name(rsrc.view.value, rsrc.resource._ptr->name);
				}
				std::initializer_list<const gpu::image_view_base*> views = { &rsrc.view.value };
				(_get_device().*write_func)(arr.set, layout, index, views);
			}
		}
	}

	void context::flush_descriptor_array_writes(
		_details::buffer_descriptor_array &arr, const gpu::descriptor_set_layout &layout
	) {
		if (!arr.staged_writes.empty()) {
			// TODO wait more intelligently
			auto fence = _get_device().create_fence(gpu::synchronization_state::unset);
			submit(&fence);
			_get_device().wait_for_fence(fence);

			if (arr.has_descriptor_overwrites) {
				arr.has_descriptor_overwrites = false;
			}

			auto writes = std::exchange(arr.staged_writes, {});
			std::sort(writes.begin(), writes.end());
			writes.erase(std::unique(writes.begin(), writes.end()), writes.end());
			auto write_func = gpu::device::get_write_structured_buffer_descriptor_function(arr.type);
			assert(write_func);
			for (auto index : writes) {
				const auto &buf = arr.resources[index].resource;
				// TODO batch writes
				gpu::structured_buffer_view view = nullptr;
				if (buf) {
					crash_if((buf._first + buf._count) * buf._stride > buf._ptr->size);
					view = gpu::structured_buffer_view(buf._ptr->data, buf._first, buf._count, buf._stride);
				}
				std::initializer_list<gpu::structured_buffer_view> views = { view };
				(_get_device().*write_func)(arr.set, layout, index, views);
			}
		}
	}

	void context::flush_transitions() {
		auto [image_barriers, buffer_barriers, stats] = std::exchange(transitions, nullptr).collect_transitions();
		early_statistics.transitions.emplace_back(stats);
		if (image_barriers.size() > 0 || buffer_barriers.size() > 0) {
			get_command_list().insert_marker(u8"Flush transitions", linear_rgba_u8(0, 0, 255, 255));

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

	void context::invalidate_timer() {
		_fresh_timestamp = false;
	}

	void context::start_timer(const commands::start_timer &cmd) {
		auto &timer = _get_queue_data().timers[std::to_underlying(cmd.index)];
		timer.name = cmd.name; // TODO make this a move?
		timer.begin_timestamp = _maybe_insert_timestamp();
	}

	void context::end_timer(const commands::end_timer &cmd) {
		_get_queue_data().timers[std::to_underlying(cmd.index)].end_timestamp = _maybe_insert_timestamp();
	}

	void context::begin_pass_preprocessing() {
		crash_if(!_pass_command_data.empty());
		_pass_preprocessing_finished = false;
	}

	void context::push_pass_command_data(pass_command_data data) {
		_pass_command_data.emplace_back(std::move(data));
	}

	void context::end_pass_preprocessing() {
		crash_if(_pass_preprocessing_finished);
		_pass_preprocessing_finished = true;
		_pass_command_data_ptr = 0;
	}

	pass_command_data context::pop_pass_command_data() {
		crash_if(!_pass_preprocessing_finished);
		return std::move(_pass_command_data[_pass_command_data_ptr++]);
	}

	void context::end_pass_command_processing() {
		crash_if(_pass_command_data_ptr != _pass_command_data.size());
		_pass_command_data.clear();
	}

	bool context::has_finished() const {
		return get_next_command_index() >= _q.batch_commands.size();
	}

	renderer::context &context::_get_context() const {
		return _q.ctx;
	}

	gpu::device &context::_get_device() const {
		return _get_context()._device;
	}

	batch_resources::queue_data &context::_get_queue_data() const {
		return _resources.queues[_q.queue.get_index()];
	}

	gpu::command_allocator &context::_get_transient_command_allocator() {
		if (!_transient_cmd_alloc) {
			_transient_cmd_alloc = &_resources.record(_get_device().create_command_allocator(_q.queue.get_type()));
		}
		return *_transient_cmd_alloc;
	}

	std::uint32_t context::_maybe_insert_timestamp() {
		auto &queue_data = _get_queue_data();
		if (!_fresh_timestamp) {
			if (!queue_data.timestamp_heap) {
				queue_data.timestamp_heap = &record(_get_device().create_timestamp_query_heap(
					static_cast<std::uint32_t>(queue_data.timers.size())
				));
			}
			get_command_list().query_timestamp(*queue_data.timestamp_heap, queue_data.timestamp_count);
			++queue_data.timestamp_count;
		}
		return queue_data.timestamp_count - 1;
	}
}
