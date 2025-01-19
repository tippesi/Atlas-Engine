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

    void to_json(json& j, const VegetationGenerator::VegetationType& p) {

        j = json{
           {"id", p.id},
           {"name", p.name},
           {"offset", p.offset},
           {"slopeMin", p.slopeMin},
           {"slopeMax", p.slopeMax},
           {"heightMin", p.heightMin},
           {"heightMax", p.heightMax},
           {"excludeBasedOnCorners", p.excludeBasedOnCorners},
           {"exludeMaterialIndices", p.exludeMaterialIndices},
           {"scaleMin", p.scaleMin},
           {"scaleMax", p.scaleMax},
           {"rotationMin", p.rotationMin},
           {"rotationMax", p.rotationMax},
           {"seed", p.seed},
           {"iterations", p.iterations},
           {"initialDensity", p.initialDensity},
           {"offspringPerIteration", p.offspringPerIteration},
           {"offspringSpreadRadius", p.offspringSpreadRadius},
           {"priority", p.priority},
           {"alignToSurface", p.alignToSurface},
           {"alignBasedOnCorners", p.alignBasedOnCorners},
           {"maxAlignmentAngle", p.maxAlignmentAngle},
           {"collisionRadius", p.collisionRadius},
           {"shadeRadius", p.shadeRadius},
           {"canGrowInShade", p.canGrowInShade},
           {"growthMaxAge", p.growthMaxAge},
           {"growthMinScale", p.growthMinScale},
           {"growthMaxScale", p.growthMaxScale},
           {"attachMeshPhysicsComponent", p.attachMeshPhysicsComponent},
           {"entity", p.entity},
           {"parentEntity", p.parentEntity},
           {"entities", p.entities}
        };

    }

    void from_json(const json& j, VegetationGenerator::VegetationType& p) {

        j.at("id").get_to(p.id);
        j.at("name").get_to(p.name);
        j.at("offset").get_to(p.offset);
        j.at("slopeMin").get_to(p.slopeMin);
        j.at("slopeMax").get_to(p.slopeMax);
        j.at("heightMin").get_to(p.heightMin);
        j.at("heightMax").get_to(p.heightMax);
        j.at("excludeBasedOnCorners").get_to(p.excludeBasedOnCorners);
        j.at("exludeMaterialIndices").get_to(p.exludeMaterialIndices);
        j.at("scaleMin").get_to(p.scaleMin);
        j.at("scaleMax").get_to(p.scaleMax);
        j.at("rotationMin").get_to(p.rotationMin);
        j.at("rotationMax").get_to(p.rotationMax);
        j.at("seed").get_to(p.seed);
        j.at("iterations").get_to(p.iterations);
        j.at("initialDensity").get_to(p.initialDensity);
        j.at("offspringPerIteration").get_to(p.offspringPerIteration);
        j.at("offspringSpreadRadius").get_to(p.offspringSpreadRadius);
        j.at("priority").get_to(p.priority);
        j.at("alignToSurface").get_to(p.alignToSurface);
        j.at("alignBasedOnCorners").get_to(p.alignBasedOnCorners);
        j.at("maxAlignmentAngle").get_to(p.maxAlignmentAngle);
        j.at("collisionRadius").get_to(p.collisionRadius);
        j.at("shadeRadius").get_to(p.shadeRadius);
        j.at("canGrowInShade").get_to(p.canGrowInShade);
        j.at("growthMaxAge").get_to(p.growthMaxAge);
        j.at("growthMinScale").get_to(p.growthMinScale);
        j.at("growthMaxScale").get_to(p.growthMaxScale);
        j.at("attachMeshPhysicsComponent").get_to(p.attachMeshPhysicsComponent);
        j.at("parentEntity").get_to(p.parentEntity);
        j.at("entities").get_to(p.entities);

        if (j.contains("entity"))
            p.entity = j["entity"];

    }

    void to_json(json& j, const VegetationGenerator& p) {

        j = json{
            {"types", p.types},
            {"seed", p.seed},
            {"useSceneForCollision", p.useSceneForCollision}
        };


    }

    void from_json(const json& j, VegetationGenerator& p) {

        j.at("types").get_to(p.types);
        j.at("seed").get_to(p.seed);
        j.at("useSceneForCollision").get_to(p.useSceneForCollision);

    }

}