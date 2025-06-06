#include "lotus/renderer/loaders/assimp_loader.h"

/// \file
/// Implementation of the assimp loader.

#ifdef LOTUS_RENDERER_HAS_ASSIMP

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace lotus::renderer::assimp {
	/// Loads an input buffer.
	template <
		typename Desired, usize NumChannels, typename Real, typename Cb
	> [[nodiscard]] static assets::geometry::input_buffer _load_input_buffer(
		assets::manager &man,
		assets::identifier id,
		std::span<const aiVector3t<Real>> data,
		gpu::buffer_usage_mask usages,
		const pool &p,
		Cb &&convert
	) {
		static_assert(std::is_floating_point_v<Desired>, "Data type is not a floating-point type");
		static_assert(NumChannels >= 2 && NumChannels <= 4, "Incorrect number of channels");
		using _vec_t = column_vector<NumChannels, Desired>;

		assets::handle<assets::buffer> buf = nullptr;
		if constexpr (
			std::is_same_v<Desired, Real> &&
			NumChannels == 3 &&
			std::is_same_v<std::decay_t<Cb>, std::nullptr_t>
		) {
			auto *d = reinterpret_cast<const std::byte*>(data.data());
			buf = man.create_buffer(std::move(id), { d, d + data.size_bytes() }, usages, p);
		} else {
			std::vector<_vec_t> in_data;
			for (const auto &v : data) {
				in_data.emplace_back(convert(v));
			}
			auto *d = reinterpret_cast<const std::byte*>(in_data.data());
			buf = man.create_buffer(std::move(id), { d, d + sizeof(_vec_t) * in_data.size() }, usages, p);
		}
		u8 bits = sizeof(Desired) * 8;
		u8 bits_per_channel[4] = { 0, 0, 0, 0 };
		for (usize i = 0; i < NumChannels; ++i) {
			bits_per_channel[i] = bits;
		}
		auto fmt = gpu::format_properties::find_exact_rgba(
			bits_per_channel[0], bits_per_channel[1], bits_per_channel[2], bits_per_channel[3],
			gpu::format_properties::data_type::floating_point
		);
		return assets::geometry::input_buffer::create_simple(fmt, 0, std::move(buf));
	}
	/// Retrieves a material property.
	template <typename Ret> [[nodiscard]] static std::optional<Ret> _get_material_property(
		const aiMaterial *mat, const char *name, unsigned type, unsigned index
	) {
		Ret result;
		if (mat->Get(name, type, index, result) == aiReturn_SUCCESS) {
			return result;
		}
		return std::nullopt;
	}
	void context::load(
		const std::filesystem::path &path,
		static_function<void(assets::handle<assets::image2d>)> image_loaded_callback,
		static_function<void(assets::handle<assets::geometry>)> geometry_loaded_callback,
		static_function<void(assets::handle<assets::material>)> material_loaded_callback,
		static_function<void(instance)> instance_loaded_callback,
		static_function<void(shader_types::light)> light_loaded_callback,
		const pool &buf_pool, const pool &tex_pool
	) {
		Assimp::Importer importer;
		const aiScene *scene = importer.ReadFile(
			path.string(),
			aiProcess_GenSmoothNormals |
			aiProcess_CalcTangentSpace |
			aiProcess_Triangulate |
			aiProcess_JoinIdenticalVertices
		);
		if (!scene) {
			log().error("Failed to load {} using assimp: {}", path.string(), importer.GetErrorString());
			return;
		}
		if (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) {
			log().error("Scene is incomplete: {}", path.string());
			return;
		}

		// load images
		std::vector<assets::handle<assets::image2d>> images;
		for (unsigned i_tex = 0; i_tex < scene->mNumTextures; ++i_tex) {
			const aiTexture *tex = scene->mTextures[i_tex];
			const auto &img_handle = images.emplace_back(
				_asset_manager.get_image2d(assets::identifier(tex->mFilename.C_Str()), tex_pool)
			);
			if (image_loaded_callback) {
				image_loaded_callback(img_handle);
			}
		}

		// load geometries
		std::vector<assets::handle<assets::geometry>> geometries;
		for (unsigned i_geom = 0; i_geom < scene->mNumMeshes; ++i_geom) {
			const aiMesh *mesh = scene->mMeshes[i_geom];

			assets::geometry geom = nullptr;
			geom.topology = gpu::primitive_topology::triangle_list;

			geom.num_vertices = static_cast<u32>(mesh->mNumVertices);
			geom.vertex_buffer = _load_input_buffer<float, 3>(
				_asset_manager,
				assets::identifier(path, std::u8string(string::assume_utf8(std::format(
					"{}({}).vertices", mesh->mName.C_Str(), i_geom
				)))),
				std::span<const aiVector3D>(mesh->mVertices, mesh->mNumVertices),
				gpu::buffer_usage_mask::vertex_buffer |
				gpu::buffer_usage_mask::shader_read |
				gpu::buffer_usage_mask::acceleration_structure_build_input,
				buf_pool,
				nullptr
			);
			if (mesh->HasNormals()) {
				geom.normal_buffer = _load_input_buffer<float, 3>(
					_asset_manager,
					assets::identifier(path, std::u8string(string::assume_utf8(std::format(
						"{}({}).normals", mesh->mName.C_Str(), i_geom
					)))),
					std::span<const aiVector3D>(mesh->mNormals, mesh->mNumVertices),
					gpu::buffer_usage_mask::vertex_buffer |
					gpu::buffer_usage_mask::shader_read,
					buf_pool,
					nullptr
				);
			}
			if (mesh->HasTangentsAndBitangents()) {
				geom.tangent_buffer = _load_input_buffer<float, 4>(
					_asset_manager,
					assets::identifier(path, std::u8string(string::assume_utf8(std::format(
						"{}({}).tangents", mesh->mName.C_Str(), i_geom
					)))),
					std::span<const aiVector3D>(mesh->mTangents, mesh->mNumVertices),
					gpu::buffer_usage_mask::vertex_buffer |
					gpu::buffer_usage_mask::shader_read,
					buf_pool,
					[](aiVector3D v) {
						return cvec4f(v.x, v.y, v.z, 1.0f);
					}
				);
			}
			if (mesh->HasTextureCoords(0)) {
				geom.uv_buffer = _load_input_buffer<float, 2>(
					_asset_manager,
					assets::identifier(path, std::u8string(string::assume_utf8(std::format(
						"{}({}).uvs", mesh->mName.C_Str(), i_geom
					)))),
					std::span<const aiVector3D>(mesh->mTextureCoords[0], mesh->mNumVertices),
					gpu::buffer_usage_mask::vertex_buffer |
					gpu::buffer_usage_mask::shader_read,
					buf_pool,
					[](aiVector3D v) {
						return cvec2f(v.x, 1.0f - v.y);
					}
				);
			}

			// load indices
			std::vector<u32> indices;
			for (unsigned i_face = 0; i_face < mesh->mNumFaces; ++i_face) {
				const aiFace &face = mesh->mFaces[i_face];
				for (unsigned i_pt = 2; i_pt < face.mNumIndices; ++i_pt) {
					indices.emplace_back(face.mIndices[0]);
					indices.emplace_back(face.mIndices[i_pt - 1]);
					indices.emplace_back(face.mIndices[i_pt]);
				}
			}
			const auto *indices_data = reinterpret_cast<const std::byte*>(indices.data());
			geom.num_indices = static_cast<u32>(indices.size());
			geom.index_buffer = _asset_manager.create_buffer(
				assets::identifier(path, std::u8string(string::assume_utf8(std::format(
					"{}({}).indices", mesh->mName.C_Str(), i_geom
				)))),
				{ indices_data, indices_data + sizeof(u32) * indices.size() },
				gpu::buffer_usage_mask::index_buffer |
				gpu::buffer_usage_mask::shader_read |
				gpu::buffer_usage_mask::acceleration_structure_build_input,
				buf_pool
			);
			geom.index_format = gpu::index_format::uint32;
			geom.index_offset = 0;

			const auto &geom_handle = geometries.emplace_back(_asset_manager.register_geometry(
				assets::identifier(path, std::u8string(string::assume_utf8(std::format(
					"{}({})", mesh->mName.C_Str(), i_geom
				)))),
				std::move(geom)
			));
			if (geometry_loaded_callback) {
				geometry_loaded_callback(geom_handle);
			}
		}

		// load materials
		std::vector<assets::handle<assets::material>> materials;
		for (unsigned i_mat = 0; i_mat < scene->mNumMaterials; ++i_mat) {
			const aiMaterial *mat = scene->mMaterials[i_mat];
			auto data = std::make_unique<material_data>(_asset_manager);

			// textures
			aiString tex_path;
			auto current_texture = [&]() {
				std::string norm_path = tex_path.C_Str();
				// assume portable path names
				for (char &c : norm_path) {
					if (c == '\\') {
						c = '/';
					}
				}
				return _asset_manager.get_image2d(
					assets::identifier(path.parent_path() / norm_path), tex_pool
				);
			};
			if (mat->GetTexture(aiTextureType_BASE_COLOR, 0, &tex_path) == aiReturn_SUCCESS) {
				data->albedo_texture = current_texture();
			} else if (mat->GetTexture(aiTextureType_DIFFUSE, 0, &tex_path) == aiReturn_SUCCESS) {
				data->albedo_texture = current_texture();
			}
			if (mat->GetTexture(aiTextureType_NORMALS, 0, &tex_path) == aiReturn_SUCCESS) {
				data->normal_texture = current_texture();
			} else if (mat->GetTexture(aiTextureType_NORMAL_CAMERA, 0, &tex_path) == aiReturn_SUCCESS) {
				data->normal_texture = current_texture();
			} else {
				data->normal_texture = _asset_manager.get_default_normal_image();
			}
			if (mat->GetTexture(aiTextureType_SHININESS, 0, &tex_path) == aiReturn_SUCCESS) {
				data->properties_texture = current_texture();
			} else if (mat->GetTexture(aiTextureType_SPECULAR, 0, &tex_path) == aiReturn_SUCCESS) {
				data->properties_texture = current_texture();
			} else {
				if (mat->GetTexture(aiTextureType_METALNESS, 0, &tex_path) == aiReturn_SUCCESS) {
					data->properties_texture = current_texture();
				}
				if (mat->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &tex_path) == aiReturn_SUCCESS) {
					data->properties2_texture = current_texture();
				}
			}

			// properties
			data->properties.albedo_multiplier    = cvec4f(1.0f, 1.0f, 1.0f, 1.0f);
			data->properties.normal_scale         = 1.0f;
			data->properties.metalness_multiplier = 1.0f;
			data->properties.roughness_multiplier = 1.0f;
			data->properties.alpha_cutoff         = 0.5f;
			if (auto color = _get_material_property<aiColor4D>(mat, AI_MATKEY_BASE_COLOR)) {
				data->properties.albedo_multiplier =
					cvec4<ai_real>(color->r, color->g, color->b, color->a).into<float>();
			} else if (auto dif_color = _get_material_property<aiColor3D>(mat, AI_MATKEY_COLOR_DIFFUSE)) {
				data->properties.albedo_multiplier =
					cvec4<ai_real>(dif_color->r, dif_color->g, dif_color->b, 1.0f).into<float>();
			}
			if (auto factor = _get_material_property<ai_real>(mat, AI_MATKEY_METALLIC_FACTOR)) {
				data->properties.metalness_multiplier = static_cast<float>(factor.value());
			}
			if (auto factor = _get_material_property<ai_real>(mat, AI_MATKEY_ROUGHNESS_FACTOR)) {
				data->properties.roughness_multiplier = static_cast<float>(factor.value());
			}

			const auto &mat_handle = materials.emplace_back(_asset_manager.register_material(
				assets::identifier(path, std::u8string(string::assume_utf8(std::format(
					"{}({})", mat->GetName().C_Str(), i_mat
				)))),
				assets::material(std::move(data))
			));
			if (material_loaded_callback) {
				material_loaded_callback(mat_handle);
			}
		}

		// load lights
		std::vector<shader_types::light> lights;
		std::unordered_map<std::string, usize> lights_mapping;
		if (light_loaded_callback) {
			for (unsigned i = 0; i < scene->mNumLights; ++i) {
				const aiLight *light = scene->mLights[i];
				auto [it, inserted] = lights_mapping.emplace(light->mName.C_Str(), lights.size());
				if (!inserted) {
					log().error(
						"Light with duplicate name: {}, previous was at position {}",
						light->mName.C_Str(), it->second
					);
					continue;
				}
				auto &loaded_light = lights.emplace_back();
				switch (light->mType) {
				case aiLightSource_DIRECTIONAL:
					loaded_light.type = shader_types::light_type::directional_light;
					break;
				case aiLightSource_POINT:
					loaded_light.type = shader_types::light_type::point_light;
					break;
				case aiLightSource_SPOT:
					loaded_light.type = shader_types::light_type::spot_light;
					break;
				case aiLightSource_AMBIENT:
					log().error("Ambient light ignored: {}", light->mName.C_Str());
					break;
				case aiLightSource_AREA:
					loaded_light.type = shader_types::light_type::point_light;
					log().warn("Area light treated as a point light: {}", light->mName.C_Str());
					break;
				case aiLightSource_UNDEFINED:
					[[fallthrough]];
				case _aiLightSource_Force32Bit:
					log().error("Undefined light type: {}", light->mName.C_Str());
					break;
				}
				loaded_light.position   = cvec3f(light->mPosition.x, light->mPosition.y, light->mPosition.z);
				loaded_light.direction  = cvec3f(light->mDirection.x, light->mDirection.y, light->mDirection.z);
				// mColorSpecular is ignored
				loaded_light.irradiance =
					cvec3f(light->mColorDiffuse.r, light->mColorDiffuse.g, light->mColorDiffuse.b);
			}
		}

		{ // load instances
			std::vector<std::pair<const aiNode*, mat44f>> stack;
			stack.emplace_back(scene->mRootNode, mat44f::identity());
			while (!stack.empty()) {
				auto [node, parent_trans] = stack.back();
				stack.pop_back();

				aiMatrix4x4 m = node->mTransformation;
				mat44f local_trans{
					{ m.a1, m.a2, m.a3, m.a4 },
					{ m.b1, m.b2, m.b3, m.b4 },
					{ m.c1, m.c2, m.c3, m.c4 },
					{ m.d1, m.d2, m.d3, m.d4 },
				};
				mat44f trans = parent_trans * local_trans;

				for (unsigned i_mesh = 0; i_mesh < node->mNumMeshes; ++i_mesh) {
					instance inst = nullptr;
					unsigned mesh_id = node->mMeshes[i_mesh];
					inst.geometry  = geometries[mesh_id];
					inst.material  = materials[scene->mMeshes[mesh_id]->mMaterialIndex];
					inst.transform = inst.prev_transform = trans;
					instance_loaded_callback(std::move(inst));
				}

				// load lights
				if (light_loaded_callback) {
					auto it = lights_mapping.find(node->mName.C_Str());
					if (it != lights_mapping.end()) {
						auto &light = lights[it->second];
						light.position = (trans * cvec4f(cvec3f(light.position), 1.0f)).block<3, 1>(0, 0);
						light.direction = vecu::normalize((trans * cvec4f(cvec3f(light.direction), 0.0f)).block<3, 1>(0, 0));
						lights_mapping.erase(it);
					}
				}

				for (unsigned i_child = 0; i_child < node->mNumChildren; ++i_child) {
					stack.emplace_back(node->mChildren[i_child], trans);
				}
			}
		}

		if (light_loaded_callback) {
			if (!lights_mapping.empty()) {
				for (auto &&[k, v] : lights_mapping) {
					log().error("Light without a corresponding node: {}", k.c_str());
				}
			}
			for (const auto &l : lights) {
				light_loaded_callback(l);
			}
		}
	}
}

#endif
