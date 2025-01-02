#include "lotus/gpu/backends/metal/context.h"

/// \file
/// Objective-C implementation of the Metal context.

#include <array>

#include <AppKit/AppKit.h>
#include <QuartzCore/QuartzCore.h>

#include "lotus/gpu/common.h"

namespace lotus::gpu::backends::metal {
	std::pair<swap_chain, format> context::create_swap_chain_for_window(
		system::window &sys_wnd,
		device &dev,
		command_queue&,
		std::size_t frame_count,
		std::span<const format> formats
	) {
		auto *wnd = (__bridge NSWindow*)sys_wnd.get_native_handle();

		// determine the format
		// https://developer.apple.com/documentation/quartzcore/cametallayer/pixelformat
		static const std::array _valid_formats{
			format::b8g8r8a8_unorm,
			format::b8g8r8a8_srgb,
			format::r16g16b16a16_float,
			// TODO additional formats not implemented yet
		};
		format result_fmt = format::b8g8r8a8_unorm;
		for (format f : formats) {
			if (std::find(_valid_formats.begin(), _valid_formats.end(), f) != _valid_formats.end()) {
				result_fmt = f;
				break;
			}
		}

		// create metal layer
		auto *layer = [[CAMetalLayer alloc] init];
		layer.maximumDrawableCount = frame_count;
		layer.device               = (__bridge id<MTLDevice>)(dev._dev.get());
		layer.drawableSize         = [wnd.contentView convertSizeToBacking: wnd.contentView.frame.size];
		layer.pixelFormat          = static_cast<MTLPixelFormat>(_details::conversions::to_pixel_format(result_fmt));

		layer.delegate = wnd.contentView;
		wnd.contentView.layer = layer;

		swap_chain result = nullptr;
		result._layer = (__bridge CA::MetalLayer*)layer; // we don't own the layer - the window does
		return { std::move(result), result_fmt };
	}


	std::uint32_t swap_chain::get_image_count() const {
		return static_cast<std::uint32_t>(((__bridge CAMetalLayer*)_layer).maximumDrawableCount);
	}
}
