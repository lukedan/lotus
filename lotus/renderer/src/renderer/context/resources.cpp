#include "lotus/renderer/context/resources.h"

#include "lotus/renderer/context/caching.h"
#include "lotus/renderer/context/resource_bindings.h"

namespace lotus::renderer {
	namespace recorded_resources {
		structured_buffer_view::structured_buffer_view(const renderer::structured_buffer_view &view) :
			basic_handle(view), _stride(view._stride), _first(view._first), _count(view._count) {
		}
	}


	namespace _details {
		gpu::descriptor_type to_descriptor_type(image_binding_type type) {
			constexpr static enums::sequential_mapping<image_binding_type, gpu::descriptor_type> table{
				std::pair(image_binding_type::read_only,  gpu::descriptor_type::read_only_image ),
				std::pair(image_binding_type::read_write, gpu::descriptor_type::read_write_image),
			};
			return table[type];
		}


		pool::token pool::allocate(memory::size_alignment size_align) {
			for (std::size_t i = 0; i < _chunks.size(); ++i) {
				if (auto res = _chunks[i].allocator.allocate(size_align, 0)) {
					if (debug_log_allocations) {
						log().debug(
							"Allocating from pool {}, chunk {}, addr {}, size {}",
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
					log().debug(
						"Allocating from pool {}, chunk {}, addr {}, size {}",
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
				log().debug(
					"Freeing from pool {}, chunk {}, addr {}",
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
		crash_if((first + count) * stride > _ptr->size);
		return structured_buffer_view(_ptr, stride, first, count);
	}


	void swap_chain::resize(cvec2u32 sz) {
		_ptr->desired_size = sz;
	}
}
