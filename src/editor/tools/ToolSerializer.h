#pragma once

#include "TerrainGenerator.h"
#include "VegetationGenerator.h"

#include <common/SerializationHelper.h>

namespace Atlas::Editor {

	void to_json(json& j, const TerrainGenerator::SlopeBiome& p);

	void from_json(const json& j, TerrainGenerator::SlopeBiome& p);

	void to_json(json& j, const TerrainGenerator::MoistureBiome& p);

	void from_json(const json& j, TerrainGenerator::MoistureBiome& p);

	void to_json(json& j, const TerrainGenerator::ElevationBiome& p);

	void from_json(const json& j, TerrainGenerator::ElevationBiome& p);

	void to_json(json& j, const TerrainGenerator& p);

	void from_json(const json& j, TerrainGenerator& p);

	void to_json(json& j, const VegetationGenerator::VegetationType& p);

	void from_json(const json& j, VegetationGenerator::VegetationType& p);

	void to_json(json& j, const VegetationGenerator& p);

	void from_json(const json& j, VegetationGenerator& p);

}