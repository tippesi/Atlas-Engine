#include "PostProcessingBindings.h"

#include "postprocessing/PostProcessing.h"

namespace Atlas::Scripting::Bindings {

    void GeneratePostProcessingBindings(sol::table* ns) {

        ns->new_usertype<PostProcessing::AutoExposure>("AutoExposure",
            sol::no_constructor,
            "enable", &PostProcessing::AutoExposure::enable,
            "luminanceMin", &PostProcessing::AutoExposure::luminanceMin,
            "luminanceMax", &PostProcessing::AutoExposure::luminanceMax,
            "blackLevel", &PostProcessing::AutoExposure::blackLevel,
            "timeCoefficient", &PostProcessing::AutoExposure::timeCoefficient
        );

        ns->new_usertype<PostProcessing::Bloom>("Bloom",
            "enable", &PostProcessing::Bloom::enable,
            "strength", &PostProcessing::Bloom::strength,
            "dirtStrength", &PostProcessing::Bloom::dirtStrength,
            "threshold", &PostProcessing::Bloom::threshold,
            "filterSize", &PostProcessing::Bloom::filterSize,
            "mipLevels", &PostProcessing::Bloom::mipLevels
        );

        ns->new_usertype<PostProcessing::Vignette>("Vignette",
            "enable", &PostProcessing::Vignette::enable,
            "offset", &PostProcessing::Vignette::offset,
            "power", &PostProcessing::Vignette::power,
            "strength", &PostProcessing::Vignette::strength,
            "color", &PostProcessing::Vignette::color
        );

        ns->new_usertype<PostProcessing::ChromaticAberration>("ChromaticAberration",
            "enable", &PostProcessing::ChromaticAberration::enable,
            "strength", &PostProcessing::ChromaticAberration::strength,
            "colorsReversed", &PostProcessing::ChromaticAberration::colorsReversed
        );

        ns->new_usertype<PostProcessing::Sharpen>("Sharpen",
            "enable", &PostProcessing::Sharpen::enable,
            "factor", &PostProcessing::Sharpen::factor
        );

        ns->new_usertype<PostProcessing::FilmGrain>("FilmGrain",
            "enable", &PostProcessing::FilmGrain::enable,
            "strength", &PostProcessing::FilmGrain::strength
        );

        ns->new_usertype<PostProcessing::TAA>("TAA",
            "enable", &PostProcessing::TAA::enable,
            "jitterRange", &PostProcessing::TAA::jitterRange
        );

        ns->new_usertype<PostProcessing::PostProcessing>("PostProcessing",
            sol::no_constructor,
            "exposureCompensation", &PostProcessing::PostProcessing::exposureCompensation,
            "saturation", &PostProcessing::PostProcessing::saturation,
            "contrast", &PostProcessing::PostProcessing::contrast,
            "tint", &PostProcessing::PostProcessing::tint,
            "screenMaxLuminance", &PostProcessing::PostProcessing::screenMaxLuminance,
            "paperWhiteLuminance", &PostProcessing::PostProcessing::paperWhiteLuminance,
            "filmicTonemapping", &PostProcessing::PostProcessing::filmicTonemapping,
            "fsr2", &PostProcessing::PostProcessing::fsr2,
            "taa", &PostProcessing::PostProcessing::taa,
            "vignette", &PostProcessing::PostProcessing::vignette,
            "chromaticAberration", &PostProcessing::PostProcessing::chromaticAberration,
            "filmGrain", &PostProcessing::PostProcessing::filmGrain,
            "sharpen", &PostProcessing::PostProcessing::sharpen,
            "bloom", &PostProcessing::PostProcessing::bloom,
            "autoExposure", &PostProcessing::PostProcessing::autoExposure
        );

    }

}
