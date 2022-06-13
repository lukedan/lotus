#pragma once

/// \file
/// Interface to graphics contexts.

#include <utility>

#include "lotus/system/window.h"
#include LOTUS_GRAPHICS_BACKEND_INCLUDE
#include "common.h"
#include "device.h"
#include "frame_buffer.h"

namespace lotus::graphics {
	/// The name of this backend.
	constexpr static std::u8string_view backend_name = backend::backend_name;

	/// Represents a generic interface to the underlying graphics library.
	class context : public backend::context {
	public:
		/// No default construction.
		context() = delete;
		/// Creates a new context object.
		[[nodiscard]] inline static context create() {
			return backend::context::create();
		}
		/// No copy construction.
		context(const context&) = delete;
		/// No copy assignment.
		context &operator=(const context&) = delete;

		/// Enumerates over all adapters. The callback function will be called for every adapter, and may return a
		/// boolean indicating whether or not to continue enumeration.
		template <typename Callback> void enumerate_adapters(Callback &&cb) {
			using _result_t = std::invoke_result_t<Callback&&, adapter>;
			backend::context::enumerate_adapters([&cb](backend::adapter adap) {
				if constexpr (std::is_same_v<_result_t, bool>) {
					return cb(adapter(std::move(adap)));
				} else {
					static_assert(std::is_same_v<_result_t, void>, "Callback must return bool or nothing");
					cb(adapter(std::move(adap)));
					return true;
				}
			});
		}

		/// Creates a swap chain for the given window.
		///
		/// \param wnd The window to create the swap chain for.
		/// \param dev Device that can present to the swap chain.
		/// \param q Command queue that can present to the swap chain.
		/// \param frame_count The requested number of frames in the swap chain. The actual count may be different
		///                    and can be queried using \ref swap_chain::get_image_count().
		/// \param formats List of desired formats for the swap chain, ordered from most favorable to least
		///                favorable. If none of them is available, a random format will be chosen.
		[[nodiscard]] std::pair<swap_chain, format> create_swap_chain_for_window(
			system::window &wnd, device &dev, command_queue &q, std::size_t frame_count,
			std::span<const format> formats
		) {
			auto [swapchain, f] = backend::context::create_swap_chain_for_window(wnd, dev, q, frame_count, formats);
			return { swap_chain(std::move(swapchain)), f };
		}
		/// \overload
		[[nodiscard]] std::pair<swap_chain, format> create_swap_chain_for_window(
			system::window &wnd, device &dev, command_queue &q, std::size_t frame_count,
			std::initializer_list<format> formats
		) {
			return create_swap_chain_for_window(wnd, dev, q, frame_count, { formats.begin(), formats.end() });
		}
	protected:
		/// Initializes the base class.
		context(backend::context base) : backend::context(std::move(base)) {
		}
	};

	/// Utility class for compiling shaders and parsing shader reflection data.
	class shader_utility : public backend::shader_utility {
	public:
		/// Shader compilation result.
		class compilation_result : public backend::shader_utility::compilation_result {
			friend shader_utility;
		public:
			/// Returns whether shader compilation succeeded.
			[[nodiscard]] bool succeeded() const {
				return backend::shader_utility::compilation_result::succeeded();
			}
			/// Returns the output from the compiler.
			[[nodiscard]] std::u8string_view get_compiler_output() {
				return backend::shader_utility::compilation_result::get_compiler_output();
			}
			/// Returns the compiled binary code. Only valid if \ref succeeded() returns \p true.
			[[nodiscard]] std::span<const std::byte> get_compiled_binary() {
				return backend::shader_utility::compilation_result::get_compiled_binary();
			}
		protected:
			/// Initializes the base class.
			compilation_result(backend::shader_utility::compilation_result base) :
				backend::shader_utility::compilation_result(std::move(base)) {
			}
		};

		/// No default constructor.
		shader_utility() = delete;
		/// Creates a new object.
		[[nodiscard]] inline static shader_utility create() {
			return backend::shader_utility::create();
		}
		/// No copy construction.
		shader_utility(const shader_utility&) = delete;
		/// No copy assignment.
		shader_utility &operator=(const shader_utility&) = delete;

		/// Loads shader reflection from the given data.
		[[nodiscard]] shader_reflection load_shader_reflection(std::span<const std::byte> data) {
			return backend::shader_utility::load_shader_reflection(data);
		}
		/// Loads shader reflection from the given compiled shader.
		[[nodiscard]] shader_reflection load_shader_reflection(compilation_result &res) {
			return backend::shader_utility::load_shader_reflection(res);
		}
		// TODO more options
		/// Compiles the given shader.
		[[nodiscard]] compilation_result compile_shader(
			std::span<const std::byte> code_utf8, shader_stage stage, std::u8string_view entry,
			std::span<const std::filesystem::path> include_paths,
			std::span<const std::pair<std::u8string_view, std::u8string_view>> defines
		) {
			return backend::shader_utility::compile_shader(code_utf8, stage, entry, include_paths, defines);
		}
		/// \overload
		[[nodiscard]] compilation_result compile_shader(
			std::span<const std::byte> code_utf8, shader_stage stage, std::u8string_view entry,
			std::initializer_list<std::filesystem::path> include_paths,
			std::initializer_list<std::pair<std::u8string_view, std::u8string_view>> defines
		) {
			return compile_shader(
				code_utf8, stage, entry, { include_paths.begin(), include_paths.end() }, defines
			);
		}
		/// \overload
		[[nodiscard]] compilation_result compile_shader(
			std::span<const std::byte> code_utf8, shader_stage stage, std::u8string_view entry,
			std::span<const std::filesystem::path> include_paths,
			std::span<const std::pair<std::u8string, std::u8string>> defines
		) {
			auto bookmark = stack_allocator::for_this_thread().bookmark();
			auto defs = bookmark.create_reserved_vector_array<std::pair<std::u8string_view, std::u8string_view>>(defines.size());
			for (const auto &def : defines) {
				defs.emplace_back(def.first, def.second);
			}
			return compile_shader(code_utf8, stage, entry, include_paths, defs);
		}
	protected:
		/// Initializes the base object.
		shader_utility(backend::shader_utility src) : backend::shader_utility(std::move(src)) {
		}
	};
}
