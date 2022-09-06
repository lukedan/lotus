#include "lotus/renderer/mipmap.h"

/// \file
/// Mipmapping implementation.

#include "lotus/renderer/shader_types.h"

namespace lotus::renderer::mipmap {
	generator generator::create(assets::manager &man) {
		return generator(
			man.get_context(),
			man.compile_shader_in_filesystem(
				man.get_shader_library_path() / "downsample_mip.hlsl",
				gpu::shader_stage::compute_shader,
				u8"main",
				{}
			)
		);
	}

	void generator::generate_all(image2d_view img) {
		auto mips = gpu::mip_levels::intersection(
			gpu::mip_levels::create(0, static_cast<std::uint16_t>(img.get_num_mip_levels())),
			img.get_viewed_mip_levels()
		);
		for (std::uint16_t i = mips.minimum + 1; i < mips.maximum; ++i) {
			cvec3u32 size(img.get_size()[0] >> 1, img.get_size()[1] >> 1, 1);

			auto rsrc = all_resource_bindings::from_unsorted({
				resource_set_binding::descriptor_bindings({
					resource_binding(descriptor_resource::image2d::create_read_only(
						img.view_mips(gpu::mip_levels::only(i - 1))
					), 0),
					resource_binding(descriptor_resource::image2d::create_read_write(
						img.view_mips(gpu::mip_levels::only(i))
					), 1),
				}).at_space(0),
			});

			// TODO better description
			_ctx.run_compute_shader_with_thread_dimensions(_shader, size, std::move(rsrc), u8"Generate mip");
		}
	}
}
