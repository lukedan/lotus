#pragma once

/// \file
/// Class used to handle constant buffers.

#include "resources.h"
#include "context.h"

namespace lotus::renderer {
	/// Class that manages the uploading of constants.
	class constant_uploader {
	public:
		/// Initializes this uploader.
		constant_uploader(
			context&, context::queue q, pool upload_pool, pool constant_pool, std::uint32_t chunk_sz = 4096 * 1024
		);
		/// No copy construction.
		constant_uploader(const constant_uploader&) = delete;
		/// No copy assignment.
		constant_uploader &operator=(const constant_uploader&) = delete;

		/// Uploads the given constant buffer to the GPU and returns a corresponding
		/// \ref descriptor_resource::constant_buffer.
		[[nodiscard]] descriptor_resource::constant_buffer upload_bytes(std::span<const std::byte>);
		/// Typed version of \ref upload_bytes().
		template <typename T> [[nodiscard]] descriptor_resource::constant_buffer upload(const T &obj) {
			auto *ptr = reinterpret_cast<const std::byte*>(&obj);
			return upload_bytes({ ptr, ptr + sizeof(T) });
		}
		/// Called at the end of a frame to flush this frame's results.
		///
		/// \param release The dependency to be released to signal that all constants have been uploaded.
		void end_frame(dependency release);
	private:
		context &_ctx; ///< The context associated with this uploader.
		context::queue _upload_queue; ///< The queue used to upload constants.
		pool _upload_pool; ///< The pool to allocate upload buffers out of.
		pool _constant_pool; ///< The pool to allocate constant buffers out of.
		std::uint32_t _chunk_size = 4096 * 1024; ///< The size of a single chunk.

		buffer _current_upload_buffer; ///< The current buffer used for uploading.
		buffer _current_constant_buffer; ///< The current constant buffer.
		std::uint32_t _watermark = 0; ///< Number of bytes allocated from the current buffers.
		std::byte *_ptr = nullptr; ///< Mapped pointer for \ref _current_upload_buffer.

		/// Flushes the current upload buffer and resets the buffers.
		void _maybe_flush_current_buffer();
	};
}
