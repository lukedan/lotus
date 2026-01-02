#include "lotus/gpu/backends/vulkan/moltenvk.h"

/// \file
/// Objective-C++ code for MoltenVK.

#include <AppKit/AppKit.h>
#include <QuartzCore/QuartzCore.h>
#include <Metal/Metal.h>

namespace lotus::gpu::backends::vulkan::_details {
	CAMetalLayer *create_core_animation_metal_layer(system::window::native_handle_t wnd_handle) {
		auto *wnd = static_cast<NSWindow*>(wnd_handle);

		// initialize metal layer
		auto *layer = [[CAMetalLayer alloc] init];
		layer.device       = MTLCreateSystemDefaultDevice(); // TODO is this correct? vkGetMTLDeviceMVK() is deprecated
		layer.opaque       = true;
		layer.drawableSize = [wnd.contentView convertSizeToBacking: wnd.contentView.frame.size];

		layer.delegate = wnd.contentView;
		wnd.contentView.layer = layer;
		[layer release]; // release the layer here - the window owns the layer

		return static_cast<CAMetalLayer*>(layer);
	}
}
