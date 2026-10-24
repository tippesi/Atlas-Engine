#pragma once

#include "Image.h"
#include "SceneTools.h"

#include "terrain/Terrain.h"

namespace Atlas::Capture {

    struct TerrainSample {

        bool valid = false;
        bool hole = false;
        float height = 0.0f;
        vec3 normal = vec3(0.0f, 1.0f, 0.0f);
        float slopeDegrees = 0.0f;
        // Material slot of the nearest texel
        int32_t material = -1;

    };

    // Cells are streamed around the camera. Measurements and edits need all of them, and edited cells
    // must not be unloaded again, so the storage stays in editing mode
    const Ref<Terrain::Terrain>& GetCompleteTerrain(Scene::Scene& scene);

    // Samples the highest LoD storage cells at a world space x/z position
    TerrainSample SampleTerrain(const Ref<Terrain::Terrain>& terrain, vec2 position);

    // Extents, parameters, material slots and connected hole regions of the highest LoD
    json DescribeTerrain(const Ref<Terrain::Terrain>& terrain);

    // Top down map: material colors with hill shading and contour lines, holes, mesh entity footprints,
    // a labeled grid and optional markers. Region defaults to the whole terrain
    Ref<Image> RenderTerrainMap(Scene::Scene& scene, const json& spec);

    // Walkable route between two points (and optional via points): A* over a ground grid, see the "route" query
    json FindRoute(Scene::Scene& scene, const json& query, const fs::path& outputDirectory = {});

    // Brushes of the "terrain" edit operation, see README.md. Returns a log entry per brush
    json ApplyTerrainBrushes(Scene::Scene& scene, const json& brushes);

    // Recreates the height field body of the terrain after its heights changed (like the editor terrain panel)
    void RebuildTerrainBody(Scene::Scene& scene);

    // Copies the terrain file of the scene to a new path and loads the copy as the scene terrain
    void CopySceneTerrain(Scene::Scene& scene, const std::string& path, bool overwrite);

    // Bakes the lower LoDs and writes the terrain back to its file
    void SaveSceneTerrain(Scene::Scene& scene);

}
