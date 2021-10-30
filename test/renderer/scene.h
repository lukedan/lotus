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
	};

	[[nodiscard]] inline static scene_resources create(
		gfx::device &dev, const gfx::adapter_properties &dev_props,
		gfx::command_allocator &cmd_alloc, gfx::command_queue &cmd_queue,
		gfx::descriptor_pool &descriptor_pool, gfx::sampler &sampler,
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
			auto upload_buf = dev.create_committed_buffer_as_image2d(
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

			auto upload_buf = dev.create_committed_buffer_as_image2d(
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
			result.empty_color_view = dev.create_image2d_view_from(result.empty_color, format, gfx::mip_levels::all());
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

			auto upload_buf = dev.create_committed_buffer_as_image2d(
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
			result.empty_normal_view = dev.create_image2d_view_from(result.empty_normal, format, gfx::mip_levels::all());
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

			auto upload_buf = dev.create_committed_buffer_as_image2d(
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
			result.empty_metalness_glossiness_view = dev.create_image2d_view_from(result.empty_metalness_glossiness, format, gfx::mip_levels::all());
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

		// buffers
		result.buffers.reserve(model.buffers.size());
		for (const auto &buf : model.buffers) {
			auto cpu_buf = dev.create_committed_buffer(
				buf.data.size(), gfx::heap_type::upload,
				gfx::buffer_usage::mask::copy_source
			);
			void *data = dev.map_buffer(cpu_buf, 0, 0);
			std::memcpy(data, buf.data.data(), buf.data.size());
			dev.unmap_buffer(cpu_buf, 0, buf.data.size());

			// create gpu bufffer
			auto gpu_buf = dev.create_committed_buffer(
				buf.data.size(), gfx::heap_type::device_only,
				gfx::buffer_usage::mask::copy_destination | gfx::buffer_usage::mask::vertex_buffer | gfx::buffer_usage::mask::index_buffer
			);

			// copy to gpu
			auto copy_cmd = dev.create_and_start_command_list(cmd_alloc);
			copy_cmd.copy_buffer(cpu_buf, 0, gpu_buf, 0, buf.data.size());
			copy_cmd.resource_barrier(
				{},
				{
					gfx::buffer_barrier::create(gpu_buf, gfx::buffer_usage::copy_destination, gfx::buffer_usage::vertex_buffer),
				}
			);
			copy_cmd.finish();
			cmd_queue.submit_command_lists({ &copy_cmd }, &upload_fence);
			dev.wait_for_fence(upload_fence);
			dev.reset_fence(upload_fence);

			result.buffers.emplace_back(std::move(gpu_buf));
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
				gfx::descriptor_range_binding::create(gfx::descriptor_type::sampler, 1, 0),
				gfx::descriptor_range_binding::create(gfx::descriptor_type::constant_buffer, 1, 1),
				gfx::descriptor_range_binding::create(gfx::descriptor_type::read_only_image, 3, 2),
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

				auto &base_color_view =
					mat.pbrMetallicRoughness.baseColorTexture.index >= 0 ?
					result.image_views[mat.pbrMetallicRoughness.baseColorTexture.index] :
					result.empty_color_view;
				auto &normal_view =
					mat.normalTexture.index >= 0 ?
					result.image_views[mat.normalTexture.index] :
					result.empty_normal_view;
				auto &metalness_roughness_view =
					mat.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0 ?
					result.image_views[mat.pbrMetallicRoughness.metallicRoughnessTexture.index] :
					result.empty_metalness_glossiness_view;

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

				auto set = dev.create_descriptor_set(descriptor_pool, result.material_descriptor_layout);
				dev.write_descriptor_set_samplers(set, result.material_descriptor_layout, 0, { &sampler });
				dev.write_descriptor_set_constant_buffers(
					set, result.material_descriptor_layout, 1,
					{
						gfx::constant_buffer_view::create(result.material_buffer, i * result.aligned_material_data_size, sizeof(material_data)),
					}
				);
				dev.write_descriptor_set_images(set, result.material_descriptor_layout, 2, { &base_color_view, &normal_view, &metalness_roughness_view });
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
				gfx::descriptor_range_binding::create(gfx::descriptor_range::create(gfx::descriptor_type::constant_buffer, 1), 0),
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
	gfx::image2d_view empty_color_view = nullptr;
	gfx::image2d_view empty_normal_view = nullptr;
	gfx::image2d_view empty_metalness_glossiness_view = nullptr;

	std::vector<gfx::buffer> buffers;
	std::vector<gfx::descriptor_set> material_descriptor_sets;
	std::vector<gfx::descriptor_set> node_descriptor_sets;
	gfx::buffer node_buffer = nullptr;
	std::size_t aligned_node_data_size;
	gfx::buffer material_buffer = nullptr;
	std::size_t aligned_material_data_size;
	gfx::descriptor_set_layout material_descriptor_layout = nullptr;
	gfx::descriptor_set_layout node_descriptor_layout = nullptr;
};
