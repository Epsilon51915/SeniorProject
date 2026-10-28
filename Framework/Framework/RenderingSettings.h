namespace blendsettings
{
	enum class BlendFactor
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
        source_alpha_saturate,
	};

    enum class BlendOp
    {
        add,
        subtract,
        reverse_subtract,
        minimum,
        maximum,
    };

}