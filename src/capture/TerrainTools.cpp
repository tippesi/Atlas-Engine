#include "TerrainTools.h"

#include "Filter.h"
#include "common/MathHelper.h"
#include "loader/TerrainLoader.h"
#include "physics/ShapesManager.h"
#include "resource/ResourceManager.h"
#include "tools/TerrainTool.h"

#include <cfloat>
#include <map>
#include <queue>
#include <stdexcept>

namespace Atlas::Capture {

    using namespace Scene::Components;

    // Texels of the highest LoD, which cover the terrain without overlap (the last row/column of a cell is
    // the first of the next one)
    struct TexelGrid {

        Terrain::Terrain* terrain = nullptr;
        int32_t LoD = 0;
        int32_t cellsPerSide = 0;
        int32_t texelsPerCell = 0;
        int32_t texelsPerSide = 0;
        float texelSize = 0.0f;

        Terrain::TerrainStorageCell* GetCell(ivec2 texel, int32_t& index) const {

            if (texel.x < 0 || texel.y < 0 || texel.x >= texelsPerSide || texel.y >= texelsPerSide)
                return nullptr;

            auto cell = terrain->storage->GetCell(texel.x / texelsPerCell, texel.y / texelsPerCell, LoD);
            if (!cell || !cell->IsLoaded())
                return nullptr;

            auto local = texel % texelsPerCell;
            index = local.y * cell->heightField->width + local.x;
            return cell;

        }

        bool IsHole(ivec2 texel) const {

            int32_t index;
            auto cell = GetCell(texel, index);
            return cell && cell->heightData[index] == FLT_MAX;

        }

        // World space height, FLT_MAX for holes and texels outside of the terrain
        float GetHeight(ivec2 texel) const {

            int32_t index;
            auto cell = GetCell(texel, index);
            if (!cell || cell->heightData[index] == FLT_MAX)
                return FLT_MAX;
            return terrain->translation.y + cell->heightData[index] * terrain->heightScale;

        }

        int32_t GetMaterial(ivec2 texel) const {

            int32_t index;
            auto cell = GetCell(texel, index);
            if (!cell || cell->materialIdxData.empty())
                return -1;
            return cell->materialIdxData[index];

        }

        ivec2 ToTexel(vec2 world) const {

            auto relative = (world - vec2(terrain->translation.x, terrain->translation.z)) / texelSize;
            return ivec2(glm::floor(relative + 0.5f));

        }

        vec2 ToWorld(ivec2 texel) const {

            return vec2(terrain->translation.x, terrain->translation.z) + vec2(texel) * texelSize;

        }

    };

    static TexelGrid GetTexelGrid(const Ref<Terrain::Terrain>& terrain) {

        TexelGrid grid;
        grid.terrain = terrain.get();
        grid.LoD = terrain->LoDCount - 1;
        grid.cellsPerSide = int32_t(std::round(std::sqrt(float(terrain->storage->GetCellCount(grid.LoD)))));
        grid.texelsPerCell = 8 * terrain->patchSizeFactor;
        grid.texelsPerSide = grid.cellsPerSide * grid.texelsPerCell;
        grid.texelSize = terrain->sideLength / float(grid.texelsPerSide);
        return grid;

    }

    const Ref<Terrain::Terrain>& GetCompleteTerrain(Scene::Scene& scene) {

        if (!scene.terrain.IsLoaded())
            throw std::runtime_error("The scene has no loaded terrain");

        auto& terrain = scene.terrain.Get();
        if (!terrain->storage->inEditing) {
            Tools::TerrainTool::LoadMissingCells(terrain, scene.terrain.GetResource()->path);
            terrain->storage->inEditing = true;
        }

        return terrain;

    }

    TerrainSample SampleTerrain(const Ref<Terrain::Terrain>& terrain, vec2 position) {

        TerrainSample sample;
        auto grid = GetTexelGrid(terrain);
        auto texel = grid.ToTexel(position);

        int32_t index;
        if (!grid.GetCell(texel, index))
            return sample;

        sample.valid = true;
        sample.material = grid.GetMaterial(texel);
        if (grid.IsHole(texel)) {
            sample.hole = true;
            return sample;
        }

        // Next to holes the interpolated height is invalid, the texel height is used there
        auto center = grid.GetHeight(texel);
        sample.height = terrain->GetHeight(position.x, position.y);
        if (sample.height < terrain->translation.y)
            sample.height = center;

        // Terrain::GetHeight derives its normal with unit texel spacing, use the actual spacing
        auto heightAt = [&](ivec2 offset) {

            auto height = grid.GetHeight(texel + offset);
            return height == FLT_MAX ? center : height;

        };
        auto dx = (heightAt(ivec2(1, 0)) - heightAt(ivec2(-1, 0))) / (2.0f * grid.texelSize);
        auto dz = (heightAt(ivec2(0, 1)) - heightAt(ivec2(0, -1))) / (2.0f * grid.texelSize);
        sample.normal = glm::normalize(vec3(-dx, 1.0f, -dz));
        sample.slopeDegrees = glm::degrees(std::acos(glm::clamp(sample.normal.y, -1.0f, 1.0f)));
        return sample;

    }

    json DescribeTerrain(const Ref<Terrain::Terrain>& terrain) {

        auto grid = GetTexelGrid(terrain);

        json description;
        description["translation"] = FromVec3(terrain->translation);
        description["sideLength"] = terrain->sideLength;
        description["min"] = { terrain->translation.x, terrain->translation.z };
        description["max"] = { terrain->translation.x + terrain->sideLength, terrain->translation.z + terrain->sideLength };
        description["resolution"] = terrain->resolution;
        description["heightScale"] = terrain->heightScale;
        description["LoDCount"] = terrain->LoDCount;
        description["cellsPerSide"] = grid.cellsPerSide;
        description["texelSize"] = grid.texelSize;

        json materials = json::array();
        for (size_t slot = 0; slot < terrain->storage->materials.size(); slot++) {
            const auto& material = terrain->storage->materials[slot];
            if (material.IsValid())
                materials.push_back({ { "slot", slot }, { "material", material.GetResource()->path } });
        }
        description["materials"] = materials;

        // Connected hole regions with 8-neighborhood
        std::vector<uint8_t> visited(size_t(grid.texelsPerSide) * size_t(grid.texelsPerSide), 0);
        json holes = json::array();
        float minHeight = FLT_MAX, maxHeight = -FLT_MAX;
        std::vector<int32_t> materialCounts(terrain->storage->materials.size() + 1, 0);

        for (int32_t y = 0; y < grid.texelsPerSide; y++) {
            for (int32_t x = 0; x < grid.texelsPerSide; x++) {
                ivec2 texel(x, y);
                auto material = grid.GetMaterial(texel);
                if (material >= 0 && material < int32_t(materialCounts.size()))
                    materialCounts[material]++;

                auto height = grid.GetHeight(texel);
                if (height != FLT_MAX) {
                    minHeight = std::min(minHeight, height);
                    maxHeight = std::max(maxHeight, height);
                }

                auto start = size_t(y) * grid.texelsPerSide + x;
                if (visited[start] || !grid.IsHole(texel))
                    continue;

                std::vector<ivec2> stack = { texel };
                visited[start] = 1;
                ivec2 min(x, y), max(x, y);
                int64_t count = 0;
                vec2 sum(0.0f);
                float rimMin = FLT_MAX, rimMax = -FLT_MAX, rimSum = 0.0f;
                int32_t rimCount = 0;

                while (!stack.empty()) {
                    auto current = stack.back();
                    stack.pop_back();
                    count++;
                    sum += vec2(current);
                    min = glm::min(min, current);
                    max = glm::max(max, current);

                    for (int32_t j = -1; j <= 1; j++) {
                        for (int32_t i = -1; i <= 1; i++) {
                            auto neighbor = current + ivec2(i, j);
                            int32_t index;
                            if ((i == 0 && j == 0) || !grid.GetCell(neighbor, index))
                                continue;
                            auto neighborIndex = size_t(neighbor.y) * grid.texelsPerSide + neighbor.x;
                            if (grid.IsHole(neighbor)) {
                                if (!visited[neighborIndex]) {
                                    visited[neighborIndex] = 1;
                                    stack.push_back(neighbor);
                                }
                                continue;
                            }

                            auto rimHeight = grid.GetHeight(neighbor);
                            rimMin = std::min(rimMin, rimHeight);
                            rimMax = std::max(rimMax, rimHeight);
                            rimSum += rimHeight;
                            rimCount++;
                        }
                    }
                }

                auto worldMin = grid.ToWorld(min);
                auto worldMax = grid.ToWorld(max);
                auto center = grid.ToWorld(ivec2(0)) + sum / float(count) * grid.texelSize;
                json hole = {
                    { "texels", count },
                    { "area", float(count) * grid.texelSize * grid.texelSize },
                    { "center", { center.x, center.y } },
                    { "min", { worldMin.x, worldMin.y } },
                    { "max", { worldMax.x, worldMax.y } },
                    { "size", { worldMax.x - worldMin.x + grid.texelSize, worldMax.y - worldMin.y + grid.texelSize } }
                };
                if (rimCount > 0)
                    hole["rimHeight"] = { { "min", rimMin }, { "max", rimMax }, { "average", rimSum / float(rimCount) } };
                holes.push_back(hole);
            }
        }

        std::sort(holes.begin(), holes.end(), [](const json& a, const json& b) {

            return a["texels"].get<int64_t>() > b["texels"].get<int64_t>();

        });

        json usage = json::array();
        for (size_t slot = 0; slot < materialCounts.size(); slot++)
            if (materialCounts[slot] > 0)
                usage.push_back({ { "slot", slot },
                    { "share", float(materialCounts[slot]) / float(grid.texelsPerSide * grid.texelsPerSide) } });

        description["materialUsage"] = usage;
        description["heightRange"] = { minHeight, maxHeight };
        description["holes"] = holes;
        return description;

    }

    static u8vec4 MaterialColor(int32_t slot) {

        static const std::vector<u8vec4> colors = {
            { 92, 138, 58, 255 }, { 214, 190, 128, 255 }, { 240, 244, 250, 255 }, { 120, 116, 110, 255 },
            { 196, 204, 214, 255 }, { 138, 104, 70, 255 }, { 180, 160, 120, 255 }, { 70, 110, 140, 255 },
            { 150, 90, 150, 255 }, { 60, 90, 40, 255 } };

        if (slot < 0)
            return { 0, 0, 0, 255 };
        return colors[size_t(slot) % colors.size()];

    }

    static void BlendPixel(Image& image, int32_t x, int32_t y, u8vec4 color, float alpha) {

        if (x < 0 || y < 0 || x >= image.width || y >= image.height)
            return;

        auto& data = image.GetData();
        auto offset = (size_t(y) * image.width + x) * 4;
        for (int32_t i = 0; i < 3; i++)
            data[offset + i] = uint8_t(glm::mix(float(data[offset + i]), float(color[i]), alpha));

    }

    static void DrawLine(Image& image, vec2 a, vec2 b, u8vec4 color, float alpha = 1.0f) {

        auto steps = int32_t(std::max(std::abs(b.x - a.x), std::abs(b.y - a.y))) + 1;
        for (int32_t i = 0; i <= steps; i++) {
            auto point = glm::mix(a, b, float(i) / float(steps));
            BlendPixel(image, int32_t(point.x), int32_t(point.y), color, alpha);
        }

    }

    Ref<Image> RenderTerrainMap(Scene::Scene& scene, const json& spec) {

        const auto& terrain = GetCompleteTerrain(scene);
        auto grid = GetTexelGrid(terrain);

        vec2 regionMin(terrain->translation.x, terrain->translation.z);
        vec2 regionMax = regionMin + vec2(terrain->sideLength);
        if (spec.contains("min"))
            regionMin = vec2(spec["min"].at(0).get<float>(), spec["min"].at(1).get<float>());
        if (spec.contains("max"))
            regionMax = vec2(spec["max"].at(0).get<float>(), spec["max"].at(1).get<float>());
        if (spec.contains("center") && spec.contains("radius")) {
            auto center = vec2(spec["center"].at(0).get<float>(), spec["center"].at(1).get<float>());
            auto radius = spec["radius"].get<float>();
            regionMin = center - vec2(radius);
            regionMax = center + vec2(radius);
        }

        auto extent = regionMax - regionMin;
        if (extent.x <= 0.0f || extent.y <= 0.0f)
            throw std::runtime_error("terrainMap: max has to be larger than min");

        auto size = spec.value("size", 2048);
        auto metersPerPixel = std::max(extent.x, extent.y) / float(size);
        auto width = int32_t(std::ceil(extent.x / metersPerPixel));
        auto height = int32_t(std::ceil(extent.y / metersPerPixel));
        auto contour = spec.value("contour", 5.0f);

        // +X to the right, +Z down
        auto toPixel = [&](vec2 world) { return (world - regionMin) / metersPerPixel; };
        auto toWorld = [&](float x, float y) { return regionMin + vec2(x + 0.5f, y + 0.5f) * metersPerPixel; };

        auto image = CreateImage(width, height, u8vec4(20, 20, 30, 255));
        auto& data = image->GetData();
        auto sunDirection = glm::normalize(vec3(-1.0f, 1.4f, -0.8f));

        for (int32_t y = 0; y < height; y++) {
            for (int32_t x = 0; x < width; x++) {
                auto world = toWorld(float(x), float(y));
                auto texel = grid.ToTexel(world);
                int32_t index;
                if (!grid.GetCell(texel, index))
                    continue;

                auto offset = (size_t(y) * width + x) * 4;
                if (grid.IsHole(texel)) {
                    data[offset] = 255, data[offset + 1] = 0, data[offset + 2] = 255;
                    continue;
                }

                auto center = grid.GetHeight(texel);
                auto step = std::max(1, int32_t(metersPerPixel / grid.texelSize));
                auto right = grid.GetHeight(texel + ivec2(step, 0));
                auto down = grid.GetHeight(texel + ivec2(0, step));
                right = right == FLT_MAX ? center : right;
                down = down == FLT_MAX ? center : down;
                auto distance = float(step) * grid.texelSize;
                auto normal = glm::normalize(vec3(center - right, distance, center - down));
                auto shade = 0.45f + 0.75f * std::max(0.0f, glm::dot(normal, sunDirection));

                auto color = vec3(MaterialColor(grid.GetMaterial(texel))) * shade;
                // Contour lines where the height crosses a multiple of the contour spacing
                if (contour > 0.0f && std::floor(center / contour) != std::floor(std::min(right, down) / contour))
                    color *= 0.6f;

                color = glm::clamp(color, vec3(0.0f), vec3(255.0f));
                data[offset] = uint8_t(color.r), data[offset + 1] = uint8_t(color.g), data[offset + 2] = uint8_t(color.b);
            }
        }

        // Footprints of mesh entities. Grass and other small vegetation would hide the terrain
        // Highlighted hierarchies are drawn on top in cyan, e.g. to see the footprint of a placed prefab
        EntitySet highlighted;
        if (spec.contains("highlight"))
            for (auto entity : FindEntities(scene, spec["highlight"].get<std::vector<std::string>>()))
                CollectHierarchy(entity, highlighted);

        if (spec.value("meshes", true) || !highlighted.empty()) {
            auto smallMeshes = spec.value("smallMeshes", false);
            auto meshSubset = scene.GetSubset<MeshComponent>();
            for (auto entityId : meshSubset) {
                const auto& mesh = meshSubset.Get(entityId);
                if (!mesh.visible || !mesh.mesh.IsLoaded())
                    continue;

                auto min = toPixel(vec2(mesh.aabb.min.x, mesh.aabb.min.z));
                auto max = toPixel(vec2(mesh.aabb.max.x, mesh.aabb.max.z));
                if (highlighted.contains(entityId)) {
                    for (auto py = int32_t(min.y); py <= int32_t(max.y); py++)
                        for (auto px = int32_t(min.x); px <= int32_t(max.x); px++)
                            BlendPixel(*image, px, py, u8vec4(0, 230, 255, 255), 0.25f);
                    continue;
                }
                if (!spec.value("meshes", true))
                    continue;

                // Colored by the naming conventions of common asset packs
                Scene::Entity entity = { entityId, &scene.entityManager };
                auto name = GetEntityName(entity);
                auto meshSize = mesh.aabb.max - mesh.aabb.min;
                if (meshSize.x > 400.0f || meshSize.z > 400.0f)
                    continue;
                if (!smallMeshes && meshSize.x < 1.0f && meshSize.z < 1.0f)
                    continue;

                u8vec4 color(40, 40, 40, 255);
                float alpha = 0.55f;
                if (name.find("Grass") != std::string::npos || name.find("Flower") != std::string::npos) {
                    if (!smallMeshes)
                        continue;
                    color = u8vec4(170, 230, 90, 255);
                    alpha = 0.35f;
                }
                else if (name.find("Tree") != std::string::npos || name.find("Bush") != std::string::npos ||
                    name.find("Fern") != std::string::npos) {
                    color = u8vec4(20, 70, 20, 255);
                    alpha = 0.7f;
                }
                else if (name.find("SM_Bld") != std::string::npos || name.find("Wall") != std::string::npos) {
                    color = u8vec4(120, 30, 30, 255);
                    alpha = 0.6f;
                }
                else if (name.find("Rock") != std::string::npos || name.find("Cliff") != std::string::npos) {
                    color = u8vec4(70, 70, 80, 255);
                }

                for (auto py = int32_t(min.y); py <= int32_t(max.y); py++)
                    for (auto px = int32_t(min.x); px <= int32_t(max.x); px++)
                        BlendPixel(*image, px, py, color, alpha);
            }
        }

        auto gridSpacing = spec.value("grid", 100.0f);
        if (gridSpacing > 0.0f) {
            auto labelHeight = std::max(12.0f, float(width) / 110.0f);
            for (auto value = std::ceil(regionMin.x / gridSpacing) * gridSpacing; value <= regionMax.x; value += gridSpacing) {
                auto px = toPixel(vec2(value, 0.0f)).x;
                DrawLine(*image, vec2(px, 0.0f), vec2(px, float(height - 1)), u8vec4(255), 0.35f);
                DrawText(*image, int32_t(px) + 3, 2, "x " + FormatFloat(value, 0), labelHeight, u8vec4(255, 255, 160, 255));
            }
            for (auto value = std::ceil(regionMin.y / gridSpacing) * gridSpacing; value <= regionMax.y; value += gridSpacing) {
                auto py = toPixel(vec2(0.0f, value)).y;
                DrawLine(*image, vec2(0.0f, py), vec2(float(width - 1), py), u8vec4(255), 0.35f);
                DrawText(*image, 3, int32_t(py) + 2, "z " + FormatFloat(value, 0), labelHeight, u8vec4(255, 255, 160, 255));
            }
        }

        if (spec.contains("markers")) {
            for (const auto& marker : spec["markers"]) {
                const auto& position = marker.at("position");
                auto pixel = toPixel(ToVec2(position));
                auto color = u8vec4(vec3(ToVec3(marker.value("color", json()), vec3(1.0f, 0.1f, 0.1f))) * 255.0f, 255.0f);
                auto radius = std::max(2.0f, marker.value("radius", 0.0f) / metersPerPixel);
                for (int32_t i = 0; i < 64; i++) {
                    auto a = float(i) / 64.0f * 6.2831853f, b = float(i + 1) / 64.0f * 6.2831853f;
                    DrawLine(*image, pixel + radius * vec2(std::cos(a), std::sin(a)),
                        pixel + radius * vec2(std::cos(b), std::sin(b)), color);
                }
                if (marker.contains("label"))
                    DrawText(*image, int32_t(pixel.x + radius + 3.0f), int32_t(pixel.y - 6.0f),
                        marker["label"].get<std::string>(), std::max(14.0f, float(width) / 90.0f), color);
            }
        }

        if (spec.contains("paths")) {
            for (const auto& path : spec["paths"]) {
                auto color = u8vec4(vec3(ToVec3(path.value("color", json()), vec3(1.0f, 0.9f, 0.1f))) * 255.0f, 255.0f);
                const auto& points = path.at("points");
                for (size_t i = 1; i < points.size(); i++)
                    DrawLine(*image, toPixel(ToVec2(points[i - 1])), toPixel(ToVec2(points[i])), color);
            }
        }

        return image;

    }

    // Brush sizes are in texels and have to stay below two cell edges (the brushes work on 3x3 cells)
    static int32_t GetBrushSize(const TexelGrid& grid, float radius) {

        auto size = 2 * int32_t(std::round(radius / grid.texelSize)) + 1;
        if (size >= 2 * grid.texelsPerCell)
            throw std::runtime_error("Terrain brush radius " + FormatFloat(radius, 1) + " is too large, the maximum is " +
                FormatFloat(float(grid.texelsPerCell - 1) * grid.texelSize, 1) + " m. Split it into several brushes");
        return std::max(size, 1);

    }

    // Stamps along a polyline in steps of the spacing
    static std::vector<vec2> SamplePolyline(const json& points, float spacing) {

        std::vector<vec2> samples;
        for (size_t i = 1; i < points.size(); i++) {
            auto a = ToVec2(points[i - 1]);
            auto b = ToVec2(points[i]);
            auto length = glm::distance(a, b);
            auto steps = std::max(1, int32_t(std::ceil(length / spacing)));
            for (int32_t step = i == 1 ? 0 : 1; step <= steps; step++)
                samples.push_back(glm::mix(a, b, float(step) / float(steps)));
        }
        if (points.size() == 1)
            samples.push_back(ToVec2(points[0]));
        return samples;

    }

    // Why a route cell is blocked: no ground (holes, outside of the terrain), steep slopes, avoided surfaces (e.g. water),
    // objects lying on the ground (rocks, houses) unless they match walkOn, and obstacles at knee and chest height
    enum class BlockReason : uint8_t {
        None,
        Ground,
        Slope,
        Avoided,
        Object,
        Obstacle
    };

    struct RouteCell {

        BlockReason reason = BlockReason::None;
        float height = 0.0f;
        float slope = 0.0f;

        bool IsBlocked() const { return reason != BlockReason::None; }

    };

    json FindRoute(Scene::Scene& scene, const json& query, const fs::path& outputDirectory) {

        const auto& terrain = GetCompleteTerrain(scene);

        std::vector<vec2> stops = { ToVec2(query.at("from")) };
        for (const auto& via : query.value("via", json::array()))
            stops.push_back(ToVec2(via));
        stops.push_back(ToVec2(query.at("to")));

        auto cellSize = query.value("cell", 2.0f);
        auto margin = query.value("margin", 60.0f);
        auto maxSlope = query.value("maxSlope", 30.0f);
        auto maxStep = query.value("maxStep", 0.6f);
        auto clearance = query.value("clearance", 1);
        auto avoidMaterials = query.value("avoidMaterials", std::vector<std::string>());
        auto avoidEntities = query.value("avoid", std::vector<std::string>());
        auto walkOn = query.value("walkOn", std::vector<std::string>());
        auto slopeWeight = query.value("slopeWeight", 2.0f);

        EntitySet ignored;
        for (auto entity : FindEntities(scene, query.value("ignore", std::vector<std::string>())))
            CollectHierarchy(entity, ignored);
        // The terrain is sampled directly, its height field body would count as an obstacle on slopes
        auto rigidBodySubset = scene.GetSubset<RigidBodyComponent>();
        for (auto entity : rigidBodySubset) {
            auto& rigidBody = rigidBodySubset.Get(entity);
            if (rigidBody.IsValid() && rigidBody.GetShape()->type == Physics::ShapeType::HeightField)
                ignored.insert(entity);
        }

        vec2 regionMin(FLT_MAX), regionMax(-FLT_MAX);
        for (auto stop : stops) {
            regionMin = glm::min(regionMin, stop - vec2(margin));
            regionMax = glm::max(regionMax, stop + vec2(margin));
        }
        auto size = ivec2(glm::ceil((regionMax - regionMin) / cellSize)) + 1;
        if (size.x * size.y > 400000)
            throw std::runtime_error("route: the region has too many cells, increase cell or reduce margin");

        auto toWorld = [&](ivec2 cell) { return regionMin + vec2(cell) * cellSize; };
        auto toCell = [&](vec2 world) {
            return glm::clamp(ivec2(glm::round((world - regionMin) / cellSize)), ivec2(0), size - 1);
        };
        auto index = [&](ivec2 cell) { return size_t(cell.y) * size.x + cell.x; };

        auto nameMatches = [&](Scene::Entity entity, const std::vector<std::string>& patterns) {

            for (; entity.IsValid(); entity = scene.GetParentEntity(entity))
                if (MatchAnyPattern(patterns, GetEntityName(entity)))
                    return true;
            return false;

        };

        std::vector<RouteCell> cells(size_t(size.x) * size.y);
        std::map<std::string, int32_t> blockers;
        auto countBlocker = [&](Scene::Entity entity) {

            // Counted by the name without the instance number, e.g. "Rocks SM_Env_Rock_01 17" as "Rocks SM_Env_Rock_01"
            auto name = GetEntityName(entity);
            auto end = name.find_last_not_of("0123456789");
            if (end != std::string::npos && end + 1 < name.size() && (name[end] == '_' || name[end] == ' '))
                name = name.substr(0, end);
            blockers[name]++;

        };
        for (int32_t y = 0; y < size.y; y++) {
            for (int32_t x = 0; x < size.x; x++) {
                auto& cell = cells[index({ x, y })];
                auto world = toWorld({ x, y });
                auto sample = SampleTerrain(terrain, world);
                if (!sample.valid || sample.hole) {
                    cell.reason = BlockReason::Ground;
                    continue;
                }

                auto hit = Raycast(scene, vec3(world.x, sample.height + 3.0f, world.y), vec3(0.0f, -1.0f, 0.0f), 6.0f,
                    ignored, CollisionComponents);
                if (!hit.valid) {
                    cell.reason = BlockReason::Ground;
                    continue;
                }
                cell.height = hit.position.y;
                cell.slope = hit.kind == "terrain" ? sample.slopeDegrees :
                    glm::degrees(std::acos(glm::clamp(std::abs(hit.normal.y), 0.0f, 1.0f)));

                if (hit.kind != "terrain") {
                    auto onTop = hit.position.y - sample.height > 0.35f;
                    if (MatchAnyPattern(avoidMaterials, GetHitMaterial(hit)) || nameMatches(hit.entity, avoidEntities)) {
                        cell.reason = BlockReason::Avoided;
                        continue;
                    }
                    if (onTop && !nameMatches(hit.entity, walkOn)) {
                        cell.reason = BlockReason::Object;
                        countBlocker(hit.entity);
                        continue;
                    }
                }
                if (cell.slope > maxSlope) {
                    cell.reason = BlockReason::Slope;
                    continue;
                }

                // Obstacles standing in the cell
                for (auto height : { 0.5f, 1.3f }) {
                    for (auto direction : { vec3(1.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, 1.0f) }) {
                        auto origin = vec3(world.x, cell.height + height, world.y) - direction * (cellSize * 0.5f);
                        auto obstacle = Raycast(scene, origin, direction, cellSize, ignored, CollisionComponents);
                        if (obstacle.valid && obstacle.kind != "terrain" && !nameMatches(obstacle.entity, walkOn)) {
                            cell.reason = BlockReason::Obstacle;
                            countBlocker(obstacle.entity);
                        }
                    }
                }
            }
        }

        // Keep a distance to blocked cells, except at the stops themselves
        std::vector<uint8_t> blocked(cells.size(), 0);
        int32_t blockedCount = 0;
        for (int32_t y = 0; y < size.y; y++) {
            for (int32_t x = 0; x < size.x; x++) {
                if (!cells[index({ x, y })].IsBlocked())
                    continue;
                blockedCount++;
                for (int32_t j = -clearance; j <= clearance; j++) {
                    for (int32_t i = -clearance; i <= clearance; i++) {
                        auto neighbor = ivec2(x + i, y + j);
                        if (glm::all(glm::greaterThanEqual(neighbor, ivec2(0))) && glm::all(glm::lessThan(neighbor, size)))
                            blocked[index(neighbor)] = 1;
                    }
                }
            }
        }
        for (auto stop : stops) {
            auto cell = toCell(stop);
            if (!cells[index(cell)].IsBlocked())
                blocked[index(cell)] = 0;
        }

        // Diagnostic image: free cells shaded by height, blocked cells colored by their reason, the route in yellow
        Ref<Image> map;
        if (query.contains("map") && !outputDirectory.empty()) {
            static const u8vec4 reasonColors[] = { { 120, 120, 120, 255 }, { 0, 0, 0, 255 }, { 230, 140, 30, 255 },
                { 40, 90, 230, 255 }, { 150, 150, 160, 255 }, { 220, 30, 30, 255 } };
            map = CreateImage(size.x, size.y);
            for (int32_t y = 0; y < size.y; y++) {
                for (int32_t x = 0; x < size.x; x++) {
                    const auto& cell = cells[index({ x, y })];
                    auto color = cell.IsBlocked() ? reasonColors[size_t(cell.reason)] :
                        (blocked[index({ x, y })] ? u8vec4(90, 140, 70, 255) : u8vec4(120, 200, 100, 255));
                    map->SetData(x, y, ivec4(color));
                }
            }
        }
        auto writeMap = [&]() {

            if (map)
                WritePng(map, outputDirectory / query["map"].get<std::string>());

        };

        auto passable = [&](ivec2 a, ivec2 b) {

            if (blocked[index(b)])
                return false;
            return std::abs(cells[index(a)].height - cells[index(b)].height) <= maxStep * glm::length(vec2(b - a));

        };

        // Line of sight over passable cells, used to simplify the grid path
        auto visible = [&](ivec2 a, ivec2 b) {

            auto steps = std::max(std::abs(b.x - a.x), std::abs(b.y - a.y));
            auto last = a;
            for (int32_t step = 1; step <= steps; step++) {
                auto cell = ivec2(glm::round(glm::mix(vec2(a), vec2(b), float(step) / float(steps))));
                if (cell != last && !passable(last, cell))
                    return false;
                last = cell;
            }
            return true;

        };

        json points = json::array();
        float totalLength = 0.0f;
        auto maxSegmentCells = size_t(query.value("maxSegment", 30.0f) / cellSize);

        for (size_t leg = 0; leg + 1 < stops.size(); leg++) {
            auto start = toCell(stops[leg]), goal = toCell(stops[leg + 1]);
            std::vector<float> cost(cells.size(), FLT_MAX);
            std::vector<int64_t> previous(cells.size(), -1);
            using Node = std::pair<float, int64_t>;
            std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;
            cost[index(start)] = 0.0f;
            open.push({ 0.0f, int64_t(index(start)) });

            while (!open.empty()) {
                auto current = open.top().second;
                open.pop();
                if (current == int64_t(index(goal)))
                    break;
                auto cell = ivec2(int32_t(current % size.x), int32_t(current / size.x));
                for (int32_t j = -1; j <= 1; j++) {
                    for (int32_t i = -1; i <= 1; i++) {
                        auto next = cell + ivec2(i, j);
                        if ((i == 0 && j == 0) || glm::any(glm::lessThan(next, ivec2(0))) ||
                            glm::any(glm::greaterThanEqual(next, size)) || !passable(cell, next))
                            continue;
                        auto distance = glm::length(vec2(i, j)) * cellSize;
                        auto slope = cells[index(next)].slope / std::max(maxSlope, 1.0f);
                        auto newCost = cost[current] + distance * (1.0f + slopeWeight * slope * slope);
                        if (newCost < cost[index(next)]) {
                            cost[index(next)] = newCost;
                            previous[index(next)] = current;
                            auto heuristic = glm::distance(vec2(next), vec2(goal)) * cellSize;
                            open.push({ newCost + heuristic, int64_t(index(next)) });
                        }
                    }
                }
            }

            if (previous[index(goal)] < 0 && goal != start) {
                writeMap();
                std::vector<std::pair<int32_t, std::string>> sorted;
                for (const auto& [name, count] : blockers)
                    sorted.push_back({ count, name });
                std::sort(sorted.rbegin(), sorted.rend());
                std::string top;
                for (size_t i = 0; i < sorted.size() && i < 12; i++)
                    top += (i ? ", " : "") + sorted[i].second + " " + std::to_string(sorted[i].first);
                throw std::runtime_error("route: no walkable route from " + FormatFloat(stops[leg].x, 1) + ", " +
                    FormatFloat(stops[leg].y, 1) + " to " + FormatFloat(stops[leg + 1].x, 1) + ", " + FormatFloat(stops[leg + 1].y, 1) +
                    " (" + std::to_string(blockedCount) + " of " + std::to_string(cells.size()) + " cells blocked, most by " + top + ")");
            }

            std::vector<ivec2> path;
            for (auto current = int64_t(index(goal)); current >= 0; current = previous[current])
                path.push_back(ivec2(int32_t(current % size.x), int32_t(current / size.x)));
            std::reverse(path.begin(), path.end());
            if (map)
                for (auto cell : path)
                    map->SetData(cell.x, cell.y, ivec4(255, 230, 0, 255));

            std::vector<ivec2> simplified = { path.front() };
            size_t anchor = 0;
            for (size_t i = 2; i < path.size(); i++) {
                if (!visible(path[anchor], path[i]) || i - anchor > maxSegmentCells) {
                    anchor = i - 1;
                    simplified.push_back(path[anchor]);
                }
            }
            if (path.size() > 1)
                simplified.push_back(path.back());

            for (size_t i = leg == 0 ? 0 : 1; i < simplified.size(); i++) {
                auto world = toWorld(simplified[i]);
                auto height = cells[index(simplified[i])].height;
                if (!points.empty()) {
                    const auto& last = points.back();
                    totalLength += glm::distance(
                        vec3(last[0].get<float>(), last[1].get<float>(), last[2].get<float>()), vec3(world.x, height, world.y));
                }
                points.push_back({ world.x, height, world.y });
            }
        }

        writeMap();
        return { { "points", points }, { "length", totalLength }, { "cells", cells.size() }, { "blockedCells", blockedCount } };

    }

    json ApplyTerrainBrushes(Scene::Scene& scene, const json& brushes) {

        const auto& terrain = GetCompleteTerrain(scene);
        auto grid = GetTexelGrid(terrain);
        json log = json::array();

        for (const auto& brush : brushes) {
            auto type = brush.at("type").get<std::string>();
            json entry = { { "type", type } };

            if (type == "flatten") {
                // A falloff ring blends towards the target height with decreasing strength
                auto position = ToVec2(brush.at("position"));
                auto radius = brush.at("radius").get<float>();
                auto targetHeight = brush.at("height").get<float>();
                auto strength = brush.value("strength", 1.0f);
                auto falloff = brush.value("falloff", 0.0f);
                auto before = SampleTerrain(terrain, position);

                Tools::TerrainTool::FlattenHeight(terrain, GetBrushSize(grid, radius), strength, position, targetHeight);
                auto rings = int32_t(std::ceil(falloff / std::max(grid.texelSize * 2.0f, 0.5f)));
                for (int32_t ring = 1; ring <= rings; ring++) {
                    auto fraction = float(ring) / float(rings + 1);
                    auto ringStrength = strength * (1.0f - fraction) / float(rings) * 2.0f;
                    Tools::TerrainTool::FlattenHeight(terrain, GetBrushSize(grid, radius + falloff * float(ring) / float(rings)),
                        Helper::Math::Saturate(ringStrength), position, targetHeight);
                }
                entry["heightBefore"] = before.height;
                entry["heightAfter"] = SampleTerrain(terrain, position).height;
            }
            else if (type == "raise") {
                // Gaussian mound (or dip with a negative amount) of the given height in meters at its center
                auto position = ToVec2(brush.at("position"));
                auto radius = brush.at("radius").get<float>();
                auto amount = brush.at("amount").get<float>();
                auto halfSize = (GetBrushSize(grid, radius) - 1) / 2;

                Filter filter;
                filter.CalculateGaussianFilter(float(halfSize) / 2.5f, uint32_t(halfSize));
                std::vector<std::vector<float>> weights;
                std::vector<std::vector<ivec2>> offsets;
                filter.Get(&weights, &offsets);
                auto centerWeight = weights[halfSize][halfSize];

                auto before = SampleTerrain(terrain, position);
                Tools::TerrainTool::BrushHeight(terrain, &filter, amount / centerWeight, position);
                entry["heightBefore"] = before.height;
                entry["heightAfter"] = SampleTerrain(terrain, position).height;
            }
            else if (type == "smooth") {
                auto radius = brush.at("radius").get<float>();
                auto size = GetBrushSize(grid, radius);
                auto points = brush.contains("points") ? SamplePolyline(brush["points"], radius * 0.5f) :
                    std::vector<vec2> { ToVec2(brush.at("position")) };
                auto iterations = brush.value("iterations", 1);
                for (int32_t i = 0; i < iterations; i++)
                    for (auto point : points)
                        Tools::TerrainTool::SmoothHeight(terrain, size, brush.value("contributing", 2),
                            brush.value("strength", 0.5f), point);
                entry["stamps"] = points.size() * iterations;
            }
            else if (type == "hole") {
                auto position = ToVec2(brush.at("position"));
                Tools::TerrainTool::BrushHole(terrain, position, GetBrushSize(grid, brush.at("radius").get<float>()));
            }
            else if (type == "material" || type == "path") {
                // A path paints the material along a polyline and can level its ground with a smoothing pass
                auto radius = type == "path" ? brush.at("width").get<float>() * 0.5f : brush.at("radius").get<float>();
                auto slot = brush.at("slot").get<int32_t>();
                if (slot < 0 || slot >= int32_t(terrain->storage->materials.size()) ||
                    !terrain->storage->materials[slot].IsValid())
                    throw std::runtime_error("Terrain material slot " + std::to_string(slot) + " is empty");

                auto points = type == "path" ? SamplePolyline(brush.at("points"), brush.value("spacing", radius * 0.5f)) :
                    std::vector<vec2> { ToVec2(brush.at("position")) };
                auto size = GetBrushSize(grid, radius);
                auto smooth = brush.value("smooth", 0.0f);
                for (auto point : points) {
                    if (smooth > 0.0f)
                        Tools::TerrainTool::SmoothHeight(terrain, size, 3, smooth, point);
                    Tools::TerrainTool::BrushMaterial(terrain, point, size, slot);
                }
                entry["stamps"] = points.size();
            }
            else {
                throw std::runtime_error("Unknown terrain brush '" + type + "' (flatten, raise, smooth, hole, material, path)");
            }

            log.push_back(entry);
        }

        RebuildTerrainBody(scene);
        return log;

    }

    void RebuildTerrainBody(Scene::Scene& scene) {

        const auto& terrain = GetCompleteTerrain(scene);
        auto heightImage = terrain->GetHeightField(terrain->LoDCount - 1);

        Physics::HeightFieldShapeSettings shapeSettings {
            .heightData = heightImage.GetData(),
            .translation = terrain->translation,
            .scale = vec3(terrain->resolution, terrain->heightScale, terrain->resolution)
        };
        auto shape = Physics::ShapesManager::CreateShape(shapeSettings);

        Scene::Entity bodyEntity;
        auto rigidBodySubset = scene.GetSubset<RigidBodyComponent>();
        for (auto entity : rigidBodySubset) {
            auto& rigidBody = rigidBodySubset.Get(entity);
            if (rigidBody.IsValid() && rigidBody.GetShape()->type == Physics::ShapeType::HeightField) {
                bodyEntity = { entity, &scene.entityManager };
                break;
            }
        }
        // Scenes without physics for their terrain have nothing to update
        if (!bodyEntity.IsValid())
            return;

        bodyEntity.RemoveComponent<RigidBodyComponent>();
        bodyEntity.AddComponent<RigidBodyComponent>(Physics::BodyCreationSettings { .shape = shape });

    }

    void CopySceneTerrain(Scene::Scene& scene, const std::string& path, bool overwrite) {

        if (!scene.terrain.IsLoaded())
            throw std::runtime_error("The scene has no loaded terrain");

        auto sourcePath = scene.terrain.GetResource()->path;
        auto targetPath = ToAssetPath(path);
        if (targetPath == sourcePath)
            throw std::runtime_error("terrainCopy: the target is the current terrain " + targetPath);

        auto source = GetAssetDirectory() / sourcePath;
        auto target = GetAssetDirectory() / targetPath;
        if (fs::exists(target) && !overwrite)
            throw std::runtime_error("terrainCopy: " + targetPath + " already exists, set \"overwrite\": true");

        fs::create_directories(target.parent_path());
        fs::copy_file(source, target, fs::copy_options::overwrite_existing);

        auto copy = ResourceManager<Terrain::Terrain>::GetOrLoadResourceWithLoader(
            targetPath, ResourceOrigin::User, Loader::TerrainLoader::LoadTerrain, false);
        copy.WaitForLoad();
        if (!copy.IsLoaded())
            throw std::runtime_error("terrainCopy: couldn't load " + targetPath);

        scene.terrain = copy;
        GetCompleteTerrain(scene);

    }

    void SaveSceneTerrain(Scene::Scene& scene) {

        const auto& terrain = GetCompleteTerrain(scene);
        const auto& path = scene.terrain.GetResource()->path;

        Tools::TerrainTool::LoadMissingCells(terrain, path);
        Tools::TerrainTool::BakeTerrain(terrain);
        Loader::TerrainLoader::SaveTerrain(terrain, path);

    }

}
