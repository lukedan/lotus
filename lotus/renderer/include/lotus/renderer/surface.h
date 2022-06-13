#pragma once

/// \file
/// Surfaces and frame buffers.

#include <variant>

#include "lotus/graphics/resources.h"
#include "context.h"

namespace lotus::renderer {/*
	/// View of a 2D surface.
	struct surface2d_view {
		friend context;
	public:
		/// Initializes this surface view to empty.
		surface2d_view(std::nullptr_t) : _ctx(nullptr), _ref(nullptr) {
		}
		/// Move construction.
		surface2d_view(surface2d_view &&src) :
			_ctx(std::exchange(src._ctx, nullptr)), _ref(std::exchange(src._ref, nullptr)) {
		}
		/// No copy construction.
		surface2d_view(const surface2d_view&) = delete;
		/// Move assignment.
		surface2d_view &operator=(surface2d_view &&src) {
			if (&src != this) {
				reset();
				std::swap(_ctx, src._ctx);
				std::swap(_ref, src._ref);
			}
			return *this;
		}
		/// No copy assignment.
		surface2d_view &operator=(const surface2d_view&) = delete;
		/// Frees the surface view.
		~surface2d_view() {
			reset();
		}

		/// Returns the view of this surface.
		[[nodiscard]] const graphics::image2d_view &get_view() const {
			return _get_view_data().view;
		}
		/// Returns the current size of this surface.
		[[nodiscard]] cvec2s get_current_size() const {
			return _get_surface_data().current_size;
		}
		/// Returns the format that this surface is viewed as.
		[[nodiscard]] graphics::format get_view_format() const {
			return _get_view_data().view_format;
		}
		/// Returns the original format of this surface.
		[[nodiscard]] graphics::format get_original_format() const {
			return _get_surface_data().pixel_format;
		}

		/// Returns a new view into the same surface with the given format.
		[[nodiscard]] surface2d_view view_as(graphics::format) const;

		/// Destroys this view and resets this object to empty.
		void reset() {
			if (_ctx) {
				// TODO: Shouldn't destroy everything immediately - instead, use a queue of requests
				_ctx->_destroy_surface2d_view(_ref);
				_ctx = nullptr;
				_ref = nullptr;
			}
		}
	private:
		/// Returns the associated \ref context::_surface2d_view_data.
		[[nodiscard]] context::_surface2d_view_data &_get_view_data() const {
			return _ref.get(_ctx->_surface_view_pool);
		}
		/// Returns the associated \ref context::_surface2d_data.
		[[nodiscard]] context::_surface2d_data &_get_surface_data() const {
			return _get_view_data().surface.get(_ctx->_surface_pool);
		}

		context *_ctx; ///< Reference to the surface data.
		pool<context::_surface2d_view_data>::reference _ref; ///< Reference to the surface data.
	};

	/// Reference to a color render target.
	struct color_render_target {
	public:
		/// Initializes the render target to empty.
		color_render_target(std::nullptr_t) :
			surface(nullptr),
			load_operation(graphics::pass_load_operation::discard),
			store_operation(graphics::pass_store_operation::discard) {
		}
		/// Creates a custom render target description.
		[[nodiscard]] inline static color_render_target create_custom(
			surface2d &surf, clear_value_type clear,
			graphics::pass_load_operation load_op, graphics::pass_store_operation store_op
		) {
			return color_render_target(surf, clear, load_op, store_op);
		}
		/// Creates a render target description that reads from the surface and then writes to it.
		[[nodiscard]] inline static color_render_target create_read_write(surface2d &s) {
			return color_render_target(
				s, uninitialized, graphics::pass_load_operation::preserve, graphics::pass_store_operation::preserve
			);
		}
		/// Creates a render target description that clears the surface and then writes to it.
		[[nodiscard]] inline static color_render_target create_clear_write(surface2d &s, clear_value_type clear) {
			assert(!std::holds_alternative<uninitialized_t>(clear));
			// verify that the clear value is of the correct type
			bool is_floating_point =
				graphics::format_properties::get(s.pixel_format).type ==
				graphics::format_properties::data_type::floating_point;
			assert(is_floating_point == std::holds_alternative<cvec4f>(clear));
			return color_render_target(
				s, clear, graphics::pass_load_operation::clear, graphics::pass_store_operation::preserve
			);
		}
		/// Creates a render target description that writes to it with no regard to its previous content.
		[[nodiscard]] inline static color_render_target create_clear_write(surface2d &s) {
			return color_render_target(
				s, uninitialized, graphics::pass_load_operation::discard, graphics::pass_store_operation::preserve
			);
		}

		surface2d *surface; ///< The surface.
		clear_value_type clear_value; ///< Clear value.
		graphics::pass_load_operation  load_operation;  ///< Pass load operation.
		graphics::pass_store_operation store_operation; ///< Pass store operation.
	private:
		/// Initializes all fields of this struct.
		color_render_target(
			surface2d &surf, clear_value_type clear,
			graphics::pass_load_operation load_op, graphics::pass_store_operation store_op
		) : surface(&surf), clear_value(clear), load_operation(load_op), store_operation(store_op) {
		}
	};
	/// Reference to a depth-stencil render target.
	struct depth_stencil_render_target {
	public:
		/// Initializes this struct to refer to an empty depth-stencil render target.
		depth_stencil_render_target(std::nullptr_t) :
			surface(nullptr),
			depth_load_operation(graphics::pass_load_operation::discard),
			depth_store_operation(graphics::pass_store_operation::discard),
			stencil_load_operation(graphics::pass_load_operation::discard),
			stencil_store_operation(graphics::pass_store_operation::discard) {
		}
		/// Creates a depth-stencil render target description using the given parameters.
		[[nodiscard]] inline static depth_stencil_render_target create_custom(
			surface2d &surf, float dclear, std::uint8_t sclear,
			graphics::pass_load_operation dload, graphics::pass_store_operation dstore,
			graphics::pass_load_operation sload, graphics::pass_store_operation sstore
		) {
			return depth_stencil_render_target(surf, dclear, sclear, dload, dstore, sload, sstore);
		}

		surface2d *surface; ///< The surface.
		float        depth_clear;   ///< Depth clear value.
		std::uint8_t stencil_clear; ///< Stencil clear value.
		graphics::pass_load_operation  depth_load_operation;    ///< Depth load operation.
		graphics::pass_store_operation depth_store_operation;   ///< Depth store operation.
		graphics::pass_load_operation  stencil_load_operation;  ///< Stencil load operation.
		graphics::pass_store_operation stencil_store_operation; ///< Stencil store operation.
	private:
		/// Initializes all fields of this struct.
		depth_stencil_render_target(
			surface2d &surf, float dclear, std::uint8_t sclear,
			graphics::pass_load_operation dload, graphics::pass_store_operation dstore,
			graphics::pass_load_operation sload, graphics::pass_store_operation sstore
		) :
			surface(&surf), depth_clear(dclear), stencil_clear(sclear),
			depth_load_operation(dload), depth_store_operation(dstore),
			stencil_load_operation(sload), stencil_store_operation(sstore) {
		}
	};*/
}
