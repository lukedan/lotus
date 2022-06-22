#include "lotus/renderer/gltf_loader.h"

/// \file
/// Implementation of GLTF loader.

#include <typeindex>

#include <tiny_gltf.h>

namespace lotus::renderer::gltf {
	template <typename T> static assets::handle<assets::buffer> _load_data_buffer(
		assets::manager &man, const std::filesystem::path &path, const tinygltf::Model &model,
		int accessor_index, std::size_t expected_components, graphics::buffer_usage::mask usage_mask
	) {
		auto id = assets::identifier::create(path, std::u8string(string::assume_utf8(std::format(
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
		std::size_t buffer_elements = accessor.count * num_components;

		if (num_components != expected_components) {
			log().warn<u8"Expected {} components but getting {}">(expected_components, num_components);
			return nullptr;
		}

		std::vector<T> data(buffer_elements);

		auto *data_raw = reinterpret_cast<const std::byte*>(
			model.buffers[buffer_view.buffer].data.data() + buffer_view.byteOffset
		);
		for (int i = 0; i < accessor.count; ++i) {
			auto *current_raw = data_raw + stride * i;
			auto *target = data.data() + i * num_components;
			for (int j = 0; j < num_components; ++j) {
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
				case TINYGLTF_COMPONENT_TYPE_FLOAT:
					{
						assert(std::is_floating_point_v<T>);
						auto *current = reinterpret_cast<const float*>(current_raw);
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
	std::vector<instance> context::load(const std::filesystem::path &path) {
		tinygltf::TinyGLTF loader;
		tinygltf::Model model;

		auto &dev = _asset_manager.get_device();
		auto &cmd_alloc = _asset_manager.get_command_allocator();
		auto &cmd_queue = _asset_manager.get_command_queue();
		auto &fence = _asset_manager.get_fence();

		loader.SetImageLoader(
			[](
				tinygltf::Image*, int, std::string*, std::string*,
				int, int, const unsigned char*, int, void*
			) -> bool {
				return true; // defer image loading
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
			return {};
		}

		auto bookmark = stack_allocator::for_this_thread().bookmark();

		// load images
		auto images = bookmark.create_vector_array<assets::handle<assets::texture2d>>(model.images.size(), nullptr);
		for (std::size_t i = 0; i < images.size(); ++i) {
			images[i] = _asset_manager.get_texture2d(
				assets::identifier::create(path.parent_path() / std::filesystem::path(model.images[i].uri))
			);
		}

		// load geometries
		auto geometries = bookmark.create_reserved_vector_array<
			stack_allocator::vector_type<assets::handle<assets::geometry>>
		>(model.meshes.size());
		for (std::size_t i = 0; i < model.meshes.size(); ++i) {
			const auto &mesh = model.meshes[i];
			auto &geom_primitives = geometries.emplace_back(
				bookmark.create_vector_array<assets::handle<assets::geometry>>(mesh.primitives.size(), nullptr
			));
			for (std::size_t j = 0; j < mesh.primitives.size(); ++j) {
				const auto &prim = mesh.primitives[j];

				assets::geometry geom = nullptr;
				if (auto it = prim.attributes.find("POSITION"); it != prim.attributes.end()) {
					geom.num_vertices = static_cast<std::uint32_t>(model.accessors[it->second].count);
					geom.vertex_buffer = _load_data_buffer<float>(
						_asset_manager, path, model, it->second, 3,
						graphics::buffer_usage::mask::vertex_buffer | graphics::buffer_usage::mask::read_only_buffer
					).take_ownership();
				}
				if (auto it = prim.attributes.find("NORMAL"); it != prim.attributes.end()) {
					geom.normal_buffer = _load_data_buffer<float>(
						_asset_manager, path, model, it->second, 3,
						graphics::buffer_usage::mask::vertex_buffer | graphics::buffer_usage::mask::read_only_buffer
					).take_ownership();
				}
				if (auto it = prim.attributes.find("TANGENT"); it != prim.attributes.end()) {
					geom.tangent_buffer = _load_data_buffer<float>(
						_asset_manager, path, model, it->second, 4,
						graphics::buffer_usage::mask::vertex_buffer | graphics::buffer_usage::mask::read_only_buffer
					).take_ownership();
				}
				if (auto it = prim.attributes.find("TEXCOORD_0"); it != prim.attributes.end()) {
					geom.uv_buffer = _load_data_buffer<float>(
						_asset_manager, path, model, it->second, 2,
						graphics::buffer_usage::mask::vertex_buffer | graphics::buffer_usage::mask::read_only_buffer
					).take_ownership();
				}
				if (prim.indices >= 0) {
					geom.num_indices = static_cast<std::uint32_t>(model.accessors[prim.indices].count);
					geom.index_buffer = _load_data_buffer<std::uint32_t>(
						_asset_manager, path, model, prim.indices, 1,
						graphics::buffer_usage::mask::index_buffer | graphics::buffer_usage::mask::read_only_buffer
					).take_ownership();
				}

				{
					auto acc_geom = dev.create_bottom_level_acceleration_structure_geometry({ {
						graphics::vertex_buffer_view(
							geom.vertex_buffer.get().value.data, graphics::format::r32g32b32_float,
							0, sizeof(cvec3f), geom.num_vertices
						),
						graphics::index_buffer_view(
							geom.index_buffer.get().value.data, graphics::index_format::uint32, 0, geom.num_indices
						)
					} });
					auto build_sizes = dev.get_bottom_level_acceleration_structure_build_sizes(acc_geom);

					// allocate bvh
					geom.acceleration_structure_buffer = dev.create_committed_buffer(
						build_sizes.acceleration_structure_size,
						graphics::heap_type::device_only, graphics::buffer_usage::mask::acceleration_structure
					);
					geom.acceleration_structure = dev.create_bottom_level_acceleration_structure(
						geom.acceleration_structure_buffer, 0, build_sizes.acceleration_structure_size
					);
					
					// build bvh
					auto cmd_list = dev.create_and_start_command_list(cmd_alloc);
					auto scratch = dev.create_committed_buffer(
						build_sizes.build_scratch_size,
						graphics::heap_type::device_only, graphics::buffer_usage::mask::acceleration_structure
					);
					cmd_list.build_acceleration_structure(acc_geom, geom.acceleration_structure, scratch, 0);
					cmd_list.finish();
					cmd_queue.submit_command_lists({ &cmd_list }, &fence);
					dev.wait_for_fence(fence);
					dev.reset_fence(fence);
				}

				std::string formatted = std::format("{}@{}|{}", mesh.name, i, j);
				geom_primitives[j] = _asset_manager.register_geometry(
					assets::identifier::create(path, std::u8string(string::assume_utf8(formatted))), std::move(geom)
				);
			}
		}

		// load materials
		auto materials = bookmark.create_vector_array<assets::handle<assets::material>>(
			model.materials.size(), nullptr
		);
		for (std::size_t i = 0; i < materials.size(); ++i) {
			const auto &mat = model.materials[i];

			// TODO allocator
			auto mat_data = std::make_unique<material_data>(nullptr);
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
				images[mat.pbrMetallicRoughness.baseColorTexture.index].take_ownership();
			mat_data->normal_texture =
				mat.normalTexture.index < 0 ?
				nullptr :
				images[mat.normalTexture.index].take_ownership();
			mat_data->properties_texture =
				mat.pbrMetallicRoughness.metallicRoughnessTexture.index < 0 ?
				nullptr :
				images[mat.pbrMetallicRoughness.metallicRoughnessTexture.index].take_ownership();

			materials[i] = _asset_manager.register_material(
				assets::identifier::create(
					path, std::u8string(string::assume_utf8(std::format("{}@{}", mat.name, i)))
				),
				assets::material::create(std::move(mat_data))
			);
		}

		// load nodes
		std::vector<instance> result;
		for (std::size_t i = 0; i < model.nodes.size(); ++i) {
			const auto &node = model.nodes[i];

			mat44f trans = uninitialized;
			if (node.matrix.empty()) {
				trans = mat44f::identity();
			} else {
				assert(node.matrix.size() == 16);
				for (std::size_t row = 0; row < 4; ++row) {
					for (std::size_t col = 0; col < 4; ++col) {
						trans(row, col) = static_cast<float>(node.matrix[row * 4 + col]);
					}
				}
			}

			for (std::size_t j = 0; j < geometries[node.mesh].size(); ++j) {
				const auto &prim_handle = geometries[node.mesh][j];
				const auto &prim = model.meshes[i].primitives[j];
				const auto &mat = model.materials[prim.material];

				auto &inst = result.emplace_back(nullptr);
				inst.transform = trans;
				inst.material = materials[prim.material].take_ownership();
				inst.geometry = prim_handle.take_ownership();

				// create material pipeline
				auto defines = bookmark.create_vector_array<std::pair<std::u8string_view, std::u8string_view>>();
				if (mat.alphaMode == "MASK") {
					defines.emplace_back(u8"ALPHA_MASK", u8"");
				} else {
					if (mat.alphaMode != "OPAQUE") {
						log().warn<u8"Unknown alpha mode: {}">(mat.alphaMode);
					}
				}

				/*auto input_layouts = bookmark.create_reserved_vector_array<graphics::input_buffer_layout>(4);
				std::size_t buffer_id = 0;
				auto input_position = graphics::input_buffer_element::create(
					u8"POSITION", 0, graphics::format::r32g32b32_float, 0
				);
				auto input_uv = graphics::input_buffer_element::create(
					u8"TEXCOORD", 0, graphics::format::r32g32_float, 0
				);
				auto input_normal = graphics::input_buffer_element::create(
					u8"NORMAL", 0, graphics::format::r32g32b32_float, 0
				);
				auto input_tangent = graphics::input_buffer_element::create(
					u8"TANGENT", 0, graphics::format::r32g32b32a32_float, 0
				);
				input_layouts.emplace_back(graphics::input_buffer_layout::create_vertex_buffer<cvec3f>(
					std::span(&input_position, 1), buffer_id++
				));
				if (inst.geometry.get().value.uv_buffer) {
					input_layouts.emplace_back(graphics::input_buffer_layout::create_vertex_buffer<cvec2f>(
						std::span(&input_uv, 1), buffer_id++
					));
					defines.emplace_back(u8"VERTEX_INPUT_HAS_UV", u8"");
				}
				if (inst.geometry.get().value.normal_buffer) {
					input_layouts.emplace_back(graphics::input_buffer_layout::create_vertex_buffer<cvec3f>(
						std::span(&input_normal, 1), buffer_id++
					));
					defines.emplace_back(u8"VERTEX_INPUT_HAS_NORMAL", u8"");
				}
				if (inst.geometry.get().value.tangent_buffer) {
					input_layouts.emplace_back(graphics::input_buffer_layout::create_vertex_buffer<cvec4f>(
						std::span(&input_tangent, 1), buffer_id++
					));
					defines.emplace_back(u8"VERTEX_INPUT_HAS_TANGENT", u8"");
				}

				inst.pipeline = mat_context.create_pipeline(
					_asset_manager, "gltf_material.hlsl", defines, input_layouts
				);*/
			}
		}

		return result;
	}
}
