#include "lotus/renderer/resources.h"

#include "lotus/renderer/caching.h"
#include "lotus/renderer/resource_bindings.h"

namespace lotus::renderer {
	namespace recorded_resources {
		image2d_view::image2d_view(const renderer::image2d_view &view) :
			_surface(view._surface.get()), _view_format(view._view_format), _mip_levels(view._mip_levels) {
		}

		image2d_view image2d_view::highest_mip_with_warning() const {
			image2d_view result = *this;
			if (result._mip_levels.get_num_levels() != 1) {
				if (result._surface->num_mips - result._mip_levels.minimum > 1) {
					auto num_levels =
						std::min<std::uint32_t>(result._surface->num_mips, result._mip_levels.maximum) -
						result._mip_levels.minimum;
					log().error<u8"More than one ({}) mip specified for render target for texture {}">(
						num_levels, string::to_generic(result._surface->name)
					);
				}
				result._mip_levels = gpu::mip_levels::only(result._mip_levels.minimum);
			}
			return result;
		}


		buffer::buffer(const renderer::buffer &buf) : _buffer(buf._buffer.get()) {
		}


		swap_chain::swap_chain(const renderer::swap_chain &c) : _swap_chain(c._swap_chain.get()) {
		}


		descriptor_array::descriptor_array(const renderer::descriptor_array &arr) : _array(arr._array.get()) {
		}


		blas::blas(const renderer::blas &b) : _blas(b._blas.get()) {
		}


		tlas::tlas(const renderer::tlas &b) : _tlas(b._tlas.get()) {
		}
	}


	namespace _details {
		gpu::descriptor_type to_descriptor_type(image_binding_type type) {
			constexpr static enum_mapping<image_binding_type, gpu::descriptor_type> table{
				std::pair(image_binding_type::read_only,  gpu::descriptor_type::read_only_image ),
				std::pair(image_binding_type::read_write, gpu::descriptor_type::read_write_image),
			};
			return table[type];
		}


		cache_keys::descriptor_set_layout descriptor_array::get_layout_key() const {
			return cache_keys::descriptor_set_layout(
				{ gpu::descriptor_range_binding::create_unbounded(type, 0), },
				descriptor_set_type::variable_descriptor_count
			);
		}
	}


	namespace assets {
		input_buffer_binding geometry::input_buffer::into_input_buffer_binding(
			const char8_t *semantic, std::uint32_t semantic_index, std::uint32_t binding_index
		) const {
			return input_buffer_binding(
				binding_index, data->data, offset, stride, gpu::input_buffer_rate::per_vertex,
				{ gpu::input_buffer_element(semantic, semantic_index, format, 0) }
			);
		}
	}


	void swap_chain::resize(cvec2s sz) {
		_swap_chain->desired_size = sz;
	}
}
