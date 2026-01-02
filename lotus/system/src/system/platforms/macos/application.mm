#include "lotus/system/platforms/macos/application.h"

#include <Foundation/Foundation.h>
#include <AppKit/AppKit.h>

#include "lotus/system/platforms/macos/window.h"

namespace lotus::system::platforms::macos {
	application::~application() {
		[static_cast<NSAutoreleasePool*>(_autorelease_pool) release];
	}

	application::application(std::u8string_view name) {
		_autorelease_pool = [[NSAutoreleasePool alloc] init];
		[NSApplication sharedApplication];
		[NSApp setActivationPolicy: NSApplicationActivationPolicyRegular];
	}

	window application::create_window() const {
		return window();
	}

	/// Handles the given event.
	[[nodiscard]] static message_type _handle_event(NSEvent *event) {
		if (!event) {
			return message_type::none;
		}
		if (event.type == NSEventTypeApplicationDefined) {
			switch (static_cast<_details::custom_event_type>(event.subtype)) {
			case _details::custom_event_type::quit:
				return message_type::quit;
			default:
				break;
			}
		}
		[NSApp sendEvent: event];
		return message_type::normal;
	}

	message_type application::process_message_blocking() {
		_drain_autorelease_pool();
		NSEvent *event = [NSApp
			nextEventMatchingMask: NSEventMaskAny
			untilDate:             nullptr
			inMode:                NSDefaultRunLoopMode
			dequeue:               true
		];
		return _handle_event(event);
	}

	message_type application::process_message_nonblocking() {
		_drain_autorelease_pool();
		NSEvent *event = [NSApp
			nextEventMatchingMask: NSEventMaskAny
			untilDate:             [[NSDate alloc] init]
			inMode:                NSDefaultRunLoopMode
			dequeue:               true
		];
		return _handle_event(event);
	}

	void application::quit() {
		auto *event = [NSEvent
			otherEventWithType: NSEventTypeApplicationDefined
			location:           NSZeroPoint
			modifierFlags:      0
			timestamp:          [[NSProcessInfo processInfo] systemUptime]
			windowNumber:       0
			context:            nullptr
			subtype:            static_cast<NSEventSubtype>(_details::custom_event_type::quit)
			data1:              0
			data2:              0
		];
		[NSApp postEvent: event atStart: false];
	}

	void application::_drain_autorelease_pool() {
		[static_cast<NSAutoreleasePool*>(_autorelease_pool) release];
		_autorelease_pool = [[NSAutoreleasePool alloc] init];
	}
}
