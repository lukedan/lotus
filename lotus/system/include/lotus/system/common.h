#pragma once

/// \file
/// Common system-related types. This is the only file that platform-specific headers can include.

#include "lotus/enums.h"
#include "lotus/math/vector.h"

namespace lotus::system {
	/// Platform type.
	enum class platform_type {
		windows, ///< The Windows platform.
		macos, ///< The MacOS platform.
	};

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
namespace lotus::enums {
	/// \ref system::modifier_key_mask is a bit mask type.
	template <> struct is_bit_mask<system::modifier_key_mask> : public std::true_type {
	};
}


namespace lotus::system {
	/// A key.
	enum class key {
		f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24,
		num1, num2, num3, num4, num5, num6, num7, num8, num9, num0,
		a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z,
		numpad1, numpad2, numpad3, numpad4, numpad5, numpad6, numpad7, numpad8, numpad9, numpad0,
		escape,
		backspace,
		tab,
		caps_lock,
		left_shift, left_control, left_alt, left_super,
		right_shift, right_control, right_alt, right_super,
		space,
		enter,
		up, left, right, down,
		home, end,
		page_up, page_down,
		insert, del,

		unknown, ///< A key that is not handled.

		num_enumerators ///< Total number of keys.
	};

	namespace window_events {
		/// Information about the user requesting the window to close.
		struct close_request {
			bool should_close = false; ///< Indicates whether the window should respond by closing.
		};
		/// Information about the user resizing the window.
		struct resize {
			/// Initializes \ref new_size.
			explicit resize(cvec2u32 size) : new_size(size) {
			}

			const cvec2u32 new_size; ///< New size of this window.
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
				/// Initializes all fields of this struct.
				button_up(cvec2i pos, mouse_button btn, modifier_key_mask mods) :
					position(pos), button(btn), modifiers(mods) {
				}

				/// The position of the mouse when the button is released, relative to the client are of the window.
				const cvec2i position;
				const mouse_button button; ///< The mouse button.
				const modifier_key_mask modifiers; ///< Modifier keys that are pressed.
			};
			/// Information about scrolling.
			struct scroll {
				/// Initializes all fields of this struct.
				scroll(cvec2i pos, cvec2f off, modifier_key_mask mods) :
					position(pos), offset(off), modifiers(mods) {
				}

				/// The position of the mouse when scrolling happened, relative to the client area of the window.
				const cvec2i position;
				const cvec2f offset; ///< Scrolling offset.
				const modifier_key_mask modifiers; ///< Modifier keys that are pressed.
			};
		}

		/// Information about a key being pressed.
		struct key_down {
			/// Initializes all fields of this struct.
			key_down(key k, modifier_key_mask mods) : key_code(k), modifiers(mods) {
			}

			const key key_code; ///< The key being pressed.
			const modifier_key_mask modifiers; ///< Modifier keys that are pressed.
		};
		/// Information about a key being released.
		struct key_up {
			/// Initializes all fields of this struct.
			key_up(key k, modifier_key_mask mods) : key_code(k), modifiers(mods) {
			}

			const key key_code; ///< The key being released.
			const modifier_key_mask modifiers; ///< Modifier keys that are pressed.
		};
		/// Information about text input.
		struct text_input {
			/// Initializes all fields of this struct.
			explicit text_input(std::u8string t) : text(std::move(t)) {
			}

			const std::u8string text; ///< Input text.
		};
	}
}
