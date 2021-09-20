#include <iostream>
#include <fstream>
#include <filesystem>

#include <tiny_gltf.h>

#include <lotus/math/vector.h>
#include <lotus/graphics/context.h>
#include <lotus/graphics/commands.h>
#include <lotus/graphics/descriptors.h>
#include <lotus/system/window.h>
#include <lotus/system/application.h>
#include <lotus/utils/camera.h>

namespace gltf = tinygltf;

using namespace lotus;
namespace sys = lotus::system;
namespace gfx = lotus::graphics;

std::vector<std::byte> load_binary_file(const std::filesystem::path &p) {
	std::ifstream fin(p, std::ios::binary | std::ios::ate);
	assert(fin.good());
	std::vector<std::byte> result(fin.tellg());
	fin.seekg(0, std::ios::beg);
	fin.read(reinterpret_cast<char*>(result.data()), result.size());
	return result;
}

int main() {
	sys::application app(u8"test");
	gfx::context ctx;

	gfx::device dev = nullptr;
	ctx.enumerate_adapters([&](gfx::adapter adap) {
		auto properties = adap.get_properties();
		dev = adap.create_device();
		return false;
	});
	auto cmd_queue = dev.create_command_queue();
	auto cmd_alloc = dev.create_command_allocator();

	constexpr std::size_t num_swapchain_images = 2;

	auto wnd = app.create_window();
	auto swapchain = ctx.create_swap_chain_for_window(
		wnd, dev, cmd_queue, num_swapchain_images, gfx::format::r8g8b8a8_unorm
	);

	auto pass_resources = dev.create_pass_resources(
		{
			gfx::render_target_pass_options::create(
				gfx::format::r8g8b8a8_unorm,
				gfx::pass_load_operation::clear, gfx::pass_store_operation::preserve
			)
		},
		gfx::depth_stencil_pass_options::create(
			gfx::format::d32_float,
			gfx::pass_load_operation::clear, gfx::pass_store_operation::preserve,
			gfx::pass_load_operation::discard, gfx::pass_store_operation::discard
		)
	);

	std::array<gfx::image2d_view, num_swapchain_images> views{ nullptr, nullptr };
	std::array<gfx::image2d, num_swapchain_images> depth_buffers{ nullptr, nullptr };
	std::array<gfx::frame_buffer, num_swapchain_images> frame_buffers{ nullptr, nullptr };
	std::array<gfx::image2d_view, num_swapchain_images> depth_buffer_views{ nullptr, nullptr };
	for (std::size_t i = 0; i < num_swapchain_images; ++i) {
		views[i] = dev.create_image2d_view_from(
			swapchain.get_image(i),
			gfx::format::r8g8b8a8_unorm,
			gfx::mip_levels::only_highest()
		);
		depth_buffers[i] = dev.create_committed_image2d(
			800, 600, 1, 1, gfx::format::d32_float, gfx::image_tiling::optimal,
			gfx::image_usage::mask::depth_stencil_render_target, gfx::image_usage::depth_stencil_render_target
		);
		depth_buffer_views[i] = dev.create_image2d_view_from(
			depth_buffers[i], gfx::format::d32_float, gfx::mip_levels::only_highest()
		);
		frame_buffers[i] = dev.create_frame_buffer({ &views[i] }, &depth_buffer_views[i], pass_resources);
	}


	gltf::Model model;
	{
		gltf::TinyGLTF loader;
		std::string err;
		std::string warn;
		std::string path = "../../thirdparty/glTF-Sample-Models/2.0/Sponza/glTF/Sponza.gltf";
		if (!loader.LoadASCIIFromFile(&model, &err, &warn, path)) {
			std::cout << "Failed to load scene: " << err << "\n";
		}
		std::cout << warn << "\n";
	}


	std::vector<gfx::image2d> images;
	std::vector<gfx::buffer> buffers;
	{ // upload resources
		auto upload_fence = dev.create_fence(gfx::synchronization_state::unset);

		// images
		images.reserve(model.images.size());
		for (const auto &img : model.images) {
			std::size_t bytes_per_pixel = (img.component * img.bits + 7) / 8;
			std::size_t bytes_per_row   = bytes_per_pixel * img.width;
			// TODO find correct pixel format
			auto format = gfx::format::r8g8b8a8_unorm;
			auto [upload_buf, upload_buf_layout] = dev.create_committed_buffer_as_image2d(
				img.width, img.height, format, gfx::heap_type::upload,
				gfx::buffer_usage::mask::copy_source, gfx::buffer_usage::copy_source
			);
			// copy data
			auto *dest = static_cast<std::byte*>(dev.map_buffer(upload_buf, 0, 0));
			auto *source = img.image.data();
			for (
				std::size_t y = 0;
				y < img.height;
				++y, dest += upload_buf_layout.row_pitch, source += bytes_per_row
			) {
				std::memcpy(dest, source, bytes_per_row);
			}
			dev.unmap_buffer(upload_buf, 0, upload_buf_layout.total_size);

			// create gpu image
			auto gpu_img = dev.create_committed_image2d(
				img.width, img.height, 1, 1, format, gfx::image_tiling::optimal,
				gfx::image_usage::mask::copy_destination | gfx::image_usage::mask::read_only_texture,
				gfx::image_usage::copy_destination
			);
			dev.set_debug_name(gpu_img, reinterpret_cast<const char8_t*>(img.name.c_str()));

			// copy to gpu
			auto copy_cmd = dev.create_command_list(cmd_alloc);
			copy_cmd.start();
			copy_cmd.copy_buffer_to_image(
				upload_buf, 0, upload_buf_layout.row_pitch,
				aab2s::create_from_min_max(zero, cvec2s(img.width, img.height)), gpu_img, 0, zero
			);
			copy_cmd.resource_barrier(
				{
					gfx::image_barrier::create(gpu_img, gfx::image_usage::copy_destination, gfx::image_usage::read_only_texture),
				},
				{}
			);
			copy_cmd.finish();
			cmd_queue.submit_command_lists({ &copy_cmd }, &upload_fence);
			dev.wait_for_fence(upload_fence);
			dev.reset_fence(upload_fence);

			images.emplace_back(std::move(gpu_img));
		}

		// buffers
		buffers.reserve(model.buffers.size());
		for (const auto &buf : model.buffers) {
			auto cpu_buf = dev.create_committed_buffer(
				buf.data.size(), gfx::heap_type::upload,
				gfx::buffer_usage::mask::copy_source, gfx::buffer_usage::copy_source
			);
			void *data = dev.map_buffer(cpu_buf, 0, 0);
			std::memcpy(data, buf.data.data(), buf.data.size());
			dev.unmap_buffer(cpu_buf, 0, buf.data.size());

			// create gpu bufffer
			auto gpu_buf = dev.create_committed_buffer(
				buf.data.size(), gfx::heap_type::device_only,
				gfx::buffer_usage::mask::copy_destination | gfx::buffer_usage::mask::read_only_buffer,
				gfx::buffer_usage::copy_destination
			);

			// copy to gpu
			auto copy_cmd = dev.create_command_list(cmd_alloc);
			copy_cmd.start();
			copy_cmd.copy_buffer(cpu_buf, 0, gpu_buf, 0, buf.data.size());
			copy_cmd.resource_barrier(
				{},
				{
					gfx::buffer_barrier::create(gpu_buf, gfx::buffer_usage::copy_destination, gfx::buffer_usage::read_only_buffer),
				}
			);
			copy_cmd.finish();
			cmd_queue.submit_command_lists({ &copy_cmd }, &upload_fence);
			dev.wait_for_fence(upload_fence);
			dev.reset_fence(upload_fence);

			buffers.emplace_back(std::move(gpu_buf));
		}
	}


	auto material_set_layout = dev.create_descriptor_set_layout(
		{
			gfx::descriptor_range_binding::create(gfx::descriptor_range::create(gfx::descriptor_type::read_only_image, 1), 0),
			gfx::descriptor_range_binding::create(gfx::descriptor_range::create(gfx::descriptor_type::sampler, 1), 1),
		},
		gfx::shader_stage_mask::pixel_shader
	);
	auto constant_set_layout = dev.create_descriptor_set_layout(
		{
			gfx::descriptor_range_binding::create(gfx::descriptor_range::create(gfx::descriptor_type::read_only_buffer, 2), 0),
			gfx::descriptor_range_binding::create(gfx::descriptor_range::create(gfx::descriptor_type::sampler, 1), 2),
		},
		gfx::shader_stage_mask::vertex_shader
	);
	auto descriptor_pool = dev.create_descriptor_pool(
		{
			gfx::descriptor_range::create(gfx::descriptor_type::read_only_image, 100),
			gfx::descriptor_range::create(gfx::descriptor_type::sampler, 100),
		}, 100
	);
	auto sampler = dev.create_sampler(
		gfx::filtering::linear, gfx::filtering::linear, gfx::filtering::linear, 0, 0, 1, 16,
		gfx::sampler_address_mode::repeat, gfx::sampler_address_mode::repeat, gfx::sampler_address_mode::repeat,
		linear_rgba_f(1.f, 1.f, 1.f, 1.f), std::nullopt
	);

	// shaders
	std::vector<std::byte> vert_shader_code = load_binary_file("test.vs.o"); // TODO
	std::vector<std::byte> pix_shader_code = load_binary_file("test.ps.o"); // TODO
	auto vert_shader = dev.load_shader(vert_shader_code);
	auto pix_shader = dev.load_shader(pix_shader_code);

	auto pipeline_rsrc = dev.create_pipeline_resources({ &material_set_layout, &constant_set_layout });
	auto shaders = gfx::shader_set::create(vert_shader, pix_shader);
	auto blend = gfx::blend_options::create_for_render_targets(
		gfx::render_target_blend_options::disabled()
	);
	auto rasterizer = gfx::rasterizer_options::create(zero, gfx::front_facing_mode::clockwise, gfx::cull_mode::none);
	auto depth_stencil = gfx::depth_stencil_options::create(
		true, true, gfx::comparison_function::less,
		false, 0, 0, gfx::stencil_options::always_pass_no_op(), gfx::stencil_options::always_pass_no_op()
	);

	// create primitive pipelines
	std::vector<std::vector<gfx::pipeline_state>> pipelines;
	pipelines.reserve(model.meshes.size());
	for (const auto &mesh : model.meshes) {
		auto &prim_pipelines = pipelines.emplace_back();
		prim_pipelines.reserve(mesh.primitives.size());
		for (const auto &prim : mesh.primitives) {
			std::vector<gfx::input_buffer_element> vert_buffer_elements;
			std::vector<gfx::input_buffer_layout> vert_buffer_layouts;
			vert_buffer_elements.reserve(prim.attributes.size());
			vert_buffer_layouts.reserve(prim.attributes.size());
			std::size_t i = 0;
			for (auto it = prim.attributes.begin(); it != prim.attributes.end(); ++it, ++i) {
				const auto &attr = *it;
				const auto &accessor = model.accessors[attr.second];
				gfx::input_buffer_element elem = uninitialized;
				if (attr.first == "POSITION") {
					elem = gfx::input_buffer_element::create("POSITION", 0, gfx::format::r32g32b32_float, 0);
				} else if (attr.first == "NORMAL") {
					elem = gfx::input_buffer_element::create("NORMAL", 0, gfx::format::r32g32b32_float, 0);
				} else if (attr.first == "TANGENT") {
					elem = gfx::input_buffer_element::create("TANGENT", 0, gfx::format::r32g32b32a32_float, 0);
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
					elem = gfx::input_buffer_element::create("TEXCOORD", 0, fmt, 0);
				} else {
					std::cerr << "Unhandled vertex buffer element: " << attr.first << std::endl;
					continue;
				}

				vert_buffer_elements.emplace_back(elem);
				vert_buffer_layouts.emplace_back(gfx::input_buffer_layout::create_vertex_buffer(
					{ vert_buffer_elements.end() - 1, vert_buffer_elements.end() }, accessor.ByteStride(model.bufferViews[accessor.bufferView]), i
				));
			}
			auto pipeline = dev.create_pipeline_state(
				pipeline_rsrc, shaders, blend, rasterizer, depth_stencil,
				vert_buffer_layouts, gfx::primitive_topology::triangle_list, pass_resources
			);
			prim_pipelines.emplace_back(std::move(pipeline));
		}
	}

	// buffers for transforms
	struct node_data {
		mat44f transform = uninitialized; ///< Transform of the primitive.
	};
	struct global_data {
		mat44f view = uninitialized; ///< View matrix.
		mat44f projection_view = uninitialized; ///< Projection matrix times view matrix.
	};
	std::size_t node_buffer_size = sizeof(node_data) * model.nodes.size();
	auto node_buffer = dev.create_committed_buffer(
		node_buffer_size, gfx::heap_type::device_only,
		gfx::buffer_usage::mask::copy_destination | gfx::buffer_usage::mask::read_only_buffer, gfx::buffer_usage::copy_destination
	);
	{ // upload data
		auto upload_buf = dev.create_committed_buffer(
			node_buffer_size, gfx::heap_type::upload,
			gfx::buffer_usage::mask::copy_source, gfx::buffer_usage::copy_source
		);
		auto *ptr = static_cast<node_data*>(dev.map_buffer(upload_buf, 0, 0));
		for (const auto &node : model.nodes) {
			new (ptr) node_data();
			if (node.matrix.size() > 0) {
				for (std::size_t y = 0; y < 4; ++y) {
					for (std::size_t x = 0; x < 4; ++x) {
						ptr->transform(y, x) = static_cast<float>(node.matrix[y * 4 + x]);
					}
				}
			} else {
				ptr->transform = mat44f::identity();
			}
			++ptr;
		}
		dev.unmap_buffer(upload_buf, 0, node_buffer_size);

		auto fence = dev.create_fence(gfx::synchronization_state::unset);
		auto list = dev.create_command_list(cmd_alloc);
		list.start();
		list.copy_buffer(upload_buf, 0, node_buffer, 0, node_buffer_size);
		list.resource_barrier(
			{},
			{
				gfx::buffer_barrier::create(node_buffer, gfx::buffer_usage::copy_destination, gfx::buffer_usage::read_only_buffer)
			}
		);
		list.finish();
		cmd_queue.submit_command_lists({ &list }, &fence);
		dev.wait_for_fence(fence);
	}
	std::array<gfx::buffer, 2> global_buffers{
		dev.create_committed_buffer(sizeof(global_data), gfx::heap_type::upload, gfx::buffer_usage::mask::read_only_buffer, gfx::buffer_usage::read_only_buffer),
		dev.create_committed_buffer(sizeof(global_data), gfx::heap_type::upload, gfx::buffer_usage::mask::read_only_buffer, gfx::buffer_usage::read_only_buffer),
	};

	// descriptor sets
	std::vector<gfx::descriptor_set> prim_descriptor_sets;
	{
		prim_descriptor_sets.reserve(model.materials.size());
		for (const auto &mat : model.materials) {
			auto &base_color_tex = images[mat.pbrMetallicRoughness.baseColorTexture.index];
			auto view = dev.create_image2d_view_from(base_color_tex, gfx::format::r8g8b8a8_unorm, gfx::mip_levels::only_highest());
			auto set = dev.create_descriptor_set(descriptor_pool, material_set_layout);
			dev.write_descriptor_set_images(set, material_set_layout, 0, { &view });
			dev.write_descriptor_set_samplers(set, material_set_layout, 1, { &sampler });
			prim_descriptor_sets.emplace_back(std::move(set));
		}
	}
	std::array<gfx::descriptor_set, 2> constant_descriptor_sets{
		dev.create_descriptor_set(descriptor_pool, constant_set_layout),
		dev.create_descriptor_set(descriptor_pool, constant_set_layout),
	};
	for (std::size_t i = 0; i < 2; ++i) {
		dev.write_descriptor_set_buffers(
			constant_descriptor_sets[i], constant_set_layout, 0,
			{
				gfx::buffer_view::create(node_buffer, 0, model.nodes.size(), sizeof(node_data)),
				gfx::buffer_view::create(global_buffers[i], 0, 1, sizeof(global_data)),
			}
		);
	}


	std::array<gfx::command_list, num_swapchain_images> lists{ nullptr, nullptr };
	std::array<gfx::fence, num_swapchain_images> fences{ nullptr, nullptr };
	for (std::size_t i = 0; i < num_swapchain_images; ++i) {
		char8_t buffer[20];
		std::snprintf(reinterpret_cast<char*>(buffer), std::size(buffer), "Back buffer %d", static_cast<int>(i));
		gfx::image2d img = swapchain.get_image(i);
		dev.set_debug_name(img, buffer);

		fences[i] = dev.create_fence(gfx::synchronization_state::set);

		lists[i] = dev.create_command_list(cmd_alloc);
		lists[i].start();

		lists[i].resource_barrier(
			{
				gfx::image_barrier::create(img, gfx::image_usage::present, gfx::image_usage::color_render_target)
			}, {}
		);

		lists[i].begin_pass(pass_resources, frame_buffers[i], { linear_rgba_f(0.0f, 0.0f, 0.0f, 0.0f) }, 1.0f, 0);

		lists[i].set_viewports({ gfx::viewport::create(aab2f::create_from_min_max(zero, cvec2f(800.0f, 600.0f)), 0.0f, 1.0f) });
		lists[i].set_scissor_rectangles({ aab2i::create_from_min_max(zero, cvec2i(800, 600)) });

		for (std::size_t node_i = 0; node_i < model.nodes.size(); ++node_i) {
			const auto &node = model.nodes[node_i];
			const auto &mesh = model.meshes[node.mesh];
			for (std::size_t prim_i = 0; prim_i < mesh.primitives.size(); ++prim_i) {
				const auto &prim = mesh.primitives[prim_i];

				std::vector<gfx::vertex_buffer> vert_buffers;
				vert_buffers.reserve(prim.attributes.size());
				for (const auto &attr : prim.attributes) {
					const auto &accessor = model.accessors[attr.second];
					const auto &view = model.bufferViews[accessor.bufferView];
					vert_buffers.emplace_back(gfx::vertex_buffer::from_buffer_offset_stride(
						buffers[view.buffer], view.byteOffset, accessor.ByteStride(view)
					));
				}

				lists[i].bind_pipeline_state(pipelines[node.mesh][prim_i]);
				lists[i].bind_vertex_buffers(0, vert_buffers);
				lists[i].bind_descriptor_sets(0, { &prim_descriptor_sets[prim.material], &constant_descriptor_sets[i] });

				if (prim.indices < 0) {
					lists[i].draw_instanced(0, model.accessors[prim.attributes.begin()->second].count, node_i, 1);
				} else {
					const auto &index_accessor = model.accessors[prim.indices];
					const auto &index_buf_view = model.bufferViews[index_accessor.bufferView];
					gfx::index_format fmt;
					switch (index_accessor.componentType) {
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
						fmt = gfx::index_format::uint16;
						break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
						fmt = gfx::index_format::uint32;
						break;
					default:
						std::cerr << "Unhandled index buffer format: " << index_accessor.componentType << std::endl;
						continue;
					}
					lists[i].bind_index_buffer(buffers[index_buf_view.buffer], index_buf_view.byteOffset, fmt);
					lists[i].draw_indexed_instanced(0, index_accessor.count, 0, node_i, 1);
				}
			}
		}

		lists[i].end_pass();

		lists[i].resource_barrier(
			{
				gfx::image_barrier::create(img, gfx::image_usage::color_render_target, gfx::image_usage::present)
			},
			{}
		);

		lists[i].finish();
	}

	auto cam_params = camera_parameters<float>::create_look_at(cvec3f(0, 100, 0), cvec3f(500, 100, 0));
	cam_params.far_plane = 10000.0f;
	auto cam = camera<float>::from_parameters(cam_params);

	wnd.show_and_activate();
	while (app.process_message_nonblocking() != sys::message_type::quit) {
		auto back_buffer = swapchain.acquire_back_buffer();

		if (back_buffer.on_presented) {
			dev.wait_for_fence(*back_buffer.on_presented);
			dev.reset_fence(*back_buffer.on_presented);
		}

		cam_params.rotate_around_axis(cvec3f(0.0f, 1.0f, 0.0f), cvec2f(0.001f, 0.0f));
		cam = camera<float>::from_parameters(cam_params);

		auto *data = static_cast<global_data*>(dev.map_buffer(global_buffers[back_buffer.index], 0, 0));
		data->view = cam.view_matrix.into<float>();
		data->projection_view = cam.projection_view_matrix.into<float>();
		dev.unmap_buffer(global_buffers[back_buffer.index], 0, sizeof(global_data));

		cmd_queue.submit_command_lists({ &lists[back_buffer.index] }, nullptr);

		cmd_queue.present(swapchain, &fences[back_buffer.index]);
	}

	return 0;
}
