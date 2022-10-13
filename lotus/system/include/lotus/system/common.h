#pragma once

/// \file
/// Common system-related types. This is the only file that platform-specific headers can include.

#include "lotus/math/vector.h"

namespace lotus::system {
	/// The type of a message processed by the message queue.
	enum class message_type {
		none,   ///< No message was processed.
		normal, ///< A normal message.
		quit,   ///< A message that indicates that the application should quit.
	};

	/// Mouse buttons.
	enum class mouse_button {
		primary,   ///< The primary button, usually on the left.
		secondary, ///< The secondary button, usually on the right.
		middle,    ///< The middle button.

		num_enumerators ///< Total number of supported mouse buttons.
	};
	/// Bit mask for modifier keys.
	enum class modifier_key_mask : std::uint8_t {
		none    = 0,      ///< No modifier keys.
		control = 1 << 0, ///< "Control".
		shift   = 1 << 1, ///< "Shift".
		alt     = 1 << 2, ///< "Alt".
		super   = 1 << 3, ///< "Super".

		num_enumerators ///< Total number of available modifier keys.
	};
}
namespace lotus {
	/// Enable bitwise operations for \ref system::modifier_key_mask.
	template <> struct enable_enum_bitwise_operators<system::modifier_key_mask> : public std::true_type {
	};
	/// Enable \ref is_empty for \ref system::modifier_key_mask.
	template <> struct enable_enum_is_empty<system::modifier_key_mask> : public std::true_type {
	};
}


namespace lotus::system {
	namespace window_events {
		/// Information about the user requesting the window to close.
		struct close_request {
			bool should_close = false; ///< Indicates whether the window should respond by closing.
		};
		/// Information about the user resizing the window.
		struct resize {
			/// Initializes \ref new_size.
			explicit resize(cvec2s size) : new_size(size) {
			}

			const cvec2s new_size; ///< New size of this window.
		};

		namespace mouse {
			/// Information about mouse movement.
			struct move {
				/// Initializes \ref new_position.
				explicit move(cvec2i pos, modifier_key_mask mods) : new_position(pos), modifiers(mods) {
				}

				const cvec2i new_position; ///< New mouse position.
				const modifier_key_mask modifiers; ///< Modifier keys that are pressed.
			};
			/// Information about a mouse button being pressed.
			struct button_down {
				/// Initializes all fields of the struct.
				button_down(cvec2i pos, mouse_button btn, modifier_key_mask mods) :
					position(pos), button(btn), modifiers(mods) {
				}

				/// The position of the mouse when the button is pressed, relative to the client are of the window.
				const cvec2i position;
				const mouse_button button; ///< The mouse button.
				const modifier_key_mask modifiers; ///< Modifier keys that are pressed.
			};
			/// Information about a mouse button being released.
			struct button_up {
				/// Initializes \ref button.
				button_up(cvec2i pos, mouse_button btn, modifier_key_mask mods) :
					position(pos), button(btn), modifiers(mods) {
				}

				/// The position of the mouse when the button is released, relative to the client are of the window.
				const cvec2i position;
				const mouse_button button; ///< The mouse button.
				const modifier_key_mask modifiers; ///< Modifier keys that are pressed.
			};
		}
	}
}
