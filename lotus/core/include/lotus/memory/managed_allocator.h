#pragma once

/// \file
/// Implementation of an allocator that manages blocks of memory.

#include <map>
#include <vector>
#include <optional>
#include <compare>

#include "lotus/containers/static_optional.h"
#include "common.h"

namespace lotus::memory {
	// TODO better algorithm
	/// An allocator that allocates arbitrarily sized blocks out of a memory range, but does not actually manage the
	/// memory.
	///
	/// \tparam Data Data associated with an allocated range.
	/// \tparam GhostData Data associated with a range that has been freed. When a new piece of memory is allocated,
	///                   the user will be able to enumerate over the ghost data of all ranges that overlaps with the
	///                   newly allocated range. Multiple copies may be made.
	template <typename Data, typename GhostData = std::nullopt_t> struct managed_allocator {
	private:
		/// Whether ghost data is enabled.
		constexpr static bool _has_ghost_data = !std::is_same_v<GhostData, std::nullopt_t>;
	public:
		/// Creates a new allocator object.
		[[nodiscard]] inline static managed_allocator create(std::size_t size, std::size_t threshold = 0) {
			managed_allocator result;
			result._total_size = size;
			result._size_threshold = threshold;
			result._free_ranges.emplace(_range(0, result._total_size), _free_range_data());
			return result;
		}

		/// Allocates a block of memory. If allocation fails, the input data will not be moved from.
		///
		/// \return The offset to the beginning of the allocated memory block, or \p std::nullopt.
		template <typename GhostCallback> [[nodiscard]] std::enable_if_t<
			// the callback is only available if we actually have data to call it with
			std::is_same_v<std::decay_t<GhostCallback>, std::nullopt_t> || _has_ghost_data,
			std::optional<std::pair<std::size_t, Data&>>
		> allocate(
			size_alignment size_align, Data &&data, GhostCallback &&callback = std::nullopt
		) {
			constexpr bool has_callback = std::is_same_v<std::decay_t<GhostCallback>, std::nullopt_t>;
			for (auto it = _free_ranges.begin(); it != _free_ranges.end(); ++it) {
				auto addr = align_up(it->first.begin, size_align.alignment);
				auto end = addr + size_align.size;
				if (end <= it->first.end) { // found a suitable range, allocate
					_range allocated(addr, addr + size_align.size);
					if constexpr (_has_ghost_data) {
						// invoke the callback for all ghosts
						for (const auto &ghost : it->second.ghosts) {
							if constexpr (has_callback) {
								if (_range::get_intersection(allocated, ghost.range)) {
									_maybe_invoke(callback, ghost.data);
								}
							}
						}
						// cut the free range into 3 and record the allocation
						// part before the allocation
						auto size_before = addr - it->first.begin;
						if (size_before > 0 && size_before >= _size_threshold) {
							auto [before_it, inserted] = _free_ranges.emplace(
								_range(it->first.begin, addr), _free_range_data()
							);
							assert(inserted);
							for (const auto &ghost : it->second.ghosts) {
								if (_range::get_intersection(ghost.range, before_it->first)) {
									before_it->second.ghosts.emplace_back(ghost);
								}
							}
						}
						// part after the allocation
						auto size_after = it->first.end - allocated.end;
						if (size_after > 0 && size_after >= _size_threshold) {
							auto [after_it, inserted] = _free_ranges.emplace(
								_range(allocated.end, it->first.end), _free_range_data()
							);
							assert(inserted);
							for (const auto &ghost : it->second.ghosts) {
								if (_range::get_intersection(ghost.range, after_it->first)) {
									after_it->second.ghosts.emplace_back(ghost);
								}
							}
						}
						// record allocation
						auto [alloc_it, inserted] = _allocated_ranges.emplace(allocated, std::move(data));
						assert(inserted);
						_free_ranges.erase(it);
						return std::make_pair(allocated.begin, std::ref(alloc_it->second));
					}
				}
			}
			return std::nullopt;
		}

		/// Frees the range starting from the given location.
		void free(std::size_t addr) {
			auto it = _allocated_ranges.lower_bound(_range(addr, 0));
			assert(it != _allocated_ranges.end());
			assert(it->first.begin == addr);

			std::size_t before = 0;
			std::size_t after = _total_size;
			if (it != _allocated_ranges.begin()) {
				auto before_it = it;
				--before_it;
				before = before_it->first.end;
			}
			auto after_it = it;
			if (++after_it != _allocated_ranges.end()) {
				after = after_it->first.begin;
			}


		}
	private:
		using _range = linear_size_t_range; ///< A memory range.
		/// An allocation made previously that has been freed.
		struct _ghost {
			_range range; ///< The original range of this allocation.
			GhostData data; ///< Data associated with this allocation.
		};
		/// Data associated with a range that is not allocated.
		struct _free_range_data {
			std::vector<_ghost> ghosts; ///< Ghosts in this free range.
		};

		std::map<_range, Data> _allocated_ranges; ///< All allocated ranges.
		std::map<_range, _free_range_data> _free_ranges; ///< Available free ranges.
		std::size_t _total_size = 0; ///< Total size of the managed block.
		/// Threshold for when a free range is considered too small to be worth considering when allocating. Set to 0
		/// to disable this behavior.
		std::size_t _size_threshold = 0;

		/// Does not invoke a \p std::nullopt_t object.
		template <typename ...Args> void _maybe_invoke(std::nullopt_t, Args&&...) {
		}
		/// Invokes the callback function.
		template <typename Cb, typename ...Args> void _maybe_invoke(Cb &&cb, Args &&...args) {
			cb(std::forward<Args>(args)...);
		}
	};
}
