#pragma once

/// \file
/// Class that manages all geometry in a scene.

#include <deque>
#include <unordered_map>
#include <variant>
#include <filesystem>
#include <memory>
#include <any>
#include <mutex>
#include <condition_variable>

#include "lotus/logging.h"
#include "lotus/containers/maybe_uninitialized.h"
#include "lotus/gpu/common.h"
#include "lotus/gpu/commands.h"
#include "lotus/gpu/device.h"
#include "lotus/gpu/resources.h"
#include "lotus/renderer/common.h"
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
				gpu::device &dev,
				std::filesystem::path shader_lib_path = {},
				gpu::shader_utility *shader_utils = nullptr
			) {
				return manager(ctx, dev, std::move(shader_lib_path), shader_utils);
			}

			/// Retrieves a image with the given ID. If it has not been loaded, it will be loaded and allocated out
			/// of the given pool.
			[[nodiscard]] handle<image2d> get_image2d(const identifier&, const pool&);

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
				identifier, std::span<const std::byte>, gpu::buffer_usage_mask, const pool&
			);
			/// \overload
			template <typename T> [[nodiscard]] std::enable_if_t<
				std::is_trivially_copyable_v<std::decay_t<T>>, handle<buffer>
			> create_buffer(identifier id, std::span<T> contents, gpu::buffer_usage_mask usages, const pool &p) {
				return create_buffer(
					std::move(id),
					std::span<const std::byte>(
						reinterpret_cast<const std::byte*>(contents.data()), contents.size_bytes()
					),
					usages,
					p
				);
			}

			/// Compiles and loads the given shader. \ref identifier::subpath contains first the profile of the
			/// shader, then the entry point, then optionally a list of defines, separated by `|'.
			[[nodiscard]] handle<shader> compile_shader_from_source(
				const std::filesystem::path &id_path,
				std::span<const std::byte>,
				gpu::shader_stage,
				std::u8string_view entry_point,
				std::span<const std::pair<std::u8string_view, std::u8string_view>> defines = {}
			);
			/// \overload
			[[nodiscard]] handle<shader> compile_shader_from_source(
				const std::filesystem::path &id_path,
				std::span<const std::byte> code,
				gpu::shader_stage stage,
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
				gpu::shader_stage stage,
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
				gpu::shader_stage,
				std::u8string_view entry_point,
				std::span<const std::pair<std::u8string_view, std::u8string_view>> defines = {}
			);
			/// \overload
			[[nodiscard]] handle<shader> compile_shader_in_filesystem(
				const std::filesystem::path &path,
				gpu::shader_stage stage,
				std::u8string_view entry_point,
				std::initializer_list<const std::pair<std::u8string_view, std::u8string_view>> defines
			) {
				return compile_shader_in_filesystem(path, stage, entry_point, { defines.begin(), defines.end() });
			}
			/// \overload
			[[nodiscard]] handle<shader> compile_shader_in_filesystem(
				const std::filesystem::path &path,
				gpu::shader_stage stage,
				std::u8string_view entry_point,
				std::span<const std::pair<std::u8string, std::u8string>> defines
			) {
				std::vector<std::pair<std::u8string_view, std::u8string_view>> def_views;
				for (const auto &d : defines) {
					def_views.emplace_back(d.first, d.second);
				}
				return compile_shader_in_filesystem(path, stage, entry_point, def_views);
			}

			/// Compiles and loads the given shader library. \ref identifier::subpath contains `lib' and then
			/// optionally a list of defines, separated by `|'.
			[[nodiscard]] handle<shader_library> compile_shader_library_from_source(
				const std::filesystem::path &id_path,
				std::span<const std::byte> code,
				std::span<const std::pair<std::u8string_view, std::u8string_view>> defines = {}
			);
			/// Similar to \ref compile_shader_library_from_source(), but loads the shader source code from the file
			/// system.
			[[nodiscard]] handle<shader_library> compile_shader_library_in_filesystem(
				const std::filesystem::path &path,
				std::span<const std::pair<std::u8string_view, std::u8string_view>> defines = {}
			);

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
			[[nodiscard]] recorded_resources::image_descriptor_array get_images() {
				return _image2d_descriptors;
			}
			/// Returns the descriptor array with descriptors of all samplers.
			[[nodiscard]] recorded_resources::cached_descriptor_set get_samplers() {
				return _sampler_descriptors;
			}
			/// Returns a handle for the image that indicates an invalid image.
			[[nodiscard]] const handle<image2d> &get_invalid_image() const {
				return _invalid_image;
			}

			/// Updates resource loading.
			void update();

			/// Returns the device associated with this asset manager.
			[[nodiscard]] gpu::device &get_device() const {
				return _device;
			}
			/// Returns the path that contains shader files.
			[[nodiscard]] const std::filesystem::path &get_shader_library_path() const {
				return _shader_library_path;
			}

			/// Returns the \ref context this manager is associated with.
			[[nodiscard]] context &get_context() const {
				return _context;
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
			using _image_map          = _map<image2d>;        ///< Texture map.
			using _buffer_map         = _map<buffer>;         ///< Buffer map.
			using _geometry_map       = _map<geometry>;       ///< Geometry map.
			using _shader_map         = _map<shader>;         ///< Shader map.
			using _shader_library_map = _map<shader_library>; ///< Shader library map.
			using _material_map       = _map<material>;       ///< Material map.

			/// Manages a thread that asynchronously loads resources.
			class _async_loader {
			public:
				/// The state of this loader.
				enum class state {
					running, ///< The loader is running normally.
					shutting_down ///< The loader is being shut down.
				};
				/// Results from jobs.
				enum class loader_type {
					stbi, ///< The image has successfully been loaded using stbi.
					dds,  ///< The image has successfully been loaded using \ref dds::loader.
				};
				/// A job.
				struct job {
					/// Initializes this job to empty.
					job(std::nullptr_t) : target(nullptr), memory_pool(nullptr) {
					}
					/// Initializes the job from a point where it's safe to access the identifier.
					job(handle<image2d> t, pool p) : target(std::move(t)), memory_pool(std::move(p)) {
						path = target.get().get_id().path;
					}

					handle<image2d> target; ///< Target image to load, to keep it alive.
					/// Path of the image. This is duplicated because it's not safe to access the \ref identifier
					/// from other threads.
					std::filesystem::path path;
					pool memory_pool; ///< Memory pool to allocate the texture from.
				};
				/// Result of a finished job.
				struct job_result {
					/// Function type used to free resources after the loaded data has been processed.
					using destroy_func = static_function<void()>;

					/// Initializes all fields of this struct.
					job_result(
						job j, loader_type t, const std::byte *res, cvec2s sz, gpu::format f, destroy_func d
					) : input(std::move(j)), type(t), data(res), size(sz), pixel_format(f), destroy(std::move(d)) {
					}
					/// Initializes this job with no return data.
					job_result(job j, std::nullptr_t) : input(std::move(j)), size(zero), destroy(nullptr) {
					}

					job input; ///< Original job description.

					loader_type type; ///< Job result.
					const std::byte *data = nullptr; ///< Loaded data.
					cvec2s size; ///< Size of the loaded image.
					gpu::format pixel_format = gpu::format::none; ///< Format of the loaded image.
					destroy_func destroy; ///< Called to free any intermediate resources.
				};

				/// Starts the worker thread.
				_async_loader();
				/// Terminates the loading thread.
				~_async_loader();

				/// Adds the given jobs to the job queue.
				void add_jobs(std::vector<job>);
				/// Returns a list of jobs that have been completed.
				[[nodiscard]] std::vector<job_result> get_completed_jobs();
			private:
				std::vector<job> _inputs; ///< Inputs.
				std::vector<job_result> _outputs; ///< Outputs.

				std::condition_variable _signal; ///< Used to signal that there are new jobs available.
				std::mutex _input_mtx; ///< Protects \ref _inputs.
				std::mutex _output_mtx; ///< Protects \ref _outputs.
				std::thread _job_thread; ///< The worker thread.
				std::atomic<state> _state; ///< The state of this loader.

				/// Function that is ran by the job thread.
				void _job_thread_func();
				/// Processes one job.
				[[nodiscard]] job_result _process_job(job);
			};

			/// Initializes this manager.
			manager(context&, gpu::device&, std::filesystem::path, gpu::shader_utility*);

			/// Generic interface for registering an asset.
			template <typename T> handle<T> _register_asset(identifier id, T value, _map<T> &mp) {
				auto *asset_ptr = new asset<T>(std::move(value));
				auto ptr = std::shared_ptr<asset<T>>(asset_ptr);
				auto [it, inserted] = mp.emplace(std::move(id), ptr);
				if (!inserted) {
					assert(it->second.lock() == nullptr);
				}
				ptr->_id = &it->first;
				ptr->_uid = static_cast<assets::unique_id>(++_uid_alloc);
				return handle<T>(std::move(ptr));
			}

			/// Assembles the subid of the shader.
			[[nodiscard]] std::u8string _assemble_shader_subid(
				gpu::shader_stage,
				std::u8string_view entry_point,
				std::span<const std::pair<std::u8string_view, std::u8string_view>> defines
			);
			/// Assembles the subid of the shader library.
			[[nodiscard]] std::u8string _assemble_shader_library_subid(
				std::span<const std::pair<std::u8string_view, std::u8string_view>> defines
			);
			/// Compiles a shader from the given source without checking if it has already been registered.
			[[nodiscard]] handle<shader> _do_compile_shader_from_source(
				identifier,
				std::span<const std::byte> code,
				gpu::shader_stage,
				std::u8string_view entry_point,
				std::span<const std::pair<std::u8string_view, std::u8string_view>> defines
			);
			/// Compiles a shader library from the given source without checking if it has already been registered.
			[[nodiscard]] handle<shader_library> _do_compile_shader_library_from_source(
				identifier,
				std::span<const std::byte> code,
				std::span<const std::pair<std::u8string_view, std::u8string_view>> defines
			);

			/// Allocates a descriptor index.
			[[nodiscard]] std::uint32_t _allocate_descriptor_index() {
				if (_image2d_descriptor_index_alloc.size() == 1) {
					return _image2d_descriptor_index_alloc[0]++;
				}
				std::uint32_t result = _image2d_descriptor_index_alloc.back();
				_image2d_descriptor_index_alloc.pop_back();
				return result;
			}
			/// Frees a descriptor index.
			void _free_descriptor_index(std::uint32_t id) {
				_image2d_descriptor_index_alloc.emplace_back(id);
			}

			std::underlying_type_t<assets::unique_id> _uid_alloc = 0; ///< Unique ID allocation.

			_image_map          _images;           ///< All loaded images.
			_buffer_map         _buffers;          ///< All loaded buffers.
			_geometry_map       _geometries;       ///< All loaded geometries.
			_shader_map         _shaders;          ///< All loaded shaders.
			_shader_library_map _shader_libraries; ///< All loaded shader libraries.
			_material_map       _materials;        ///< All loaded materials.

			gpu::device &_device; ///< Device that all assets are loaded onto.
			gpu::shader_utility *_shader_utilities; ///< Used for compiling shaders.

			context &_context; ///< Associated context.

			_async_loader _image_loader; ///< Loader for images.
			/// Buffered input jobs. These will be submitted in \ref update().
			std::vector<_async_loader::job> _input_jobs;

			image_descriptor_array _image2d_descriptors; ///< Bindless descriptor array of all images.
			cached_descriptor_set _sampler_descriptors; ///< Descriptors of all samplers.
			handle<image2d> _invalid_image; ///< Index of a image indicating "invalid image".
			std::vector<std::uint32_t> _image2d_descriptor_index_alloc; ///< Used to allocate descriptor indices.

			std::filesystem::path _shader_library_path; ///< Path containing all shaders.
		};
	}
}
