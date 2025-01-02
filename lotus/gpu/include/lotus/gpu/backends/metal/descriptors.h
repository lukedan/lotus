#pragma once

/// \file
/// Metal descriptors.

#include <cstddef>

namespace lotus::gpu::backends::metal {
	// TODO
	class descriptor_pool {
	protected:
		descriptor_pool(std::nullptr_t) {
			// TODO
		}
	};

	// TODO
	class descriptor_set_layout {
	protected:
		descriptor_set_layout(std::nullptr_t) {
			// TODO
		}

		[[nodiscard]] bool is_valid() const {
			// TODO
		}
	};

	// TODO
	class descriptor_set {
	protected:
		descriptor_set(std::nullptr_t) {
			// TODO
		}

		[[nodiscard]] bool is_valid() const {
			// TODO
		}
	};
}
