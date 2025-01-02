#pragma once

/// \file
/// Implementation details and helpers for the Metal backend.

#include <cstddef>
#include <utility>

#include <Metal/Metal.hpp>

#include "lotus/utils/static_function.h"
#include "lotus/gpu/common.h"

namespace lotus::gpu::backends::metal::_details {
	// TODO
	/// Metal does not seem to have custom debug callbacks.
	using debug_message_id = int;
	/// Debug callback type for the Metal backend.
	using debug_message_callback =
		static_function<void(debug_message_severity, debug_message_id, std::u8string_view)>;

	/// Smart pointer class that automatically calls the \p retain() and \p release() methods.
	template <typename T> struct metal_ptr {
	public:
		/// Default constructor.
		metal_ptr() = default;
		/// Initializes this pointer to null.
		metal_ptr(std::nullptr_t) {
		}
		/// Move constructor.
		metal_ptr(metal_ptr &&src) noexcept : _ptr(std::exchange(src._ptr, nullptr)) {
		}
		/// Copy constructor.
		metal_ptr(const metal_ptr &src) : _ptr(src._ptr) {
			if (_ptr) {
				_ptr->retain();
			}
		}
		/// Move assignment.
		metal_ptr &operator=(metal_ptr &&src) noexcept {
			if (&src != this) {
				release();
				_ptr = std::exchange(src._ptr, nullptr);
			}
			return *this;
		}
		/// Copy assignment.
		metal_ptr &operator=(const metal_ptr &src) {
			if (&src != this) {
				release();
				_ptr = src._ptr;
				_ptr->retain();
			}
			return *this;
		}
		/// Releases the object, if any.
		~metal_ptr() {
			release();
		}

		/// Takes ownership of the given pointer. The input pointer should not be used afterwards.
		[[nodiscard]] static metal_ptr take_ownership(T *&&ptr) {
			return metal_ptr(std::exchange(ptr, nullptr));
		}
		/// Shares ownership of the given pointer.
		[[nodiscard]] static metal_ptr share_ownership(T *ptr) {
			ptr->retain();
			return metal_ptr(ptr);
		}

		/// Releases the object, if any.
		void release() {
			if (_ptr) {
				_ptr->release();
				_ptr = nullptr;
			}
		}

		/// Returns a pointer to the object.
		[[nodiscard]] T *get() const {
			return _ptr;
		}
		/// \overload
		[[nodiscard]] T *operator->() const {
			return _ptr;
		}
		/// \overload
		[[nodiscard]] T &operator*() const {
			return *_ptr;
		}

		/// Checks if this pointer points to a valid object.
		[[nodiscard]] bool is_valid() const {
			return _ptr != nullptr;
		}
		/// \overload
		[[nodiscard]] explicit operator bool() const {
			return is_valid();
		}
	private:
		T *_ptr = nullptr; ///< The pointer.

		/// Initializes \ref _ptr to the given value.
		explicit metal_ptr(T *p) : _ptr(p) {
		}
	};
	/// Convenience function for taking the ownership of a given Metal object.
	template <typename T> metal_ptr<T> take_ownership(T *&&p) {
		return metal_ptr<T>::take_ownership(std::move(p));
	}
	/// Convenience function for sharing the ownership of a given Metal object.
	template <typename T> metal_ptr<T> share_ownership(T *p) {
		return metal_ptr<T>::share_ownership(p);
	}


	/// Memory types supported by Metal.
	enum class memory_type_index : std::underlying_type_t<gpu::memory_type_index> {
		shared_cpu_cached,   ///< \p MTL::StorageModeShared with CPU-side caching enabled.
		shared_cpu_uncached, ///< \p MTL::StorageModeShared with CPU-side caching disabled.
		device_private,      ///< \p MTL::StroageModePrivate.

		num_enumerators ///< The number of enumerators.
	};

	/// Conversion helpers between lotus and Metal data types.
	namespace conversions {
		/// Converts a \ref format to a \p MTL::PixelFormat.
		[[nodiscard]] MTL::PixelFormat to_pixel_format(format);
		/// Converts a \ref memory_type_index to a \p MTL::ResourceOptions.
		[[nodiscard]] MTL::ResourceOptions to_resource_options(_details::memory_type_index);
		/// \overload
		[[nodiscard]] inline MTL::ResourceOptions to_resource_options(gpu::memory_type_index i) {
			return to_resource_options(static_cast<memory_type_index>(i));
		}
		/// Converts a \ref mip_levels to a \p NS::Range based on an associated texture.
		[[nodiscard]] NS::Range to_range(mip_levels, MTL::Texture*);
		/// Converts a \ref image_usage_mask to a \p MTL::TextureUsage.
		[[nodiscard]] MTL::TextureUsage to_texture_usage(image_usage_mask);
		/// Converts a \ref sampler_address_mode to a \p MTL::SamplerAddressMode.
		[[nodiscard]] MTL::SamplerAddressMode to_sampler_address_mode(sampler_address_mode);
		/// Converts a \ref filtering to a \p MTL::SamplerMinMagFilter.
		[[nodiscard]] MTL::SamplerMinMagFilter to_sampler_min_mag_filter(filtering);
		/// Converts a \ref filtering to a \p MTL::SamplerMipFilter.
		[[nodiscard]] MTL::SamplerMipFilter to_sampler_mip_filter(filtering);
		/// Converts a C-style string to a \p NS::String.
		[[nodiscard]] _details::metal_ptr<NS::String> to_string(const char8_t*);

		/// Converts a \p NS::String back to a \p std::string.
		[[nodiscard]] std::u8string back_to_string(NS::String*);
		/// Converts a \p MTL::SizeAndAlign back to a \ref memory::size_alignment.
		[[nodiscard]] memory::size_alignment back_to_size_alignment(MTL::SizeAndAlign);
	}

	/// Creates a new \p MTL::TextureDescriptor based on the given settings.
	[[nodiscard]] _details::metal_ptr<MTL::TextureDescriptor> create_texture_descriptor(
		MTL::TextureType,
		format,
		cvec3u32 size,
		std::uint32_t mip_levels,
		MTL::ResourceOptions,
		image_usage_mask
	);
}
