#pragma once

/// \file
/// Asset types.

#include <filesystem>

#include "resource_bindings.h"
#include "resources.h"

namespace lotus::renderer {
	namespace assets {
		/// Unique identifier of an asset.
		struct identifier {
			/// Creates an empty identifier.
			identifier(std::nullptr_t) {
			}
			/// Initializes all fields of this struct.
			explicit identifier(std::filesystem::path p, std::u8string sub = {}) :
				path(std::move(p)), subpath(std::move(sub)) {
			}

			/// Equality comparison.
			[[nodiscard]] friend bool operator==(const identifier&, const identifier&) = default;

			/// Computes a hash for this identifier.
			[[nodiscard]] usize hash() const {
				return hash_combine(std::filesystem::hash_value(path), std::hash<std::u8string>()(subpath));
			}

			std::filesystem::path path; ///< Path to the asset.
			std::u8string subpath; ///< Additional identification of the asset within the file.
		};

		/// Type of the ID used to uniquely identify assets. Two assets may have the same identifier due to reloading
		/// etc. but will not have the same unique ID. Valid unique ID's are nonzero.
		enum class unique_id : u32 {
			invalid = 0 ///< Indicates an invalid asset.
		};
	}

	/// An asset.
	template <typename T> class asset {
		friend assets::manager;
	public:
		T value; ///< The asset object.
		void *user_data = nullptr; ///< User data.

		/// Returns the ID that uniquely identifies this object.
		[[nodiscard]] assets::unique_id get_unique_id() const {
			assert(_uid != assets::unique_id::invalid);
			return _uid;
		}
		/// Retrieves the ID of this asset.
		[[nodiscard]] const assets::identifier &get_id() const {
			return *_id;
		}
	private:
		/// Initializes this asset.
		template <typename ...Args> asset(Args &&...args) : value(std::forward<Args>(args)...) {
		}

		/// ID of this asset. This will point to the key stored in the \p std::unordered_map.
		const assets::identifier *_id = nullptr;
		assets::unique_id _uid = assets::unique_id::invalid; ///< Unique ID to each asset.
	};


	namespace assets {
		/// An owning handle of an asset - \ref asset::ref_count is updated automatically.
		template <typename T> struct handle {
			friend manager;
			friend std::hash<handle<T>>;
		public:
			/// Initializes this handle to empty.
			handle(std::nullptr_t) : _ptr(nullptr) {
			}

			/// Returns a reference to the user data.
			[[nodiscard]] void *&user_data() const {
				return _ptr->user_data;
			}

			/// Returns the asset.
			[[nodiscard]] const asset<T> &get() const {
				return *_ptr;
			}

			/// Returns the value of this asset.
			[[nodiscard]] const T &get_value() const {
				return _ptr->value;
			}
			/// \overload
			[[nodiscard]] const T *operator->() const {
				return &get_value();
			}

			/// Returns the unique ID of the assets.
			[[nodiscard]] std::optional<assets::unique_id> get_unique_id() const {
				if (_ptr) {
					return _ptr->get_unique_id();
				}
				return std::nullopt;
			}

			/// Equality and inequality compares the unique ID.
			[[nodiscard]] friend bool operator==(const handle &lhs, const handle &rhs) {
				return lhs.get_unique_id() == rhs.get_unique_id();
			}

			/// Returns whether this handle is valid.
			[[nodiscard]] bool is_valid() const {
				return _ptr != nullptr;
			}
			/// \overload
			[[nodiscard]] explicit operator bool() const {
				return is_valid();
			}
		private:
			/// Initializes this handle to the given asset and increases \ref asset::ref_count.
			explicit handle(std::shared_ptr<asset<T>> ass) : _ptr(std::move(ass)) {
			}

			std::shared_ptr<asset<T>> _ptr; ///< Pointer to the asset.
		};
	}
}
namespace std {
	/// Hash function for \ref lotus::renderer::assets::handle.
	template <typename T> struct hash<lotus::renderer::assets::handle<T>> {
		/// Hashes the given object.
		[[nodiscard]] size_t operator()(const lotus::renderer::assets::handle<T> &h) const {
			return lotus::compute_hash(h.get().get_unique_id());
		}
	};
}

namespace lotus::renderer::assets {
	/// A loaded 2D image.
	struct image2d {
		/// Initializes this texture to empty.
		image2d(std::nullptr_t) : image(nullptr) {
		}

		image2d_view image; ///< The image.
		u32 highest_mip_loaded = 0; ///< The highest mip that has been loaded.
		u32 descriptor_index = 0; ///< Index of this texture in the global bindless descriptor table.
	};
	/// A generic data buffer.
	struct buffer {
		/// Initializes this buffer to empty.
		buffer(std::nullptr_t) : data(nullptr) {
		}

		renderer::buffer data; ///< The buffer.
	};
	/// A loaded shader.
	struct shader {
		/// Initializes this object to empty.
		shader(std::nullptr_t) : binary(nullptr), reflection(nullptr) {
		}

		gpu::shader_binary binary; ///< Shader binary.
		gpu::shader_reflection reflection; ///< Reflection data.
	};
	/// A collection of raytracing shaders.
	struct shader_library {
		/// Initializes this object to empty.
		shader_library(std::nullptr_t) : binary(nullptr), reflection(nullptr) {
		}

		gpu::shader_binary binary; ///< Shader binary.
		gpu::shader_library_reflection reflection; ///< Reflectino data.
	};
	/// A material.
	struct material {
	public:
		/// Base class of context-specific material data.
		class context_data {
		public:
			/// Default virtual destructor.
			virtual ~context_data() = default;

			/// Returns the file to include to use this type of material.
			[[nodiscard]] virtual std::u8string_view get_material_include() const = 0;
			/// Creates resource bindings for this material.
			[[nodiscard]] virtual all_resource_bindings create_resource_bindings(constant_uploader&) const = 0;
			/// No additional defines by default.
			[[nodiscard]] virtual std::vector<
				std::pair<std::u8string_view, std::u8string_view>
			> get_additional_ps_defines() const {
				return {};
			}
		};

		/// Initializes this material to empty.
		material(std::nullptr_t) {
		}
		/// Initializes material data.
		explicit material(std::unique_ptr<context_data> d) : data(std::move(d)) {
		}

		std::unique_ptr<context_data> data; ///< Material data.
	};
	/// A loaded geometry.
	struct geometry {
		/// Information about a buffer used as a rasterization stage input.
		struct input_buffer {
		public:
			/// Initializes this buffer to empty.
			input_buffer(std::nullptr_t) : data(nullptr) {
			}
			/// Creates a simple input buffer description, with the stide inferred from the pixel format.
			[[nodiscard]] inline static input_buffer create_simple(
				gpu::format f, u32 off, handle<buffer> d
			) {
				return input_buffer(std::move(d), off, gpu::format_properties::get(f).bytes_per_fragment, f);
			}

			/// Creates a \ref input_buffer_binding from this buffer.
			[[nodiscard]] input_buffer_binding into_input_buffer_binding(
				const char8_t *semantic, u32 semantic_index, u32 binding_index
			) const;

			handle<buffer> data; ///< Data of this input buffer.
			u32 offset = 0; ///< Offset of the first element in bytes.
			u32 stride = 0; ///< Stride between consecutive buffer elements in bytes.
			gpu::format format = gpu::format::none; ///< Format of an element.
		private:
			/// Initializes all fields of this struct.
			input_buffer(handle<buffer> d, u32 off, u32 str, gpu::format f) :
				data(std::move(d)), offset(off), stride(str), format(f) {
			}
		};

		/// Initializes this geometry to empty.
		geometry(std::nullptr_t) :
			vertex_buffer(nullptr),
			uv_buffer(nullptr),
			normal_buffer(nullptr),
			tangent_buffer(nullptr),
			index_buffer(nullptr) {
		}

		/// Returns a \ref index_buffer_binding for the index buffer of this geometry.
		[[nodiscard]] index_buffer_binding get_index_buffer_binding() const {
			return index_buffer_binding(index_buffer ? index_buffer->data : nullptr, 0, index_format);
		}
		/// Returns a \ref geometry_buffers_view for this geometry.
		[[nodiscard]] geometry_buffers_view get_geometry_buffers_view(gpu::raytracing_geometry_flags flags) const {
			return geometry_buffers_view(
				vertex_buffer.data->data,
				vertex_buffer.format,
				vertex_buffer.offset,
				vertex_buffer.stride,
				num_vertices,
				index_buffer ? index_buffer->data : nullptr,
				index_format,
				index_offset,
				num_indices,
				flags
			);
		}

		input_buffer vertex_buffer;  ///< Vertex buffer.
		input_buffer uv_buffer;      ///< UV buffer.
		input_buffer normal_buffer;  ///< Normal buffer.
		input_buffer tangent_buffer; ///< Tangent buffer.
		u32 num_vertices = 0; ///< Total number of vertices.

		handle<buffer> index_buffer; ///< The index buffer.
		u32 index_offset = 0; ///< Offset to the first index.
		u32 num_indices  = 0; ///< Total number of indices.
		gpu::index_format index_format = gpu::index_format::num_enumerators; ///< Format of indices.

		/// Primitive topology.
		gpu::primitive_topology topology = gpu::primitive_topology::num_enumerators;
	};
}


namespace lotus::renderer {
	/// References a function in a shader library.
	struct shader_function {
		/// Initializes this object to empty.
		shader_function(std::nullptr_t) : shader_library(nullptr) {
		}
		/// Initializes all fields of this struct.
		shader_function(assets::handle<assets::shader_library> lib, const char8_t *entry, gpu::shader_stage s) :
			shader_library(std::move(lib)), entry_point(entry), stage(s) {
		}

		/// Equality and inequality comparisons.
		[[nodiscard]] friend bool operator==(const shader_function &lhs, const shader_function &rhs) {
			return
				lhs.shader_library == rhs.shader_library &&
				std::strcmp(
					reinterpret_cast<const char*>(lhs.entry_point),
					reinterpret_cast<const char*>(rhs.entry_point)
				) == 0 &&
				lhs.stage == rhs.stage;
		}

		assets::handle<assets::shader_library> shader_library; ///< The shader library.
		const char8_t *entry_point = nullptr; ///< Entry point.
		gpu::shader_stage stage = gpu::shader_stage::all; ///< Shader stage of the entry point.
	};
}
namespace std {
	/// Hash function for \ref lotus::renderer::shader_function.
	template <> struct hash<lotus::renderer::shader_function> {
		/// Hashes the given object.
		[[nodiscard]] size_t operator()(const lotus::renderer::shader_function &func) const {
			return lotus::hash_combine({
				lotus::compute_hash(func.shader_library),
				lotus::compute_hash(std::u8string_view(func.entry_point)),
				lotus::compute_hash(func.stage),
			});
		}
	};
}
