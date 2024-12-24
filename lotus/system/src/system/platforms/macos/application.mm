#include "lotus/system/platforms/macos/application.h"

#include <Foundation/Foundation.h>
#include <AppKit/AppKit.h>

#include "lotus/system/platforms/macos/window.h"

namespace lotus::system::platforms::macos {
	application::~application() {
		[(NSAutoreleasePool*)_pool release];
	}

	application::application(std::u8string_view name) {
		_pool = [[NSAutoreleasePool alloc] init];
		[NSApplication sharedApplication];
	}

	window application::create_window() const {
		return window();
	}

	/// Handles the given event.
	[[nodiscard]] message_type _handle_event(NSEvent *event) {
		if (!event) {
			return message_type::none;
		}
		[NSApp sendEvent: event];
		return message_type::normal;
	}

	message_type application::process_message_blocking() {
		NSEvent *event = [NSApp
			nextEventMatchingMask: NSEventMaskAny
			untilDate:             nullptr
			inMode:                NSDefaultRunLoopMode
			dequeue:               true
		];
		return _handle_event(event);
	}

	message_type application::process_message_nonblocking() {
		NSEvent *event = [NSApp
			nextEventMatchingMask: NSEventMaskAny
			untilDate:             [[NSDate alloc] init]
			inMode:                NSDefaultRunLoopMode
			dequeue:               true
		];
		return _handle_event(event);
	}

	void application::quit() {
		[NSApp terminate: nullptr]; // TODO this won't generate an event - investigate some other method
	}
}
