#include "lotus/gpu/backends/metal/device.h"

/// \file
/// Objectie-C implementation of Metal rendering devices.

#include <AppKit/AppKit.h>
#include <QuartzCore/QuartzCore.h>

namespace lotus::gpu::backends::metal {
	void device::resize_swap_chain_buffers(swap_chain &chain, cvec2u32 size) {
		// erase any unpresented drawables from the mapping
		if (chain._drawable) {
			const auto it = _drawable_mapping->find(chain._drawable->texture()->gpuResourceID());
			crash_if(it == _drawable_mapping->end());
			_drawable_mapping->erase(it);
		}
		chain._layer->setDrawableSize(CGSizeMake(size[0], size[1]));
	}
}
