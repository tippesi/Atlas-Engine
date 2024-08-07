#pragma once

#include "common/SerializationHelper.h"

#include "Shadow.h"
#include "AO.h"
#include "SSGI.h"
#include "RTGI.h"
#include "EnvironmentProbe.h"
#include "SSS.h"
#include "IrradianceVolume.h"
#include "Atmosphere.h"
#include "Reflection.h"
#include "Fog.h"
#include "VolumetricClouds.h"
#include "Sky.h"

namespace Atlas::Lighting {

    void to_json(json& j, const AO& p);

    void from_json(const json& j, AO& p);

    void to_json(json& j, const EnvironmentProbe& p);

    void from_json(const json& j, EnvironmentProbe& p);

    void to_json(json& j, const Atmosphere& p);

    void from_json(const json& j, Atmosphere& p);

    void to_json(json& j, const Fog& p);

    void from_json(const json& j, Fog& p);

    void to_json(json& j, const IrradianceVolume& p);

    void from_json(const json& j, IrradianceVolume& p);

    void to_json(json& j, const Reflection& p);

    void from_json(const json& j, Reflection& p);

    void to_json(json& j, const RTGI& p);

    void from_json(const json& j, RTGI& p);

    void to_json(json& j, const ShadowView& p);

    void from_json(const json& j, ShadowView& p);

    void to_json(json& j, const Shadow& p);

    void from_json(const json& j, Shadow& p);

    void to_json(json& j, const SSGI& p);

    void from_json(const json& j, SSGI& p);

    void to_json(json& j, const SSS& p);

    void from_json(const json& j, SSS& p);

    void to_json(json& j, const VolumetricClouds::Scattering& p);

    void from_json(const json& j, VolumetricClouds::Scattering& p);

    void to_json(json& j, const VolumetricClouds& p);

    void from_json(const json& j, VolumetricClouds& p);

    void to_json(json& j, const Sky& p);

    void from_json(const json& j, Sky& p);

}