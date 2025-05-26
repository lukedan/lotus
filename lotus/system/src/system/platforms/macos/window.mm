#include "lotus/system/platforms/macos/window.h"

/// \file
/// Implementation of MacOS windows.

#include <Foundation/Foundation.h>
#include <AppKit/AppKit.h>
#include <Cocoa/Cocoa.h>

#include "lotus/logging.h"
#include "lotus/system/window.h"
#include "lotus/system/platforms/macos/application.h"
#include "lotus/system/platforms/macos/details.h"

using _window_ptr_t = lotus::system::window*; ///< Window pointer type.
using _custom_event_type_t = lotus::system::platforms::macos::_details::custom_event_type; ///< Custom event type.

/// Window delegate.
@interface lotus_window_delegate : NSObject<NSWindowDelegate>

/// Initializes the window with the given pointer to the \ref lotus::system::window.
- (instancetype)init_with_window_ptr: (_window_ptr_t)ptr;

/// Returns a pointer to the window pointer.
- (_window_ptr_t*)get_window_ptr;

@end

@implementation lotus_window_delegate {
	_window_ptr_t _ptr; ///< Pointer to the window.
}

- (instancetype)init_with_window_ptr: (_window_ptr_t)ptr {
	if (self = [super init]) {
		_ptr = ptr;
	}
	return self;
}

- (_window_ptr_t*)get_window_ptr {
	return &_ptr;
}

- (void)windowDidResize: (NSNotification*)notification {
	if (_ptr->on_resize) {
		auto *wnd = (__bridge NSWindow*)_ptr->get_native_handle();
		const NSSize size = [wnd.contentView convertSizeToBacking: wnd.contentView.frame.size];
		lotus::system::window_events::resize event(lotus::cvec2d(size.width, size.height).into<lotus::u32>());
		_ptr->on_resize(event);
	}
}

- (BOOL)windowShouldClose: (NSWindow*)sender {
	if (_ptr->on_close_request) {
		lotus::system::window_events::close_request event;
		_ptr->on_close_request(event);
		return event.should_close;
	}
	return false;
}

@end


/// Inherited window class to receive mouse and keyboard events.
@interface lotus_window : NSWindow

@end

@implementation lotus_window

/// Converts a mouse position from the window's base coordinate system to its backing coordinate system.
- (lotus::cvec2i)convert_mouse_position: (NSPoint)pos {
	const NSPoint pt = [self.contentView convertPointToBacking: pos];
	const NSSize sz = [self.contentView convertSizeToBacking: self.contentView.frame.size];
	return lotus::cvec2d(pt.x, sz.height - pt.y).into<int>();
}

/// Extracts modifier keys information from the given event.
- (lotus::system::modifier_key_mask)convert_modifier_keys: (NSEventModifierFlags)mods {
	auto result = lotus::system::modifier_key_mask::none;
	if (mods & NSEventModifierFlagCommand) {
		result |= lotus::system::modifier_key_mask::control;
	}
	if (mods & NSEventModifierFlagOption) {
		result |= lotus::system::modifier_key_mask::alt;
	}
	if (mods & NSEventModifierFlagControl) {
		result |= lotus::system::modifier_key_mask::super;
	}
	if (mods & NSEventModifierFlagShift) {
		result |= lotus::system::modifier_key_mask::shift;
	}
	return result;
}

/// \return Pointer to the lotus window associated with this \p NSWindow.
- (_window_ptr_t)get_window_ptr {
	return *[static_cast<lotus_window_delegate*>(self.delegate) get_window_ptr];
}

/// Handles a mouse move event.
- (void)handle_mouse_move_event: (NSEvent*)e {
	if (!NSMouseInRect(e.locationInWindow, self.contentView.frame, false)) {
		return; // the mouse is out of the client area
	}
	const _window_ptr_t wnd = [self get_window_ptr];
	if (wnd->on_mouse_move) {
		lotus::system::window_events::mouse::move event(
			[self convert_mouse_position: e.locationInWindow],
			[self convert_modifier_keys: e.modifierFlags]
		);
		wnd->on_mouse_move(event);
	}
}

/// Handles a mouse button down event.
- (void)handle_mouse_down_event: (NSEvent*)e button: (lotus::system::mouse_button)button {
	if (!NSMouseInRect(e.locationInWindow, self.contentView.frame, false)) {
		return; // the mouse is out of the client area
	}
	const _window_ptr_t wnd = [self get_window_ptr];
	if (wnd->on_mouse_button_down) {
		lotus::system::window_events::mouse::button_down event(
			[self convert_mouse_position: e.locationInWindow],
			button,
			[self convert_modifier_keys: e.modifierFlags]
		);
		wnd->on_mouse_button_down(event);
	}
}

/// Handles a mouse button up event.
- (void)handle_mouse_up_event: (NSEvent*)e button: (lotus::system::mouse_button)button {
	if (!NSMouseInRect(e.locationInWindow, self.contentView.frame, false)) {
		return; // the mouse is out of the client area
	}
	const _window_ptr_t wnd = [self get_window_ptr];
	if (wnd->on_mouse_button_up) {
		lotus::system::window_events::mouse::button_up event(
			[self convert_mouse_position: e.locationInWindow],
			button,
			[self convert_modifier_keys: e.modifierFlags]
		);
		wnd->on_mouse_button_up(event);
	}
}

- (void)mouseMoved: (NSEvent*)e {
	[self handle_mouse_move_event: e];
}

- (void)mouseDragged: (NSEvent*)e {
	[self handle_mouse_move_event: e];
}

- (void)rightMouseDragged: (NSEvent*)e {
	[self handle_mouse_move_event: e];
}

- (void)otherMouseDragged: (NSEvent*)e {
	// TODO check mouse button number
	[self handle_mouse_move_event: e];
}

- (void)mouseDown: (NSEvent*)e {
	[self handle_mouse_down_event: e button: lotus::system::mouse_button::primary];
}

- (void)mouseUp: (NSEvent*)e {
	[self handle_mouse_up_event: e button: lotus::system::mouse_button::primary];
}

- (void)rightMouseDown: (NSEvent*)e {
	[self handle_mouse_down_event: e button: lotus::system::mouse_button::secondary];
}

- (void)rightMouseUp: (NSEvent*)e {
	[self handle_mouse_up_event: e button: lotus::system::mouse_button::secondary];
}

- (void)otherMouseDown: (NSEvent*)e {
	// TODO check mouse button number
	[self handle_mouse_down_event: e button: lotus::system::mouse_button::middle];
}

- (void)otherMouseUp: (NSEvent*)e {
	// TODO check mouse button number
	[self handle_mouse_up_event: e button: lotus::system::mouse_button::middle];
}

- (void)mouseExited: (NSEvent*)e {
	const _window_ptr_t wnd = [self get_window_ptr];
	if (wnd->on_mouse_leave) {
		wnd->on_mouse_leave();
	}
}

- (void)scrollWheel: (NSEvent*)e {
	const _window_ptr_t wnd = [self get_window_ptr];
	if (wnd->on_mouse_scroll) {
		lotus::system::window_events::mouse::scroll event(
			[self convert_mouse_position: e.locationInWindow],
			lotus::cvec2d(e.scrollingDeltaX, e.scrollingDeltaY).into<float>(),
			[self convert_modifier_keys: e.modifierFlags]
		);
		wnd->on_mouse_scroll(event);
	}
}

- (void)keyDown: (NSEvent*)e {
	const _window_ptr_t wnd = [self get_window_ptr];
	if (wnd->on_key_down) {
		lotus::system::window_events::key_down event(
			lotus::system::platforms::macos::_details::conversions::to_key(e.keyCode),
			[self convert_modifier_keys: e.modifierFlags]
		);
		wnd->on_key_down(event);
	}
}

- (void)keyUp: (NSEvent*)e {
	const _window_ptr_t wnd = [self get_window_ptr];
	if (wnd->on_key_up) {
		lotus::system::window_events::key_up event(
			lotus::system::platforms::macos::_details::conversions::to_key(e.keyCode),
			[self convert_modifier_keys: e.modifierFlags]
		);
		wnd->on_key_up(event);
	}
}

- (void)sendEvent: (NSEvent*)event {
	if (event.type == NSEventTypeApplicationDefined) {
		switch (static_cast<_custom_event_type_t>(event.subtype)) {
		case _custom_event_type_t::window_initialized:
			{
				const _window_ptr_t wnd = [self get_window_ptr];
				if (wnd->on_resize) {
					const NSSize size = [self.contentView convertSizeToBacking: self.contentView.frame.size];
					lotus::system::window_events::resize event(
						lotus::cvec2d(size.width, size.height).into<lotus::u32>()
					);
					wnd->on_resize(event);
				}
				return;
			}
		default:
			break;
		}
	}
	[super sendEvent: event];
}

@end


namespace lotus::system::platforms::macos {
	window::~window() {
		[[maybe_unused]] auto *wnd = (__bridge_transfer lotus_window*)_handle;
		[[maybe_unused]] auto *delegate = (__bridge_transfer lotus_window_delegate*)_delegate;
	}

	window::window(window &&src) :
		_handle(std::exchange(src._handle, {})),
		_delegate(std::exchange(src._delegate, nullptr)),
		_window_ptr(std::exchange(src._window_ptr, nullptr)) {

		if (_window_ptr) {
			*_window_ptr = static_cast<system::window*>(this);
		}
	}

	void window::show() {
		auto *wnd = (__bridge NSWindow*)_handle;
		[wnd setIsVisible: true];
	}

	void window::show_and_activate() {
		auto *wnd = (__bridge NSWindow*)_handle;
		[wnd setIsVisible: true];
		[wnd makeKeyAndOrderFront: wnd];
		// MacOS has stricter rules regarding application activation, so this is not guaranteed to work
		[NSApp activate];
	}

	void window::hide() {
		auto *wnd = (__bridge NSWindow*)_handle;
		[wnd setIsVisible: false];
	}

	void window::acquire_mouse_capture() {
		// TODO
	}

	bool window::has_mouse_capture() const {
		// TODO
	}

	void window::release_mouse_capture() {
		// TODO
	}

	cvec2s window::get_size() const {
		auto *wnd = (__bridge NSWindow*)_handle;
		const NSSize size = [wnd.contentView convertSizeToBacking: wnd.contentView.frame.size];
		return cvec2d(size.width, size.height).into<usize>();
	}

	void window::set_title(std::u8string_view title) {
		auto *title_str = [[NSString alloc]
			initWithBytes: title.data()
			length:        title.size()
			encoding:      NSUTF8StringEncoding
		];
		auto *wnd = (__bridge NSWindow*)_handle;
		[wnd setTitle: title_str];
	}

	window::window() {
		// initialize window delegate
		auto *delegate = [[lotus_window_delegate alloc]
			init_with_window_ptr: static_cast<system::window*>(this)
		];
		_window_ptr = [delegate get_window_ptr];

		// initialize window
		auto *wnd = [[lotus_window alloc]
			initWithContentRect: CGRectMake(100.0f, 100.0f, 640.0f, 480.0f)
			styleMask:
				NSWindowStyleMaskTitled         |
				NSWindowStyleMaskClosable       |
				NSWindowStyleMaskMiniaturizable |
				NSWindowStyleMaskResizable
			backing:             NSBackingStoreBuffered
			defer:               false
		];
		wnd.delegate           = delegate;
		wnd.releasedWhenClosed = false;

		// add a tracking area to the window to enable mouse leave events
		auto *tracking_area = [[NSTrackingArea alloc]
			initWithRect: NSZeroRect
			options:
				NSTrackingMouseEnteredAndExited |
				NSTrackingMouseMoved            |
				NSTrackingCursorUpdate          |
				NSTrackingActiveAlways          |
				NSTrackingInVisibleRect // to avoid having to manually resize
			owner:        wnd
			userInfo:     nullptr
		];
		[wnd.contentView addTrackingArea: tracking_area];

		_handle   = (__bridge_retained void*)wnd;
		_delegate = (__bridge_retained void*)delegate;

		// post a custom "initialized" event to the message queue so that the initial window size event can be sent
		auto *event = [NSEvent
			otherEventWithType: NSEventTypeApplicationDefined
			location:           NSZeroPoint
			modifierFlags:      0
			timestamp:          [[NSProcessInfo processInfo] systemUptime]
			windowNumber:       wnd.windowNumber
			context:            nullptr
			subtype:            static_cast<NSEventSubtype>(_details::custom_event_type::window_initialized)
			data1:              0
			data2:              0
		];
		[NSApp postEvent: event atStart: false];
	}
}
