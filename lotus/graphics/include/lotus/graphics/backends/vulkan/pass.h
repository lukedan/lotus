#pragma once

/// \file
/// Pass related classes.

#include "details.h"

namespace lotus::graphics::backends::vulkan {
	class command_list;
	class device;


	/// Contains a \p vk::UniqueRenderPass.
	class pass_resources {
		friend command_list;
		friend device;
	protected:
	private:
		vk::UniqueRenderPass _pass; ///< The pass.
	};
}
