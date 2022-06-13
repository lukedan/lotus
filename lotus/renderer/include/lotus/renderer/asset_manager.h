#pragma once

/// \file
/// Class that manages all geometry in a scene.

#include <deque>
#include <variant>
#include <filesystem>
#include <any>

#include "lotus/logging.h"
#include "lotus/containers/maybe_uninitialized.h"
#include "lotus/containers/pool.h"
#include "lotus/containers/pooled_hash_table.h"
#include "lotus/graphics/common.h"
#include "lotus/graphics/commands.h"
#include "lotus/graphics/device.h"
#include "lotus/graphics/resources.h"
#include "shader_types.h"
#include "common.h"

namespace lotus::renderer {
	namespace assets {
		class manager;
		template <typename T> struct handle;

		/// Unique identifier of an asset.
		struct identifier {
		public:
			/// Creates an empty identifier.
			identifier(std::nullptr_t) {
			}
			/// Creates a new identifier from the given path and subpath.
			[[nodiscard]] inline static identifier create(std::filesystem::path p, std::u8string sub = {}) {
				return identifier(std::move(p), std::move(sub));
			}

			/// Equality comparison.
			[[nodiscard]] friend bool operator==(const identifier&, const identifier&) = default;

			/// Computes a hash for this identifier.
			[[nodiscard]] std::size_t hash() const {
				return hash_combine(std::filesystem::hash_value(path), std::hash<std::u8string>()(subpath));
			}

			std::filesystem::path path; ///< Path to the asset.
			std::u8string subpath; ///< Additional identification of the asset within the file.
		private:
			/// Initializes all fields of this struct.
			identifier(std::filesystem::path p, std::u8string sub) : path(std::move(p)), subpath(std::move(sub)) {
			}
		};
	}

	/// An asset.
	template <typename T> class asset {
		friend assets::manager;
	public:
		/// Default move constructor.
		asset(asset&&) = default;
		/// Default move assignment.
		asset &operator=(asset&&) = default;

		assets::identifier id; ///< ID of this asset.
		T value; ///< The asset object.
		std::uint32_t ref_count; ///< Reference count for this asset.
	private:
		/// Initializes this asset.
		template <typename ...Args> asset(assets::identifier i, Args &&...args) :
			id(std::move(i)), value(std::forward<Args>(args)...), ref_count(0) {
		}
	};

	namespace assets {
		/// An owning handle of an asset - \ref asset::ref_count is updated automatically.
		template <typename T> struct owning_handle {
			friend handle<T>;
		public:
			/// Initializes this handle to empty.
			owning_handle(std::nullptr_t) : _ptr(nullptr) {
			}
			/// Move constructor.
			owning_handle(owning_handle &&src) : _ptr(std::exchange(src._ptr, nullptr)) {
			}
			/// No copy constructor; use \ref duplicate() instead.
			owning_handle(const owning_handle&&) = delete;
			/// Move assignment.
			owning_handle &operator=(owning_handle &&src) {
				if (&src != this) {
					if (_ptr) {
						--_ptr->ref_count;
					}
				}
				_ptr = std::exchange(src._ptr, nullptr);
				return *this;
			}
			/// No copy assignment; use \ref duplicate() instead.
			owning_handle &operator=(const owning_handle&) = delete;
			/// Destructor.
			~owning_handle() {
				if (_ptr) {
					--_ptr->ref_count;
				}
			}

			/// Returns the asset.
			[[nodiscard]] const asset<T> &get() const {
				return *_ptr;
			}

			/// Duplicates this handle.
			[[nodiscard]] owning_handle duplicate() const {
				return owning_handle(*_ptr);
			}

			/// Returns whether this handle is valid.
			[[nodiscard]] bool is_valid() const {
				return _ptr;
			}
			/// \overload
			[[nodiscard]] explicit operator bool() const {
				return is_valid();
			}
		private:
			/// Initializes this handle to the given asset and increases \ref asset::ref_count.
			explicit owning_handle(asset<T> &ass) : _ptr(&ass) {
				++_ptr->ref_count;
			}

			asset<T> *_ptr; ///< Pointer to the asset.
		};
		/// A non-owning handle of an asset.
		template <typename T> struct handle {
			friend owning_handle<T>;
			friend manager;
		public:
			/// Initializes this handle to empty.
			handle(std::nullptr_t) : _ptr(nullptr) {
			}

			/// Returns the asset.
			[[nodiscard]] const asset<T> &get() const {
				return *_ptr;
			}

			/// Creates a \ref owning_handle for the asset. This handle must be valid.
			[[nodiscard]] owning_handle<T> take_ownership() const {
				assert(_ptr);
				return owning_handle<T>(*_ptr);
			}

			/// Checks whether this handle is valid.
			[[nodiscard]] bool is_valid() const {
				return _ptr != nullptr;
			}
			/// \overload
			[[nodiscard]] explicit operator bool() const {
				return is_valid();
			}
		private:
			/// Initializes this handle to the given asset.
			explicit handle(asset<T> &ass) : _ptr(&ass) {
			}

			asset<T> *_ptr; ///< Pointer to the asset.
		};



		/// The type of an asset.
		enum class type {
			texture,  ///< A texture.
			shader,   ///< A shader.
			material, ///< A material that references a set of shaders and a number of textures.
			geometry, ///< A 3D mesh. Essentially a set of vertices and their attributes.
			blob,     ///< A large binary chunk of data.

			num_enumerators ///< The total number of types.
		};

		/// A loaded 2D texture.
		struct texture2d {
			/// Initializes this texture to empty.
			texture2d(std::nullptr_t) :
				image(nullptr), image_view(nullptr),
				pixel_format(graphics::format::none), size(zero), num_mips(0), highest_mip_loaded(0) {
			}

			graphics::image2d image; ///< The texture.
			graphics::image2d_view image_view; ///< View of the texture.
			graphics::format pixel_format; ///< Pixel format of this texture.
			cvec2<std::uint32_t> size; ///< The size of this texture.
			std::uint32_t num_mips; ///< Total number of mipmaps.
			std::uint32_t highest_mip_loaded; ///< The highest mip that has been loaded.
		};
		/// A generic data buffer.
		struct buffer {
			/// Initializes this buffer to empty.
			buffer(std::nullptr_t) : data(nullptr) {
			}

			graphics::buffer data; ///< The buffer.

			std::uint32_t byte_size = 0; ///< The size of this buffer in bytes.
			/// The size of an element in the buffer in bytes. Zero indicates an unstructured buffer.
			std::uint32_t byte_stride = 0;
			graphics::buffer_usage::mask usages = graphics::buffer_usage::mask::none; ///< Allowed usages.
		};
		/// A loaded shader.
		struct shader {
			/// Initializes this object to empty.
			shader(std::nullptr_t) : binary(nullptr), reflection(nullptr) {
			}

			graphics::shader_binary binary; ///< Shader binary.
			graphics::shader_reflection reflection; ///< Reflection data.

			shader_descriptor_bindings descriptor_bindings; ///< Descriptor bindings of this shader.
		};
		/// A material.
		struct material {
		public:
			/// Base class of context-specific material data.
			class context_data {
			public:
				/// Default virtual destructor.
				virtual ~context_data() = default;
			};

			/// Initializes this material to empty.
			material(std::nullptr_t) {
			}
			/// Creates a material with the given \ref context_data.
			[[nodiscard]] inline static material create(std::unique_ptr<context_data> d) {
				material result = nullptr;
				result.data = std::move(d);
				return result;
			}

			std::unique_ptr<context_data> data; ///< Material data.
		};
		/// A loaded geometry.
		struct geometry {
			/// Initializes this geometry to empty.
			geometry(std::nullptr_t) :
				vertex_buffer(nullptr), uv_buffer(nullptr), normal_buffer(nullptr), tangent_buffer(nullptr),
				index_buffer(nullptr), acceleration_structure_buffer(nullptr), acceleration_structure(nullptr) {
			}

			owning_handle<buffer> vertex_buffer;  ///< Vertex buffer.
			owning_handle<buffer> uv_buffer;      ///< UV buffer.
			owning_handle<buffer> normal_buffer;  ///< Normal buffer.
			owning_handle<buffer> tangent_buffer; ///< Tangent buffer.
			owning_handle<buffer> index_buffer;   ///< The index buffer.
			std::uint32_t num_vertices = 0; ///< Total number of vertices.
			std::uint32_t num_indices  = 0;  ///< Total number of indices.

			graphics::buffer acceleration_structure_buffer; ///< Buffer for acceleration structure.
			graphics::bottom_level_acceleration_structure acceleration_structure; ///< Acceleration structure.
		};
	}


	namespace assets {
		/// Manages the loading of all assets.
		class manager {
		public:
			/// Creates a new instance.
			[[nodiscard]] inline static manager create(
				graphics::device &dev,
				graphics::command_queue &cmd_queue,
				graphics::shader_utility *shader_utils = nullptr
			) {
				return manager(dev, cmd_queue, shader_utils);
			}

			/// Retrieves a texture with the given ID, loading it if necessary.
			[[nodiscard]] handle<texture2d> get_texture2d(const identifier&);

			/// Finds the buffer with the given identifier. Returns \p nullptr if none exists.
			[[nodiscard]] handle<buffer> find_buffer(const identifier &id) {
				if (auto ref = _find_asset(id, _buffers)) {
					return handle<buffer>(_buffers.at(ref));
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
			[[nodiscard]] handle<shader> compile_shader_from_source(const identifier&, std::span<const std::byte>);
			/// Similar to \ref compile_shader_from_source(), but loads the shader source code from the file system.
			[[nodiscard]] handle<shader> compile_shader_in_filesystem(const identifier&);

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
		private:
			/// Hashes \ref asset::id.
			template <typename T> struct _asset_hash {
				/// Calls \ref identifier::hash().
				[[nodiscard]] std::size_t operator()(const asset<T> &as) const {
					return as.id.hash();
				}
			};
			using _texture_pool  = pooled_hash_table<asset<texture2d>, _asset_hash<texture2d>>; ///< Texture pool.
			using _buffer_pool   = pooled_hash_table<asset<buffer>,    _asset_hash<buffer>>;    ///< Buffer pool.
			using _geometry_pool = pooled_hash_table<asset<geometry>,  _asset_hash<geometry>>;  ///< Geometry pool.
			using _shader_pool   = pooled_hash_table<asset<shader>,    _asset_hash<shader>>;    ///< Shader pool.
			using _material_pool = pooled_hash_table<asset<material>,  _asset_hash<material>>;  ///< Material pool.

			/// Initializes this manager.
			manager(graphics::device&, graphics::command_queue&, graphics::shader_utility*);

			/// Generic interface for registering an asset.
			template <typename T> handle<T> _register_asset(
				identifier id, T value, pooled_hash_table<asset<T>, _asset_hash<T>> &table
			) {
				assert(!table.find(static_cast<std::uint32_t>(id.hash()), [&id](const asset<T> &obj) {
					return id == obj.id;
				}).is_valid());
				auto ref = table.emplace(asset<T>(std::move(id), std::move(value)));
				return handle<T>(table.at(ref));
			}
			/// Tries to find the specified asset in the given pool.
			template <typename T> pooled_hash_table<asset<T>, _asset_hash<T>>::reference _find_asset(
				const identifier &id, pooled_hash_table<asset<T>, _asset_hash<T>> &table
			) {
				return table.find(
					static_cast<typename pooled_hash_table<asset<T>, _asset_hash<T>>::index_type>(id.hash()),
					[&id](const asset<T> &obj) {
						return id == obj.id;
					}
				);
			}

			_texture_pool  _textures;   ///< All loaded textures.
			_buffer_pool   _buffers;    ///< All loaded buffers.
			_geometry_pool _geometries; ///< All loaded geometries.
			_shader_pool   _shaders;    ///< All loaded shaders.
			_material_pool _materials;  ///< All loaded materials.

			graphics::device &_device; ///< Device that all assets are loaded onto.
			graphics::command_queue &_cmd_queue; ///< Command queue for texture and buffer copies.
			graphics::shader_utility *_shader_utilities; ///< Used for compiling shaders.

			graphics::command_allocator _cmd_alloc; ///< Command allocator for texture and buffer copies.
			graphics::fence _fence; ///< Fence used for synchronization.
			graphics::descriptor_pool _pool; ///< Pool of descriptors.
			graphics::descriptor_set_layout _texture2d_descriptor_layout; ///< Descriptor layout for 2D textures.
			graphics::descriptor_set _texture2d_descriptors; ///< Descriptors for 2D textures.
		};
	}
}