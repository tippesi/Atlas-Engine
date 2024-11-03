#pragma once

#include "PostProcessing.h"

#include "../common/SerializationHelper.h"

namespace Atlas::PostProcessing {

    void to_json(json& j, const ChromaticAberration& p);

    void from_json(const json& j, ChromaticAberration& p);

    void to_json(json& j, const FilmGrain& p);

    void from_json(const json& j, FilmGrain& p);

    void to_json(json& j, const Sharpen& p);

    void from_json(const json& j, Sharpen& p);

    void to_json(json& j, const TAA& p);

    void from_json(const json& j, TAA& p);

    void to_json(json& j, const Vignette& p);

    void from_json(const json& j, Vignette& p);

    void to_json(json& j, const Bloom& p);

    void from_json(const json& j, Bloom& p);

    void to_json(json& j, const PostProcessing& p);

    void from_json(const json& j, PostProcessing& p);

}