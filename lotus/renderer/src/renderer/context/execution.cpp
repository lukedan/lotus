#include "lotus/renderer/context/execution.h"

/// \file
/// Implementation of command execution related functions.

#include "lotus/renderer/context/context.h"

namespace lotus::renderer::execution {
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


	void transition_buffer::stage_transition(
		_details::image2d &surf, gpu::mip_levels mips, _details::image_access access
	) {
		_image2d_transitions.emplace_back(surf, mips, access);
		for (const auto &ref : surf.array_references) {
			const auto &img = ref.array->resources[ref.index].resource;
			assert(img._image == &surf);
			if (!gpu::mip_levels::intersection(img._mip_levels, mips).is_empty()) {
				ref.array->staged_transitions.emplace_back(ref.index);
			}
		}
	}

	void transition_buffer::stage_transition(_details::buffer &buf, _details::buffer_access access) {
		_buffer_transitions.emplace_back(buf, access);
		for (const auto &ref : buf.array_references) {
			crash_if(ref.array->resources[ref.index].resource._buffer != &buf);
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
			_image2d_transitions.emplace_back(
				*img._image,
				img._mip_levels,
				_details::image_access(gpu::synchronization_point_mask::all, access, layout)
			);
		}
	}

	void transition_buffer::stage_all_transitions_for(_details::buffer_descriptor_array &arr) {
		auto transitions = std::exchange(arr.staged_transitions, {});
		std::sort(transitions.begin(), transitions.end());
		transitions.erase(std::unique(transitions.begin(), transitions.end()), transitions.end());
		auto access = gpu::to_buffer_access_mask(arr.type);
		assert(access != gpu::buffer_access_mask::none);
		for (auto index : transitions) {
			const auto &buf = arr.resources[index].resource;
			_buffer_transitions.emplace_back(
				*buf._buffer, _details::buffer_access(gpu::synchronization_point_mask::all, access)
			);
		}
	}

	void transition_buffer::prepare() {
		// sort image transitions
		std::sort(
			_image2d_transitions.begin(), _image2d_transitions.end(),
			[](const transition_records::image2d &lhs, const transition_records::image2d &rhs) {
				if (lhs.target == rhs.target) {
					return lhs.mip_levels.minimum < rhs.mip_levels.minimum;
				}
				assert(lhs.target->id != rhs.target->id);
				return lhs.target->id < rhs.target->id;
			}
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
	}

	std::pair<
		std::vector<gpu::image_barrier>, std::vector<gpu::buffer_barrier>
	> transition_buffer::collect_transitions() const {
		std::vector<gpu::image_barrier> image_barriers;
		std::vector<gpu::buffer_barrier> buffer_barriers;

		{ // handle image transitions
			for (auto it = _image2d_transitions.begin(); it != _image2d_transitions.end(); ) {
				// transition resources
				std::size_t first_barrier = image_barriers.size();
				auto *surf = it->target;
				for (auto first = it; it != _image2d_transitions.end() && it->target == first->target; ++it) {
					auto max_mip = std::min(it->target->num_mips, it->mip_levels.maximum);
					for (std::uint32_t mip = it->mip_levels.minimum; mip < max_mip; ++mip) {
						// check if a transition is really necessary
						auto &current_access = it->target->current_usages[mip];
						// when these accesses are enabled, force a barrier
						constexpr auto force_sync_bits =
							gpu::image_access_mask::shader_write |
							gpu::image_access_mask::copy_destination;
						if (
							current_access.access == it->access.access &&
							current_access.layout == it->access.layout &&
							is_empty(current_access.access & force_sync_bits)
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
						image_barriers.emplace_back(
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
					image_barriers.begin() + first_barrier, image_barriers.end(),
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
				if (image_barriers.size() > first_barrier) {
					// TODO deduplicate
					auto last = image_barriers.begin() + first_barrier;
					for (auto cur = last; cur != image_barriers.end(); ++cur) {
						if (cur->subresources == last->subresources) {
							if (cur->to_access != last->to_access) {
								log().error<
									u8"Multiple transition targets for image resource {} slice {} mip {}. "
									u8"Maybe a flush_transitions() call is missing?"
								>(
									string::to_generic(surf->name),
									last->subresources.first_array_slice, last->subresources.first_mip_level
								);
							}
						} else {
							*++last = *cur;
						}
					}
					image_barriers.erase(last + 1, image_barriers.end());
					for (auto cur = image_barriers.begin() + first_barrier; cur != image_barriers.end(); ++cur) {
						surf->current_usages[cur->subresources.first_mip_level] = _details::image_access(
							cur->to_point, cur->to_access, cur->to_layout
						);
					}
				}
			}
		}

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
			_details::buffer *prev = nullptr;
			for (const auto &trans : _buffer_transitions) {
				if (trans.target == prev) {
					log().error<u8"Multiple transitions staged for buffer {}">(
						string::to_generic(trans.target->name)
					);
					continue;
				}
				prev = trans.target;
				// for any of these accesses, we want to insert barriers even if the usage does not change
				// basically all write accesses
				constexpr auto force_sync_bits =
					gpu::buffer_access_mask::shader_write                 |
					gpu::buffer_access_mask::acceleration_structure_write |
					gpu::buffer_access_mask::copy_destination;
				if ( 
					trans.access.access == trans.target->access.access &&
					is_empty(trans.access.access & force_sync_bits)
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
		}

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

		return { std::move(image_barriers), std::move(buffer_barriers) };
	}


	gpu::command_list &context::get_command_list() {
		if (!_list) {
			_list = &record(_ctx._device.create_and_start_command_list(_ctx._cmd_alloc));
		}
		return *_list;
	}

	gpu::buffer &context::create_buffer(
		std::size_t size, gpu::memory_type_index mem_type, gpu::buffer_usage_mask usage
	) {
		return _resources.buffers.emplace_back(_ctx._device.create_committed_buffer(size, mem_type, usage));
	}

	gpu::frame_buffer &context::create_frame_buffer(
		std::span<const gpu::image2d_view *const> color_rts,
		const gpu::image2d_view *ds_rt,
		cvec2u32 size
	) {
		return _resources.frame_buffers.emplace_back(
			_ctx._device.create_frame_buffer(color_rts, ds_rt, size)
		);
	}

	std::pair<
		gpu::constant_buffer_view, std::byte*
	> context::stage_immediate_constant_buffer(memory::size_alignment size_align) {
		if (_immediate_constant_device_buffer) {
			_immediate_constant_buffer_used = memory::align_up(_immediate_constant_buffer_used, size_align.alignment);
			if (_immediate_constant_buffer_used + size_align.size > immediate_constant_buffer_cache_size) {
				flush_immediate_constant_buffers();
			}
		}

		if (!_immediate_constant_device_buffer) {
			_immediate_constant_device_buffer = _ctx._device.create_committed_buffer(
				immediate_constant_buffer_cache_size,
				_ctx._device_memory_index,
				gpu::buffer_usage_mask::copy_destination |
				gpu::buffer_usage_mask::shader_read      |
				gpu::buffer_usage_mask::shader_record_table
			);
			_immediate_constant_upload_buffer = _ctx._device.create_committed_buffer(
				immediate_constant_buffer_cache_size,
				_ctx._upload_memory_index,
				gpu::buffer_usage_mask::copy_source
			);
			_immediate_constant_upload_buffer_ptr = _ctx._device.map_buffer(_immediate_constant_upload_buffer, 0, 0);
		}

		gpu::constant_buffer_view result(
			_immediate_constant_device_buffer, _immediate_constant_buffer_used, size_align.size
		);

		crash_if(_immediate_constant_buffer_used + size_align.size > immediate_constant_buffer_cache_size);
		std::byte *data_addr = _immediate_constant_upload_buffer_ptr + _immediate_constant_buffer_used;
		_immediate_constant_buffer_used += size_align.size;

		return { result, data_addr };
	}

	[[nodiscard]] gpu::constant_buffer_view context::stage_immediate_constant_buffer(
		std::span<const std::byte> data, std::size_t alignment
	) {
		if (alignment == 0) {
			alignment = _ctx._adapter_properties.constant_buffer_alignment;
		}
		auto [res, dst] = stage_immediate_constant_buffer(memory::size_alignment(data.size(), alignment));
		std::memcpy(dst, data.data(), data.size());
		return res;
	}

	void context::flush_immediate_constant_buffers() {
		if (!_immediate_constant_upload_buffer) {
			return;
		}

		_ctx._device.unmap_buffer(_immediate_constant_upload_buffer, 0, _immediate_constant_buffer_used);

		{
			// we need to immediately submit the command list because other commands have already been recorded that
			// use these constant buffers
			// alternatively, we can scan all commands for immediate constant buffers
			auto &cmd_list = _resources.record(
				_ctx._device.create_and_start_command_list(_ctx._transient_cmd_alloc)
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
			_ctx._queue.submit_command_lists({ &cmd_list }, nullptr);
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
			auto fence = _ctx._device.create_fence(gpu::synchronization_state::unset);
			submit(_ctx._queue, &fence);
			_ctx._device.wait_for_fence(fence);
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
					rsrc.view.value = _ctx._device.create_image2d_view_from(
						rsrc.resource._image->image, rsrc.resource._view_format, rsrc.resource._mip_levels
					);
					_ctx._device.set_debug_name(rsrc.view.value, rsrc.resource._image->name);
				}
				std::initializer_list<const gpu::image_view_base*> views = { &rsrc.view.value };
				(_ctx._device.*write_func)(arr.set, layout, index, views);
			}
		}
	}

	void context::flush_descriptor_array_writes(
		_details::buffer_descriptor_array &arr, const gpu::descriptor_set_layout &layout
	) {
		if (!arr.staged_writes.empty()) {
			// TODO wait more intelligently
			auto fence = _ctx._device.create_fence(gpu::synchronization_state::unset);
			submit(_ctx._queue, &fence);
			_ctx._device.wait_for_fence(fence);
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
					assert((buf._first + buf._count) * buf._stride <= buf._buffer->size);
					view = gpu::structured_buffer_view(buf._buffer->data, buf._first, buf._count, buf._stride);
				}
				std::initializer_list<gpu::structured_buffer_view> views = { view };
				(_ctx._device.*write_func)(arr.set, layout, index, views);
			}
		}
	}

	void context::flush_transitions() {
		transitions.prepare();
		auto [image_barriers, buffer_barriers] = transitions.collect_transitions();
		transitions = nullptr;
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
}
