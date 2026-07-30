#include "lotus/system/platforms/macos/thread_handle.h"

/// \file
/// Implementation of thread handles on MacOS.

#include <Foundation/NSString.h>
#include <Foundation/NSThread.h>

#include "lotus/system/platforms/macos/details.h"

namespace lotus::system::platforms::macos {
	void thread_handle::set_name(std::u8string_view name) {
		NSString *n = _details::conversions::to_ns_string(name);
		static_cast<NSThread*>(_handle).name = n;
		[n release];
	}

	std::u8string thread_handle::get_name() const {
		return std::u8string(reinterpret_cast<const char8_t*>(static_cast<NSThread*>(_handle).name.UTF8String));
	}

	thread_handle thread_handle::current() {
		return thread_handle([NSThread currentThread]);
	}
}
