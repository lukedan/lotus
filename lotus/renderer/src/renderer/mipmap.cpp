#include "lotus/renderer/mipmap.h"

/// \file
/// Mipmapping implementation.

#include "lotus/renderer/shader_types.h"

namespace lotus::renderer::mipmap {
	generator generator::create(assets::manager &man) {
		return generator(
			man.get_context(),
			man.compile_shader_in_filesystem(
				man.shader_library_path / "utils/downsample_mip.hlsl",
				gpu::shader_stage::compute_shader,
				u8"main",
				{}
			)
		);
	}

	void generator::generate_all(image2d_view img) {
		auto mips = gpu::mip_levels::intersection(
			gpu::mip_levels::create(0, img.get_num_mip_levels()),
			img.get_viewed_mip_levels()
		);
		for (std::uint32_t i = mips.minimum + 1; i < mips.maximum; ++i) {
			all_resource_bindings rsrc(
				{
					{ 0, {
						{ 0, img.view_mips(gpu::mip_levels::only(i - 1)).bind_as_read_only() },
						{ 1, img.view_mips(gpu::mip_levels::only(i)).bind_as_read_write() },
					} },
				},
				{}
			);

			auto size = (img.get_size() / 2).into<std::uint32_t>();
			// TODO better description
			_ctx.run_compute_shader_with_thread_dimensions(
				_shader, cvec3u32(size[0], size[1], 1), std::move(rsrc), u8"Generate mip"
			);
		}
	}
}
