/// \file
/// A wrapper for including shader types. Note that this file does not has an include guard in order to support
/// multiple different includes.

#include "shader_types_impl.h"

#define bool     ::lotus::renderer::shader_types::bool_
#define int      ::lotus::renderer::shader_types::int_
#define int64_t  ::lotus::renderer::shader_types::int64_t
#define uint     ::lotus::renderer::shader_types::uint
#define uint64_t ::lotus::renderer::shader_types::uint64_t
#define dword    ::lotus::renderer::shader_types::dword

#define half     ::lotus::renderer::shader_types::half
#define float    ::lotus::renderer::shader_types::float_
#define double   ::lotus::renderer::shader_types::double_

#define int2     ::lotus::renderer::shader_types::int2
#define int3     ::lotus::renderer::shader_types::int3
#define int4     ::lotus::renderer::shader_types::int4

#define uint2    ::lotus::renderer::shader_types::uint2
#define uint3    ::lotus::renderer::shader_types::uint3
#define uint4    ::lotus::renderer::shader_types::uint4

#define float2   ::lotus::renderer::shader_types::float2
#define float3   ::lotus::renderer::shader_types::float3
#define float4   ::lotus::renderer::shader_types::float4


#define float1x2 ::lotus::renderer::shader_types::float1x2
#define float1x3 ::lotus::renderer::shader_types::float1x3
#define float1x4 ::lotus::renderer::shader_types::float1x4

#define float2x1 ::lotus::renderer::shader_types::float2x1
#define float2x2 ::lotus::renderer::shader_types::float2x2
#define float2x3 ::lotus::renderer::shader_types::float2x3
#define float2x4 ::lotus::renderer::shader_types::float2x4

#define float3x1 ::lotus::renderer::shader_types::float3x1
#define float3x2 ::lotus::renderer::shader_types::float3x2
#define float3x3 ::lotus::renderer::shader_types::float3x3
#define float3x4 ::lotus::renderer::shader_types::float3x4

#define float4x1 ::lotus::renderer::shader_types::float4x1
#define float4x2 ::lotus::renderer::shader_types::float4x2
#define float4x3 ::lotus::renderer::shader_types::float4x3
#define float4x4 ::lotus::renderer::shader_types::float4x4

namespace LOTUS_SHADER_TYPES_INCLUDE_NAMESPACE {
#include LOTUS_SHADER_TYPES_INCLUDE
}
#undef LOTUS_SHADER_TYPES_INCLUDE
#undef LOTUS_SHADER_TYPES_INCLUDE_NAMESPACE

#undef bool
#undef int
#undef int64_t
#undef uint
#undef uint64_t
#undef dword

#undef half
#undef float
#undef double

#undef int2
#undef int3
#undef int4

#undef uint2
#undef uint3
#undef uint4

#undef float2
#undef float3
#undef float4


#undef float1x2
#undef float1x3
#undef float1x4

#undef float2x1
#undef float2x2
#undef float2x3
#undef float2x4

#undef float3x1
#undef float3x2
#undef float3x3
#undef float3x4

#undef float4x1
#undef float4x2
#undef float4x3
#undef float4x4
