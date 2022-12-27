#include "lotus/renderer/context/resources.h"

#include "lotus/renderer/context/caching.h"
#include "lotus/renderer/context/resource_bindings.h"

namespace lotus::renderer {
	namespace recorded_resources {
		image2d_view::image2d_view(const renderer::image2d_view &view) :
			_image(view._ptr.get()), _view_format(view._view_format), _mip_levels(view._mip_levels) {
		}

		image2d_view image2d_view::highest_mip_with_warning() const {
			image2d_view result = *this;
			if (result._mip_levels.get_num_levels() != 1) {
				if (result._image->num_mips - result._mip_levels.minimum > 1) {
					auto num_levels =
						std::min<std::uint32_t>(result._image->num_mips, result._mip_levels.maximum) -
						result._mip_levels.minimum;
					log().error<u8"More than one ({}) mip specified for render target for texture {}">(
						num_levels, string::to_generic(result._image->name)
					);
				}
				result._mip_levels = gpu::mip_levels::only(result._mip_levels.minimum);
			}
			return result;
		}


		structured_buffer_view::structured_buffer_view(const renderer::structured_buffer_view &view) :
			_buffer(view._ptr.get()), _stride(view._stride), _first(view._first), _count(view._count) {
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


		pool::token pool::allocate(memory::size_alignment size_align) {
			for (std::size_t i = 0; i < _chunks.size(); ++i) {
				if (auto res = _chunks[i].allocator.allocate(size_align, 0)) {
					if (debug_log_allocations) {
						log().debug<u8"Allocating from pool {}, chunk {}, addr {}, size {}">(
							string::to_generic(name), i, res->first, size_align.size
						);
					}
					return token(static_cast<std::uint32_t>(i), static_cast<std::uint32_t>(res->first));
				}
			}
			std::size_t index = _chunks.size();
			auto new_chunk_size = chunk_size;
			while (new_chunk_size < size_align.size) {
				new_chunk_size *= 2;
			}
			auto &chk = _chunks.emplace_back(
				allocate_memory(new_chunk_size), _chunk::allocator_t::create(new_chunk_size)
			);
			if (auto res = chk.allocator.allocate(size_align, 0)) {
				if (debug_log_allocations) {
					log().debug<u8"Allocating from pool {}, chunk {}, addr {}, size {}">(
						string::to_generic(name), index, res->first, size_align.size
					);
				}
				return token(static_cast<std::uint32_t>(index), static_cast<std::uint32_t>(res->first));
			}
			crash_if(true); // don't know how this could happen
			return nullptr;
		}

		void pool::free(token tok) {
			crash_if(tok._chunk_index >= _chunks.size());
			_chunks[tok._chunk_index].allocator.free(tok._address);
			if (debug_log_allocations) {
				log().debug<u8"Freeing from pool {}, chunk {}, addr {}">(
					string::to_generic(name), tok._chunk_index, tok._address
				);
			}
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


	structured_buffer_view buffer::get_view(
		std::uint32_t stride, std::uint32_t first, std::uint32_t count
	) const {
		return structured_buffer_view(_ptr, stride, first, count);
	}


	void swap_chain::resize(cvec2s sz) {
		_ptr->desired_size = sz;
	}
}
