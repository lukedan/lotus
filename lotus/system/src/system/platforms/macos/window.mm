#include "lotus/system/platforms/macos/window.h"

/// \file
/// Implementation of MacOS windows.

#include <AppKit/AppKit.h>
#include <Cocoa/Cocoa.h>
#include <QuartzCore/QuartzCore.h>
#include <Metal/Metal.h>

#include "lotus/logging.h"
#include "lotus/system/window.h"

using _window_ptr_t = lotus::system::window*; ///< Window pointer type.

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
		NSView *view = ((NSWindow*)_ptr->get_native_handle().window).contentView;
		const NSSize size = [view convertSizeToBacking: view.frame.size];
		lotus::system::window_events::resize event(lotus::cvec2u32(size.width, size.height));
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
	return lotus::cvec2i(pt.x, sz.height - pt.y);
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

/// Handles a mouse move event.
- (void)handle_mouse_move_event: (NSEvent*)e {
	_window_ptr_t wnd = *[(lotus_window_delegate*)self.delegate get_window_ptr];
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
	_window_ptr_t wnd = *[(lotus_window_delegate*)self.delegate get_window_ptr];
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
	_window_ptr_t wnd = *[(lotus_window_delegate*)self.delegate get_window_ptr];
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

- (void)mouseExited: (NSEvent*)e {
	_window_ptr_t wnd = *[(lotus_window_delegate*)self.delegate get_window_ptr];
	if (wnd->on_mouse_leave) {
		wnd->on_mouse_leave();
	}
}

@end


namespace lotus::system::platforms::macos {
	window::~window() {
		[(lotus_window*)_handle.window release];
		[(CAMetalLayer*)_handle.metal_layer release];
		[(lotus_window_delegate*)_delegate release];
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
		NSWindow *wnd = (NSWindow*)_handle.window;
		[wnd setIsVisible: true];
	}

	void window::show_and_activate() {
		NSWindow *wnd = (NSWindow*)_handle.window;
		[wnd setIsVisible: true];
		[wnd makeKeyAndOrderFront: nullptr];
	}

	void window::hide() {
		NSWindow *wnd = (NSWindow*)_handle.window;
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
		NSWindow *wnd = (NSWindow*)_handle.window;
		const NSSize size = [wnd.contentView convertSizeToBacking: wnd.contentView.frame.size];
		return cvec2s(size.width, size.height);
	}

	void window::set_title(std::u8string_view title) {
		NSString *title_str = [[NSString alloc]
			initWithBytes: title.data()
			length:        title.size()
			encoding:      NSUTF8StringEncoding
		];
		NSWindow *wnd = (NSWindow*)_handle.window;
		[wnd setTitle: title_str];
	}

	window::window() {
		// initialize window delegate
		lotus_window_delegate *delegate = [[lotus_window_delegate alloc]
			init_with_window_ptr: static_cast<system::window*>(this)
		];
		_window_ptr = [delegate get_window_ptr];

		// initialize window
		NSWindow *wnd = [[lotus_window alloc]
			initWithContentRect: CGRectMake(100.0f, 100.0f, 640.0f, 480.0f)
			styleMask:
				NSWindowStyleMaskTitled         |
				NSWindowStyleMaskClosable       |
				NSWindowStyleMaskMiniaturizable |
				NSWindowStyleMaskResizable
			backing:             NSBackingStoreBuffered
			defer:               false
		];
		wnd.delegate                = delegate;
		wnd.releasedWhenClosed      = false;
		wnd.acceptsMouseMovedEvents = true;

		// initialize metal layer
		CAMetalLayer *layer = [[CAMetalLayer alloc] init];
		layer.device       = MTLCreateSystemDefaultDevice();
		layer.opaque       = true;
		layer.drawableSize = [wnd.contentView convertSizeToBacking: wnd.contentView.frame.size];

		wnd.contentView.layer = layer;

		window::native_handle_t handle;
		_handle.window      = wnd;
		_handle.metal_layer = layer;
	}
}
