#include "ToolSerializer.h"
#include "resource/ResourceManager.h"
#include "../FileImporter.h"

namespace Atlas::Editor {

    void to_json(json& j, const TerrainGenerator::SlopeBiome& p) {

        j = json{
            {"slope", p.slope},
            {"materialIdx", p.materialIdx}
        };

    }

    void from_json(const json& j, TerrainGenerator::SlopeBiome& p) {

        j.at("slope").get_to(p.slope);
        j.at("materialIdx").get_to(p.materialIdx);

    }

    void to_json(json& j, const TerrainGenerator::MoistureBiome& p) {

        j = json{
            {"moisture", p.moisture},
            {"materialIdx", p.materialIdx}
        };

    }

    void from_json(const json& j, TerrainGenerator::MoistureBiome& p) {

        j.at("moisture").get_to(p.moisture);
        j.at("materialIdx").get_to(p.materialIdx);

    }

    void to_json(json& j, const TerrainGenerator::ElevationBiome& p) {

        j = json{
            {"elevation", p.elevation},
            {"less", p.less},
            {"moistureBiomes", p.moistureBiomes},
            {"slopeBiomes", p.slopeBiomes},
            {"materialIdx", p.materialIdx}
        };

    }

    void from_json(const json& j, TerrainGenerator::ElevationBiome& p) {

        j.at("elevation").get_to(p.elevation);
        j.at("less").get_to(p.less);
        j.at("moistureBiomes").get_to(p.moistureBiomes);
        j.at("slopeBiomes").get_to(p.slopeBiomes);
        j.at("materialIdx").get_to(p.materialIdx);

    }

    void to_json(json& j, const TerrainGenerator& p) {

        j = json{
            {"elevationBiomes", p.elevationBiomes},
            {"heightAmplitudes", p.heightAmplitudes},
            {"heightExp", p.heightExp},
            {"heightSeed", p.heightSeed},
            {"moistureAmplitudes", p.moistureAmplitudes},
            {"moistureSeed", p.moistureSeed},
            {"name", p.name},
            {"LoDCount", p.LoDCount},
            {"patchSize", p.patchSize},
            {"resolution", p.resolution},
            {"height", p.height},
            {"resolutionSelection", p.resolutionSelection},
            {"materialSelection", p.materialSelection},
            {"loadFromFile", p.heightMapSelection},
            {"advanced", p.advanced}
        };

        if (p.heightMap.IsValid())
            j["heightMap"] = p.heightMap.GetResource()->path;
        if (p.selectedMaterial.IsValid())
            j["selectedMaterial"] = p.selectedMaterial.GetResource()->path;

        int32_t count = 0;
        for (const auto& [material, color] : p.materials) {
            if (material.IsValid())
                j["materials"][count]["material"] = material.GetResource()->path;
            j["materials"][count]["color"] = color;
            count++;
        }

    }

    void from_json(const json& j, TerrainGenerator& p) {

        j.at("elevationBiomes").get_to(p.elevationBiomes);
        j.at("heightAmplitudes").get_to(p.heightAmplitudes);
        j.at("heightExp").get_to(p.heightExp);
        j.at("heightSeed").get_to(p.heightSeed);
        j.at("moistureAmplitudes").get_to(p.moistureAmplitudes);
        j.at("moistureSeed").get_to(p.moistureSeed);
        j.at("name").get_to(p.name);
        j.at("LoDCount").get_to(p.LoDCount);
        j.at("patchSize").get_to(p.patchSize);
        j.at("resolution").get_to(p.resolution);
        j.at("height").get_to(p.height);
        j.at("resolutionSelection").get_to(p.resolutionSelection);
        j.at("materialSelection").get_to(p.materialSelection);
        j.at("loadFromFile").get_to(p.heightMapSelection);
        j.at("advanced").get_to(p.advanced);

        if (j.contains("heightMap"))
            p.heightMap = FileImporter::ImportFile<Texture::Texture2D>(j["heightMap"]);
        if (j.contains("selectedMaterial"))
            p.selectedMaterial = FileImporter::ImportFile<Material>(j["selectedMaterial"]);

        if (j.contains("materials")) {
            for (const auto& m : j["materials"]) {
                p.materials.emplace_back(
                    FileImporter::ImportFile<Material>(m["material"]), m["color"]
                    );
            }
        }

    }

}