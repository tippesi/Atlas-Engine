#pragma once

#include "PostProcessing.h"

namespace Atlas::PostProcessing {

    void to_json(json& j, const ChromaticAberration& p) {
        j = json {
            {"enable", p.enable},
            {"strength", p.strength},
            {"colorsReversed", p.colorsReversed},
        };
    }

    void from_json(const json& j, ChromaticAberration& p) {
        j.at("enable").get_to(p.enable);
        j.at("strength").get_to(p.strength);
        j.at("colorsReversed").get_to(p.colorsReversed);
    }

    void to_json(json& j, const FilmGrain& p) {
        j = json {
            {"enable", p.enable},
            {"strength", p.strength},
        };
    }

    void from_json(const json& j, FilmGrain& p) {
        j.at("enable").get_to(p.enable);
        j.at("strength").get_to(p.strength);
    }

    void to_json(json& j, const Sharpen& p) {
        j = json {
            {"enable", p.enable},
            {"factor", p.factor},
        };
    }

    void from_json(const json& j, Sharpen& p) {
        j.at("enable").get_to(p.enable);
        j.at("factor").get_to(p.factor);
    }

    void to_json(json& j, const TAA& p) {
        j = json {
            {"enable", p.enable},
            {"jitterRange", p.jitterRange},
        };
    }

    void from_json(const json& j, TAA& p) {
        j.at("enable").get_to(p.enable);
        j.at("jitterRange").get_to(p.jitterRange);
    }

    void to_json(json& j, const Vignette& p) {
        j = json {
            {"enable", p.enable},
            {"offset", p.offset},
            {"power", p.power},
            {"strength", p.strength},
            {"color", p.color},
        };
    }

    void from_json(const json& j, Vignette& p) {
        j.at("enable").get_to(p.enable);
        j.at("offset").get_to(p.offset);
        j.at("power").get_to(p.power);
        j.at("strength").get_to(p.strength);
        j.at("color").get_to(p.color);
    }

    void to_json(json& j, const PostProcessing& p) {
        j = json {
            {"saturation", p.saturation},
            {"contrast", p.contrast},
            {"paperWhiteLuminance", p.paperWhiteLuminance},
            {"screenMaxLuminance", p.screenMaxLuminance},
            {"filmicTonemapping", p.filmicTonemapping},
            {"taa", p.taa},
            {"vignette", p.vignette},
            {"chromaticAberration", p.chromaticAberration},
            {"filmGrain", p.filmGrain},
            {"sharpen", p.sharpen},
        };
    }

    void from_json(const json& j, PostProcessing& p) {
        j.at("saturation").get_to(p.saturation);
        j.at("contrast").get_to(p.contrast);
        j.at("paperWhiteLuminance").get_to(p.paperWhiteLuminance);
        j.at("screenMaxLuminance").get_to(p.screenMaxLuminance);
        j.at("filmicTonemapping").get_to(p.filmicTonemapping);
        j.at("taa").get_to(p.taa);
        j.at("vignette").get_to(p.vignette);
        j.at("chromaticAberration").get_to(p.chromaticAberration);
        j.at("filmGrain").get_to(p.filmGrain);
        j.at("sharpen").get_to(p.sharpen);
    }

}