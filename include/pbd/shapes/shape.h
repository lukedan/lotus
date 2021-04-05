#pragma once

/// \file
/// Different shapes.

#include <variant>

#include "simple.h"

namespace pbd {
	/// A generic shape.
	struct shape {
		/// The type of a shape. The order of entries in this \p enum must match that in \ref storage.
		enum class type : unsigned char {
			plane, ///< \ref shapes::plane.
			sphere, ///< \ref shapes::sphere.
		};

		/// A union for the storage of shapes. The order of types must match the \ref type enum.
		using storage = std::variant<shapes::plane, shapes::sphere>;

		/// Returns the type of the stored shape.
		[[nodiscard]] type get_type() const {
			return static_cast<type>(value.index());
		}

		storage value; ///< The value of this shape.
	};
}
