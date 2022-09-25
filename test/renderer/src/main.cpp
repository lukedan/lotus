#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>

#include <tiny_gltf.h>

#include <lotus/math/vector.h>
#include <lotus/utils/camera.h>
#include <lotus/utils/misc.h>

#include <lotus/system/window.h>
#include <lotus/system/application.h>

#include <lotus/gpu/context.h>
#include <lotus/gpu/commands.h>
#include <lotus/gpu/descriptors.h>

#include <lotus/renderer/asset_manager.h>
#include <lotus/renderer/g_buffer.h>
#include <lotus/renderer/mipmap.h>

#include <lotus/renderer/loaders/gltf_loader.h>
#include <lotus/renderer/loaders/fbx_loader.h>

#include "common.h"
/*#include "scene.h"
/*#include "gbuffer_pass.h"
#include "composite_pass.h"
#include "raytrace_pass.h"
#include "rt_resolve_pass.h"*/

#include <lotus/renderer/shader_types_include_wrapper.h>
namespace shader_types {
#include "shaders/types.hlsli"
}
#include <lotus/renderer/shader_types_include_wrapper.h>

int main(int argc, char **argv) {
	if (argc < 2) {
		std::cout << "No model file specified\n";
		return 1;
	}

	std::cout << "Backend: " << std::string_view(reinterpret_cast<const char*>(lgpu::backend_name.data()), lgpu::backend_name.size()) << "\n";
	std::cout << "Working dir: " << std::filesystem::current_path() << "\n";

	lsys::application app(u8"test");
	auto wnd = app.create_window();

	auto gctx_options = true ? lgpu::context_options::enable_validation : lgpu::context_options::none;
	auto gctx = lgpu::context::create(gctx_options);
	auto shader_util = lgpu::shader_utility::create();
	lgpu::device gdev = nullptr;
	lgpu::adapter_properties dev_prop = uninitialized;
	gctx.enumerate_adapters([&](lgpu::adapter adap) {
		dev_prop = adap.get_properties();
		log().debug<u8"Device name: {}">(lstr::to_generic(dev_prop.name));
		if (dev_prop.is_discrete) {
			log().debug<u8"Selected">();
			gdev = adap.create_device();
			return false;
		}
		return true;
	});
	auto cmd_queue = gdev.create_command_queue();
	auto cmd_alloc = gdev.create_command_allocator();

	auto rctx = lren::context::create(gctx, dev_prop, gdev, cmd_queue);
	auto asset_man = lren::assets::manager::create(rctx, gdev, "D:/Documents/Projects/lotus/lotus/renderer/include/lotus/renderer/shaders", &shader_util);
	auto mip_gen = lren::mipmap::generator::create(asset_man);
	lren::gltf::context gltf_ctx(asset_man);
	auto fbx_ctx = lren::fbx::context::create(asset_man);

	// model & resources
	std::vector<lren::instance> instances;
	std::vector<shader_types::instance_data> instance_data;
	std::vector<lren::blas_reference> tlas_instances;
	std::vector<lren::assets::handle<lren::assets::material>> material_assets;

	lren::buffer_descriptor_array vertex_buffers = rctx.request_buffer_descriptor_array(u8"Vertex buffers", lgpu::descriptor_type::read_only_buffer, 16384);
	lren::buffer_descriptor_array normal_buffers = rctx.request_buffer_descriptor_array(u8"Normal buffers", lgpu::descriptor_type::read_only_buffer, 16384);
	lren::buffer_descriptor_array tangent_buffers = rctx.request_buffer_descriptor_array(u8"Tangent buffers", lgpu::descriptor_type::read_only_buffer, 16384);
	lren::buffer_descriptor_array uv_buffers = rctx.request_buffer_descriptor_array(u8"UV buffers", lgpu::descriptor_type::read_only_buffer, 16384);
	lren::buffer_descriptor_array index_buffers = rctx.request_buffer_descriptor_array(u8"Index buffers", lgpu::descriptor_type::read_only_buffer, 16384);
	std::uint32_t buffer_alloc = 0;
	std::uint32_t index_alloc = 0;
	std::vector<shader_types::geometry_data> geometries;
	std::vector<shader_types::material_data> materials;
	std::vector<lren::blas> blases;

	auto on_texture_loaded = [&](lren::assets::handle<lren::assets::texture2d> tex) {
		/*mip_gen.generate_all(tex->image);*/
	};
	auto on_geometry_loaded = [&](lren::assets::handle<lren::assets::geometry> geom) {
		geom.user_data() = reinterpret_cast<void*>(blases.size());

		auto &blas = blases.emplace_back(rctx.request_blas(
			geom.get().get_id().subpath, { geom->get_geometry_buffers_view() }
		));
		rctx.build_blas(blas, u8"Build BLAS");

		auto &inst = geometries.emplace_back();
		if (geom->index_buffer) {
			inst.index_buffer = index_alloc++;
			rctx.write_buffer_descriptors(index_buffers, inst.index_buffer, {
				geom->index_buffer->data.get_view(
					geom->index_format == lgpu::index_format::uint16 ?
						sizeof(std::uint16_t) : sizeof(std::uint32_t),
					geom->index_offset,
					geom->num_indices
				)
			});
		}
		inst.vertex_buffer = inst.normal_buffer = inst.tangent_buffer = inst.uv_buffer = buffer_alloc++;
		rctx.write_buffer_descriptors(vertex_buffers, inst.vertex_buffer, {
			geom->vertex_buffer.data->data.get_view(
				geom->vertex_buffer.stride, geom->vertex_buffer.offset, geom->num_vertices
			)
		});
		if (geom->normal_buffer.data) {
			rctx.write_buffer_descriptors(normal_buffers, inst.normal_buffer, {
				geom->normal_buffer.data->data.get_view(
					geom->normal_buffer.stride, geom->normal_buffer.offset, geom->num_vertices
				)
			});
		}
		if (geom->tangent_buffer.data) {
			rctx.write_buffer_descriptors(tangent_buffers, inst.tangent_buffer, {
				geom->tangent_buffer.data->data.get_view(
					geom->tangent_buffer.stride, geom->tangent_buffer.offset, geom->num_vertices
				)
			});
		}
		if (geom->uv_buffer.data) {
			rctx.write_buffer_descriptors(uv_buffers, inst.uv_buffer, {
				geom->uv_buffer.data->data.get_view(
					geom->uv_buffer.stride, geom->uv_buffer.offset, geom->num_vertices
				)
			});
		}
	};
	auto on_material_loaded = [&](lren::assets::handle<lren::assets::material> mat) {
		mat.user_data() = reinterpret_cast<void*>(static_cast<std::uintptr_t>(materials.size()));
		auto &mat_data = materials.emplace_back();
		if (auto *data = dynamic_cast<lren::gltf::material_data*>(mat->data.get())) {
			std::uint32_t invalid_tex = asset_man.get_invalid_texture()->descriptor_index;
			mat_data.base_color_index = invalid_tex;
			mat_data.metallic_roughness_index = invalid_tex;
			mat_data.normal_index = invalid_tex;
			if (data->albedo_texture) {
				mat_data.base_color_index = data->albedo_texture->descriptor_index;
			}
			if (data->properties_texture) {
				mat_data.metallic_roughness_index = data->properties_texture->descriptor_index;
			}
			if (data->normal_texture) {
				mat_data.normal_index = data->normal_texture->descriptor_index;
			}
			mat_data.is_metallic_roughness = true; // TODO
			mat_data.base_color = data->properties.albedo_multiplier;
			mat_data.normal_scale = data->properties.normal_scale;
			mat_data.metalness = data->properties.metalness_multiplier;
			mat_data.roughness = data->properties.roughness_multiplier;
		}
		material_assets.emplace_back(std::move(mat));
	};
	auto on_instance_loaded = [&](lren::instance inst) {
		if (inst.geometry) {
			if (inst.geometry.get().get_id().subpath.find(u8"decal") != std::u8string::npos) {
				return;
			}

			auto geom_index = reinterpret_cast<std::uintptr_t>(inst.geometry.user_data());
			auto mat_index = reinterpret_cast<std::uintptr_t>(inst.material.user_data());
			auto inst_index = instances.size();
			tlas_instances.emplace_back(
				blases[geom_index], inst.transform, inst_index, 0xFF, inst.geometry->index_buffer ? 0 : 1
			);
			instances.emplace_back(std::move(inst));
			auto &gpu_inst = instance_data.emplace_back();
			gpu_inst.geometry_index = geom_index;
			gpu_inst.material_index = mat_index;
		}
	};

	for (int i = 1; i < argc; ++i) {
		std::filesystem::path path = argv[i];
		if (path.extension() == ".gltf") {
			gltf_ctx.load(
				argv[i],
				[&](lren::assets::handle<lren::assets::texture2d> tex) {
					on_texture_loaded(std::move(tex));
				},
				[&](lren::assets::handle<lren::assets::geometry> geom) {
					on_geometry_loaded(std::move(geom));
				},
				[&](lren::assets::handle<lren::assets::material> mat) {
					on_material_loaded(std::move(mat));
				},
				[&](lren::instance inst) {
					on_instance_loaded(std::move(inst));
				}
			);
		} else if (path.extension() == ".fbx") {
			fbx_ctx.load(
				argv[i],
				[&](lren::assets::handle<lren::assets::texture2d> tex) {
					on_texture_loaded(std::move(tex));
				},
				[&](lren::assets::handle<lren::assets::geometry> geom) {
					on_geometry_loaded(std::move(geom));
				},
				[&](lren::assets::handle<lren::assets::material> mat) {
					on_material_loaded(std::move(mat));
				},
				[&](lren::instance inst) {
					on_instance_loaded(std::move(inst));
				}
			);
		} else {
			log().error<u8"Unknown file type: {}">(path.string());
		}
	}
	auto tlas = rctx.request_tlas(u8"TLAS", tlas_instances);
	rctx.build_tlas(tlas, u8"Build TLAS");

	auto geom_buf = rctx.request_buffer(
		u8"Geometry buffer",
		sizeof(shader_types::geometry_data) * geometries.size(),
		lgpu::buffer_usage_mask::copy_destination | lgpu::buffer_usage_mask::shader_read_only
	);
	rctx.upload_buffer<shader_types::geometry_data>(geom_buf, { geometries.begin(), geometries.end() }, 0, u8"Upload geometry buffer");
	auto geom_structured_buf = geom_buf.get_view<shader_types::geometry_data>(0, geometries.size());

	auto mat_buf = rctx.request_buffer(
		u8"Material buffer",
		sizeof(shader_types::material_data) * materials.size(),
		lgpu::buffer_usage_mask::copy_destination | lgpu::buffer_usage_mask::shader_read_only
	);
	rctx.upload_buffer<shader_types::material_data>(mat_buf, { materials.begin(), materials.end() }, 0, u8"Upload material buffer");
	auto mat_structured_buf = mat_buf.get_view<shader_types::material_data>(0, materials.size());

	auto inst_buf = rctx.request_buffer(
		u8"Instance buffer",
		sizeof(shader_types::instance_data) * instance_data.size(),
		lgpu::buffer_usage_mask::copy_destination | lgpu::buffer_usage_mask::shader_read_only
	);
	rctx.upload_buffer<shader_types::instance_data>(inst_buf, { instance_data.begin(), instance_data.end() }, 0, u8"Upload instance buffer");
	auto inst_structured_buf = inst_buf.get_view<shader_types::instance_data>(0, instance_data.size());

	auto rt_shader = asset_man.compile_shader_library_in_filesystem("src/shaders/raytracing.hlsl", {});

	/*composite_pass comp_pass(dev);*/

	/*raytrace_pass rt_pass(dev, model_resources, dev_prop);
	raytrace_resolve_pass rt_resolve_pass(dev);*/

	/*composite_pass::input_resources comp_input;
	std::vector<composite_pass::output_resources> comp_output;*/

	// raytracing buffers
	/*lgpu::image2d raytrace_buffer = nullptr;
	lgpu::image2d_view raytrace_buffer_view = nullptr;
	raytrace_pass::input_resources rt_input;

	raytrace_resolve_pass::input_resources rt_resolve_input;
	std::vector<raytrace_resolve_pass::output_resources> rt_resolve_output;*/

	auto blit_vs = asset_man.compile_shader_in_filesystem(
		asset_man.get_shader_library_path() / "fullscreen_quad_vs.hlsl", lgpu::shader_stage::vertex_shader, u8"main_vs", {}
	);
	auto blit_ps = asset_man.compile_shader_in_filesystem(
		asset_man.get_shader_library_path() / "blit_ps.hlsl", lgpu::shader_stage::pixel_shader, u8"main_ps", {}
	);

	auto resolve_ps = asset_man.compile_shader_in_filesystem("src/shaders/rt_resolve.hlsl", lgpu::shader_stage::pixel_shader, u8"main_ps", {});

	auto swap_chain = rctx.request_swap_chain(
		u8"Main swap chain", wnd, 2, { lgpu::format::r8g8b8a8_srgb, lgpu::format::b8g8r8a8_srgb }
	);
	std::size_t frame_index = 0;

	lren::image2d_view rt_result = nullptr;

	auto cam_params = lotus::camera_parameters<float>::create_look_at(cvec3f(0.0f, 10.0f, 0.0f), cvec3f(50.0f, 10.0f, 0.0f));
	{
		auto size = wnd.get_size();
		cam_params.far_plane = 4000.0f;
		cam_params.aspect_ratio = size[0] / static_cast<float>(size[1]);
	}
	auto cam = cam_params.into_camera();
	bool is_rotating = false;
	bool is_zooming = false;
	bool is_moving = false;
	cvec2i prev_mouse = zero;
	cvec2s window_size = zero;

	auto on_resize = [&](lsys::window&, lsys::window_events::resize &info) {
		window_size = info.new_size;
		frame_index = 0;
		swap_chain.resize(info.new_size);
		cam_params.aspect_ratio = info.new_size[0] / static_cast<float>(info.new_size[1]);
		rt_result = rctx.request_image2d(
			u8"Raytracing result", window_size, 1, lgpu::format::r32g32b32a32_float,
			lgpu::image_usage_mask::shader_read_only | lgpu::image_usage_mask::shader_read_write
		);
	};
	auto on_mouse_move = [&](lsys::window&, lsys::window_events::mouse::move &move) {
		cvec2f offset = (move.new_position - prev_mouse).into<float>();
		offset[0] = -offset[0];
		if (is_rotating) {
			cam_params.rotate_around_world_up(offset * 0.004f);
			frame_index = 0;
		}
		if (is_zooming) {
			cvec3f cam_offset = cam_params.position - cam_params.look_at;
			cam_offset *= std::exp(-0.005f * offset[1]);
			cam_params.position = cam_params.look_at + cam_offset;
			frame_index = 0;
		}
		if (is_moving) {
			cvec3f x = cam.unit_right * offset[0];
			cvec3f y = cam.unit_up * offset[1];
			float distance = (cam_params.position - cam_params.look_at).norm() * 0.001f;
			cvec3f cam_off = (x + y) * distance;
			cam_params.position += cam_off;
			cam_params.look_at += cam_off;
			frame_index = 0;
		}
		prev_mouse = move.new_position;
	};
	auto on_mouse_down = [&](lsys::window &wnd, lsys::window_events::mouse::button_down &down) {
		switch (down.button) {
		case lsys::mouse_button::primary:
			is_rotating = true;
			break;
		case lsys::mouse_button::secondary:
			is_zooming = true;
			break;
		case lsys::mouse_button::middle:
			is_moving = true;
			break;
		}
		wnd.acquire_mouse_capture();
	};
	auto on_mouse_up = [&](lsys::window &wnd, lsys::window_events::mouse::button_up &up) {
		switch (up.button) {
		case lsys::mouse_button::primary:
			is_rotating = false;
			break;
		case lsys::mouse_button::secondary:
			is_zooming = false;
			break;
		case lsys::mouse_button::middle:
			is_moving = false;
			break;
		}
		if (!is_rotating && !is_zooming && !is_moving) {
			wnd.release_mouse_capture();
		}
	};
	auto on_capture_broken = [&](lsys::window&) {
		is_rotating = is_zooming = is_moving = false;
	};

	auto size_node = wnd.on_resize.create_linked_node(
		[&](lsys::window &wnd, lsys::window_events::resize &info) {
			on_resize(wnd, info);
		}
	);
	auto mouse_move_node = wnd.on_mouse_move.create_linked_node(
		[&](lsys::window &wnd, lsys::window_events::mouse::move &move) {
			on_mouse_move(wnd, move);
		}
	);
	auto mouse_down_node = wnd.on_mouse_button_down.create_linked_node(
		[&](lsys::window &wnd, lsys::window_events::mouse::button_down &down) {
			on_mouse_down(wnd, down);
		}
	);
	auto mouse_up_node = wnd.on_mouse_button_up.create_linked_node(
		[&](lsys::window &wnd, lsys::window_events::mouse::button_up &up) {
			on_mouse_up(wnd, up);
		}
	);
	auto capture_broken_node = wnd.on_capture_broken.create_linked_node(
		[&](lsys::window &wnd) {
			on_capture_broken(wnd);
		}
	);
	auto quit_node = wnd.on_close_request.create_linked_node(
		[&](lsys::window&, lsys::window_events::close_request &req) {
			req.should_close = true;
			app.quit();
		}
	);


	wnd.show_and_activate();
	while (app.process_message_nonblocking() != lsys::message_type::quit) {
		if (window_size == zero) {
			continue;
		}

		auto start = std::chrono::high_resolution_clock::now();

		{
			asset_man.update();

			cam = cam_params.into_camera();

			/*auto gbuffer = lren::g_buffer::view::create(rctx, window_size);
			{
				auto pass = gbuffer.begin_pass(rctx);
				lren::g_buffer::render_instances(pass, asset_man, instances, cam.view_matrix, cam.projection_matrix);
				pass.end();
			}*/

			float tan_half_fovy = tan(cam_params.fov_y_radians * 0.5);
			auto right_half = cam.unit_right * tan_half_fovy * cam_params.aspect_ratio;
			auto up_half = cam.unit_up * tan_half_fovy;

			shader_types::global_data globals;
			globals.camera_position = cam_params.position;
			globals.t_min = 0.001f;
			globals.top_left = cam.unit_forward - right_half + up_half;
			globals.t_max = 1000.0f;
			globals.right = right_half / (window_size[0] * 0.5f);
			globals.down = -up_half / (window_size[1] * 0.5f);
			globals.frame_index = frame_index;

			{
				auto resources = lren::all_resource_bindings::from_unsorted(
					{
						lren::resource_set_binding::descriptor_bindings({
							lren::resource_binding(lren::descriptor_resource::tlas(tlas), 0),
							lren::resource_binding(lren::descriptor_resource::immediate_constant_buffer::create_for(globals), 1),
							lren::resource_binding(lren::descriptor_resource::image2d::create_read_write(rt_result), 2),
							lren::resource_binding(lren::descriptor_resource::sampler(), 3),
						}).at_space(0),
						lren::resource_set_binding(asset_man.get_images(), 1),
						lren::resource_set_binding(vertex_buffers, 2),
						lren::resource_set_binding(normal_buffers, 3),
						lren::resource_set_binding(tangent_buffers, 4),
						lren::resource_set_binding(uv_buffers, 5),
						lren::resource_set_binding(index_buffers, 6),
						lren::resource_set_binding::descriptor_bindings({
							lren::resource_binding(lren::descriptor_resource::structured_buffer::create_read_only(inst_structured_buf), 0),
							lren::resource_binding(lren::descriptor_resource::structured_buffer::create_read_only(geom_structured_buf), 1),
							lren::resource_binding(lren::descriptor_resource::structured_buffer::create_read_only(mat_structured_buf), 2),
						}).at_space(7),
					}
				);
				rctx.trace_rays(
					{
						lren::shader_function(rt_shader, u8"main_anyhit_indexed", lgpu::shader_stage::any_hit_shader),
						lren::shader_function(rt_shader, u8"main_anyhit_unindexed", lgpu::shader_stage::any_hit_shader),
						lren::shader_function(rt_shader, u8"main_closesthit_indexed", lgpu::shader_stage::closest_hit_shader),
						lren::shader_function(rt_shader, u8"main_closesthit_unindexed", lgpu::shader_stage::closest_hit_shader),
					},
					{
						lgpu::hit_shader_group(2, 0),
						lgpu::hit_shader_group(3, 1),
					},
					{
						lren::shader_function(rt_shader, u8"main_raygen", lgpu::shader_stage::ray_generation_shader),
						lren::shader_function(rt_shader, u8"main_miss", lgpu::shader_stage::miss_shader),
					},
					2, { 3 }, { 0, 1 },
					20, 32, 32,
					cvec3u32(window_size[0], window_size[1], 1),
					std::move(resources),
					u8"Trace rays"
				);
			}

			/*{
				auto *data = static_cast<gbuffer_pass::constants*>(dev.map_buffer(gbuf_input.constant_buffer, 0, 0));
				data->view = cam.view_matrix.into<float>();
				data->projection_view = cam.projection_view_matrix.into<float>();
				dev.unmap_buffer(gbuf_input.constant_buffer, 0, sizeof(gbuffer_pass::constants));
			}

			{
				auto *data = static_cast<raytrace_pass::global_data*>(dev.map_buffer(rt_input.constant_buffer, 0, 0));
				data->camera_position = cam_params.position;
				data->t_min = 0.001f;
				float tan_half_y = std::tan(0.5f * cam_params.fov_y_radians);
				auto x = cam.unit_right * cam_params.aspect_ratio * tan_half_y;
				auto y = cam.unit_up * tan_half_y;
				data->top_left = cam.unit_forward - x + y;
				data->t_max = 10000.0f;
				data->right = matf::concat_rows(x * (2.0f / rt_input.output_size[0]), column_vector<1, float>(0.0f));
				data->down = y * (-2.0f / rt_input.output_size[1]);
				data->frame_index = frame_index;
				dev.unmap_buffer(rt_input.constant_buffer, 0, sizeof(raytrace_pass::global_data));
			}

			{
				auto *data = static_cast<raytrace_resolve_pass::global_data*>(dev.map_buffer(rt_resolve_input.globals_buffer, 0, 0));
				data->frame_index = frame_index;
				dev.unmap_buffer(rt_resolve_input.globals_buffer, 0, sizeof(raytrace_resolve_pass::global_data));
			}*/

			/*if constexpr (0) {
				gbuf_pass.record_commands(cmd_list, gbuf, model, model_resources, gbuf_input, gbuf_output);
				comp_pass.record_commands(cmd_list, image, comp_input, comp_output[back_buffer.index]);
			} else {
				rt_pass.record_commands(cmd_list, model, model_resources, rt_input, raytrace_buffer);
				rt_resolve_pass.record_commands(cmd_list, image, raytrace_buffer, rt_resolve_input, rt_resolve_output[back_buffer.index]);
			}*/

			{
				auto pass = rctx.begin_pass({
					lren::surface2d_color(
						swap_chain,
						lgpu::color_render_target_access::create_clear(cvec4d(0.0f, 0.0f, 0.0f, 0.0f))
					)
				}, nullptr, window_size, u8"Final blit");
				lren::graphics_pipeline_state state(
					{
						lgpu::render_target_blend_options::disabled()
					},
					nullptr,
					nullptr
				);
				pass.draw_instanced(
					{}, 3, nullptr, 0, lgpu::primitive_topology::triangle_list,
					lren::all_resource_bindings::from_unsorted({
						lren::resource_set_binding::descriptor_bindings({
							lren::resource_binding(lren::descriptor_resource::image2d(
								rt_result, lren::image_binding_type::read_only
							), 0),
							lren::resource_binding(lren::descriptor_resource::sampler(), 1),
							lren::resource_binding(lren::descriptor_resource::immediate_constant_buffer::create_for(globals), 2)
						}).at_space(0),
					}),
					blit_vs, resolve_ps, state, 1, u8"Final blit"
				);
				pass.end();
			}

			rctx.present(swap_chain, u8"Present");
		}

		rctx.flush();

		++frame_index;

		auto end = std::chrono::high_resolution_clock::now();

		log().debug<"CPU frame {}: {} ms">(frame_index, std::chrono::duration<float, std::milli>(end - start).count());
	}

	return 0;
}
