#include "lotus/renderer/loaders/fbx_loader.h"

#ifdef LOTUS_RENDERER_HAS_FBX

#include <cassert>
#include <unordered_map>
#include <functional>

#define FBXSDK_NAMESPACE_USING 0 // do not import the fbxsdk namespace
#include <fbxsdk.h>

#include "lotus/logging.h"
#include "lotus/utils/strings.h"
#include "lotus/math/vector.h"
#include "lotus/renderer/context/asset_manager.h"

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
		static_function<void(assets::handle<assets::image2d>)> image_loaded_callback,
		static_function<void(assets::handle<assets::geometry>)> geometry_loaded_callback,
		static_function<void(assets::handle<assets::material>)> material_loaded_callback,
		static_function<void(instance)> instance_loaded_callback,
		const pool &buf_pool, const pool &tex_pool
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

		// we get link errors if we use FbxFileTexture::ClassId directly
		auto file_texture_classid = _sdk->manager->FindClass("FbxFileTexture");
		assert(file_texture_classid.IsValid());

		// load textures
		std::unordered_map<fbxsdk::FbxUInt64, assets::handle<assets::image2d>> images;
		int tex_count = scene->GetTextureCount();
		for (int i = 0; i < tex_count; ++i) {
			auto *base_tex = scene->GetTexture(i);
			if (base_tex->IsRuntime(file_texture_classid)) {
				auto *tex = static_cast<fbxsdk::FbxFileTexture*>(base_tex);
				if (
					tex->GetTranslationU() != 0.0f || tex->GetTranslationV() != 0.0f ||
					tex->GetScaleU() != 1.0f || tex->GetScaleV() != 1.0f
				) {
					log().warn<u8"Texture {} has non-identity UV transform which is not supported.">(tex->GetName());
				}
				auto handle = _asset_manager.get_image2d(assets::identifier(tex->GetFileName()), tex_pool);
				auto [it, inserted] = images.emplace(tex->GetUniqueID(), std::move(handle));
				crash_if(!inserted);
				if (image_loaded_callback) {
					image_loaded_callback(it->second);
				}
			}
		}

		// load geometries
		std::unordered_map<fbxsdk::FbxUInt64, std::vector<assets::handle<assets::geometry>>> geometries;
		int geom_count = scene->GetGeometryCount();
		for (int i_geom = 0; i_geom < geom_count; ++i_geom) {
			auto *geom = scene->GetGeometry(i_geom);
			if (geom->GetAttributeType() != fbxsdk::FbxNodeAttribute::eMesh) {
				continue;
			}

			const auto *mesh = static_cast<fbxsdk::FbxMesh*>(geom);
			auto unique_id = mesh->GetUniqueID();
			const auto *control_points = mesh->GetControlPoints();
			const auto *polygon_materials =
				mesh->GetElementMaterial()->GetMappingMode() == fbxsdk::FbxLayerElement::eByPolygon ?
				&mesh->GetElementMaterial()->GetIndexArray() :
				nullptr;

			auto *normals = mesh->GetElementNormalCount() > 0 ? mesh->GetElementNormal(0) : nullptr;
			auto *uvs = mesh->GetElementUVCount() > 0 ? mesh->GetElementUV(0) : nullptr;
			if (normals && normals->GetMappingMode() == fbxsdk::FbxGeometryElement::eNone) {
				normals = nullptr;
			}
			if (uvs && uvs->GetMappingMode() == fbxsdk::FbxGeometryElement::eNone) {
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

			// if we use indices, all the sub meshes will share vertex, uv, and normal buffers, so there will only be
			// one element in mesh_positions, mesh_normals, and mesh_uvs
			std::vector<std::vector<cvec3f>> mesh_positions = { { } };
			std::vector<std::vector<cvec3f>> mesh_normals = { { } };
			std::vector<std::vector<cvec2f>> mesh_uvs = { { } };
			std::vector<std::vector<std::uint32_t>> mesh_indices;

			int vert_count = mesh->GetControlPointsCount();
			if (can_use_indices) {
				for (int i_vert = 0; i_vert < vert_count; ++i_vert) {
					const auto &v = control_points[i_vert];
					mesh_positions[0].emplace_back(cvec3d(v[0], v[1], v[2]).into<float>());
					if (normals) {
						int i_norm = i_vert;
						if (normals->GetReferenceMode() == fbxsdk::FbxGeometryElement::eIndexToDirect) {
							i_norm = normals->GetIndexArray()[i_norm];
						}
						const auto &n = normals->GetDirectArray()[i_norm];
						mesh_normals[0].emplace_back(cvec3d(n[0], n[1], n[2]).into<float>());
					}
					if (uvs) {
						int i_uv = i_vert;
						if (uvs->GetReferenceMode() == fbxsdk::FbxGeometryElement::eIndexToDirect) {
							i_uv = uvs->GetIndexArray()[i_uv];
						}
						const auto &uv = uvs->GetDirectArray()[i_uv];
						mesh_uvs[0].emplace_back(cvec2d(uv[0], 1.0 - uv[1]).into<float>());
					}
				}
				int poly_count = mesh->GetPolygonCount();
				for (int i_poly = 0; i_poly < poly_count; ++i_poly) {
					int poly_size = mesh->GetPolygonSize(i_poly);
					if (poly_size < 3) {
						log().error<u8"Mesh {} polygon {} is degenerate, skipping">(mesh->GetName(), i_poly);
						continue;
					}

					int material_index = polygon_materials ? polygon_materials->GetAt(i_poly) : 0;
					if (material_index + 1 > mesh_indices.size()) {
						mesh_indices.resize(material_index + 1);
					}

					auto get_index = [mesh, i_poly, vert_count](int i_pt) -> std::uint32_t {
						int i_vert = mesh->GetPolygonVertex(i_poly, i_pt);
						if (i_vert < 0 || i_vert >= vert_count) {
							log().error<u8"Invalid vertex index for mesh {}, polygon {}, vertex {}">(
								mesh->GetName(), i_poly, i_pt
							);
							return 0;
						}
						return static_cast<std::uint32_t>(i_vert);
					};

					std::uint32_t i0 = get_index(0);
					std::uint32_t i_prev = get_index(1);
					for (int i_pt = 2; i_pt < poly_size; ++i_pt) {
						std::uint32_t i_current = get_index(i_pt);
						mesh_indices[material_index].emplace_back(i0);
						mesh_indices[material_index].emplace_back(i_prev);
						mesh_indices[material_index].emplace_back(i_current);
						i_prev = i_current;
					}
				}
			} else {
				int poly_count = mesh->GetPolygonCount();
				for (int i_poly = 0; i_poly < poly_count; ++i_poly) {
					int poly_size = mesh->GetPolygonSize(i_poly);
					if (poly_size < 3) {
						log().error<u8"Mesh {} polygon {} is degenerate, skipping">(mesh->GetName(), i_poly);
						continue;
					}

					int material_index = polygon_materials ? polygon_materials->GetAt(i_poly) : 0;
					if (material_index >= mesh_positions.size()) {
						mesh_positions.resize(material_index + 1);
						mesh_normals.resize(material_index + 1);
						mesh_uvs.resize(material_index + 1);
					}

					auto append_vertex = [&](int i_pt) {
						int i_vert = mesh->GetPolygonVertex(i_poly, i_pt);
						fbxsdk::FbxVector4 v;
						if (i_vert < 0 || i_vert >= vert_count) {
							log().error<u8"Invalid vertex index for mesh {}, polygon {}, vertex {}">(
								mesh->GetName(), i_poly, i_pt
							);
						} else {
							v = control_points[i_vert];
						}
						mesh_positions[material_index].emplace_back(cvec3d(v[0], v[1], v[2]).into<float>());
						if (normals) {
							fbxsdk::FbxVector4 n;
							if (!mesh->GetPolygonVertexNormal(i_poly, i_pt, n)) {
								log().error<u8"Failed to get normal for mesh {}, polygon {}, vertex {}">(
									mesh->GetName(), i_poly, i_pt
								);
							}
							mesh_normals[material_index].emplace_back(cvec3d(n[0], n[1], n[2]).into<float>());
						}
						if (uvs) {
							fbxsdk::FbxVector2 uv;
							bool unmapped = true;
							if (!mesh->GetPolygonVertexUV(i_poly, i_pt, uv_sets[0], uv, unmapped)) {
								log().error<u8"Failed to get UV for mesh {}, polygon {}, vertex {}">(
									mesh->GetName(), i_poly, i_pt
								);
							}  
							mesh_uvs[material_index].emplace_back(cvec2d(uv[0], 1.0 - uv[1]).into<float>());
						}
					};

					for (int i_pt = 2; i_pt < poly_size; ++i_pt) {
						append_vertex(0);
						append_vertex(i_pt - 1);
						append_vertex(i_pt);
					}
				}
			}

			std::vector<assets::geometry::input_buffer> position_inputs;
			std::vector<assets::geometry::input_buffer> normal_inputs;
			std::vector<assets::geometry::input_buffer> uv_inputs;
			std::vector<assets::handle<assets::buffer>> index_inputs;
			for (const auto &in : mesh_positions) {
				auto vert_buffer_name = std::format(
					"{}({}).verts[{}]", mesh->GetName(), unique_id, position_inputs.size()
				);
				position_inputs.emplace_back(assets::geometry::input_buffer::create_simple(
					gpu::format::r32g32b32_float, 0,
					_asset_manager.create_buffer(
						assets::identifier(file, std::u8string(string::assume_utf8(vert_buffer_name))),
						std::span(in),
						gpu::buffer_usage_mask::copy_destination |
						gpu::buffer_usage_mask::vertex_buffer |
						gpu::buffer_usage_mask::shader_read |
						gpu::buffer_usage_mask::acceleration_structure_build_input,
						buf_pool
					)
				));
			}
			for (const auto &in : mesh_normals) {
				auto normal_buffer_name = std::format(
					"{}({}).normals[{}]", mesh->GetName(), unique_id, normal_inputs.size()
				);
				if (in.empty()) {
					normal_inputs.emplace_back(nullptr);
				} else {
					normal_inputs.emplace_back(assets::geometry::input_buffer::create_simple(
						gpu::format::r32g32b32_float, 0,
						_asset_manager.create_buffer(
							assets::identifier(file, std::u8string(string::assume_utf8(normal_buffer_name))),
							std::span(in),
							gpu::buffer_usage_mask::copy_destination |
							gpu::buffer_usage_mask::vertex_buffer |
							gpu::buffer_usage_mask::shader_read,
							buf_pool
						)
					));
				}
			}
			for (const auto &in : mesh_uvs) {
				auto uv_buffer_name = std::format(
					"{}({}).uvs[{}]", mesh->GetName(), unique_id, uv_inputs.size()
				);
				if (in.empty()) {
					uv_inputs.emplace_back(nullptr);
				} else {
					uv_inputs.emplace_back(assets::geometry::input_buffer::create_simple(
						gpu::format::r32g32_float, 0,
						_asset_manager.create_buffer(
							assets::identifier(file, std::u8string(string::assume_utf8(uv_buffer_name))),
							std::span(in),
							gpu::buffer_usage_mask::copy_destination |
							gpu::buffer_usage_mask::vertex_buffer |
							gpu::buffer_usage_mask::shader_read,
							buf_pool
						)
					));
				}
			}
			for (const auto &in : mesh_indices) {
				auto index_buffer_name = std::format(
					"{}({}).indices[{}]", mesh->GetName(), unique_id, index_inputs.size()
				);
				if (in.empty()) {
					index_inputs.emplace_back(nullptr);
				} else {
					index_inputs.emplace_back(_asset_manager.create_buffer(
						assets::identifier(file, std::u8string(string::assume_utf8(index_buffer_name))),
						std::span(in),
						gpu::buffer_usage_mask::copy_destination |
						gpu::buffer_usage_mask::index_buffer     |
						gpu::buffer_usage_mask::shader_read      |
						gpu::buffer_usage_mask::acceleration_structure_build_input,
						buf_pool
					));
				}
			}

			auto [map_it, inserted] = geometries.try_emplace(mesh->GetUniqueID());
			crash_if(!inserted);
			if (can_use_indices) {
				assert(position_inputs.size() == 1 && normal_inputs.size() == 1 && uv_inputs.size() == 1);
				for (std::size_t i = 0; i < index_inputs.size(); ++i) {
					assets::geometry loaded_geom = nullptr;
					loaded_geom.num_vertices = static_cast<std::uint32_t>(mesh_positions[i].size());
					loaded_geom.num_indices  = static_cast<std::uint32_t>(mesh_indices[i].size());
					loaded_geom.topology     = gpu::primitive_topology::triangle_list;

					loaded_geom.vertex_buffer = position_inputs[0];
					loaded_geom.normal_buffer = normal_inputs[0];
					loaded_geom.uv_buffer     = uv_inputs[0];

					loaded_geom.index_format = gpu::index_format::uint32;
					loaded_geom.index_offset = 0;
					loaded_geom.index_buffer = std::move(index_inputs[i]);

					auto geom_name = std::format("{}({})[{}]", mesh->GetName(), unique_id, i);
					map_it->second.emplace_back(_asset_manager.register_geometry(
						assets::identifier(file, std::u8string(string::assume_utf8(geom_name))),
						std::move(loaded_geom)
					));
					if (geometry_loaded_callback) {
						geometry_loaded_callback(map_it->second.back());
					}
				}
			} else {
				assert(
					position_inputs.size() == normal_inputs.size() &&
					position_inputs.size() == uv_inputs.size() &&
					index_inputs.empty()
				);
				for (std::size_t i = 0; i < position_inputs.size(); ++i) {
					assets::geometry loaded_geom = nullptr;
					loaded_geom.num_vertices = static_cast<std::uint32_t>(mesh_positions[i].size());
					loaded_geom.num_indices  = 0;
					loaded_geom.topology     = gpu::primitive_topology::triangle_list;

					loaded_geom.vertex_buffer = std::move(position_inputs[i]);
					loaded_geom.normal_buffer = std::move(normal_inputs[i]);
					loaded_geom.uv_buffer     = std::move(uv_inputs[i]);

					loaded_geom.index_format = gpu::index_format::uint32;
					loaded_geom.index_offset = 0;
					loaded_geom.index_buffer = nullptr;

					auto geom_name = std::format("{}({})[{}]", mesh->GetName(), unique_id, i);
					map_it->second.emplace_back(_asset_manager.register_geometry(
						assets::identifier(file, std::u8string(string::assume_utf8(geom_name))),
						std::move(loaded_geom)
					));
					if (geometry_loaded_callback) {
						geometry_loaded_callback(map_it->second.back());
					}
				}
			}
		}

		// load materials
		constexpr bool _debug_dump_material_info = false;
		if constexpr (_debug_dump_material_info) {
			int mat_count = scene->GetMaterialCount();
			for (int i = 0; i < mat_count; ++i) {
				auto *mat = scene->GetMaterial(i);

				std::function<void(const fbxsdk::FbxProperty&, std::string)> dump_property =
					[&](const fbxsdk::FbxProperty &prop, std::string indent) {
						log().debug<u8"{}Property name: {}, type: {}">(
							indent, prop.GetNameAsCStr(), prop.GetPropertyDataType().GetName()
						);
						for (int j = 0; j < prop.GetSrcObjectCount(); ++j) {
							auto *obj = prop.GetSrcObject(j);
							auto id = obj->GetClassId();
							log().debug<u8"{}  Subobject: {}, type {}">(indent, obj->GetName(), id.GetName());
						}

						indent += "  ";
						fbxsdk::FbxProperty child = prop.GetChild();
						for (; child.IsValid(); child = child.GetSibling()) {
							dump_property(child, indent);
						}
					};

				log().debug<u8"Material {}: {}">(mat->GetUniqueID(), mat->GetName());
				fbxsdk::FbxProperty prop = mat->GetFirstProperty();
				for (; prop.IsValid(); prop = mat->GetNextProperty(prop)) {
					dump_property(prop, "  ");
				}
			}
		}

		auto find_texture = [&file_texture_classid](const fbxsdk::FbxProperty &prop) -> fbxsdk::FbxFileTexture* {
			int num_objects = prop.GetSrcObjectCount();
			for (int i = 0; i < num_objects; ++i) {
				auto *obj = prop.GetSrcObject(i);
				if (obj->IsRuntime(file_texture_classid)) {
					return static_cast<fbxsdk::FbxFileTexture*>(obj);
				}
			}
			return nullptr;
		};
		auto find_property_texture = [&find_texture, &images](
			fbxsdk::FbxSurfaceMaterial *mat, const char *name
		) -> assets::handle<assets::image2d> {
			if (auto prop = mat->FindProperty(name); prop.IsValid()) {
				if (auto *tex = find_texture(prop)) {
					if (auto tex_it = images.find(tex->GetUniqueID()); tex_it != images.end()) {
						return tex_it->second;
					} else {
						log().error<u8"Material {}({}) referencing unknown texture {}({})">(
							mat->GetName(), mat->GetUniqueID(), tex->GetName(), tex->GetUniqueID()
						);
					}
				}
			}
			return nullptr;
		};

		std::unordered_map<fbxsdk::FbxUInt64, assets::handle<assets::material>> materials;
		int mat_count = scene->GetMaterialCount();
		for (int i = 0; i < mat_count; ++i) {
			auto *mat = scene->GetMaterial(i);
			auto mat_data = std::make_unique<material_data>(_asset_manager);

			// TODO can't use the static names provided by the library - link errors
			mat_data->albedo_texture      = find_property_texture(mat, "DiffuseColor");
			mat_data->normal_texture      = find_property_texture(mat, "NormalMap");
			mat_data->properties_texture  = find_property_texture(mat, "ShininessExponent");
			mat_data->properties2_texture = find_property_texture(mat, "TransparentColor");
			if (auto diffuse = mat->FindProperty("DiffuseFactor"); diffuse.IsValid()) {
				// TODO
			}
			// TODO
			
			mat_data->properties.albedo_multiplier    = cvec4f(1.0f, 1.0f, 1.0f, 1.0f);
			mat_data->properties.normal_scale         = 1.0f;
			mat_data->properties.metalness_multiplier = 1.0f;
			mat_data->properties.roughness_multiplier = 1.0f;
			mat_data->properties.alpha_cutoff         = 0.5f;

			assets::material loaded_mat = nullptr;
			loaded_mat.data = std::move(mat_data);
			auto mat_name = std::format("{}({})", mat->GetName(), mat->GetUniqueID());
			auto handle = _asset_manager.register_material(
				assets::identifier(file, std::u8string(string::assume_utf8(mat_name))),
				std::move(loaded_mat)
			);
			auto [it, inserted] = materials.emplace(mat->GetUniqueID(), std::move(handle));
			assert(inserted);
			if (material_loaded_callback) {
				material_loaded_callback(it->second);
			}
		}

		// load instances
		if (instance_loaded_callback) {
			std::vector<std::pair<fbxsdk::FbxNode*, mat44f>> stack;
			if (auto *root = scene->GetRootNode()) {
				stack.emplace_back(root, mat44f::identity());
			}
			while (!stack.empty()) {
				auto [node, parent_trans] = stack.back();
				stack.pop_back();
				 
				fbxsdk::FbxAMatrix geom_matrix;
				geom_matrix.SetIdentity();
				if (node->GetNodeAttribute()) {
					geom_matrix.SetT(node->GetGeometricTranslation(fbxsdk::FbxNode::eSourcePivot));
					geom_matrix.SetR(node->GetGeometricRotation(fbxsdk::FbxNode::eSourcePivot));
					geom_matrix.SetS(node->GetGeometricScaling(fbxsdk::FbxNode::eSourcePivot));
				}
				auto matrix = node->EvaluateLocalTransform() * geom_matrix;
				mat44f local_trans = uninitialized;
				for (std::size_t y = 0; y < 4; ++y) {
					for (std::size_t x = 0; x < 4; ++x) {
						// transposed
						local_trans(y, x) = static_cast<float>(matrix[static_cast<int>(x)][static_cast<int>(y)]);
					}
				}
				mat44f node_trans = parent_trans * local_trans;

				if (auto *mesh = node->GetMesh()) {
					auto mesh_it = geometries.find(mesh->GetUniqueID());
					crash_if(mesh_it == geometries.end());

					auto material_count = std::min(
						node->GetMaterialCount(), static_cast<int>(mesh_it->second.size())
					);
					for (int i = 0; i < material_count; ++i) {
						auto mat_it = materials.find(node->GetMaterial(i)->GetUniqueID());
						crash_if(mat_it == materials.end());

						instance inst = nullptr;
						inst.geometry  = mesh_it->second[i];
						inst.material  = mat_it->second;
						inst.transform = node_trans;
						instance_loaded_callback(std::move(inst));
					}
				}

				int num_children = node->GetChildCount();
				for (int i = 0; i < num_children; ++i) {
					auto *child = node->GetChild(i);
					stack.emplace_back(child, node_trans);
				}
			}
		}
	}
}

#endif // LOTUS_RENDERER_HAS_FBX
