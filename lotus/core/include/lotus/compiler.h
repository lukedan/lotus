#pragma once

/// \file
/// Compiler-specific directives.

#if defined(__clang__)
#define CAT(A, B) A ## B
#	define LOTUS_PRAGMA_PUSH_DIAGNOSTICS                           _Pragma("clang diagnostic push")
#	define LOTUS_PRAGMA_IGNORE_DIAGNOSTICS_SIGN_CONVERSION         _Pragma("clang diagnostic ignored \"-Wsign-conversion\"")
#	define LOTUS_PRAGMA_IGNORE_DIAGNOSTICS_IMPLICIT_INT_CONVERSION _Pragma("clang diagnostic ignored \"-Wimplicit-int-conversion\"")
#	define LOTUS_PRAGMA_IGNORE_DIAGNOSTICS_UNUSED_PARAMETER        _Pragma("clang diagnostic ignored \"-Wunused-parameter\"")
#	define LOTUS_PRAGMA_POP_DIAGNOSTICS                            _Pragma("clang diagnostic pop")
#else
#endif
