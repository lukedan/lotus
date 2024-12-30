#pragma once

/// \file
/// Metal acceleration structures.

#include <cstddef>

namespace lotus::gpu::backends::metal {
	// TODO
	class bottom_level_acceleration_structure_geometry {
	protected:
		bottom_level_acceleration_structure_geometry(std::nullptr_t); // TODO
	};

	// TODO
	class instance_description {
	protected:
		instance_description(uninitialized_t); // TODO
	};

	// TODO
	class bottom_level_acceleration_structure {
	protected:
		bottom_level_acceleration_structure(std::nullptr_t); // TODO

		[[nodiscard]] bool is_valid() const; // TODO
	};

	// TODO
	class top_level_acceleration_structure {
	protected:
		top_level_acceleration_structure(std::nullptr_t); // TODO

		[[nodiscard]] bool is_valid() const; // TODO
	};
}
