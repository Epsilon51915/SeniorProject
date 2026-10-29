#pragma once
#include <cstdint>

namespace blendsettings
{
	enum class BlendFactor: uint8_t
	{
		zero,
        one,
        source_color,
        one_minus_source_color,
        destination_color,
        one_minus_destination_color,
        source_alpha,
        one_minus_source_alpha,
        destination_alpha,
        one_minus_destination_alpha,
        source_alpha_saturate
	};

    enum class BlendOp : uint8_t
    {
        add,
        subtract,
        reverse_subtract,
        minimum,
        maximum
    };

}

namespace samplers
{
    struct SamplerDesc
    {
        uint32_t anistropic = false; // x2 x4 x6 x8
        bool compare_enabled = false;
    };
}