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


	namespace statistics {
		/// Result of a single timer.
		struct timer_result {
			/// Initializes this object to empty.
			timer_result(std::nullptr_t) {
			}

			std::u8string name; ///< The name of this timer.
			float duration_ms = 0.0f; ///< Duration of the timer in milliseconds.
		};

		/// Statistics about transitions.
		struct transitions {
			/// Initializes all statistics to zero.
			transitions(zero_t) {
			}

			std::uint32_t requested_image2d_transitions    = 0; ///< Number of 2D image transitions requested.
			std::uint32_t requested_image3d_transitions    = 0; ///< Number of 3D image transitions requested.
			std::uint32_t requested_buffer_transitions     = 0; ///< Number of buffer transitions requested.
			std::uint32_t requested_raw_buffer_transitions = 0; ///< Number of raw buffer transitions requested.

			std::uint32_t submitted_image2d_transitions    = 0; ///< Number of 2D image transitions submitted.
			std::uint32_t submitted_image3d_transitions    = 0; ///< Number of 3D image transitions submitted.
			std::uint32_t submitted_buffer_transitions     = 0; ///< Number of buffer transitions submitted.
			std::uint32_t submitted_raw_buffer_transitions = 0; ///< Number of raw buffer transitions submitted.
		};
		/// Statistics about immediate constant buffers.
		struct immediate_constant_buffers {
			/// Initializes all statistics to zero.
			immediate_constant_buffers(zero_t) {
			}

			std::uint32_t num_immediate_constant_buffers = 0; ///< Number of immediate constant buffer instances.
			std::uint32_t immediate_constant_buffer_size = 0; ///< Total size of all immediate constant buffers.
			/// Total size of all immediate constant buffers without padding.
			std::uint32_t immediate_constant_buffer_size_no_padding = 0;
		};
		/// Signature of a constant buffer.
		struct constant_buffer_signature {
			/// Initializes this object to zero.
			constexpr constant_buffer_signature(zero_t) {
			}

			/// Default equality comparison.
			[[nodiscard]] friend bool operator==(constant_buffer_signature, constant_buffer_signature) = default;

			std::uint32_t hash = 0; ///< Hash of the buffer's data.
			std::uint32_t size = 0; ///< Size of the buffer.
		};
	}
}
namespace std {
	/// Hash function for \ref lotus::renderer::statistics::constant_buffer_signature.
	template <> struct hash<lotus::renderer::statistics::constant_buffer_signature> {
		/// The hash function.
		[[nodiscard]] constexpr std::size_t operator()(
			lotus::renderer::statistics::constant_buffer_signature sig
		) const {
			return lotus::hash_combine({
				lotus::compute_hash(sig.hash),
				lotus::compute_hash(sig.size),
			});
		}
	};
}
namespace lotus::renderer {
	/// Batch statistics that are available as soon as a batch has been submitted.
	struct batch_statistics_early {
		/// Initializes all statistics to zero.
		batch_statistics_early(zero_t) {
		}

		std::vector<statistics::transitions> transitions; ///< Transition statistics.
		// TODO remove these
		/// Immediate constant buffer statistics.
		std::vector<statistics::immediate_constant_buffers> immediate_constant_buffers;
		/// Constant buffer information. This is costly to gather, so it's only filled if
		/// \ref execution::collect_constant_buffer_signature is on.
		std::unordered_map<statistics::constant_buffer_signature, std::uint32_t> constant_buffer_counts;
	};
	/// Batch statistics that are only available once a batch has finished execution.
	struct batch_statistics_late {
		/// Initializes all statistics to zero.
		batch_statistics_late(zero_t) {
		}

		std::vector<std::vector<statistics::timer_result>> timer_results; ///< Timer results for each queue.
	};
}
