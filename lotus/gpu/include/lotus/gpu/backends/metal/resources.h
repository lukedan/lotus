#pragma once

/// \file
/// Metal buffers and textures.

#include <cstddef>

#include "details.h"

namespace lotus::gpu::backends::metal {
	class command_list;
	class device;
	class swap_chain;

	/// Holds a \p MTL::Heap.
	class memory_block {
		friend device;
	private:
		NS::SharedPtr<MTL::Heap> _heap; ///< The heap.

		/// Initializes \ref _heap.
		explicit memory_block(NS::SharedPtr<MTL::Heap> heap) : _heap(std::move(heap)) {
		}
	};

	/// Holds a \p MTL::Buffer.
	class buffer {
		friend command_list;
		friend device;
	protected:
		/// Initializes this object to empty.
		buffer(std::nullptr_t) {
		}

		/// Checks if this object is valid.
		[[nodiscard]] bool is_valid() const {
			return !!_buf;
		}
	private:
		NS::SharedPtr<MTL::Buffer> _buf; ///< The buffer.

		/// Initializes \ref _buf.
		explicit buffer(NS::SharedPtr<MTL::Buffer> buf) : _buf(std::move(buf)) {
		}
	};

	namespace _details {
		/// Base class for images that holds a \p MTL::Texture.
		class basic_image_base : public gpu::image_base {
			friend command_list;
			friend device;
		protected:
			/// Initializes this object to empty.
			basic_image_base(std::nullptr_t) {
			}

			/// Checks if this object is valid.
			[[nodiscard]] bool is_valid() const {
				return !!_tex;
			}

			/// Initializes \ref _tex.
			explicit basic_image_base(NS::SharedPtr<MTL::Texture> tex) : _tex(std::move(tex)) {
			}
		private:
			NS::SharedPtr<MTL::Texture> _tex; ///< The texture.
		};

		/// Base class for image views that holds a \p MTL::Texture created using \p MTL::Texture::newTextureView().
		class basic_image_view_base : public gpu::image_view_base {
			friend device;
		protected:
			/// Initializes this object to empty.
			basic_image_view_base(std::nullptr_t) {
			}

			/// Checks if this object is valid.
			[[nodiscard]] bool is_valid() const {
				return !!_tex;
			}

			/// Initializes \ref _tex.
			explicit basic_image_view_base(NS::SharedPtr<MTL::Texture> tex) : _tex(std::move(tex)) {
			}
		private:
			NS::SharedPtr<MTL::Texture> _tex; ///< The texture.
		};
	}

	/// A typed texture that inherits from \ref _details::basic_image_base.
	template <image_type Type> class basic_image : public _details::basic_image_base {
		friend device;
		friend swap_chain;
	protected:
		/// Initializes this object to empty.
		basic_image(std::nullptr_t) : basic_image_base(nullptr) {
		}
	private:
		/// Initializes the base class.
		explicit basic_image(NS::SharedPtr<MTL::Texture> tex) : basic_image_base(std::move(tex)) {
		}
	};
	using image2d = basic_image<image_type::type_2d>; ///< 2D images.
	using image3d = basic_image<image_type::type_3d>; ///< 3D images.

	/// Holds a \p MTL::Texture created using \p MTL::Texture::newTextureView().
	template <image_type Type> class basic_image_view : public _details::basic_image_view_base {
		friend device;
	protected:
		/// Initializes this object to empty.
		basic_image_view(std::nullptr_t) : basic_image_view_base(nullptr) {
		}
	private:
		/// Initializes the base class.
		explicit basic_image_view(NS::SharedPtr<MTL::Texture> tex) : basic_image_view_base(std::move(tex)) {
		}
	};
	using image2d_view = basic_image_view<image_type::type_2d>; ///< 2D image views.
	using image3d_view = basic_image_view<image_type::type_3d>; ///< 3D image views.

	/// Holds a \p MTL::SamplerState.
	class sampler {
		friend device;
	protected:
		/// Initializes this object to empty.
		sampler(std::nullptr_t) {
		}
	private:
		NS::SharedPtr<MTL::SamplerState> _smp; ///< The sampler.

		/// Initializes \ref _smp.
		explicit sampler(NS::SharedPtr<MTL::SamplerState> smp) : _smp(std::move(smp)) {
		}
	};
}
