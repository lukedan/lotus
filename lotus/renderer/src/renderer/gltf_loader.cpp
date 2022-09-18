#include "lotus/renderer/gltf_loader.h"

/// \file
/// Implementation of GLTF loader.

#include <typeindex>

#include <tiny_gltf.h>

#include <lotus/math/quaternion.h>
#include <lotus/renderer/mipmap.h>

namespace lotus::renderer::gltf {
	/// Loads a data buffer with the given properties.
	template <typename T> static assets::handle<assets::buffer> _load_data_buffer(
		assets::manager &man, const std::filesystem::path &path, const tinygltf::Model &model,
		int accessor_index, std::size_t expected_components, gpu::buffer_usage_mask usage_mask
	) {
		auto id = assets::identifier(path, std::u8string(string::assume_utf8(std::format(
			"buffer{}|{}|{}({})", accessor_index, expected_components, typeid(T).hash_code(), typeid(T).name()
		))));
		if (auto res = man.find_buffer(id)) {
			return res;
		}

		const auto &accessor = model.accessors[accessor_index];
		const auto &buffer_view = model.bufferViews[accessor.bufferView];
		int stride = accessor.ByteStride(buffer_view);
		int component_type = accessor.componentType;
		std::int32_t num_components = tinygltf::GetNumComponentsInType(accessor.type);
		std::size_t buffer_elements = accessor.count * expected_components;

		if (num_components != expected_components) {
			log().warn<u8"Expected {} components but getting {}">(expected_components, num_components);
		}

		std::vector<T> data(buffer_elements);

		auto *data_raw = reinterpret_cast<const std::byte*>(
			model.buffers[buffer_view.buffer].data.data() + buffer_view.byteOffset + accessor.byteOffset
		);
		for (int i = 0; i < accessor.count; ++i) {
			auto *current_raw = data_raw + stride * i;
			auto *target = data.data() + i * expected_components;
			for (int j = 0; j < std::min(static_cast<std::size_t>(num_components), expected_components); ++j) {
				switch (component_type) {
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
					{
						auto *current = reinterpret_cast<const std::uint8_t*>(current_raw);
						if constexpr (std::is_integral_v<T>) {
							target[j] = static_cast<T>(current[j]);
						} else if constexpr (std::is_floating_point_v<T>) {
							target[j] = static_cast<T>(current[j] / 255.0f);
						} else {
							assert(false); // invalid type
						}
					}
					break;
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
					{
						auto *current = reinterpret_cast<const std::uint16_t*>(current_raw);
						if constexpr (std::is_integral_v<T>) {
							target[j] = static_cast<T>(current[j]);
						} else if constexpr (std::is_floating_point_v<T>) {
							target[j] = static_cast<T>(current[j] / 65535.0f);
						} else {
							assert(false); // invalid type
						}
					}
					break;
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
					{
						assert(std::is_integral_v<T>);
						auto *current = reinterpret_cast<const std::uint32_t*>(current_raw);
						target[j] = static_cast<T>(current[j]);
					}
					break;
				case TINYGLTF_COMPONENT_TYPE_FLOAT:
					{
						assert(std::is_floating_point_v<T>);
						auto *current = reinterpret_cast<const float*>(current_raw);
						target[j] = static_cast<T>(current[j]);
					}
					break;
				case TINYGLTF_COMPONENT_TYPE_DOUBLE:
					{
						assert(std::is_floating_point_v<T>);
						auto *current = reinterpret_cast<const double*>(current_raw);
						target[j] = static_cast<T>(current[j]);
					}
					break;
				default:
					{
						log().error<u8"Unrecognized GLTF data buffer type for accessor {}: {}">(
							accessor_index, accessor.componentType
						);
						return nullptr;
					}
				}
			}
		}

		return man.create_buffer(std::move(id), std::span(data.begin(), data.end()), usage_mask);
	}
	/// Wrapper around \ref _load_data_buffer() that loads an \ref assets::geometry::input_buffer.
	template <typename T> static assets::geometry::input_buffer _load_input_buffer(
		assets::manager &man, const std::filesystem::path &path, const tinygltf::Model &model,
		int accessor_index, std::size_t expected_components, gpu::buffer_usage_mask usage_mask
	) {
		assets::geometry::input_buffer result = nullptr;
		result.data = _load_data_buffer<T>(man, path, model, accessor_index, expected_components, usage_mask);
		result.offset = 0;
		result.stride = static_cast<std::uint32_t>(sizeof(T) * expected_components);

		std::uint8_t count[4] = { 0, 0, 0, 0 };
		for (std::size_t i = 0; i < expected_components; ++i) {
			count[i] = sizeof(T) * 8;
		}
		auto ty = gpu::format_properties::data_type::unknown;
		if constexpr (std::is_same_v<T, float>) {
			ty = gpu::format_properties::data_type::floating_point;
		} else if constexpr (std::is_same_v<T, std::uint32_t>) {
			ty = gpu::format_properties::data_type::unsigned_int;
		} else {
			static_assert(sizeof(T*) == 0, "Unhandled data type");
		}
		result.format = gpu::format_properties::find_exact_rgba(count[0], count[1], count[2], count[3], ty);

		return result;
	}
	void context::load(
		const std::filesystem::path &path,
		static_function<void(assets::handle<assets::texture2d>)> image_loaded_callback,
		static_function<void(assets::handle<assets::geometry>)> geometry_loaded_callback,
		static_function<void(assets::handle<assets::material>)> material_loaded_callback,
		static_function<void(instance)> instance_loaded_callback
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
			log().error<u8"Failed to load GLTF ASCII {}, errors: {}, warnings: {}">(
				path.string(), err, warn
			);
			return;
		}

		auto bookmark = stack_allocator::for_this_thread().bookmark();

		// load images
		auto images = bookmark.create_vector_array<assets::handle<assets::texture2d>>(model.images.size(), nullptr);
		for (std::size_t i = 0; i < images.size(); ++i) {
			constexpr bool _debug_disable_images = false;
			if constexpr (_debug_disable_images) {
				images[i] = _asset_manager.get_invalid_texture();
			} else {
				images[i] = _asset_manager.get_texture2d(
					assets::identifier(path.parent_path() / std::filesystem::path(model.images[i].uri))
				);
				if (image_loaded_callback) {
					image_loaded_callback(images[i]);
				}
			}
		}

		// load geometries
		auto geometries = bookmark.create_reserved_vector_array<
			stack_allocator::vector_type<assets::handle<assets::geometry>>
		>(model.meshes.size());
		for (std::size_t i = 0; i < model.meshes.size(); ++i) {
			const auto &mesh = model.meshes[i];
			auto &geom_primitives = geometries.emplace_back(
				bookmark.create_vector_array<assets::handle<assets::geometry>>(mesh.primitives.size(), nullptr)
			);
			for (std::size_t j = 0; j < mesh.primitives.size(); ++j) {
				const auto &prim = mesh.primitives[j];

				assets::geometry geom = nullptr;
				if (auto it = prim.attributes.find("POSITION"); it != prim.attributes.end()) {
					geom.num_vertices = static_cast<std::uint32_t>(model.accessors[it->second].count);
					geom.vertex_buffer = _load_input_buffer<float>(
						_asset_manager, path, model, it->second, 3,
						gpu::buffer_usage_mask::vertex_buffer | gpu::buffer_usage_mask::shader_read_only | gpu::buffer_usage_mask::acceleration_structure_build_input
					);
				}
				if (auto it = prim.attributes.find("NORMAL"); it != prim.attributes.end()) {
					geom.normal_buffer = _load_input_buffer<float>(
						_asset_manager, path, model, it->second, 3,
						gpu::buffer_usage_mask::vertex_buffer | gpu::buffer_usage_mask::shader_read_only
					);
				}
				if (auto it = prim.attributes.find("TANGENT"); it != prim.attributes.end()) {
					geom.tangent_buffer = _load_input_buffer<float>(
						_asset_manager, path, model, it->second, 3,
						gpu::buffer_usage_mask::vertex_buffer | gpu::buffer_usage_mask::shader_read_only
					);
				}
				if (auto it = prim.attributes.find("TEXCOORD_0"); it != prim.attributes.end()) {
					geom.uv_buffer = _load_input_buffer<float>(
						_asset_manager, path, model, it->second, 2,
						gpu::buffer_usage_mask::vertex_buffer | gpu::buffer_usage_mask::shader_read_only
					);
				}
				if (prim.indices >= 0) {
					geom.index_buffer = _load_data_buffer<std::uint32_t>(
						_asset_manager, path, model, prim.indices, 1,
						gpu::buffer_usage_mask::index_buffer | gpu::buffer_usage_mask::shader_read_only | gpu::buffer_usage_mask::acceleration_structure_build_input
					);
					geom.index_format = gpu::index_format::uint32;
					geom.index_offset = 0;
					geom.num_indices = static_cast<std::uint32_t>(model.accessors[prim.indices].count);
				}

				switch (prim.mode) {
				case TINYGLTF_MODE_POINTS:
					geom.topology = gpu::primitive_topology::point_list;
					break;
				case TINYGLTF_MODE_LINE:
					geom.topology = gpu::primitive_topology::line_list;
					break;
				case TINYGLTF_MODE_LINE_LOOP: // no support
					log().error<
						u8"Line loop topology is not supported, used by mesh {} \"{}\" primitive {}"
					>(i, mesh.name, j);
					[[fallthrough]];
				case TINYGLTF_MODE_LINE_STRIP:
					geom.topology = gpu::primitive_topology::line_strip;
					break;
				case TINYGLTF_MODE_TRIANGLES:
					geom.topology = gpu::primitive_topology::triangle_list;
					break;
				case TINYGLTF_MODE_TRIANGLE_FAN: // no support
					log().error<
						u8"Triangle fan topology is not supported, used by mesh {} \"{}\" primitive {}"
					>(i, mesh.name, j);
					[[fallthrough]];
				case TINYGLTF_MODE_TRIANGLE_STRIP:
					geom.topology = gpu::primitive_topology::triangle_strip;
					break;
				default:
					assert(false); // unhandled
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
		for (std::size_t i = 0; i < materials.size(); ++i) {
			const auto &mat = model.materials[i];

			// TODO allocator
			auto mat_data = std::make_unique<material_data>(_asset_manager);
			mat_data->properties.albedo_multiplier =
				mat.pbrMetallicRoughness.baseColorFactor.empty() ?
				cvec4f(1.0f, 1.0f, 1.0f, 1.0f) :
				cvec4f(
					static_cast<float>(mat.pbrMetallicRoughness.baseColorFactor[0]),
					static_cast<float>(mat.pbrMetallicRoughness.baseColorFactor[1]),
					static_cast<float>(mat.pbrMetallicRoughness.baseColorFactor[2]),
					static_cast<float>(mat.pbrMetallicRoughness.baseColorFactor[3])
				);
			mat_data->properties.normal_scale         = static_cast<float>(mat.normalTexture.scale);
			mat_data->properties.metalness_multiplier = static_cast<float>(mat.pbrMetallicRoughness.metallicFactor);
			mat_data->properties.roughness_multiplier = static_cast<float>(mat.pbrMetallicRoughness.roughnessFactor);
			mat_data->properties.alpha_cutoff         = static_cast<float>(mat.alphaCutoff);

			mat_data->albedo_texture =
				mat.pbrMetallicRoughness.baseColorTexture.index < 0 ?
				nullptr :
				images[model.textures[mat.pbrMetallicRoughness.baseColorTexture.index].source];
			mat_data->normal_texture =
				mat.normalTexture.index < 0 ?
				nullptr :
				images[model.textures[mat.normalTexture.index].source];
			mat_data->properties_texture =
				mat.pbrMetallicRoughness.metallicRoughnessTexture.index < 0 ?
				nullptr :
				images[model.textures[mat.pbrMetallicRoughness.metallicRoughnessTexture.index].source];

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

		// load nodes
		if (instance_loaded_callback) {
			std::vector<std::pair<int, mat44f>> stack;
			for (const auto &node : model.scenes[model.defaultScene].nodes) {
				stack.emplace_back(node, mat44f::identity());
			}
			while (!stack.empty()) {
				auto [node_id, transform] = stack.back();
				stack.pop_back();
				const auto &node = model.nodes[node_id];

				mat44f trans = uninitialized;
				if (node.matrix.empty()) {
					auto scale =
						node.scale.empty() ?
						mat33f::identity() :
						mat33d::diagonal(node.scale[0], node.scale[1], node.scale[2]).into<float>();
					auto rotation =
						node.rotation.empty() ?
						mat33f::identity() :
						quat::unsafe_normalize(quatd::from_wxyz(
							node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]
						).into<float>()).into_matrix();
					auto translation =
						node.translation.empty() ?
						cvec3f(zero) :
						cvec3d(node.translation[0], node.translation[1], node.translation[2]).into<float>();
					trans = matf::concat_rows(
						matf::concat_columns(rotation * scale, translation),
						rvec4f(0.0f, 0.0f, 0.0f, 1.0f)
					);
				} else {
					assert(node.matrix.size() == 16);
					for (std::size_t row = 0; row < 4; ++row) {
						for (std::size_t col = 0; col < 4; ++col) {
							trans(row, col) = static_cast<float>(node.matrix[row * 4 + col]);
						}
					}
				}
				trans = transform * trans;

				if (node.mesh >= 0) {
					for (std::size_t j = 0; j < geometries[node.mesh].size(); ++j) {
						const auto &prim_handle = geometries[node.mesh][j];
						const auto &prim = model.meshes[node.mesh].primitives[j];

						instance inst = nullptr;
						inst.transform = trans;
						inst.material = materials[prim.material];
						inst.geometry = prim_handle;
						instance_loaded_callback(inst);
					}
				}

				for (auto child : node.children) {
					stack.emplace_back(child, trans);
				}
			}
		}
	}


	all_resource_bindings material_data::create_resource_bindings() const {
		all_resource_bindings result = nullptr;

		result.sets.emplace_back(manager->get_images(), 0);

		resource_set_binding::descriptor_bindings set1({});

		shader_types::gltf_material mat;
		mat.properties = properties;
		mat.assets.albedo_texture     = albedo_texture     ? albedo_texture->descriptor_index     : manager->get_invalid_texture()->descriptor_index;
		mat.assets.normal_texture     = normal_texture     ? normal_texture->descriptor_index     : manager->get_invalid_texture()->descriptor_index;
		mat.assets.properties_texture = properties_texture ? properties_texture->descriptor_index : manager->get_invalid_texture()->descriptor_index;
		set1.bindings.emplace_back(descriptor_resource::immediate_constant_buffer::create_for(mat), 0);

		set1.bindings.emplace_back(descriptor_resource::sampler(), 3);

		result.sets.emplace_back(set1, 1);

		return result;
	}
}
