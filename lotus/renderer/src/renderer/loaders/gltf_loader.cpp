#include "lotus/renderer/loaders/gltf_loader.h"

/// \file
/// Implementation of GLTF loader.

#include <typeindex>

#include <tiny_gltf.h>

#include <lotus/math/quaternion.h>

namespace lotus::renderer::gltf {
	/// Loads a data buffer with the given properties.
	template <typename T> static assets::handle<assets::buffer> _load_data_buffer(
		assets::manager &man, const std::filesystem::path &path, const tinygltf::Model &model,
		int accessor_index, i32 expected_components, gpu::buffer_usage_mask usage_mask, const pool &p
	) {
		auto id = assets::identifier(path, std::u8string(string::assume_utf8(std::format(
			"buffer{}|{}|{}({})", accessor_index, expected_components, typeid(T).hash_code(), typeid(T).name()
		))));
		if (auto res = man.find_buffer(id)) {
			return res;
		}

		const auto &accessor = model.accessors[static_cast<usize>(accessor_index)];
		if (accessor.count == 0) {
			return nullptr;
		}
		const auto &buffer_view = model.bufferViews[static_cast<usize>(accessor.bufferView)];
		const auto stride = static_cast<usize>(accessor.ByteStride(buffer_view));
		const int component_type = accessor.componentType;
		const i32 num_components = tinygltf::GetNumComponentsInType(static_cast<u32>(accessor.type));
		const usize buffer_elements = accessor.count * static_cast<usize>(expected_components);

		if (num_components != expected_components) {
			log().warn("Expected {} components but getting {}", expected_components, num_components);
		}

		std::vector<T> data(buffer_elements);

		auto *data_raw = reinterpret_cast<const std::byte*>(
			model.buffers[static_cast<usize>(buffer_view.buffer)].data.data() +
				buffer_view.byteOffset + accessor.byteOffset
		);
		for (usize i = 0; i < accessor.count; ++i) {
			const std::byte *current_raw = data_raw + stride * i;
			T *target = data.data() + i * static_cast<usize>(expected_components);
			for (i32 j = 0; j < std::min(num_components, expected_components); ++j) {
				switch (component_type) {
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
					{
						auto *current = reinterpret_cast<const u8*>(current_raw);
						if constexpr (std::is_integral_v<T>) {
							target[j] = static_cast<T>(current[j]);
						} else if constexpr (std::is_floating_point_v<T>) {
							target[j] = static_cast<T>(current[j] / 255.0f);
						} else {
							static_assert(sizeof(T*) != sizeof(T*), "Invalid data type");
						}
					}
					break;
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
					{
						auto *current = reinterpret_cast<const u16*>(current_raw);
						if constexpr (std::is_integral_v<T>) {
							target[j] = static_cast<T>(current[j]);
						} else if constexpr (std::is_floating_point_v<T>) {
							target[j] = static_cast<T>(current[j] / 65535.0f);
						} else {
							static_assert(sizeof(T*) != sizeof(T*), "Invalid data type");
						}
					}
					break;
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
					{
						assert(std::is_integral_v<T>);
						auto *current = reinterpret_cast<const u32*>(current_raw);
						target[j] = static_cast<T>(current[j]);
					}
					break;
				case TINYGLTF_COMPONENT_TYPE_FLOAT:
					{
						assert(std::is_floating_point_v<T>);
						auto *current = reinterpret_cast<const f32*>(current_raw);
						target[j] = static_cast<T>(current[j]);
					}
					break;
				case TINYGLTF_COMPONENT_TYPE_DOUBLE:
					{
						assert(std::is_floating_point_v<T>);
						auto *current = reinterpret_cast<const f64*>(current_raw);
						target[j] = static_cast<T>(current[j]);
					}
					break;
				default:
					{
						log().error(
							"Unrecognized GLTF data buffer type for accessor {}: {}",
							accessor_index, accessor.componentType
						);
						return nullptr;
					}
				}
			}
		}

		return man.create_typed_buffer<T>(std::move(id), data, usage_mask, p);
	}
	/// Wrapper around \ref _load_data_buffer() that loads an \ref assets::geometry::input_buffer.
	template <typename T> static assets::geometry::input_buffer _load_input_buffer(
		assets::manager &man, const std::filesystem::path &path, const tinygltf::Model &model,
		int accessor_index, i32 expected_components, gpu::buffer_usage_mask usage_mask, const pool &p
	) {
		assets::geometry::input_buffer result = nullptr;
		result.data = _load_data_buffer<T>(man, path, model, accessor_index, expected_components, usage_mask, p);
		result.offset = 0;
		result.stride = static_cast<u32>(sizeof(T) * static_cast<usize>(expected_components));

		u8 count[4] = { 0, 0, 0, 0 };
		for (i32 i = 0; i < expected_components; ++i) {
			count[i] = sizeof(T) * 8;
		}
		auto ty = gpu::format_properties::data_type::unknown;
		if constexpr (std::is_same_v<T, f32>) {
			ty = gpu::format_properties::data_type::floating_point;
		} else if constexpr (std::is_same_v<T, u32>) {
			ty = gpu::format_properties::data_type::unsigned_int;
		} else {
			static_assert(sizeof(T*) == 0, "Unhandled data type");
		}
		result.format = gpu::format_properties::find_exact_rgba(count[0], count[1], count[2], count[3], ty);

		return result;
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
		tinygltf::TinyGLTF loader;
		tinygltf::Model model;

		loader.SetImageLoader(
			[](
				tinygltf::Image*, int, std::string*, std::string*,
				int, int, const unsigned char*, int, void*
			) -> bool {
				return true; // don't load image binary using tinygltf
			},
			nullptr
		);

		// TODO allocator
		std::string err;
		std::string warn;
		if (!loader.LoadASCIIFromFile(&model, &err, &warn, path.string())) {
			log().error("Failed to load GLTF ASCII {}, errors: {}, warnings: {}", path.string(), err, warn);
			return;
		}

		auto bookmark = get_scratch_bookmark();

		// load images
		auto images = bookmark.create_vector_array<assets::handle<assets::image2d>>(model.images.size(), nullptr);
		for (usize i = 0; i < images.size(); ++i) {
			constexpr bool _debug_disable_images = false;
			if constexpr (_debug_disable_images) {
				images[i] = _asset_manager.get_invalid_image();
			} else {
				images[i] = _asset_manager.get_image2d(
					assets::identifier(path.parent_path() / std::filesystem::path(model.images[i].uri)), tex_pool
				);
				if (image_loaded_callback) {
					image_loaded_callback(images[i]);
				}
			}
		}

		// load geometries
		auto geometries = bookmark.create_reserved_vector_array<
			memory::stack_allocator::vector_type<assets::handle<assets::geometry>>
		>(model.meshes.size());
		for (usize i = 0; i < model.meshes.size(); ++i) {
			const auto &mesh = model.meshes[i];
			auto &geom_primitives = geometries.emplace_back(
				bookmark.create_vector_array<assets::handle<assets::geometry>>(mesh.primitives.size(), nullptr)
			);
			for (usize j = 0; j < mesh.primitives.size(); ++j) {
				const auto &prim = mesh.primitives[j];

				assets::geometry geom = nullptr;
				if (auto it = prim.attributes.find("POSITION"); it != prim.attributes.end()) {
					geom.num_vertices  =
						static_cast<u32>(model.accessors[static_cast<usize>(it->second)].count);
					geom.vertex_buffer = _load_input_buffer<f32>(
						_asset_manager, path, model, it->second, 3,
						gpu::buffer_usage_mask::vertex_buffer |
						gpu::buffer_usage_mask::shader_read |
						gpu::buffer_usage_mask::acceleration_structure_build_input,
						buf_pool
					);
				}
				if (auto it = prim.attributes.find("NORMAL"); it != prim.attributes.end()) {
					geom.normal_buffer = _load_input_buffer<f32>(
						_asset_manager, path, model, it->second, 3,
						gpu::buffer_usage_mask::vertex_buffer | gpu::buffer_usage_mask::shader_read,
						buf_pool
					);
				}
				if (auto it = prim.attributes.find("TANGENT"); it != prim.attributes.end()) {
					geom.tangent_buffer = _load_input_buffer<f32>(
						_asset_manager, path, model, it->second, 3,
						gpu::buffer_usage_mask::vertex_buffer | gpu::buffer_usage_mask::shader_read,
						buf_pool
					);
				}
				if (auto it = prim.attributes.find("TEXCOORD_0"); it != prim.attributes.end()) {
					geom.uv_buffer = _load_input_buffer<f32>(
						_asset_manager, path, model, it->second, 2,
						gpu::buffer_usage_mask::vertex_buffer | gpu::buffer_usage_mask::shader_read,
						buf_pool
					);
				}
				if (prim.indices >= 0) {
					geom.index_buffer = _load_data_buffer<u32>(
						_asset_manager, path, model, prim.indices, 1,
						gpu::buffer_usage_mask::index_buffer |
						gpu::buffer_usage_mask::shader_read |
						gpu::buffer_usage_mask::acceleration_structure_build_input,
						buf_pool
					);
					geom.index_format = gpu::index_format::uint32;
					geom.index_offset = 0;
					geom.num_indices  =
						static_cast<u32>(model.accessors[static_cast<usize>(prim.indices)].count);
				}

				switch (prim.mode) {
				case TINYGLTF_MODE_POINTS:
					geom.topology = gpu::primitive_topology::point_list;
					break;
				case TINYGLTF_MODE_LINE:
					geom.topology = gpu::primitive_topology::line_list;
					break;
				case TINYGLTF_MODE_LINE_LOOP: // no support
					log().error(
						"Line loop topology is not supported, used by mesh {} \"{}\" primitive {}", i, mesh.name, j
					);
					[[fallthrough]];
				case TINYGLTF_MODE_LINE_STRIP:
					geom.topology = gpu::primitive_topology::line_strip;
					break;
				case TINYGLTF_MODE_TRIANGLES:
					geom.topology = gpu::primitive_topology::triangle_list;
					break;
				case TINYGLTF_MODE_TRIANGLE_FAN: // no support
					log().error(
						"Triangle fan topology is not supported, used by mesh {} \"{}\" primitive {}",
						i, mesh.name, j
					);
					[[fallthrough]];
				case TINYGLTF_MODE_TRIANGLE_STRIP:
					geom.topology = gpu::primitive_topology::triangle_strip;
					break;
				default:
					log().error("Unhandled topology: {}, falling back to points", prim.mode);
					geom.topology = gpu::primitive_topology::point_list;
					break;
				}

				std::string formatted = std::format("{}@{}|{}", mesh.name, i, j);
				geom_primitives[j] = _asset_manager.register_geometry(
					assets::identifier(path, std::u8string(string::assume_utf8(formatted))), std::move(geom)
				);
				if (geometry_loaded_callback) {
					geometry_loaded_callback(geom_primitives[j]);
				}
			}
		}

		// load materials
		auto materials = bookmark.create_vector_array<assets::handle<assets::material>>(
			model.materials.size(), nullptr
		);
		for (usize i = 0; i < materials.size(); ++i) {
			const auto &mat = model.materials[i];

			// TODO allocator
			auto mat_data = std::make_unique<material_data>(_asset_manager);
			mat_data->properties.albedo_multiplier =
				mat.pbrMetallicRoughness.baseColorFactor.empty() ?
				cvec4f32(1.0f, 1.0f, 1.0f, 1.0f) :
				cvec4f32(
					static_cast<f32>(mat.pbrMetallicRoughness.baseColorFactor[0]),
					static_cast<f32>(mat.pbrMetallicRoughness.baseColorFactor[1]),
					static_cast<f32>(mat.pbrMetallicRoughness.baseColorFactor[2]),
					static_cast<f32>(mat.pbrMetallicRoughness.baseColorFactor[3])
				);
			mat_data->properties.normal_scale         = static_cast<f32>(mat.normalTexture.scale);
			mat_data->properties.metalness_multiplier = static_cast<f32>(mat.pbrMetallicRoughness.metallicFactor);
			mat_data->properties.roughness_multiplier = static_cast<f32>(mat.pbrMetallicRoughness.roughnessFactor);
			mat_data->properties.alpha_cutoff         =
				mat.alphaMode == "MASK" ? static_cast<f32>(mat.alphaCutoff) : 0.0f;

			const auto get_tex = [&images, &model](
				int index, const assets::handle<assets::image2d> &default_img = nullptr
			) -> assets::handle<assets::image2d> {
				if (index < 0) {
					return default_img;
				}
				return images[static_cast<usize>(model.textures[static_cast<usize>(index)].source)];
			};
			mat_data->albedo_texture     = get_tex(mat.pbrMetallicRoughness.baseColorTexture.index);
			mat_data->normal_texture     =
				get_tex(mat.normalTexture.index, _asset_manager.get_default_normal_image());
			mat_data->properties_texture = get_tex(mat.pbrMetallicRoughness.metallicRoughnessTexture.index);

			if (mat.alphaMode != "BLEND") {
				materials[i] = _asset_manager.register_material(
					assets::identifier(
						path, std::u8string(string::assume_utf8(std::format("{}@{}", mat.name, i)))
					),
					assets::material(std::move(mat_data))
				);
				if (material_loaded_callback) {
					material_loaded_callback(materials[i]);
				}
			}
		}

		// load nodes
		if (instance_loaded_callback) {
			std::vector<std::pair<int, mat44f32>> stack;
			for (const auto &node : model.scenes[static_cast<usize>(model.defaultScene)].nodes) {
				stack.emplace_back(node, mat44f32::identity());
			}
			while (!stack.empty()) {
				const auto [node_id, transform] = stack.back();
				stack.pop_back();
				const auto &node = model.nodes[static_cast<usize>(node_id)];

				mat44f32 trans = uninitialized;
				if (node.matrix.empty()) {
					auto scale =
						node.scale.empty() ?
						mat33f32::identity() :
						mat33f64::diagonal(node.scale[0], node.scale[1], node.scale[2]).into<f32>();
					auto rotation =
						node.rotation.empty() ?
						mat33f32::identity() :
						quatu::normalize(quatf64::from_wxyz(
							node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]
						).into<f32>()).into_rotation_matrix();
					auto translation =
						node.translation.empty() ?
						cvec3f32(zero) :
						cvec3f64(node.translation[0], node.translation[1], node.translation[2]).into<f32>();
					trans = mat::concat_rows(
						mat::concat_columns(rotation * scale, translation),
						rvec4f32(0.0f, 0.0f, 0.0f, 1.0f)
					);
				} else {
					if (node.matrix.size() != 16) {
						log().error(
							"Transformation matrix for node {} has {} elements", node.name, node.matrix.size()
						);
					}
					for (usize row = 0; row < 4; ++row) {
						for (usize col = 0; col < 4; ++col) {
							usize index = row * 4 + col;
							if (index >= node.matrix.size()) {
								break;
							}
							trans(row, col) = static_cast<f32>(node.matrix[index]);
						}
					}
				}
				trans = transform * trans;

				if (node.mesh >= 0) {
					const auto mesh_idx = static_cast<usize>(node.mesh);
					const auto &geom_vec = geometries[mesh_idx];
					for (usize j = 0; j < geom_vec.size(); ++j) {
						const assets::handle<assets::geometry> &prim_handle = geom_vec[j];
						const tinygltf::Primitive &prim = model.meshes[mesh_idx].primitives[j];

						instance inst = nullptr;
						inst.material  = materials[static_cast<usize>(prim.material)];
						inst.geometry  = prim_handle;
						inst.transform = inst.prev_transform = trans;
						instance_loaded_callback(inst);
					}
				}

				if (light_loaded_callback) {
					auto light = node.extensions.find("KHR_lights_punctual");
					if (light != node.extensions.end() && light->second.IsObject()) {
						if (const auto &index_val = light->second.Get("light"); index_val.IsInt()) {
							const tinygltf::Light &light_def =
								model.lights[static_cast<usize>(index_val.GetNumberAsInt())];

							cvec3f64 light_color(1.0f, 1.0f, 1.0f);
							for (usize i = 0; i < std::min<usize>(light_def.color.size(), 3); ++i) {
								light_color[i] = light_def.color[i];
							}

							shader_types::light light_data;
							if (light_def.type == "directional") {
								light_data.type = shader_types::light_type::directional_light;
							} else if (light_def.type == "point") {
								light_data.type = shader_types::light_type::point_light;
							} else if (light_def.type == "spot") {
								light_data.type = shader_types::light_type::spot_light;
							}
							light_data.position   = trans.block<3, 1>(0, 3);
							light_data.direction  = vecu::normalize((trans * cvec4f32(0.0f, 0.0f, -1.0f, 0.0f)).block<3, 1>(0, 0));
							light_data.irradiance = (light_def.intensity * light_color).into<f32>();
							light_loaded_callback(light_data);
						}
					}
				}

				for (auto child : node.children) {
					stack.emplace_back(child, trans);
				}
			}
		}
	}
}
