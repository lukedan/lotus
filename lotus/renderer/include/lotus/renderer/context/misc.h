#pragma once

/// \file
/// Miscellaneous types used during context execution.

#include "lotus/utils/static_function.h"

namespace lotus::renderer {
	class context;
	namespace _details {
		struct queue_data;
	}

	/// Manages large buffers suballocated for upload operations.
	struct upload_buffers {
	public:
		constexpr static std::size_t default_buffer_size = 10 * 1024 * 1024; ///< Default size for upload buffers.

		/// Function used to allocate new buffers. The returned buffer should not be moved after it is created.
		using allocate_buffer_func = static_function<gpu::buffer&(std::size_t)>;
		/// The type of allocation performed by \ref stage().
		enum class allocation_type {
			invalid, ///< Invalid.
			/// The allocation comes from the same buffer as the previous one that's not an individual allocataion.
			same_buffer,
			/// A new buffer has been created for this allocation and maybe some following allocations, if there is
			/// enough space.
			new_buffer,
			/// The allocation is too large and a dedicated buffer is created for it.
			individual_buffer,
		};
		/// Result of 
		struct result {
			friend upload_buffers;
		public:
			gpu::buffer *buffer = nullptr; ///< Buffer that the allocation is from.
			std::uint32_t offset = 0; ///< Offset of the allocation in bytes from the start of the buffer.
			allocation_type type = allocation_type::invalid; ///< The type of this allocation.
		private:
			/// Initializes all fields of this struct.
			result(gpu::buffer &b, std::uint32_t off, allocation_type t) : buffer(&b), offset(off), type(t) {
			}
		};

		/// Initializes this object to empty.
		upload_buffers(std::nullptr_t) : allocate_buffer(nullptr), _current(nullptr) {
		}
		/// Initializes this object with the given device and parameters.
		explicit upload_buffers(
			context &ctx, allocate_buffer_func alloc, std::size_t buf_size = default_buffer_size
		) : allocate_buffer(std::move(alloc)), _current(nullptr), _buffer_size(buf_size), _context(&ctx) {
		}

		/// Allocates space for a chunk of the given size and writes the given data into it.
		[[nodiscard]] result stage(std::span<const std::byte>, std::size_t alignment);
		/// Flushes the current buffer. The caller only need to transition the buffers from CPU write to copy source.
		void flush();

		/// Returns the size of regular allocated buffers.
		[[nodiscard]] std::size_t get_buffer_size() const {
			return _buffer_size;
		}

		/// Returns whether this object is valid.
		[[nodiscard]] bool is_valid() const {
			return _context;
		}
		/// \overload
		[[nodiscard]] explicit operator bool() const {
			return is_valid();
		}

		/// Callback used by this object to allocate a new buffer.
		static_function<gpu::buffer&(std::size_t)> allocate_buffer;
	private:
		gpu::buffer *_current; ///< Current buffer that's being filled out.
		std::byte *_current_ptr = nullptr; ///< Mapped pointer of \ref _current.
		std::size_t _current_used = 0; ///< Amount used in \ref _current.

		std::size_t _buffer_size = 0; ///< Size of \ref _current or any newly allocated buffers.
		context *_context = nullptr; ///< Associated context.
	};
}
