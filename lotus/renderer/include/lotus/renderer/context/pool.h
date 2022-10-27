#pragma once

/// \file
/// Resource pools.

#include "lotus/gpu/resources.h"
#include "lotus/memory/managed_allocator.h"

namespace lotus::renderer {
	/// A pool that resources can be allocated out of.
	class pool {
	public:
		constexpr static std::uint32_t default_chunk_size = 100 * 1024 * 1024; ///< 100 MiB per chunk by default.

		/// A token of an allocation.
		struct token {
			friend pool;
		public:
			/// Initializes this token to empty.
			constexpr token(std::nullptr_t) {
			}

			/// Returns \p true if this represents a valid allocation.
			[[nodiscard]] constexpr bool is_valid() const {
				return _chunk_index != invalid_chunk_index;
			}
			/// \overload
			[[nodiscard]] constexpr explicit operator bool() const {
				return is_valid();
			}
		private:
			/// Index indicating an invalid token.
			constexpr static std::uint32_t invalid_chunk_index = std::numeric_limits<std::uint32_t>::max();

			std::uint32_t _chunk_index = invalid_chunk_index; ///< The index of the chunk.
			/// Address of the memory block within the chunk.
			std::uint32_t _address = 0;

			/// Initializes all fields of this struct.
			token(std::uint32_t ch, std::uint32_t addr) : _chunk_index(ch), _address(addr) {
			}
		};

		/// Initializes the pool.
		explicit pool(
			std::u8string n, gpu::memory_type_index mem_type, std::uint32_t chunk_sz = default_chunk_size
		) : _memory_type(mem_type), _chunk_size(chunk_sz), _name(std::move(n)) {
		}

		/// Allocates a memory block.
		[[nodiscard]] token allocate(gpu::device &dev, memory::size_alignment size_align) {
			crash_if(size_align.size > _chunk_size);
			for (std::size_t i = 0; i < _chunks.size(); ++i) {
				if (auto res = _chunks[i].allocator.allocate(size_align, 0)) {
					/*log().debug<u8"Allocating from pool {}, chunk {}, addr {}, size {}">(
						string::to_generic(_name), i, res->first, size_align.size
					);*/
					return token(static_cast<std::uint32_t>(i), static_cast<std::uint32_t>(res->first));
				}
			}
			std::size_t index = _chunks.size();
			auto &chk = _chunks.emplace_back(_chunk::create(
				dev, _memory_type, std::max(static_cast<std::uint32_t>(size_align.size), _chunk_size)
			));
			if (auto res = chk.allocator.allocate(size_align, 0)) {
				/*log().debug<u8"Allocating from pool {}, chunk {}, addr {}, size {}">(
					string::to_generic(_name), index, res->first, size_align.size
				);*/
				return token(static_cast<std::uint32_t>(index), static_cast<std::uint32_t>(res->first));
			}
			return nullptr;
		}
		/// Frees the given memory block.
		void free(token tok) {
			crash_if(tok._chunk_index >= _chunks.size());
			_chunks[tok._chunk_index].allocator.free(tok._address);
			/*log().debug<u8"Freeing from pool {}, chunk {}, addr {}">(
				string::to_generic(_name), tok._chunk_index, tok._address
			);*/
		}

		/// Given a \ref token, returns the corresponding memory block and its offset within it.
		[[nodiscare]] std::pair<const gpu::memory_block&, std::uint32_t> get_memory_and_offset(token tk) const {
			return { _chunks[tk._chunk_index].memory, tk._address };
		}
	private:
		/// A chunk of GPU memory managed by this pool.
		struct _chunk {
		public:
			/// Allocates a new chunk.
			[[nodiscard]] inline static _chunk create(
				gpu::device &dev, gpu::memory_type_index type, std::uint32_t size
			) {
				return _chunk(dev.allocate_memory(size, type), memory::managed_allocator<int>::create(size));
			}

			gpu::memory_block memory; ///< The memory block.
			// TODO what data should we include in the allocations?
			memory::managed_allocator<int> allocator; ///< Allocator.
		private:
			/// Initializes all fields of this struct.
			_chunk(gpu::memory_block mem, memory::managed_allocator<int> alloc) :
				memory(std::move(mem)), allocator(std::move(alloc)) {
			}
		};

		std::deque<_chunk> _chunks; ///< Allocated chunks.
		/// Memory type index for allocations.
		gpu::memory_type_index _memory_type = gpu::memory_type_index::invalid;
		std::uint32_t _chunk_size = 0; ///< Chunk size.

		std::u8string _name; ///< Name of this pool.
	};
}
