#include "lotus/renderer/loaders/fbx_loader.h"

#ifdef LOTUS_RENDERER_HAS_FBX

#include <cassert>

#define FBXSDK_NAMESPACE_USING 0 // do not import the fbxsdk namespace
#include <fbxsdk.h>

#include "lotus/logging.h"
#include "lotus/utils/strings.h"
#include "lotus/math/vector.h"
#include "lotus/renderer/asset_manager.h"

namespace lotus::renderer::fbx {
	namespace _details {
		/// A handle of a FBX SDK object.
		template <typename T, auto ...DestroyArgs> struct handle {
		public:
			/// Initializes this handle to empty.
			handle(std::nullptr_t) {
			}
			/// Initializes \ref _ptr.
			handle(T *p) : _ptr(p) {
			}
			/// Move constructor.
			handle(handle &&src) : _ptr(std::exchange(src._ptr, nullptr)) {
			}
			/// No copy constructor.
			handle(const handle&) = delete;
			/// Move assignment.
			handle &operator=(handle &&src) {
				reset();
				_ptr = std::exchange(src._ptr, nullptr);
			}
			/// Destructor.
			~handle() {
				reset();
			}

			/// Returns the pointer.
			[[nodiscard]] T *get() const {
				return _ptr;
			}
			/// Dereferencing.
			[[nodiscard]] T *operator->() const {
				return _ptr;
			}

			/// Sets this handle to empty.
			void reset() {
				if (_ptr) {
					_ptr->Destroy(DestroyArgs...);
					_ptr = nullptr;
				}
			}

			/// Returns whether this handle holds a valid object.
			[[nodiscard]] bool is_valid() const {
				return _ptr != nullptr;
			}
			/// \overload
			[[nodiscard]] explicit operator bool() const {
				return is_valid();
			}
		private:
			T *_ptr = nullptr; ///< Pointer to the object.
		};

		/// FBX SDK handle.
		struct sdk {
			/// Initializes the SDK handle.
			sdk() : manager(fbxsdk::FbxManager::Create()) {
				assert(manager);

				fbxsdk::FbxString app_path = fbxsdk::FbxGetApplicationDirectory();
				manager->LoadPluginsDirectory(app_path.Buffer());
			}

			handle<fbxsdk::FbxManager> manager; ///< FBX manager.
		};
	}


	context context::create(assets::manager &man) {
		return context(man, new _details::sdk());
	}

	context::~context() {
		if (_sdk) {
			delete _sdk;
		}
	}

	void context::load(
		const std::filesystem::path &file,
		static_function<void(assets::handle<assets::texture2d>)> image_loaded_callback,
		static_function<void(assets::handle<assets::geometry>)> geometry_loaded_callback,
		static_function<void(assets::handle<assets::material>)> material_loaded_callback,
		static_function<void(instance)> instance_loaded_callback
	) {
		auto file_str = file.string();
		_details::handle<fbxsdk::FbxScene, true> scene =
			fbxsdk::FbxScene::Create(_sdk->manager.get(), file_str.c_str());
		assert(scene);
		_details::handle<fbxsdk::FbxImporter> importer =
			fbxsdk::FbxImporter::Create(_sdk->manager.get(), "Importer");
		assert(importer);

		if (!importer->Initialize(file_str.c_str(), -1, _sdk->manager->GetIOSettings())) {
			log().error<u8"Failed to initialize FBX importer: {}">(importer->GetStatus().GetErrorString());
			return;
		}

		if (!importer->Import(scene.get())) {
			if (importer->GetStatus().GetCode() == fbxsdk::FbxStatus::ePasswordError) {
				// TODO password???
			}
			log().error<u8"Failed to import FBX file: {}">(importer->GetStatus().GetErrorString());
		}

		// load geometries
		int geom_count = scene->GetGeometryCount();
		std::vector<assets::handle<assets::geometry>> geometries(static_cast<std::size_t>(geom_count), nullptr);
		for (int i = 0; i < geom_count; ++i) {
			auto *geom = scene->GetGeometry(i);
			if (geom->GetAttributeType() == fbxsdk::FbxNodeAttribute::eMesh) {
				const auto *mesh = static_cast<fbxsdk::FbxMesh*>(geom);
				const auto *control_points = mesh->GetControlPoints();

				auto *normals = mesh->GetElementNormalCount() > 0 ? mesh->GetElementNormal(0) : nullptr;
				auto *uvs = mesh->GetElementUVCount() > 0 ? mesh->GetElementUV(0) : nullptr;
				if (normals && normals->GetMappingMode() != fbxsdk::FbxGeometryElement::eNone) {
					normals = nullptr;
				}
				if (uvs && uvs->GetMappingMode() != fbxsdk::FbxGeometryElement::eNone) {
					uvs = nullptr;
				}

				fbxsdk::FbxStringList uv_sets;
				mesh->GetUVSetNames(uv_sets);

				bool can_use_indices = true;
				if (normals && normals->GetMappingMode() != fbxsdk::FbxGeometryElement::eByControlPoint) {
					can_use_indices = false;
				}
				if (uvs && uvs->GetMappingMode() != fbxsdk::FbxGeometryElement::eByControlPoint) {
					can_use_indices = false;
				}

				std::vector<cvec3f> mesh_positions;
				std::vector<cvec3f> mesh_normals;
				std::vector<cvec2f> mesh_uvs;
				std::vector<std::uint32_t> mesh_indices;

				if (can_use_indices) {
					int vert_count = mesh->GetControlPointsCount();
					for (int i_vert = 0; i_vert < vert_count; ++i_vert) {
						const auto &v = control_points[i_vert];
						mesh_positions.emplace_back(cvec3d(v[0], v[1], v[2]).into<float>());
						if (normals) {
							int i_norm = i_vert;
							if (normals->GetReferenceMode() == fbxsdk::FbxGeometryElement::eIndexToDirect) {
								i_norm = normals->GetIndexArray()[i_norm];
							}
							const auto &n = normals->GetDirectArray()[i_norm];
							mesh_normals.emplace_back(cvec3d(v[0], v[1], v[2]).into<float>());
						}
						if (uvs) {
							int i_uv = i_vert;
							if (uvs->GetReferenceMode() == fbxsdk::FbxGeometryElement::eIndexToDirect) {
								i_uv = uvs->GetIndexArray()[i_uv];
							}
							const auto &uv = uvs->GetDirectArray()[i_uv];
							mesh_uvs.emplace_back(cvec2d(uv[0], uv[1]).into<float>());
						}
					}
					int poly_count = mesh->GetPolygonCount();
					for (int i_poly = 0; i_poly < poly_count; ++i_poly) {
						int poly_size = mesh->GetPolygonSize(i_poly);
						if (poly_size != 3) {
							log().error<u8"Mesh {} polygon {} is not a triangle, skipping">(mesh->GetName(), i_poly);
							continue;
						}
						for (int i_pt = 0; i_pt < poly_size; ++i_pt) {
							int i_vert = mesh->GetPolygonVertex(i_poly, i_pt);
							if (i_vert < 0) {
								log().error<u8"Invalid vertex index for mesh {}, polygon {}, vertex {}">(
									mesh->GetName(), i_poly, i_pt
								);
								i_vert = 0;
							}
							mesh_indices.emplace_back(static_cast<std::uint32_t>(i_vert));
						}
					}
				} else {
					int poly_count = mesh->GetPolygonCount();
					for (int i_poly = 0; i_poly < poly_count; ++i_poly) {
						int poly_size = mesh->GetPolygonSize(i_poly);
						if (poly_size != 3) {
							log().error<u8"Mesh {} polygon {} is not a triangle, skipping">(mesh->GetName(), i_poly);
							continue;
						}
						for (int i_pt = 0; i_pt < poly_size; ++i_pt) {
							int i_vert = mesh->GetPolygonVertex(i_poly, i_pt);
							fbxsdk::FbxVector4 v;
							if (i_vert < 0) {
								log().error<u8"Invalid vertex index for mesh {}, polygon {}, vertex {}">(
									mesh->GetName(), i_poly, i_pt
								);
							} else {
								v = control_points[i_vert];
							}
							mesh_positions.emplace_back(cvec3d(v[0], v[1], v[2]).into<float>());
							if (normals) {
								fbxsdk::FbxVector4 n;
								if (!mesh->GetPolygonVertexNormal(i_poly, i_pt, n)) {
									log().error<u8"Failed to get normal for mesh {}, polygon {}, vertex {}">(
										mesh->GetName(), i_poly, i_pt
									);
								}
								mesh_normals.emplace_back(cvec3d(n[0], n[1], n[2]).into<float>());
							}
							if (uvs) {
								fbxsdk::FbxVector2 uv;
								bool unmapped = true;
								if (!mesh->GetPolygonVertexUV(i_poly, i_pt, uv_sets[0], uv, unmapped)) {
									log().error<u8"Failed to get UV for mesh {}, polygon {}, vertex {}">(
										mesh->GetName(), i_poly, i_pt
									);
								}
								mesh_uvs.emplace_back(cvec2d(uv[0], uv[1]).into<float>());
							}
						}
					}
				}

				assets::geometry loaded_geom = nullptr;

				auto vert_buffer_name = std::format("{}({}).verts", mesh->GetName(), i);
				loaded_geom.vertex_buffer.format = gpu::format::r32g32b32_float;
				loaded_geom.vertex_buffer.offset = 0;
				loaded_geom.vertex_buffer.stride = sizeof(cvec3f);
				loaded_geom.vertex_buffer.data = _asset_manager.create_buffer(
					assets::identifier(file, std::u8string(string::assume_utf8(vert_buffer_name))),
					std::span(mesh_positions),
					gpu::buffer_usage_mask::copy_destination |
					gpu::buffer_usage_mask::vertex_buffer    |
					gpu::buffer_usage_mask::shader_read_only |
					gpu::buffer_usage_mask::acceleration_structure_build_input
				);

				if (!mesh_normals.empty()) {
					auto normal_buffer_name = std::format("{}({}).normals", mesh->GetName(), i);
					loaded_geom.normal_buffer.format = gpu::format::r32g32b32_float;
					loaded_geom.normal_buffer.offset = 0;
					loaded_geom.normal_buffer.stride = sizeof(cvec3f);
					loaded_geom.normal_buffer.data = _asset_manager.create_buffer(
						assets::identifier(file, std::u8string(string::assume_utf8(normal_buffer_name))),
						std::span(mesh_normals),
						gpu::buffer_usage_mask::copy_destination |
						gpu::buffer_usage_mask::vertex_buffer    |
						gpu::buffer_usage_mask::shader_read_only
					);
				}

				if (!mesh_uvs.empty()) {
					auto uv_buffer_name = std::format("{}({}).uvs", mesh->GetName(), i);
					loaded_geom.uv_buffer.format = gpu::format::r32g32_float;
					loaded_geom.uv_buffer.offset = 0;
					loaded_geom.uv_buffer.stride = sizeof(cvec2f);
					loaded_geom.uv_buffer.data = _asset_manager.create_buffer(
						assets::identifier(file, std::u8string(string::assume_utf8(uv_buffer_name))),
						std::span(mesh_uvs),
						gpu::buffer_usage_mask::copy_destination |
						gpu::buffer_usage_mask::vertex_buffer    |
						gpu::buffer_usage_mask::shader_read_only
					);
				}

				if (!mesh_indices.empty()) {
					auto index_buffer_name = std::format("{}({}).indices", mesh->GetName(), i);
					loaded_geom.index_format = gpu::index_format::uint32;
					loaded_geom.index_offset = 0;
					loaded_geom.index_buffer = _asset_manager.create_buffer(
						assets::identifier(file, std::u8string(string::assume_utf8(index_buffer_name))),
						std::span(mesh_indices),
						gpu::buffer_usage_mask::copy_destination |
						gpu::buffer_usage_mask::index_buffer     |
						gpu::buffer_usage_mask::shader_read_only |
						gpu::buffer_usage_mask::acceleration_structure_build_input
					);
				}

				auto geom_name = std::format("{}({})", mesh->GetName(), i);
				geometries[i] = _asset_manager.register_geometry(
					assets::identifier(file, std::u8string(string::assume_utf8(geom_name))),
					std::move(loaded_geom)
				);
				if (geometry_loaded_callback) {
					geometry_loaded_callback(geometries[i]);
				}
			}
		}
	}
}

#endif // LOTUS_RENDERER_HAS_FBX
