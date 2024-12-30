#pragma once

/// \file
/// Metal buffers and textures.

#include <cstddef>

namespace lotus::gpu::backends::metal {
	// TODO
	class memory_block {
	};

	// TODO
	class buffer {
	protected:
		buffer(std::nullptr_t); // TODO

		[[nodiscard]] bool is_valid() const; // TODO
	};

	// TODO
	struct staging_buffer_metadata {
	protected:
		staging_buffer_metadata(uninitialized_t); // TODO

		[[nodiscard]] std::size_t get_pitch_in_bytes() const; // TODO
		[[nodiscard]] cvec2u32 get_size() const; // TODO
		[[nodiscard]] gpu::format get_format() const; // TODO
	};

	// TODO
	class staging_buffer {

	};

	// TODO
	template <image_type Type> class basic_image : public image_base {
	protected:
		basic_image(std::nullptr_t); // TODO

		[[nodiscard]] bool is_valid() const; // TODO
	};

	// TODO
	template <image_type Type> class basic_image_view : public image_view_base {
	protected:
		basic_image_view(std::nullptr_t); // TODO

		[[nodiscard]] bool is_valid() const; // TODO
	};

	// TODO
	class sampler {
	protected:
		sampler(std::nullptr_t); // TODO
	};
}
