#pragma once

#include <lotus/system/application.h>
#include <lotus/renderer/context/context.h>

using lotus::zero;
using lotus::uninitialized;
using lotus::log;
using namespace lotus::vector_types;
using namespace lotus::matrix_types;
namespace lstr = lotus::string;
namespace lsys = lotus::system;
namespace lgpu = lotus::gpu;
namespace lren = lotus::renderer;

#include <lotus/renderer/shader_types_include_wrapper.h>
namespace shader_types {
#include "common_shaders/types.hlsli"
}
#include <lotus/renderer/shader_types_include_wrapper.h>