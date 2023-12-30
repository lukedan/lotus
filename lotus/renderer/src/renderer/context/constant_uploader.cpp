#include "lotus/renderer/context/constant_uploader.h"

/// \file
/// Implementation of the constant uploader.

namespace lotus::renderer {
	constant_uploader::constant_uploader(
		context &ctx, context::queue q, pool upload_pool, pool constant_pool, std::uint32_t chunk_sz
	) :
		_ctx(ctx), _upload_queue(q), _upload_pool(upload_pool), _constant_pool(constant_pool), _chunk_size(chunk_sz),
		_current_upload_buffer(nullptr), _current_constant_buffer(nullptr) {
	}

	descriptor_resource::constant_buffer constant_uploader::upload_bytes(std::span<const std::byte> data) {
		constexpr static auto _constant_buf_usage =
			gpu::buffer_usage_mask::copy_destination | gpu::buffer_usage_mask::shader_read;
		constexpr static auto _upload_buf_usage = gpu::buffer_usage_mask::copy_source;

		const auto data_size = static_cast<std::uint32_t>(data.size());

		if (data.size() > _chunk_size) {
			auto constant_buf = _ctx.request_buffer(
				u8"Constant buffer", data_size, _constant_buf_usage, _constant_pool
			);
			auto upload_buf = _ctx.request_buffer(
				u8"Constant upload buffer", data_size, _upload_buf_usage, _upload_pool
			);
			_ctx.write_data_to_buffer(upload_buf, data);
			_upload_queue.copy_buffer(upload_buf, constant_buf, 0, 0, data_size, u8"Upload constant buffer");
			return descriptor_resource::constant_buffer(constant_buf, 0, constant_buf.get_size_in_bytes());
		}

		const bool needs_new_buffer =
			!_current_constant_buffer.is_valid() ||
			_watermark + data_size > _current_constant_buffer.get_size_in_bytes();
		if (needs_new_buffer) {
			_maybe_flush_current_buffer();

			// allocate new buffers
			_current_constant_buffer = _ctx.request_buffer(
				u8"Constant buffer", _chunk_size, _constant_buf_usage, _constant_pool
			);
			_current_upload_buffer = _ctx.request_buffer(
				u8"Constant upload buffer", _chunk_size, _upload_buf_usage, _upload_pool
			);
			_ptr = _ctx.map_buffer(_current_upload_buffer);
		}

		const auto old_watermark = _watermark;
		const auto alignment = _ctx.get_adapter_properties().constant_buffer_alignment;
		_watermark = static_cast<std::uint32_t>(memory::align_up(_watermark + data_size, alignment));
		std::memcpy(_ptr + old_watermark, data.data(), data.size());
		return descriptor_resource::constant_buffer(_current_constant_buffer, old_watermark, data_size);
	}

	void constant_uploader::end_frame(dependency dep) {
		_maybe_flush_current_buffer();
		_upload_queue.release_dependency(dep, u8"Finish constant upload");
	}

	void constant_uploader::_maybe_flush_current_buffer() {
		if (_current_constant_buffer.is_valid()) {
			_ctx.flush_mapped_buffer_to_device(_current_upload_buffer, 0, _watermark);
			_ctx.unmap_buffer(_current_upload_buffer);
			_upload_queue.copy_buffer(
				_current_upload_buffer, _current_constant_buffer, 0, 0, _watermark, u8"Upload constant buffer"
			);

			// reset the buffers
			_current_constant_buffer = nullptr;
			_current_upload_buffer = nullptr;
			_watermark = 0;
			_ptr = nullptr;
		}
	}
}
