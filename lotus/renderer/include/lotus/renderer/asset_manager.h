#pragma once

/// \file
/// Class that manages all geometry in a scene.

#include <deque>
#include <unordered_map>
#include <variant>
#include <filesystem>
#include <memory>
#include <any>

#include "lotus/logging.h"
#include "lotus/containers/maybe_uninitialized.h"
#include "lotus/graphics/common.h"
#include "lotus/graphics/commands.h"
#include "lotus/graphics/device.h"
#include "lotus/graphics/resources.h"
#include "common.h"
#include "context.h"

namespace lotus::renderer {
	namespace assets {
		/// Manages the loading of all assets.
		class manager {
		public:
			/// No move construction.
			manager(manager&&) = delete;
			/// No move assignment.
			manager &operator=(manager&&) = delete;

			/// Creates a new instance.
			[[nodiscard]] inline static manager create(
				context &ctx,
				graphics::device &dev,
				graphics::command_queue &cmd_queue,
				std::filesystem::path shader_lib_path = {},
				graphics::shader_utility *shader_utils = nullptr
			) {
				return manager(ctx, dev, cmd_queue, std::move(shader_lib_path), shader_utils);
			}

			/// Retrieves a texture with the given ID, loading it if necessary.
			[[nodiscard]] handle<texture2d> get_texture2d(const identifier&);

			/// Finds the buffer with the given identifier. Returns \p nullptr if none exists.
			[[nodiscard]] handle<buffer> find_buffer(const identifier &id) {
				if (auto it = _buffers.find(id); it != _buffers.end()) {
					if (auto ptr = it->second.lock()) {
						return handle<buffer>(std::move(ptr));
					}
				}
				return nullptr;
			}
			/// Creates a buffer with the given contents and usage mask.
			[[nodiscard]] handle<buffer> create_buffer(
				identifier, std::span<const std::byte>, std::uint32_t byte_stride, graphics::buffer_usage::mask
			);
			/// \overload
			template <typename T> [[nodiscard]] std::enable_if_t<
				std::is_trivially_copyable_v<std::decay_t<T>>, handle<buffer>
			> create_buffer(identifier id, std::span<T> contents, graphics::buffer_usage::mask usages) {
				return create_buffer(
					std::move(id),
					std::span<const std::byte>(
						static_cast<const std::byte*>(static_cast<const void*>(contents.data())),
						contents.size() * sizeof(T)
					),
					sizeof(T),
					usages
				);
			}

			/// Compiles and loads the given shader. \ref identifier::subpath contains first the profile of the
			/// shader, then the entry point, then optionally a list of defines, separated by `|'.
			[[nodiscard]] handle<shader> compile_shader_from_source(
				const std::filesystem::path &id_path,
				std::span<const std::byte>,
				graphics::shader_stage,
				std::u8string_view entry_point,
				std::span<const std::pair<std::u8string_view, std::u8string_view>> defines = {}
			);
			/// \overload
			[[nodiscard]] handle<shader> compile_shader_from_source(
				const std::filesystem::path &id_path,
				std::span<const std::byte> code,
				graphics::shader_stage stage,
				std::u8string_view entry_point,
				std::initializer_list<const std::pair<std::u8string_view, std::u8string_view>> defines
			) {
				return compile_shader_from_source(
					id_path, code, stage, entry_point, { defines.begin(), defines.end() }
				);
			}
			/// \overload
			[[nodiscard]] handle<shader> compile_shader_from_source(
				const std::filesystem::path &id_path,
				std::span<const std::byte> code,
				graphics::shader_stage stage,
				std::u8string_view entry_point,
				std::span<const std::pair<std::u8string, std::u8string>> defines
			) {
				std::vector<std::pair<std::u8string_view, std::u8string_view>> def_views;
				for (const auto &d : defines) {
					def_views.emplace_back(d.first, d.second);
				}
				return compile_shader_from_source(id_path, code, stage, entry_point, def_views);
			}
			/// Similar to \ref compile_shader_from_source(), but loads the shader source code from the file system.
			[[nodiscard]] handle<shader> compile_shader_in_filesystem(
				const std::filesystem::path &path,
				graphics::shader_stage,
				std::u8string_view entry_point,
				std::span<const std::pair<std::u8string_view, std::u8string_view>> defines = {}
			);
			/// \overload
			[[nodiscard]] handle<shader> compile_shader_in_filesystem(
				const std::filesystem::path &path,
				graphics::shader_stage stage,
				std::u8string_view entry_point,
				std::initializer_list<const std::pair<std::u8string_view, std::u8string_view>> defines
			) {
				return compile_shader_in_filesystem(path, stage, entry_point, { defines.begin(), defines.end() });
			}
			/// \overload
			[[nodiscard]] handle<shader> compile_shader_in_filesystem(
				const std::filesystem::path &path,
				graphics::shader_stage stage,
				std::u8string_view entry_point,
				std::span<const std::pair<std::u8string, std::u8string>> defines
			) {
				std::vector<std::pair<std::u8string_view, std::u8string_view>> def_views;
				for (const auto &d : defines) {
					def_views.emplace_back(d.first, d.second);
				}
				return compile_shader_in_filesystem(path, stage, entry_point, def_views);
			}

			/// Registers a texture asset.
			handle<texture2d> register_texture2d(identifier id, texture2d tex) {
				return _register_asset(std::move(id), std::move(tex), _textures);
			}
			/// Registers a buffer asset.
			handle<buffer> register_buffer(identifier id, buffer buf) {
				return _register_asset(std::move(id), std::move(buf), _buffers);
			}
			/// Registers a geometry asset.
			handle<geometry> register_geometry(identifier id, geometry geom) {
				return _register_asset(std::move(id), std::move(geom), _geometries);
			}
			/// Registers a shader asset.
			handle<shader> register_shader(identifier id, shader sh) {
				return _register_asset(std::move(id), std::move(sh), _shaders);
			}
			/// Registers a material asset.
			handle<material> register_material(identifier id, material mat) {
				return _register_asset(std::move(id), std::move(mat), _materials);
			}

			/// Returns the descriptor array with descriptors of all loaded images.
			[[nodiscard]] recorded_resources::descriptor_array get_images() {
				return _texture2d_descriptors;
			}

			/// Returns the device associated with this asset manager.
			[[nodiscard]] graphics::device &get_device() const {
				return _device;
			}
			/// Returns the command queue used for copying assets to the GPU.
			[[nodiscard]] graphics::command_queue &get_command_queue() const {
				return _cmd_queue;
			}
			/// Returns the command allocator used for copying assets to the GPU.
			[[nodiscard]] graphics::command_allocator &get_command_allocator() {
				return _cmd_alloc;
			}
			/// Returns the fence used for synchronizing resource upload.
			[[nodiscard]] graphics::fence &get_fence() {
				return _fence;
			}
			/// Returns the path that contains shader files.
			[[nodiscard]] const std::filesystem::path &get_shader_library_path() const {
				return _shader_library_path;
			}
		private:
			/// Hashes an \ref identifier.
			struct _id_hash {
				/// Calls \ref identifier::hash().
				[[nodiscard]] std::size_t operator()(const identifier &id) const {
					return id.hash();
				}
			};
			///< A map containing a specific type of assets.
			template <typename T> using _map = std::unordered_map<identifier, std::weak_ptr<asset<T>>, _id_hash>;
			using _texture_map  = _map<texture2d>; ///< Texture map.
			using _buffer_map   = _map<buffer>;    ///< Buffer map.
			using _geometry_map = _map<geometry>;  ///< Geometry map.
			using _shader_map   = _map<shader>;    ///< Shader map.
			using _material_map = _map<material>;  ///< Material map.

			/// Initializes this manager.
			manager(context&, graphics::device&, graphics::command_queue&, std::filesystem::path, graphics::shader_utility*);

			/// Generic interface for registering an asset.
			template <typename T> handle<T> _register_asset(
				identifier id, T value, _map<T> &mp
			) {
				auto *asset_ptr = new asset<T>(std::move(value));
				auto ptr = std::shared_ptr<asset<T>>(asset_ptr);
				auto [it, inserted] = mp.emplace(std::move(id), ptr);
				if (!inserted) {
					assert(it->second.lock() == nullptr);
				}
				ptr->_id = &it->first;
				return handle<T>(std::move(ptr));
			}

			/// Allocates a descriptor index.
			[[nodiscard]] std::uint32_t _allocate_descriptor_index() {
				if (_texture2d_descriptor_index_alloc.size() == 1) {
					return _texture2d_descriptor_index_alloc[0]++;
				}
				std::uint32_t result = _texture2d_descriptor_index_alloc.back();
				_texture2d_descriptor_index_alloc.pop_back();
				return result;
			}
			/// Frees a descriptor index.
			void _free_descriptor_index(std::uint32_t id) {
				_texture2d_descriptor_index_alloc.emplace_back(id);
			}

			_texture_map  _textures;   ///< All loaded textures.
			_buffer_map   _buffers;    ///< All loaded buffers.
			_geometry_map _geometries; ///< All loaded geometries.
			_shader_map   _shaders;    ///< All loaded shaders.
			_material_map _materials;  ///< All loaded materials.

			graphics::device &_device; ///< Device that all assets are loaded onto.
			graphics::command_queue &_cmd_queue; ///< Command queue for texture and buffer copies.
			graphics::shader_utility *_shader_utilities; ///< Used for compiling shaders.

			context &_context; ///< Associated context.

			graphics::command_allocator _cmd_alloc; ///< Command allocator for texture and buffer copies.
			graphics::fence _fence; ///< Fence used for synchronization.

			descriptor_array _texture2d_descriptors; ///< Bindless descriptor array of all textures.
			std::vector<std::uint32_t> _texture2d_descriptor_index_alloc; ///< Used to allocate descriptor indices.

			std::filesystem::path _shader_library_path; ///< Path containing all shaders.
		};
	}
}
