#pragma once

/// \file
/// Interface to graphics contexts.

#include <utility>
#include <filesystem>

#include "lotus/system/window.h"
#include "lotus/utils/static_function.h"
#include LOTUS_GPU_BACKEND_INCLUDE_COMMON
#include LOTUS_GPU_BACKEND_INCLUDE_CONTEXT
#include "common.h"
#include "device.h"
#include "frame_buffer.h"

namespace lotus::gpu {
	/// The name of the backend that is currently active.
	constexpr static backend_type current_backend = backend::type;

	/// Represents a generic interface to the underlying graphics library.
	class context : public backend::context {
	public:
		/// Backend-specific ID type used to identify debug messages.
		using debug_message_id = backend::context::debug_message_id;
		/// Callback type for debug messages from the graphics API.
		using debug_message_callback = static_function<void(
			debug_message_severity,
			debug_message_id,
			std::u8string_view
		)>;

		/// No default construction.
		context() = delete;
		/// Creates a new context object.
		[[nodiscard]] inline static context create(context_options opt, debug_message_callback debug_msg_cb = nullptr) {
			return backend::context::create(opt, std::move(debug_msg_cb));
		}
		/// No copy construction.
		context(const context&) = delete;
		/// No copy assignment.
		context &operator=(const context&) = delete;

		/// Returns a list of all available graphics adapters.
		[[nodiscard]] std::vector<adapter> get_all_adapters() const {
			std::vector<backend::adapter> adapters = backend::context::get_all_adapters();
			std::vector<adapter> result;
			result.reserve(adapters.size());
			for (backend::adapter &adap : adapters) {
				result.emplace_back(adapter(std::move(adap)));
			}
			return result;
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
			/// Move constructor.
			compilation_result(compilation_result &&src) :
				backend::shader_utility::compilation_result(std::move(src)) {
			}
			/// No copy constructor.
			compilation_result(const compilation_result&) = delete;
			/// Move assignment.
			compilation_result &operator=(compilation_result &&src) {
				backend::shader_utility::compilation_result::operator=(std::move(src));
				return *this;
			}
			/// No copy assignment.
			compilation_result &operator=(const compilation_result&) = delete;

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
		/// Loads shader library reflection from the given data.
		[[nodiscard]] shader_library_reflection load_shader_library_reflection(std::span<const std::byte> data) {
			return backend::shader_utility::load_shader_library_reflection(data);
		}
		// TODO more options
		/// Compiles the given shader.
		[[nodiscard]] compilation_result compile_shader(
			std::span<const std::byte> code_utf8, shader_stage stage, std::u8string_view entry,
			const std::filesystem::path &shader_path, std::span<const std::filesystem::path> include_paths,
			std::span<const std::pair<std::u8string_view, std::u8string_view>> defines
		) {
			return backend::shader_utility::compile_shader(
				code_utf8, stage, entry, shader_path, include_paths, defines
			);
		}
		/// \overload
		[[nodiscard]] compilation_result compile_shader(
			std::span<const std::byte> code_utf8, shader_stage stage, std::u8string_view entry,
			const std::filesystem::path &shader_path, std::initializer_list<std::filesystem::path> include_paths,
			std::initializer_list<std::pair<std::u8string_view, std::u8string_view>> defines
		) {
			return compile_shader(
				code_utf8, stage, entry, shader_path, { include_paths.begin(), include_paths.end() }, defines
			);
		}
		/// \overload
		[[nodiscard]] compilation_result compile_shader(
			std::span<const std::byte> code_utf8, shader_stage stage, std::u8string_view entry,
			const std::filesystem::path &shader_path, std::span<const std::filesystem::path> include_paths,
			std::span<const std::pair<std::u8string, std::u8string>> defines
		) {
			auto bookmark = get_scratch_bookmark();
			auto defs = bookmark.create_reserved_vector_array<
				std::pair<std::u8string_view, std::u8string_view>
			>(defines.size());
			for (const auto &def : defines) {
				defs.emplace_back(def.first, def.second);
			}
			return compile_shader(code_utf8, stage, entry, shader_path, include_paths, defs);
		}

		/// Compiles the given raytracing shader library.
		[[nodiscard]] compilation_result compile_shader_library(
			std::span<const std::byte> code_utf8,
			const std::filesystem::path &shader_path, std::span<const std::filesystem::path> include_paths,
			std::span<const std::pair<std::u8string_view, std::u8string_view>> defines
		) {
			return backend::shader_utility::compile_shader_library(code_utf8, shader_path, include_paths, defines);
		}
		/// \overload
		[[nodiscard]] compilation_result compile_shader_library(
			std::span<const std::byte> code_utf8,
			const std::filesystem::path &shader_path, std::initializer_list<std::filesystem::path> include_paths,
			std::initializer_list<std::pair<std::u8string_view, std::u8string_view>> defines
		) {
			return compile_shader_library(
				code_utf8, shader_path, { include_paths.begin(), include_paths.end() }, defines
			);
		}
		/// \overload
		[[nodiscard]] compilation_result compile_shader_library(
			std::span<const std::byte> code_utf8,
			const std::filesystem::path &shader_path, std::span<const std::filesystem::path> include_paths,
			std::span<const std::pair<std::u8string, std::u8string>> defines
		) {
			auto bookmark = get_scratch_bookmark();
			auto defs = bookmark.create_reserved_vector_array<
				std::pair<std::u8string_view, std::u8string_view>
			>(defines.size());
			for (const auto &def : defines) {
				defs.emplace_back(def.first, def.second);
			}
			return compile_shader_library(code_utf8, shader_path, include_paths, defs);
		}
	protected:
		/// Initializes the base object.
		shader_utility(backend::shader_utility src) : backend::shader_utility(std::move(src)) {
		}
	};
}
