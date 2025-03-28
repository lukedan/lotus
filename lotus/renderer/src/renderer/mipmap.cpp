#include "lotus/renderer/mipmap.h"

/// \file
/// Mipmapping implementation.

#include "lotus/renderer/shader_types.h"

namespace lotus::renderer::mipmap {
	generator generator::create(assets::manager &man, context::queue q) {
		return generator(
			q,
			man.compile_shader_in_filesystem(
				man.asset_library_path / "shaders/misc/downsample_mip.hlsl",
				gpu::shader_stage::compute_shader,
				u8"main",
				{}
			)
		);
	}

	void generator::generate_all(image2d_view img) {
		const auto mips = img.get_viewed_mip_levels().into_range_with_count(img.get_num_mip_levels());
		for (u32 i = mips.begin + 1; i < mips.end; ++i) {
			all_resource_bindings rsrc(
				{
					{ 0, {
						{ 0, img.view_mips(gpu::mip_levels::only(i - 1)).bind_as_read_only() },
						{ 1, img.view_mips(gpu::mip_levels::only(i)).bind_as_read_write() },
					} },
				},
				{}
			);

			auto size = (img.get_size() / 2u).into<u32>();
			// TODO better description
			_q.run_compute_shader_with_thread_dimensions(
				_shader, cvec3u32(size[0], size[1], 1u), std::move(rsrc), u8"Generate mip"
			);
		}
	}
}
