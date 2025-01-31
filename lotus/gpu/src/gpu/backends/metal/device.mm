#include "lotus/gpu/backends/metal/device.h"

/// \file
/// Objectie-C implementation of Metal rendering devices.

#include <AppKit/AppKit.h>
#include <QuartzCore/QuartzCore.h>

namespace lotus::gpu::backends::metal {
	void device::resize_swap_chain_buffers(swap_chain &chain, cvec2u32 size) {
		// TODO this is a hack that resolves the stuttering when the chain is resized
		//      https://developer.apple.com/forums/thread/773229
		constexpr static bool _create_new_chain = true;
		if (_create_new_chain) {
			auto *old_layer = (__bridge CAMetalLayer*)chain._layer;
			auto *wnd = (__bridge NSWindow*)chain._window;

			auto *new_layer = [[CAMetalLayer alloc] init];
			new_layer.maximumDrawableCount      = old_layer.maximumDrawableCount;
			new_layer.device                    = old_layer.device;
			new_layer.drawableSize              = CGSizeMake(size[0], size[1]);
			new_layer.pixelFormat               = old_layer.pixelFormat;
			new_layer.framebufferOnly           = old_layer.framebufferOnly;
			new_layer.allowsNextDrawableTimeout = old_layer.allowsNextDrawableTimeout;
			new_layer.opaque                    = old_layer.opaque;

			new_layer.delegate = wnd.contentView;
			wnd.contentView.layer = new_layer;

			chain._layer = (__bridge CA::MetalLayer*)new_layer;
		} else {
			chain._layer->setDrawableSize(CGSizeMake(size[0], size[1]));
		}
	}
}
