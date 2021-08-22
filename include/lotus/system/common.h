#pragma once

/// \file
/// Common system-related types. This is the only file that platform-specific headers can include.

namespace lotus::system {
	/// The type of a message processed by the message queue.
	enum class message_type : std::uint8_t {
		none, ///< No message was processed.
		normal, ///< A normal message.
		quit, ///< A message that indicates that the application should quit.
	};
}
