#pragma once

/// \file
/// Pipeline-related DirectX 12 classes.

#include <d3d12.h>

#include "details.h"

namespace lotus::graphics::backends::directx12 {
	class device;
	class command_list;

	/// Contains a \p D3D12_SHADER_BYTECODE.
	class shader {
		friend device;
	protected:
	private:
		D3D12_SHADER_BYTECODE _shader; ///< The shader code.
	};

	/// A \p ID3D12RootSignature.
	class pipeline_resources {
		friend device;
	protected:
	private:
		_details::com_ptr<ID3D12RootSignature> _signature; ///< The \p ID3D12RootSignature object.
	};

	/// A \p ID3D12PipelineState.
	class pipeline_state {
		friend device;
		friend command_list;
	protected:
	private:
		_details::com_ptr<ID3D12PipelineState> _pipeline; ///< The \p ID3D12PipelineState object.
		_details::com_ptr<ID3D12RootSignature> _root_signature; ///< The root signature.
		D3D_PRIMITIVE_TOPOLOGY _topology; ///< Primitive topology used by this pipeline.
	};
}
