#include "lotus/gpu/backends/metal/pipeline.h"

/// \file
/// Implementation of Metal pipeline objects.

namespace lotus::gpu::backends::metal {
	NS::SharedPtr<MTL::Function> shader_binary::_get_single_function() const {
		crash_if(_lib->functionNames()->count() != 1);
		return NS::TransferPtr(_lib->newFunction(_lib->functionNames()->object<NS::String>(0)));
	}
}
