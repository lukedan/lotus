#pragma once

/// \file
/// Implementation of an allocator that manages blocks of memory.

#include <map>
#include <vector>
#include <optional>
#include <compare>

#include "lotus/containers/static_optional.h"
#include "lotus/logging.h"
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
		[[nodiscard]] inline static managed_allocator create(std::size_t size) {
			managed_allocator result;
			result._total_size = size;
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
			size_alignment size_align, Data &&data, GhostCallback &&callback
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
					}
					// cut the free range into 3 and record the allocation
					// part before the allocation
					auto size_before = addr - it->first.begin;
					if (size_before > 0) {
						auto [before_it, inserted] = _free_ranges.emplace(
							_range(it->first.begin, addr), _free_range_data()
						);
						crash_if(!inserted);
						for (const auto &ghost : it->second.ghosts) {
							if (_range::get_intersection(ghost.range, before_it->first)) {
								before_it->second.ghosts.emplace_back(ghost);
							}
						}
					}
					// part after the allocation
					auto size_after = it->first.end - allocated.end;
					if (size_after > 0) {
						auto [after_it, inserted] = _free_ranges.emplace(
							_range(allocated.end, it->first.end), _free_range_data()
						);
						crash_if(!inserted);
						for (const auto &ghost : it->second.ghosts) {
							if (_range::get_intersection(ghost.range, after_it->first)) {
								after_it->second.ghosts.emplace_back(ghost);
							}
						}
					}
					// record allocation
					auto [alloc_it, inserted] = _allocated_ranges.emplace(allocated, std::move(data));
					crash_if(!inserted);
					_free_ranges.erase(it);
					return std::make_pair(allocated.begin, std::ref(alloc_it->second));
				}
			}
			return std::nullopt;
		}
		/// \overload
		template <typename Dummy = int> [[nodiscard]] std::enable_if_t<
			std::is_same_v<Dummy, Dummy> && !_has_ghost_data,
			std::optional<std::pair<std::size_t, Data&>>
		> allocate(
			size_alignment size_align, Data &&data
		) {
			return allocate(size_align, std::forward<Data>(data), std::nullopt);
		}

		/// Frees the range starting from the given location.
		template <typename ConvertGhost> void free(std::size_t addr, ConvertGhost &&convert) {
			auto it = _allocated_ranges.lower_bound(_range(addr, 0));
			crash_if(it == _allocated_ranges.end());
			crash_if(it->first.begin != addr);
			_range freed_range = it->first;

			_range new_free(0, _total_size);
			if (it != _allocated_ranges.begin()) {
				auto before_it = it;
				--before_it;
				new_free.begin = before_it->first.end;
			}
			if (auto after_it = it; ++after_it != _allocated_ranges.end()) {
				new_free.end = after_it->first.begin;
			}

			std::vector<_ghost> ghosts;
			if (new_free.begin != freed_range.begin) {
				auto before_it = _free_ranges.find(_range(new_free.begin, freed_range.begin));
				crash_if(before_it == _free_ranges.end());
				if constexpr (_has_ghost_data) {
					for (auto &d : before_it->second.ghosts) {
						if (!freed_range.fully_covers(d.range)) {
							ghosts.emplace_back(std::move(d));
						}
					}
				}
				_free_ranges.erase(before_it);
			}
			if (new_free.end != freed_range.end) {
				auto after_it = _free_ranges.find(_range(freed_range.end, new_free.end));
				crash_if(after_it == _free_ranges.end());
				if constexpr (_has_ghost_data) {
					for (auto &d : after_it->second.ghosts) {
						if (!freed_range.fully_covers(d.range)) {
							ghosts.emplace_back(std::move(d));
						}
					}
				}
				_free_ranges.erase(after_it);
			}
			if constexpr (_has_ghost_data) {
				ghosts.emplace_back(freed_range, convert(std::move(it->second)));
			}
			_allocated_ranges.erase(it);

			auto [new_it, inserted] = _free_ranges.emplace(new_free, _free_range_data(std::move(ghosts)));
			crash_if(!inserted);
		}
		/// \overload
		template <
			typename Dummy = void
		> std::enable_if_t<!_has_ghost_data && std::is_same_v<Dummy, Dummy>, void> free(std::size_t addr) {
			free(addr, std::nullopt);
		}

		/// Checks the integrity of this container. One scenario that this is unable to detect is missing ghosts.
		[[nodiscard]] bool check_integrity() const {
			auto alloc_it = _allocated_ranges.begin();
			auto free_it = _free_ranges.begin();
			std::size_t prev = 0;
			while (true) {
				std::size_t alloc_beg = _total_size;
				std::size_t free_beg = _total_size;
				if (alloc_it != _allocated_ranges.end()) {
					alloc_beg = alloc_it->first.begin;
					if (alloc_it->first.begin >= alloc_it->first.end) {
						log().error("Invalid allocated range [{}, {})", alloc_it->first.begin, alloc_it->first.end);
						return false;
					}
				}
				if (free_it != _free_ranges.end()) {
					free_beg = free_it->first.begin;
					if (free_it->first.begin >= free_it->first.end) {
						log().error("Inavlid free range [{}, {})", free_it->first.begin, free_it->first.end);
						return false;
					}
				}
				std::size_t current = std::min(alloc_beg, free_beg);
				if (prev != current) {
					log().error("Missing range [{}, {})", prev, current);
					return false;
				}
				if (current == _total_size) {
					break;
				}
				if (current == alloc_beg) {
					prev = alloc_it->first.end;
					++alloc_it;
				} else {
					for (const auto &gh : free_it->second) {
						if (!_range::get_intersection(gh.range, free_it->first)) {
							log().error("Ghost does not intersect with free range");
							return false;
						}
					}
					prev = free_it->first.end;
					++free_it;
				}
			}
			if (alloc_it != _allocated_ranges.end()) {
				log().error("Allocated range reaches past the memory pool");
				return false;
			}
			if (free_it != _free_ranges.end()) {
				log().error("Free range reaches past the memory pool");
				return false;
			}
			return true;
		}
	private:
		using _range = linear_size_t_range; ///< A memory range.
		/// An allocation made previously that has been freed.
		struct _ghost {
			/// Initializes all fields of this struct.
			_ghost(_range r, GhostData d) : range(r), data(std::move(d)) {
			}

			_range range; ///< The original range of this allocation.
			GhostData data; ///< Data associated with this allocation.
		};
		/// Data associated with a range that is not allocated.
		struct _free_range_data {
			/// Default constructor.
			_free_range_data() = default;
			/// Initializes all fields of this struct.
			explicit _free_range_data(std::vector<_ghost> ghs) : ghosts(std::move(ghs)) {
			}

			std::vector<_ghost> ghosts; ///< Ghosts in this free range.
		};
		/// Comparison for ranges.
		struct _compare {
			/// Comparison function.
			[[nodiscard]] constexpr bool operator()(const _range &lhs, const _range &rhs) const {
				return lhs.begin == rhs.begin ? lhs.end < rhs.end : lhs.begin < rhs.begin;
			}
		};

		std::map<_range, Data, _compare> _allocated_ranges; ///< All allocated ranges.
		std::map<_range, _free_range_data, _compare> _free_ranges; ///< Available free ranges.
		std::size_t _total_size = 0; ///< Total size of the managed block.

		/// Does not invoke a \p std::nullopt_t object.
		template <typename ...Args> void _maybe_invoke(std::nullopt_t, Args&&...) {
		}
		/// Invokes the callback function.
		template <typename Cb, typename ...Args> void _maybe_invoke(Cb &&cb, Args &&...args) {
			cb(std::forward<Args>(args)...);
		}
	};
}
