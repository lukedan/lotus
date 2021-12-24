#pragma once

#include <vector>

#include <lotus/graphics/resources.h>

#include "common.h"

struct scene_resources {
	struct node_data {
		node_data(uninitialized_t) {
		}

		mat44f transform = uninitialized; ///< Transform of the primitive.
	};
	struct material_data {
		material_data(uninitialized_t) {
		}

		cvec4f base_color = uninitialized;

		float normal_scale;
		float metalness;
		float roughness;
		float alpha_cutoff;

		std::uint32_t base_color_index;
		std::uint32_t normal_index;
		std::uint32_t metallic_roughness_index;
		std::uint32_t _padding;
	};
	struct vertex {
		vertex(uninitialized_t) {
		}

		cvec3f position = uninitialized;
		cvec3f normal = uninitialized;
		cvec2f uv = uninitialized;
		cvec4f tangent = uninitialized;
	};
	struct instance_data {
		instance_data(uninitialized_t) {
		}

		std::uint32_t first_index;
		std::uint32_t first_vertex;
		std::uint32_t material_index;
	};

	[[nodiscard]] inline static scene_resources create(
		gfx::device &dev, const gfx::adapter_properties &dev_props,
		gfx::command_allocator &cmd_alloc, gfx::command_queue &cmd_queue,
		gfx::descriptor_pool &descriptor_pool,
		const gltf::Model &model
	) {
		scene_resources result;

		auto upload_fence = dev.create_fence(gfx::synchronization_state::unset);

		// images
		std::vector<gfx::format> image_formats;
		image_formats.reserve(model.images.size());
		result.images.reserve(model.images.size());
		for (const auto &img : model.images) {
			auto type = gfx::format_properties::data_type::unknown;
			switch (img.pixel_type) {
			case TINYGLTF_COMPONENT_TYPE_BYTE: [[fallthrough]];
			case TINYGLTF_COMPONENT_TYPE_SHORT: [[fallthrough]];
			case TINYGLTF_COMPONENT_TYPE_INT:
				type = gfx::format_properties::data_type::signed_norm;
				break;
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: [[fallthrough]];
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: [[fallthrough]];
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
				type = gfx::format_properties::data_type::unsigned_norm;
				break;
			case TINYGLTF_COMPONENT_TYPE_FLOAT: [[fallthrough]];
			case TINYGLTF_COMPONENT_TYPE_DOUBLE:
				type = gfx::format_properties::data_type::floatint_point;
				break;
			}

			std::uint8_t bits[4] = { 0, 0, 0, 0 };
			for (int i = 0; i < img.component; ++i) {
				bits[i] = static_cast<std::uint8_t>(img.bits);
			}
			auto format = gfx::format_properties::find_exact_rgba(bits[0], bits[1], bits[2], bits[3], type);

			std::size_t bytes_per_pixel = (img.component * img.bits + 7) / 8;
			std::size_t bytes_per_row   = bytes_per_pixel * img.width;
			// TODO find correct pixel format
			auto upload_buf = dev.create_committed_staging_buffer(
				img.width, img.height, format, gfx::heap_type::upload,
				gfx::buffer_usage::mask::copy_source
			);
			// copy data
			auto *dest = static_cast<std::byte*>(dev.map_buffer(upload_buf.data, 0, 0));
			auto *source = img.image.data();
			std::size_t pitch_bytes = upload_buf.row_pitch.get_pitch_in_bytes();
			for (
				std::size_t y = 0;
				y < img.height;
				++y, dest += pitch_bytes, source += bytes_per_row
			) {
				std::memcpy(dest, source, bytes_per_row);
			}
			dev.unmap_buffer(upload_buf.data, 0, upload_buf.total_size);

			// create gpu image
			auto gpu_img = dev.create_committed_image2d(
				img.width, img.height, 1, 1, format, gfx::image_tiling::optimal,
				gfx::image_usage::mask::copy_destination | gfx::image_usage::mask::read_only_texture
			);
			dev.set_debug_name(gpu_img, reinterpret_cast<const char8_t*>(img.name.c_str()));

			// copy to gpu
			auto copy_cmd = dev.create_and_start_command_list(cmd_alloc);
			copy_cmd.resource_barrier(
				{
					gfx::image_barrier::create(gfx::subresource_index::first_color(), gpu_img, gfx::image_usage::initial, gfx::image_usage::copy_destination),
				},
				{}
			);
			copy_cmd.copy_buffer_to_image(
				upload_buf.data, 0, upload_buf.row_pitch,
				aab2s::create_from_min_max(zero, cvec2s(img.width, img.height)),
				gpu_img, gfx::subresource_index::first_color(), zero
			);
			copy_cmd.resource_barrier(
				{
					gfx::image_barrier::create(gfx::subresource_index::first_color(), gpu_img, gfx::image_usage::copy_destination, gfx::image_usage::read_only_texture),
				},
				{}
			);
			copy_cmd.finish();
			cmd_queue.submit_command_lists({ &copy_cmd }, &upload_fence);
			dev.wait_for_fence(upload_fence);
			dev.reset_fence(upload_fence);

			result.images.emplace_back(std::move(gpu_img));
			image_formats.emplace_back(format);
		}

		result.image_views.reserve(model.textures.size());
		for (const auto &tex : model.textures) {
			result.image_views.emplace_back(dev.create_image2d_view_from(
				result.images[tex.source], image_formats[tex.source], gfx::mip_levels::all()
			));
		}

		{ // empty color texture
			auto format = gfx::format::r8g8b8a8_snorm;

			auto upload_buf = dev.create_committed_staging_buffer(
				1, 1, format,
				gfx::heap_type::upload, gfx::buffer_usage::mask::copy_source
			);
			auto *source = static_cast<char*>(dev.map_buffer(upload_buf.data, 0, 0));
			source[0] = std::numeric_limits<char>::max();
			source[1] = std::numeric_limits<char>::max();
			source[2] = std::numeric_limits<char>::max();
			dev.unmap_buffer(upload_buf.data, 0, upload_buf.total_size);

			result.empty_color = dev.create_committed_image2d(
				1, 1, 1, 1, format, gfx::image_tiling::optimal,
				gfx::image_usage::mask::copy_destination | gfx::image_usage::mask::read_only_texture
			);
			result.empty_color_view_index = result.image_views.size();
			result.image_views.emplace_back(dev.create_image2d_view_from(result.empty_color, format, gfx::mip_levels::all()));
			dev.set_debug_name(result.empty_color, u8"Empty color");

			auto copy_cmd = dev.create_and_start_command_list(cmd_alloc);
			copy_cmd.resource_barrier(
				{
					gfx::image_barrier::create(gfx::subresource_index::first_color(), result.empty_color, gfx::image_usage::initial, gfx::image_usage::copy_destination),
				},
				{}
			);
			copy_cmd.copy_buffer_to_image(
				upload_buf.data, 0, upload_buf.row_pitch, aab2s::create_from_min_max(zero, cvec2s(1, 1)),
				result.empty_color, gfx::subresource_index::first_color(), zero
			);
			copy_cmd.resource_barrier(
				{
					gfx::image_barrier::create(gfx::subresource_index::first_color(), result.empty_color, gfx::image_usage::copy_destination, gfx::image_usage::read_only_texture),
				},
				{}
			);
			copy_cmd.finish();
			cmd_queue.submit_command_lists({ &copy_cmd }, &upload_fence);
			dev.wait_for_fence(upload_fence);
			dev.reset_fence(upload_fence);
		}

		{ // empty normal texture
			auto format = gfx::format::r8g8b8a8_snorm;

			auto upload_buf = dev.create_committed_staging_buffer(
				1, 1, format,
				gfx::heap_type::upload, gfx::buffer_usage::mask::copy_source
			);
			auto *source = static_cast<char*>(dev.map_buffer(upload_buf.data, 0, 0));
			source[0] = 0;
			source[1] = 0;
			source[2] = std::numeric_limits<char>::max();
			dev.unmap_buffer(upload_buf.data, 0, upload_buf.total_size);

			result.empty_normal = dev.create_committed_image2d(
				1, 1, 1, 1, format, gfx::image_tiling::optimal,
				gfx::image_usage::mask::copy_destination | gfx::image_usage::mask::read_only_texture
			);
			result.empty_normal_view_index = result.image_views.size();
			result.image_views.emplace_back(dev.create_image2d_view_from(result.empty_normal, format, gfx::mip_levels::all()));
			dev.set_debug_name(result.empty_normal, u8"Empty normal");

			auto copy_cmd = dev.create_and_start_command_list(cmd_alloc);
			copy_cmd.resource_barrier(
				{
					gfx::image_barrier::create(gfx::subresource_index::first_color(), result.empty_normal, gfx::image_usage::initial, gfx::image_usage::copy_destination),
				},
				{}
			);
			copy_cmd.copy_buffer_to_image(
				upload_buf.data, 0, upload_buf.row_pitch, aab2s::create_from_min_max(zero, cvec2s(1, 1)),
				result.empty_normal, gfx::subresource_index::first_color(), zero
			);
			copy_cmd.resource_barrier(
				{
					gfx::image_barrier::create(gfx::subresource_index::first_color(), result.empty_normal, gfx::image_usage::copy_destination, gfx::image_usage::read_only_texture),
				},
				{}
			);
			copy_cmd.finish();
			cmd_queue.submit_command_lists({ &copy_cmd }, &upload_fence);
			dev.wait_for_fence(upload_fence);
			dev.reset_fence(upload_fence);
		}

		{ // empty material properties texture
			auto format = gfx::format::r8g8b8a8_unorm;

			auto upload_buf = dev.create_committed_staging_buffer(
				1, 1, format,
				gfx::heap_type::upload, gfx::buffer_usage::mask::copy_source
			);
			auto *source = static_cast<unsigned char*>(dev.map_buffer(upload_buf.data, 0, 0));
			source[0] = 0;
			source[1] = std::numeric_limits<unsigned char>::max();
			source[2] = std::numeric_limits<unsigned char>::max();
			dev.unmap_buffer(upload_buf.data, 0, upload_buf.total_size);

			result.empty_metalness_glossiness = dev.create_committed_image2d(
				1, 1, 1, 1, format, gfx::image_tiling::optimal,
				gfx::image_usage::mask::copy_destination | gfx::image_usage::mask::read_only_texture
			);
			result.empty_metalness_glossiness_view_index = result.image_views.size();
			result.image_views.emplace_back(dev.create_image2d_view_from(result.empty_metalness_glossiness, format, gfx::mip_levels::all()));
			dev.set_debug_name(result.empty_metalness_glossiness, u8"Empty material properties");

			auto copy_cmd = dev.create_and_start_command_list(cmd_alloc);
			copy_cmd.resource_barrier(
				{
					gfx::image_barrier::create(gfx::subresource_index::first_color(), result.empty_metalness_glossiness, gfx::image_usage::initial, gfx::image_usage::copy_destination),
				},
				{}
			);
			copy_cmd.copy_buffer_to_image(
				upload_buf.data, 0, upload_buf.row_pitch, aab2s::create_from_min_max(zero, cvec2s(1, 1)),
				result.empty_metalness_glossiness, gfx::subresource_index::first_color(), zero
			);
			copy_cmd.resource_barrier(
				{
					gfx::image_barrier::create(gfx::subresource_index::first_color(), result.empty_metalness_glossiness, gfx::image_usage::copy_destination, gfx::image_usage::read_only_texture),
				},
				{}
			);
			copy_cmd.finish();
			cmd_queue.submit_command_lists({ &copy_cmd }, &upload_fence);
			dev.wait_for_fence(upload_fence);
			dev.reset_fence(upload_fence);
		}

		// vertex buffers & index buffers
		result.instance_indices.reserve(model.meshes.size());
		result.vertex_count = 0;
		result.index_count = 0;
		for (const auto &mesh : model.meshes) {
			auto &instance_indices = result.instance_indices.emplace_back();
			instance_indices.reserve(mesh.primitives.size());
			for (const auto &prim : mesh.primitives) {
				instance_indices.emplace_back(result.instances.size());
				auto &instance = result.instances.emplace_back(uninitialized);
				instance.material_index = prim.material;
				instance.first_vertex = result.vertex_count;
				result.vertex_count += model.accessors[prim.attributes.begin()->second].count;
				if (prim.indices < 0) {
					instance.first_index = std::numeric_limits<std::uint32_t>::max();
				} else {
					instance.first_index = result.index_count;
					result.index_count += model.accessors[prim.indices].count;
				}
			}
		}
		{
			std::size_t vert_buffer_size = sizeof(vertex) * result.vertex_count;
			std::size_t index_buffer_size = sizeof(std::uint32_t) * result.index_count;
			result.vertex_buffer = dev.create_committed_buffer(vert_buffer_size, gfx::heap_type::device_only, gfx::buffer_usage::mask::copy_destination | gfx::buffer_usage::mask::read_only_buffer | gfx::buffer_usage::mask::vertex_buffer);
			result.index_buffer = dev.create_committed_buffer(index_buffer_size, gfx::heap_type::device_only, gfx::buffer_usage::mask::copy_destination | gfx::buffer_usage::mask::read_only_buffer | gfx::buffer_usage::mask::index_buffer);
			auto vert_upload = dev.create_committed_buffer(vert_buffer_size, gfx::heap_type::upload, gfx::buffer_usage::mask::copy_source);
			auto index_upload = dev.create_committed_buffer(index_buffer_size, gfx::heap_type::upload, gfx::buffer_usage::mask::copy_source);
			auto *verts = static_cast<vertex*>(dev.map_buffer(vert_upload, 0, 0));
			auto *indices = static_cast<std::uint32_t*>(dev.map_buffer(index_upload, 0, 0));
			for (std::size_t mesh_i = 0; mesh_i < model.meshes.size(); ++mesh_i) {
				const auto &mesh = model.meshes[mesh_i];
				for (std::size_t prim_i = 0; prim_i < mesh.primitives.size(); ++prim_i) {
					const auto &prim = mesh.primitives[prim_i];
					const auto &instance = result.instances[result.instance_indices[mesh_i][prim_i]];
					if (auto it = prim.attributes.find("POSITION"); it != prim.attributes.end()) {
						const auto &accessor = model.accessors[it->second];
						const auto &buf_view = model.bufferViews[accessor.bufferView];
						std::size_t stride = accessor.ByteStride(buf_view);
						for (std::size_t i = 0; i < accessor.count; ++i) {
							auto *data = reinterpret_cast<const float*>(model.buffers[buf_view.buffer].data.data() + buf_view.byteOffset + stride * i);
							verts[instance.first_vertex + i].position = cvec3f(data[0], data[1], data[2]);
						}
					}
					if (auto it = prim.attributes.find("NORMAL"); it != prim.attributes.end()) {
						const auto &accessor = model.accessors[it->second];
						const auto &buf_view = model.bufferViews[accessor.bufferView];
						std::size_t stride = accessor.ByteStride(buf_view);
						for (std::size_t i = 0; i < accessor.count; ++i) {
							auto *data = reinterpret_cast<const float*>(model.buffers[buf_view.buffer].data.data() + buf_view.byteOffset + stride * i);
							verts[instance.first_vertex + i].normal = cvec3f(data[0], data[1], data[2]);
						}
					}
					if (auto it = prim.attributes.find("TANGENT"); it != prim.attributes.end()) {
						const auto &accessor = model.accessors[it->second];
						const auto &buf_view = model.bufferViews[accessor.bufferView];
						std::size_t stride = accessor.ByteStride(buf_view);
						for (std::size_t i = 0; i < accessor.count; ++i) {
							auto *data = reinterpret_cast<const float*>(model.buffers[buf_view.buffer].data.data() + buf_view.byteOffset + stride * i);
							verts[instance.first_vertex + i].tangent = cvec4f(data[0], data[1], data[2], data[3]);
						}
					}
					if (auto it = prim.attributes.find("TEXCOORD_0"); it != prim.attributes.end()) {
						const auto &accessor = model.accessors[it->second];
						const auto &buf_view = model.bufferViews[accessor.bufferView];
						std::size_t stride = accessor.ByteStride(buf_view);
						for (std::size_t i = 0; i < accessor.count; ++i) {
							auto *data_raw = model.buffers[buf_view.buffer].data.data() + buf_view.byteOffset + stride * i;
							auto &vert = verts[instance.first_vertex + i];
							switch (accessor.componentType) {
							case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
								{
									auto *data = reinterpret_cast<const std::uint8_t*>(data_raw);
									vert.uv = cvec2f(data[0], data[1]) / 255.0f;
								}
								break;
							case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
								{
									auto *data = reinterpret_cast<const std::uint16_t*>(data_raw);
									vert.uv = cvec2f(data[0], data[1]) / 65535.0f;
								}
								break;
							case TINYGLTF_COMPONENT_TYPE_FLOAT:
								{
									auto *data = reinterpret_cast<const float*>(data_raw);
									vert.uv = cvec2f(data[0], data[1]);
								}
								break;
							default:
								std::cerr << "Unhandled UV format\n";
								break;
							}
						}
					}
					if (prim.indices >= 0) {
						const auto &index_accessor = model.accessors[prim.indices];
						const auto &buf_view = model.bufferViews[index_accessor.bufferView];
						std::size_t stride = index_accessor.ByteStride(buf_view);
						for (std::size_t i = 0; i < index_accessor.count; ++i) {
							auto *data_raw = model.buffers[buf_view.buffer].data.data() + buf_view.byteOffset + stride * i;
							auto &index = indices[instance.first_index + i];
							switch (index_accessor.componentType) {
							case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
								index = *reinterpret_cast<const std::uint16_t*>(data_raw);
								break;
							case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
								index = *reinterpret_cast<const std::uint32_t*>(data_raw);
								break;
							default:
								std::cerr << "Unhandled index format\n";
							}
						}
					}
				}
			}
			dev.unmap_buffer(vert_upload, 0, vert_buffer_size);
			dev.unmap_buffer(index_upload, 0, index_buffer_size);
			// copy to gpu
			auto copy_cmd = dev.create_and_start_command_list(cmd_alloc);
			copy_cmd.copy_buffer(vert_upload, 0, result.vertex_buffer, 0, vert_buffer_size);
			copy_cmd.copy_buffer(index_upload, 0, result.index_buffer, 0, index_buffer_size);
			copy_cmd.resource_barrier(
				{},
				{
					gfx::buffer_barrier::create(result.vertex_buffer, gfx::buffer_usage::copy_destination, gfx::buffer_usage::vertex_buffer),
					gfx::buffer_barrier::create(result.index_buffer, gfx::buffer_usage::copy_destination, gfx::buffer_usage::vertex_buffer),
				}
			);
			copy_cmd.finish();
			cmd_queue.submit_command_lists({ &copy_cmd }, &upload_fence);
			dev.wait_for_fence(upload_fence);
			dev.reset_fence(upload_fence);
		}

		{ // instances
			std::size_t instance_buf_size = sizeof(instance_data) * result.instances.size();
			result.instance_buffer = dev.create_committed_buffer(instance_buf_size, gfx::heap_type::device_only, gfx::buffer_usage::mask::copy_destination | gfx::buffer_usage::mask::read_only_buffer);
			auto upload_buf = dev.create_committed_buffer(instance_buf_size, gfx::heap_type::upload, gfx::buffer_usage::mask::copy_source);
			void *ptr = dev.map_buffer(upload_buf, 0, 0);
			std::memcpy(ptr, result.instances.data(), instance_buf_size);
			dev.unmap_buffer(upload_buf, 0, instance_buf_size);
			auto copy_cmd = dev.create_and_start_command_list(cmd_alloc);
			copy_cmd.copy_buffer(upload_buf, 0, result.instance_buffer, 0, instance_buf_size);
			copy_cmd.resource_barrier(
				{},
				{
					gfx::buffer_barrier::create(result.instance_buffer, gfx::buffer_usage::copy_destination, gfx::buffer_usage::read_only_buffer),
				}
			);
			copy_cmd.finish();
			cmd_queue.submit_command_lists({ &copy_cmd }, &upload_fence);
			dev.wait_for_fence(upload_fence);
			dev.reset_fence(upload_fence);
		}

		// node buffer
		result.aligned_node_data_size = align_size(sizeof(node_data), dev_props.constant_buffer_alignment);
		std::size_t node_buffer_size = result.aligned_node_data_size * model.nodes.size();
		result.node_buffer = dev.create_committed_buffer(
			node_buffer_size, gfx::heap_type::device_only,
			gfx::buffer_usage::mask::copy_destination | gfx::buffer_usage::mask::read_only_buffer
		);
		{ // upload data
			auto upload_buf = dev.create_committed_buffer(
				node_buffer_size, gfx::heap_type::upload,
				gfx::buffer_usage::mask::copy_source
			);
			void *ptr = dev.map_buffer(upload_buf, 0, 0);
			for (const auto &node : model.nodes) {
				auto *data = new (ptr) node_data(uninitialized);
				if (node.matrix.size() > 0) {
					for (std::size_t y = 0; y < 4; ++y) {
						for (std::size_t x = 0; x < 4; ++x) {
							data->transform(y, x) = static_cast<float>(node.matrix[y * 4 + x]);
						}
					}
				} else {
					data->transform = mat44f::identity();
				}
				ptr = reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(ptr) + result.aligned_node_data_size);
			}
			dev.unmap_buffer(upload_buf, 0, node_buffer_size);

			auto list = dev.create_and_start_command_list(cmd_alloc);
			list.copy_buffer(upload_buf, 0, result.node_buffer, 0, node_buffer_size);
			list.resource_barrier(
				{},
				{
					gfx::buffer_barrier::create(result.node_buffer, gfx::buffer_usage::copy_destination, gfx::buffer_usage::read_only_buffer)
				}
			);
			list.finish();
			cmd_queue.submit_command_lists({ &list }, &upload_fence);
			dev.wait_for_fence(upload_fence);
			dev.reset_fence(upload_fence);
		}

		// materials
		result.material_descriptor_layout = dev.create_descriptor_set_layout(
			{
				gfx::descriptor_range_binding::create(gfx::descriptor_type::constant_buffer, 1, 0)
			},
			gfx::shader_stage::all
		);
		result.material_descriptor_sets.reserve(model.materials.size());
		result.aligned_material_data_size = align_size(sizeof(material_data), dev_props.constant_buffer_alignment);
		result.material_buffer = dev.create_committed_buffer(
			result.aligned_material_data_size * model.materials.size(),
			gfx::heap_type::device_only,
			gfx::buffer_usage::mask::read_only_buffer | gfx::buffer_usage::mask::copy_destination
		);
		{
			auto material_upload_buffer = dev.create_committed_buffer(
				result.aligned_material_data_size * model.materials.size(),
				gfx::heap_type::upload, gfx::buffer_usage::mask::copy_source
			);
			void *buffer_ptr = dev.map_buffer(material_upload_buffer, 0, 0);
			for (std::size_t i = 0; i < model.materials.size(); ++i) {
				const auto &mat = model.materials[i];

				std::size_t base_color_index =
					mat.pbrMetallicRoughness.baseColorTexture.index >= 0 ?
					mat.pbrMetallicRoughness.baseColorTexture.index :
					result.empty_color_view_index;
				std::size_t normal_index =
					mat.normalTexture.index >= 0 ?
					mat.normalTexture.index :
					result.empty_normal_view_index;
				std::size_t metalness_roughness_index =
					mat.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0 ?
					mat.pbrMetallicRoughness.metallicRoughnessTexture.index :
					result.empty_metalness_glossiness_view_index;

				auto *buffer = reinterpret_cast<material_data*>(reinterpret_cast<std::uintptr_t>(buffer_ptr) + i * result.aligned_material_data_size);
				buffer->base_color = cvec4d(
					mat.pbrMetallicRoughness.baseColorFactor[0],
					mat.pbrMetallicRoughness.baseColorFactor[1],
					mat.pbrMetallicRoughness.baseColorFactor[2],
					mat.pbrMetallicRoughness.baseColorFactor[3]
				).into<float>();

				buffer->normal_scale = static_cast<float>(mat.normalTexture.scale);
				buffer->metalness = static_cast<float>(mat.pbrMetallicRoughness.metallicFactor);
				buffer->roughness = static_cast<float>(mat.pbrMetallicRoughness.roughnessFactor);
				if (mat.alphaMode == "OPAQUE") {
					buffer->alpha_cutoff = 0.0f;
				} else if (mat.alphaMode == "MASK") {
					buffer->alpha_cutoff = static_cast<float>(mat.alphaCutoff);
				} else {
					std::cerr << "Unknown alpha mode: " << mat.alphaMode << "\n";
				}

				buffer->base_color_index = base_color_index;
				buffer->normal_index = normal_index;
				buffer->metallic_roughness_index = metalness_roughness_index;

				auto set = dev.create_descriptor_set(descriptor_pool, result.material_descriptor_layout);
				dev.write_descriptor_set_constant_buffers(
					set, result.material_descriptor_layout, 0,
					{
						gfx::constant_buffer_view::create(result.material_buffer, i * result.aligned_material_data_size, sizeof(material_data)),
					}
				);
				result.material_descriptor_sets.emplace_back(std::move(set));
			}
			dev.unmap_buffer(material_upload_buffer, 0, model.materials.size() * result.aligned_material_data_size);

			// upload constants
			auto cmd_list = dev.create_and_start_command_list(cmd_alloc);
			cmd_list.copy_buffer(material_upload_buffer, 0, result.material_buffer, 0, model.materials.size() * result.aligned_material_data_size);
			cmd_list.resource_barrier(
				{},
				{
					gfx::buffer_barrier::create(result.material_buffer, gfx::buffer_usage::copy_destination, gfx::buffer_usage::read_only_buffer),
				}
			);
			cmd_list.finish();
			cmd_queue.submit_command_lists({ &cmd_list }, &upload_fence);
			dev.wait_for_fence(upload_fence);
			dev.reset_fence(upload_fence);
		}

		result.node_descriptor_layout = dev.create_descriptor_set_layout(
			{
				gfx::descriptor_range_binding::create(gfx::descriptor_type::constant_buffer, 1, 0),
			},
			gfx::shader_stage::all
		);
		result.node_descriptor_sets.reserve(model.nodes.size());
		for (std::size_t i = 0; i < model.nodes.size(); ++i) {
			auto set = dev.create_descriptor_set(descriptor_pool, result.node_descriptor_layout);
			dev.write_descriptor_set_constant_buffers(
				set, result.node_descriptor_layout, 0,
				{
					gfx::constant_buffer_view::create(result.node_buffer, i * result.aligned_node_data_size, sizeof(scene_resources::node_data)),
				}
			);
			result.node_descriptor_sets.emplace_back(std::move(set));
		}

		// bindless textures
		result.textures_descriptor_layout = dev.create_descriptor_set_layout(
			{
				gfx::descriptor_range_binding::create_unbounded(gfx::descriptor_type::read_only_image, 0),
			},
			gfx::shader_stage::all
		);
		result.textures_descriptor_set = dev.create_descriptor_set(
			descriptor_pool, result.textures_descriptor_layout, result.image_views.size()
		);
		std::vector<gfx::image_view*> views(result.image_views.size());
		for (std::size_t i = 0; i < result.image_views.size(); ++i) {
			views[i] = &result.image_views[i];
		}
		dev.write_descriptor_set_read_only_images(result.textures_descriptor_set, result.textures_descriptor_layout, 0, views);

		// bottom level acceleration structures
		for (std::size_t mesh_i = 0; mesh_i < model.nodes.size(); ++mesh_i) {
			const auto &mesh = model.meshes[mesh_i];
			auto &mesh_blases = result.blas.emplace_back();
			for (std::size_t prim_i = 0; prim_i < mesh.primitives.size(); ++prim_i) {
				const auto &prim = mesh.primitives[prim_i];
				const auto &instance = result.instances[result.instance_indices[mesh_i][prim_i]];

				// index buffer
				gfx::index_buffer_view index_buf = nullptr;
				index_buf = gfx::index_buffer_view::create(
					result.index_buffer, gfx::index_format::uint32,
					sizeof(std::uint32_t) * instance.first_index,
					model.accessors[prim.indices].count
				);

				gfx::vertex_buffer_view vert_buf = nullptr;
				if (auto position_it = prim.attributes.find("POSITION"); position_it != prim.attributes.end()) {
					const auto &accessor = model.accessors[position_it->second];
					vert_buf = gfx::vertex_buffer_view::create(
						result.vertex_buffer, gfx::format::r32g32b32_float,
						sizeof(vertex) * instance.first_vertex,
						sizeof(vertex),
						accessor.count
					);
				} else {
					assert(false);
				}

				auto geom = gfx::bottom_level_acceleration_structure_geometry::create({ { vert_buf, index_buf } });

				auto build_sizes = dev.get_bottom_level_acceleration_structure_build_sizes(geom);
				auto buf = dev.create_committed_buffer(build_sizes.acceleration_structure_size, gfx::heap_type::device_only, gfx::buffer_usage::mask::acceleration_structure);
				dev.set_debug_name(buf, u8"Acceleration structure");
				auto tmp_buf = dev.create_committed_buffer(build_sizes.build_scratch_size, gfx::heap_type::device_only, gfx::buffer_usage::mask::read_write_buffer);
				dev.set_debug_name(tmp_buf, u8"Acceleration structure build scratch");
				auto blas = dev.create_bottom_level_acceleration_structure(buf, 0, build_sizes.acceleration_structure_size);

				auto cmd_list = dev.create_and_start_command_list(cmd_alloc);
				cmd_list.build_acceleration_structure(geom, blas, tmp_buf, 0);
				cmd_list.finish();
				auto fence = dev.create_fence(gfx::synchronization_state::unset);
				cmd_queue.submit_command_lists({ &cmd_list }, &fence);
				dev.wait_for_fence(fence);

				mesh_blases.emplace_back(std::move(blas));
			}
		}

		// top level acceleration structure
		std::size_t num_instances = 0;
		for (const auto &prim : model.nodes) {
			num_instances += model.meshes[prim.mesh].primitives.size();
		}
		std::size_t tlas_buf_size = num_instances * sizeof(gfx::instance_description);
		auto tlas_buf = dev.create_committed_buffer(tlas_buf_size, gfx::heap_type::upload, gfx::buffer_usage::mask::read_only_buffer);
		auto *ptr = static_cast<gfx::instance_description*>(dev.map_buffer(tlas_buf, 0, 0));
		std::size_t instance_i = 0;
		for (const auto &node : model.nodes) {
			auto &mesh = model.meshes[node.mesh];
			auto transform = mat44f::identity();
			if (!node.matrix.empty()) {
				for (std::size_t row = 0; row < 4; ++row) {
					for (std::size_t col = 0; col < 4; ++col) {
						transform(row, col) = node.matrix[row * 4 + col];
					}
				}
			}
			for (std::size_t prim_i = 0; prim_i < mesh.primitives.size(); ++prim_i) {
				const auto &prim = mesh.primitives[prim_i];
				ptr[instance_i] = result.blas[node.mesh][prim_i].get_description(transform, result.instance_indices[node.mesh][prim_i], 0xFFu, prim.indices < 0 ? 1 : 0);
				++instance_i;
			}
		}
		assert(instance_i == num_instances);
		dev.unmap_buffer(tlas_buf, 0, tlas_buf_size);
		auto tlas_sizes = dev.get_top_level_acceleration_structure_build_sizes(tlas_buf, 0, num_instances);
		auto tlas_data_buf = dev.create_committed_buffer(tlas_sizes.acceleration_structure_size, gfx::heap_type::device_only, gfx::buffer_usage::mask::acceleration_structure);
		result.tlas = dev.create_top_level_acceleration_structure(tlas_data_buf, 0, tlas_sizes.acceleration_structure_size);
		{
			auto tlas_scratch = dev.create_committed_buffer(tlas_sizes.build_scratch_size, gfx::heap_type::device_only, gfx::buffer_usage::mask::read_write_buffer);
			auto cmd_list = dev.create_and_start_command_list(cmd_alloc);
			cmd_list.resource_barrier({}, { gfx::buffer_barrier::create(tlas_scratch, gfx::buffer_usage::read_only_buffer, gfx::buffer_usage::read_write_buffer)});
			cmd_list.build_acceleration_structure(tlas_buf, 0, num_instances, result.tlas, tlas_scratch, 0);
			cmd_list.finish();
			auto fence = dev.create_fence(gfx::synchronization_state::unset);
			cmd_queue.submit_command_lists({ &cmd_list }, &fence);
			dev.wait_for_fence(fence);
		}

		return result;
	}

	template <
		typename Callback
	> [[nodiscard]] inline static std::vector<std::vector<gfx::graphics_pipeline_state>> create_pipeline_states(
		const gltf::Model &model, Callback &&create_pipeline
	) {
		std::vector<std::vector<gfx::graphics_pipeline_state>> pipelines;
		pipelines.reserve(model.meshes.size());
		{
			std::vector<gfx::input_buffer_element> vert_buffer_elements;
			std::vector<gfx::input_buffer_layout> vert_buffer_layouts;
			for (const auto &mesh : model.meshes) {
				auto &prim_pipelines = pipelines.emplace_back();
				prim_pipelines.reserve(mesh.primitives.size());
				for (const auto &prim : mesh.primitives) {
					vert_buffer_elements.clear();
					vert_buffer_elements.reserve(prim.attributes.size());
					vert_buffer_layouts.clear();
					vert_buffer_layouts.reserve(prim.attributes.size());
					for (auto it = prim.attributes.begin(); it != prim.attributes.end(); ++it) {
						const auto &attr = *it;
						const auto &accessor = model.accessors[attr.second];
						gfx::input_buffer_element elem = uninitialized;
						if (attr.first == "POSITION") {
							elem = gfx::input_buffer_element::create(
								u8"POSITION", 0, gfx::format::r32g32b32_float, 0
							);
						} else if (attr.first == "NORMAL") {
							elem = gfx::input_buffer_element::create(
								u8"NORMAL", 0, gfx::format::r32g32b32_float, 0
							);
						} else if (attr.first == "TANGENT") {
							elem = gfx::input_buffer_element::create(
								u8"TANGENT", 0, gfx::format::r32g32b32a32_float, 0
							);
						} else if (attr.first == "TEXCOORD_0") {
							gfx::format fmt;
							switch (accessor.componentType) {
							case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
								fmt = gfx::format::r8g8_unorm;
								break;
							case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
								fmt = gfx::format::r16g16_unorm;
								break;
							case TINYGLTF_COMPONENT_TYPE_FLOAT:
								fmt = gfx::format::r32g32_float;
								break;
							default:
								std::cerr << "Unhandled texcoord format: " << accessor.componentType << std::endl;
								continue;
							}
							elem = gfx::input_buffer_element::create(u8"TEXCOORD", 0, fmt, 0);
						} else {
							std::cerr << "Unhandled vertex buffer element: " << attr.first << std::endl;
							continue;
						}
						std::size_t index = vert_buffer_elements.size();
						vert_buffer_elements.emplace_back(elem);
						vert_buffer_layouts.emplace_back(gfx::input_buffer_layout::create_vertex_buffer(
							{ vert_buffer_elements.end() - 1, vert_buffer_elements.end() },
							accessor.ByteStride(model.bufferViews[accessor.bufferView]), index
						));
					}
					prim_pipelines.emplace_back(create_pipeline(vert_buffer_layouts));
				}
			}
		}
		return pipelines;
	}

	std::vector<gfx::image2d> images;
	std::vector<gfx::image2d_view> image_views;
	gfx::image2d empty_color = nullptr;
	gfx::image2d empty_normal = nullptr;
	gfx::image2d empty_metalness_glossiness = nullptr;
	std::size_t empty_color_view_index;
	std::size_t empty_normal_view_index;
	std::size_t empty_metalness_glossiness_view_index;

	std::vector<std::vector<std::size_t>> instance_indices;
	std::vector<instance_data> instances;

	gfx::buffer vertex_buffer = nullptr;
	std::size_t vertex_count;
	gfx::buffer index_buffer = nullptr;
	std::size_t index_count;
	gfx::buffer instance_buffer = nullptr;

	gfx::descriptor_set textures_descriptor_set = nullptr;
	std::vector<gfx::descriptor_set> material_descriptor_sets;
	std::vector<gfx::descriptor_set> node_descriptor_sets;
	gfx::buffer node_buffer = nullptr;
	std::size_t aligned_node_data_size;
	gfx::buffer material_buffer = nullptr;
	std::size_t aligned_material_data_size;
	gfx::descriptor_set_layout textures_descriptor_layout = nullptr;
	gfx::descriptor_set_layout material_descriptor_layout = nullptr;
	gfx::descriptor_set_layout node_descriptor_layout = nullptr;

	std::vector<std::vector<gfx::bottom_level_acceleration_structure>> blas;
	gfx::top_level_acceleration_structure tlas = nullptr;
};
