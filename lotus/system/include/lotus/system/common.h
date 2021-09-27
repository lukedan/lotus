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
	};


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
				explicit move(cvec2i pos) : new_position(pos) {
				}

				const cvec2i new_position; ///< New mouse position.
			};
			/// Information about a mouse button being pressed.
			struct button_down {
				/// Initializes all fields of the struct.
				button_down(cvec2i pos, mouse_button btn) : position(pos), button(btn) {
				}

				/// The position of the mouse when the button is pressed, relative to the client are of the window.
				const cvec2i position;
				const mouse_button button; ///< The mouse button.
			};
			/// Information about a mouse button being released.
			struct button_up {
				/// Initializes \ref button.
				button_up(cvec2i pos, mouse_button btn) : position(pos), button(btn) {
				}

				/// The position of the mouse when the button is released, relative to the client are of the window.
				const cvec2i position;
				const mouse_button button; ///< The mouse button.
			};
		}
	}
}
