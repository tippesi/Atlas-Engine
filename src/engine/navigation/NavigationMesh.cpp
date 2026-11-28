#include "NavigationMesh.h"
#include "../scene/Scene.h"
#include "../physics/MathConversion.h"
#include "../jobsystem/JobSystem.h"
#include "../common/RandomHelper.h"
#include "../common/MathHelper.h"
#include "../Log.h"

#include <Recast.h>
#include <DetourNavMesh.h>
#include <DetourNavMeshBuilder.h>
#include <DetourNavMeshQuery.h>

#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/TransformedShape.h>

namespace Atlas {

    namespace Navigation {

        // Small enough for 32 bit polygon references with 1024 tiles in memory
        static constexpr int32_t maxTileCount = 1024;
        static constexpr int32_t maxPolyBits = 12;
        static constexpr int32_t maxPathPolys = 2048;
        static constexpr int32_t maxPathCorners = 256;
        static constexpr unsigned short walkableFlag = 1;

        struct NavigationMesh::TileBuild {
            ivec2 coord;
            int64_t id;
            Volume::AABB bounds;

            std::vector<float> vertices;
            std::vector<int32_t> triangles;
            // Shapes keep a reference to their immutable geometry, the triangles are read in the build job
            JPH::Array<JPH::TransformedShape> shapes;

            unsigned char* data = nullptr;
            int32_t dataSize = 0;
            std::atomic_bool finished = false;
        };

        NavigationMesh::NavigationMesh(const NavigationSettings& settings) : settings(settings) {

            tileCellCount = int32_t(glm::round(settings.tileSize / settings.cellSize));
            tileWorldSize = float(tileCellCount) * settings.cellSize;
            maxPolysPerTile = 1 << maxPolyBits;

            dtNavMeshParams params = {};
            params.tileWidth = tileWorldSize;
            params.tileHeight = tileWorldSize;
            params.maxTiles = maxTileCount;
            params.maxPolys = maxPolysPerTile;

            navMesh = dtAllocNavMesh();
            auto status = navMesh->init(&params);
            AE_ASSERT(dtStatusSucceed(status) && "Couldn't initialize navigation mesh");

            query = dtAllocNavMeshQuery();
            query->init(navMesh, 4096);

            filter = new dtQueryFilter();

        }

        NavigationMesh::~NavigationMesh() {

            JobSystem::Wait(jobGroup);

            for (auto& build : builds)
                dtFree(build->data);

            delete filter;
            dtFreeNavMeshQuery(query);
            dtFreeNavMesh(navMesh);

        }

        void NavigationMesh::Update(Scene::Scene* scene, vec3 center, float radius) {

            InvalidateChanges(scene);

            // Tiles past the capacity couldn't be added, the near ones would be missing
            radius = glm::min(radius, GetMaxRadius());

            std::erase_if(builds, [&](const Ref<TileBuild>& build) {
                if (!build->finished)
                    return false;
                FinishBuild(*build);
                return true;
                });

            auto tileDistance = [&](ivec2 coord) {
                auto min = vec2(coord) * tileWorldSize;
                auto closest = glm::clamp(vec2(center.x, center.z), min, min + tileWorldSize);
                return glm::distance(closest, vec2(center.x, center.z));
                };

            // Tiles stay a bit longer than they are needed, so moving back and forth doesn't rebuild them
            std::erase_if(tiles, [&](auto& item) {
                auto& tile = item.second;
                if (tileDistance(tile.coord) <= radius + tileWorldSize)
                    return false;
                RemoveTile(tile);
                return true;
                });

            auto minCoord = ivec2(glm::floor((vec2(center.x, center.z) - radius) / tileWorldSize));
            auto maxCoord = ivec2(glm::floor((vec2(center.x, center.z) + radius) / tileWorldSize));

            std::vector<std::pair<float, ivec2>> candidates;
            for (int32_t y = minCoord.y; y <= maxCoord.y; y++) {
                for (int32_t x = minCoord.x; x <= maxCoord.x; x++) {
                    auto coord = ivec2(x, y);
                    auto distance = tileDistance(coord);
                    if (distance > radius)
                        continue;

                    auto& tile = tiles[GetTileKey(coord)];
                    tile.coord = coord;
                    if (!tile.built && !tile.buildId)
                        candidates.emplace_back(distance, coord);
                }
            }

            std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

            auto startCount = 0;
            for (const auto& [_, coord] : candidates) {
                if (int32_t(builds.size()) >= maxConcurrentBuilds || startCount >= maxTileStartsPerUpdate)
                    break;

                StartBuild(scene, tiles[GetTileKey(coord)]);
                startCount++;
            }

        }

        void NavigationMesh::Invalidate(const Volume::AABB& aabb) {

            for (auto& [_, tile] : tiles) {
                if (!GetTileBounds(tile.coord, 0.0f).Intersects(aabb))
                    continue;

                // The current tile stays until the new one replaces it
                tile.built = false;
                tile.buildId = 0;
            }

        }

        void NavigationMesh::Invalidate() {

            for (auto& [_, tile] : tiles) {
                tile.built = false;
                tile.buildId = 0;
            }

        }

        bool NavigationMesh::IsBuilt(vec3 position) const {

            auto coord = ivec2(glm::floor(vec2(position.x, position.z) / tileWorldSize));
            auto iter = tiles.find(GetTileKey(coord));
            return iter != tiles.end() && iter->second.built;

        }

        std::vector<vec3> NavigationMesh::FindPath(vec3 start, vec3 end) const {

            dtPolyRef startRef = 0, endRef = 0;
            vec3 startPoint, endPoint;
            query->findNearestPoly(&start.x, &searchExtents.x, filter, &startRef, &startPoint.x);
            query->findNearestPoly(&end.x, &searchExtents.x, filter, &endRef, &endPoint.x);
            if (!startRef || !endRef)
                return {};

            std::vector<dtPolyRef> polys(maxPathPolys);
            int32_t polyCount = 0;
            auto status = query->findPath(startRef, endRef, &startPoint.x, &endPoint.x, filter,
                polys.data(), &polyCount, maxPathPolys);
            if (dtStatusFailed(status) || dtStatusDetail(status, DT_PARTIAL_RESULT) || !polyCount || polys[polyCount - 1] != endRef)
                return {};

            std::vector<vec3> corners(maxPathCorners);
            int32_t cornerCount = 0;
            status = query->findStraightPath(&startPoint.x, &endPoint.x, polys.data(), polyCount,
                &corners.front().x, nullptr, nullptr, &cornerCount, maxPathCorners);
            // A path cut off at the corner limit would end short of the goal
            if (dtStatusFailed(status) || dtStatusDetail(status, DT_BUFFER_TOO_SMALL))
                return {};

            corners.resize(cornerCount);
            return corners;

        }

        std::optional<vec3> NavigationMesh::FindNearestPoint(vec3 position, vec3 halfExtents) const {

            dtPolyRef ref = 0;
            vec3 point;
            query->findNearestPoly(&position.x, &halfExtents.x, filter, &ref, &point.x);
            if (!ref)
                return std::nullopt;

            return point;

        }

        std::optional<vec3> NavigationMesh::MoveAlongSurface(vec3 from, vec3 to) const {

            dtPolyRef startRef = 0;
            vec3 startPoint;
            query->findNearestPoly(&from.x, &searchExtents.x, filter, &startRef, &startPoint.x);
            if (!startRef)
                return std::nullopt;

            vec3 result;
            dtPolyRef visited[16];
            int32_t visitedCount = 0;
            auto status = query->moveAlongSurface(startRef, &startPoint.x, &to.x, filter, &result.x, visited, &visitedCount, 16);
            if (dtStatusFailed(status) || !visitedCount)
                return std::nullopt;

            // The result keeps the height of the start, the polygon knows the actual one. A long move doesn't keep
            // all polygons it crossed, the last one is then found again
            auto endRef = visited[visitedCount - 1];
            if (dtStatusDetail(status, DT_BUFFER_TOO_SMALL) || dtStatusFailed(query->getPolyHeight(endRef, &result.x, &result.y))) {
                vec3 nearest;
                query->findNearestPoly(&result.x, &searchExtents.x, filter, &endRef, &nearest.x);
                if (!endRef)
                    return std::nullopt;
                result.y = nearest.y;
            }
            return result;

        }

        bool NavigationMesh::IsWalkable(vec3 from, vec3 to) const {

            dtPolyRef startRef = 0;
            vec3 startPoint;
            query->findNearestPoly(&from.x, &searchExtents.x, filter, &startRef, &startPoint.x);
            if (!startRef)
                return false;

            auto hitFraction = 0.0f;
            vec3 hitNormal;
            dtPolyRef path[maxPathCorners];
            int32_t pathCount = 0;
            auto status = query->raycast(startRef, &startPoint.x, &to.x, filter, &hitFraction, &hitNormal.x,
                path, &pathCount, maxPathCorners);
            if (dtStatusFailed(status) || hitFraction != FLT_MAX || !pathCount)
                return false;

            // The ray is tested in 2D, the end has to be on the same floor
            auto height = 0.0f;
            if (dtStatusFailed(query->getPolyHeight(path[pathCount - 1], &to.x, &height)))
                return false;

            return glm::abs(height - to.y) <= settings.agentHeight * 0.5f;

        }

        std::optional<vec3> NavigationMesh::FindRandomPoint(vec3 center, float radius) const {

            dtPolyRef startRef = 0;
            vec3 startPoint;
            query->findNearestPoly(&center.x, &searchExtents.x, filter, &startRef, &startPoint.x);
            if (!startRef)
                return std::nullopt;

            dtPolyRef ref = 0;
            vec3 point;
            auto status = query->findRandomPointAroundCircle(startRef, &startPoint.x, radius, filter,
                Common::Random::SampleUniformFloat, &ref, &point.x);
            if (dtStatusFailed(status) || !ref)
                return std::nullopt;

            return point;

        }

        int32_t NavigationMesh::GetTileCount() const {

            return int32_t(std::count_if(tiles.begin(), tiles.end(), [](const auto& item) { return item.second.ref != 0; }));

        }

        float NavigationMesh::GetMaxRadius() const {

            // Tiles stay up to a tile further than the radius, some room is left for tiles at the edge of the circle
            return tileWorldSize * (glm::sqrt(0.9f * float(maxTileCount) / glm::pi<float>()) - 1.5f);

        }

        int32_t NavigationMesh::GetPendingTileCount() const {

            return int32_t(std::count_if(tiles.begin(), tiles.end(), [](const auto& item) { return !item.second.built; }));

        }

        std::vector<vec3> NavigationMesh::GetPolygonEdges(const Volume::AABB& aabb) const {

            std::vector<vec3> edges;
            for (const auto& [_, tile] : tiles) {
                if (!tile.ref)
                    continue;

                auto meshTile = navMesh->getTileByRef(tile.ref);
                if (!meshTile || !meshTile->header)
                    continue;

                auto header = meshTile->header;
                auto bounds = Volume::AABB(vec3(header->bmin[0], header->bmin[1], header->bmin[2]),
                    vec3(header->bmax[0], header->bmax[1], header->bmax[2]));
                if (!bounds.Intersects(aabb))
                    continue;

                for (int32_t i = 0; i < header->polyCount; i++) {
                    const auto& poly = meshTile->polys[i];
                    if (poly.getType() != DT_POLYTYPE_GROUND)
                        continue;

                    for (int32_t j = 0; j < poly.vertCount; j++) {
                        // An edge between two polygons of the tile belongs to both, the one with the higher index draws it
                        auto neighbour = poly.neis[j];
                        if (neighbour && !(neighbour & DT_EXT_LINK) && int32_t(neighbour) - 1 < i)
                            continue;

                        auto from = &meshTile->verts[poly.verts[j] * 3];
                        auto to = &meshTile->verts[poly.verts[(j + 1) % poly.vertCount] * 3];
                        edges.emplace_back(from[0], from[1], from[2]);
                        edges.emplace_back(to[0], to[1], to[2]);
                    }
                }
            }

            return edges;

        }

        void NavigationMesh::InvalidateChanges(Scene::Scene* scene) {

            auto invalidateSince = [&](const Volume::BoundsChangeLog& log, uint64_t& count) {
                for (const auto& aabb : log.GetSince(count))
                    Invalidate(aabb);
                };

            if (scene->physicsWorld.get() != builtPhysicsWorld) {
                builtPhysicsWorld = scene->physicsWorld.get();
                physicsChangeCount = 0;
                Invalidate();
            }
            if (scene->physicsWorld)
                invalidateSince(scene->physicsWorld->staticChanges, physicsChangeCount);

        }

        void NavigationMesh::StartBuild(Scene::Scene* scene, Tile& tile) {

            auto build = CreateRef<TileBuild>();
            build->coord = tile.coord;
            build->bounds = GetTileBounds(tile.coord, float(int32_t(glm::ceil(settings.agentRadius / settings.cellSize)) + 3) * settings.cellSize);

            if (scene->physicsWorld) {
                // Moving bodies aren't part of the mesh
                class StaticBodyFilter : public JPH::BodyFilter {
                public:
                    bool ShouldCollideLocked(const JPH::Body& body) const override { return body.IsStatic() && !body.IsSensor(); }
                };

                JPH::AABox box(Physics::VecToJPHVec(build->bounds.min), Physics::VecToJPHVec(build->bounds.max));

                JPH::AllHitCollisionCollector<JPH::TransformedShapeCollector> collector;
                StaticBodyFilter bodyFilter;
                scene->physicsWorld->system->GetNarrowPhaseQuery().CollectTransformedShapes(box, collector,
                    JPH::BroadPhaseLayerFilter(), JPH::ObjectLayerFilter(), bodyFilter);
                build->shapes = std::move(collector.mHits);
            }

            tile.buildId = ++buildCounter;
            build->id = tile.buildId;
            builds.push_back(build);

            // Builds started before a change of the priority finish in their pool, the group counts them all
            jobGroup.priority = buildPriority;
            JobSystem::Execute(jobGroup, [this, build](JobData&) {
                BuildTile(*build);
                build->finished = true;
                });

        }

        void NavigationMesh::BuildTile(TileBuild& build) const {

            auto& vertices = build.vertices;
            auto& triangles = build.triangles;

            // Large meshes return whole nodes of their tree, triangles outside of the tile are skipped
            JPH::AABox box(Physics::VecToJPHVec(build.bounds.min), Physics::VecToJPHVec(build.bounds.max));
            const int32_t maxTriangles = 256;
            std::vector<JPH::Float3> triangleVertices(maxTriangles * 3);
            for (const auto& shape : build.shapes) {
                JPH::TransformedShape::GetTrianglesContext context;
                shape.GetTrianglesStart(context, box, JPH::RVec3::sZero());

                while (auto count = shape.GetTrianglesNext(context, maxTriangles, triangleVertices.data())) {
                    for (int32_t i = 0; i < count; i++) {
                        const auto* triangle = &triangleVertices[i * 3];
                        JPH::AABox triangleBox;
                        for (int32_t j = 0; j < 3; j++)
                            triangleBox.Encapsulate(JPH::Vec3(triangle[j]));
                        if (!box.Overlaps(triangleBox))
                            continue;

                        for (int32_t j = 0; j < 3; j++) {
                            triangles.push_back(int32_t(vertices.size() / 3));
                            vertices.insert(vertices.end(), { triangle[j].x, triangle[j].y, triangle[j].z });
                        }
                    }
                }
            }
            build.shapes.clear();

            if (triangles.empty())
                return;

            rcConfig config = {};
            config.cs = settings.cellSize;
            config.ch = settings.cellHeight;
            config.walkableSlopeAngle = settings.agentMaxSlope;
            config.walkableHeight = int32_t(glm::ceil(settings.agentHeight / settings.cellHeight));
            config.walkableClimb = int32_t(glm::floor(settings.agentMaxClimb / settings.cellHeight));
            config.walkableRadius = int32_t(glm::ceil(settings.agentRadius / settings.cellSize));
            config.maxEdgeLen = int32_t(12.0f / settings.cellSize);
            config.maxSimplificationError = 1.3f;
            config.minRegionArea = 64;
            config.mergeRegionArea = 400;
            config.maxVertsPerPoly = DT_VERTS_PER_POLYGON;
            config.tileSize = tileCellCount;
            config.borderSize = config.walkableRadius + 3;
            config.width = config.tileSize + config.borderSize * 2;
            config.height = config.tileSize + config.borderSize * 2;
            config.detailSampleDist = settings.cellSize * 6.0f;
            config.detailSampleMaxError = settings.cellHeight;

            auto boundsMin = vec3(build.bounds.min.x, FLT_MAX, build.bounds.min.z);
            auto boundsMax = vec3(build.bounds.max.x, -FLT_MAX, build.bounds.max.z);
            for (size_t i = 1; i < vertices.size(); i += 3) {
                boundsMin.y = glm::min(boundsMin.y, vertices[i]);
                boundsMax.y = glm::max(boundsMax.y, vertices[i]);
            }
            rcVcopy(config.bmin, &boundsMin.x);
            rcVcopy(config.bmax, &boundsMax.x);

            // Meshes don't have a consistent winding, so faces pointing down count as floors as well. Below other
            // geometry they don't have the clearance of a floor and are removed by the filters
            auto walkableNormalY = glm::cos(glm::radians(settings.agentMaxSlope));
            std::vector<unsigned char> areas(triangles.size() / 3, RC_NULL_AREA);
            auto vertex = [&](int32_t idx) { return vec3(vertices[idx * 3], vertices[idx * 3 + 1], vertices[idx * 3 + 2]); };
            for (size_t i = 0; i < areas.size(); i++) {
                auto v0 = vertex(triangles[i * 3]);
                auto v1 = vertex(triangles[i * 3 + 1]);
                auto v2 = vertex(triangles[i * 3 + 2]);
                auto normal = glm::cross(v1 - v0, v2 - v0);
                auto length = glm::length(normal);
                if (length > 0.0f && glm::abs(normal.y) >= walkableNormalY * length)
                    areas[i] = RC_WALKABLE_AREA;
            }

            rcContext context(false);

            std::unique_ptr<rcHeightfield, decltype(&rcFreeHeightField)> heightfield(rcAllocHeightfield(), rcFreeHeightField);
            if (!rcCreateHeightfield(&context, *heightfield, config.width, config.height, config.bmin, config.bmax, config.cs, config.ch))
                return;

            if (!rcRasterizeTriangles(&context, vertices.data(), int32_t(vertices.size() / 3), triangles.data(), areas.data(),
                int32_t(areas.size()), *heightfield, config.walkableClimb))
                return;

            rcFilterLowHangingWalkableObstacles(&context, config.walkableClimb, *heightfield);
            rcFilterLedgeSpans(&context, config.walkableHeight, config.walkableClimb, *heightfield);
            rcFilterWalkableLowHeightSpans(&context, config.walkableHeight, *heightfield);

            std::unique_ptr<rcCompactHeightfield, decltype(&rcFreeCompactHeightfield)> compactHeightfield(
                rcAllocCompactHeightfield(), rcFreeCompactHeightfield);
            if (!rcBuildCompactHeightfield(&context, config.walkableHeight, config.walkableClimb, *heightfield, *compactHeightfield))
                return;
            heightfield.reset();

            if (!rcErodeWalkableArea(&context, config.walkableRadius, *compactHeightfield) ||
                !rcBuildDistanceField(&context, *compactHeightfield) ||
                !rcBuildRegions(&context, *compactHeightfield, config.borderSize, config.minRegionArea, config.mergeRegionArea))
                return;

            std::unique_ptr<rcContourSet, decltype(&rcFreeContourSet)> contours(rcAllocContourSet(), rcFreeContourSet);
            if (!rcBuildContours(&context, *compactHeightfield, config.maxSimplificationError, config.maxEdgeLen, *contours) ||
                !contours->nconts)
                return;

            std::unique_ptr<rcPolyMesh, decltype(&rcFreePolyMesh)> polyMesh(rcAllocPolyMesh(), rcFreePolyMesh);
            if (!rcBuildPolyMesh(&context, *contours, config.maxVertsPerPoly, *polyMesh))
                return;

            std::unique_ptr<rcPolyMeshDetail, decltype(&rcFreePolyMeshDetail)> detailMesh(rcAllocPolyMeshDetail(), rcFreePolyMeshDetail);
            if (!rcBuildPolyMeshDetail(&context, *polyMesh, *compactHeightfield, config.detailSampleDist,
                config.detailSampleMaxError, *detailMesh))
                return;

            if (!polyMesh->npolys)
                return;

            if (polyMesh->npolys > maxPolysPerTile || polyMesh->nverts >= 0xffff) {
                Log::Warning("Navigation tile " + std::to_string(build.coord.x) + ", " + std::to_string(build.coord.y) +
                    " has too many polygons (" + std::to_string(polyMesh->npolys) + "), it stays empty");
                return;
            }

            for (int32_t i = 0; i < polyMesh->npolys; i++) {
                polyMesh->flags[i] = polyMesh->areas[i] == RC_WALKABLE_AREA ? walkableFlag : 0;
            }

            dtNavMeshCreateParams params = {};
            params.verts = polyMesh->verts;
            params.vertCount = polyMesh->nverts;
            params.polys = polyMesh->polys;
            params.polyAreas = polyMesh->areas;
            params.polyFlags = polyMesh->flags;
            params.polyCount = polyMesh->npolys;
            params.nvp = polyMesh->nvp;
            params.detailMeshes = detailMesh->meshes;
            params.detailVerts = detailMesh->verts;
            params.detailVertsCount = detailMesh->nverts;
            params.detailTris = detailMesh->tris;
            params.detailTriCount = detailMesh->ntris;
            params.walkableHeight = settings.agentHeight;
            params.walkableRadius = settings.agentRadius;
            params.walkableClimb = settings.agentMaxClimb;
            params.tileX = build.coord.x;
            params.tileY = build.coord.y;
            params.tileLayer = 0;
            rcVcopy(params.bmin, polyMesh->bmin);
            rcVcopy(params.bmax, polyMesh->bmax);
            params.cs = config.cs;
            params.ch = config.ch;
            params.buildBvTree = true;

            if (!dtCreateNavMeshData(&params, &build.data, &build.dataSize)) {
                build.data = nullptr;
                build.dataSize = 0;
            }

        }

        void NavigationMesh::FinishBuild(TileBuild& build) {

            auto iter = tiles.find(GetTileKey(build.coord));
            if (iter == tiles.end() || iter->second.buildId != build.id) {
                dtFree(build.data);
                build.data = nullptr;
                return;
            }

            auto& tile = iter->second;
            tile.buildId = 0;
            tile.built = true;
            RemoveTile(tile);

            if (!build.data)
                return;

            dtTileRef ref = 0;
            auto status = navMesh->addTile(build.data, build.dataSize, DT_TILE_FREE_DATA, 0, &ref);
            if (dtStatusFailed(status)) {
                Log::Warning("Couldn't add navigation tile " + std::to_string(build.coord.x) + ", " + std::to_string(build.coord.y));
                dtFree(build.data);
                // Built again later instead of staying empty
                tile.built = false;
            }
            else {
                tile.ref = ref;
                revision++;
            }
            build.data = nullptr;

        }

        void NavigationMesh::RemoveTile(Tile& tile) {

            if (!tile.ref)
                return;

            navMesh->removeTile(tile.ref, nullptr, nullptr);
            tile.ref = 0;
            revision++;

        }

        Volume::AABB NavigationMesh::GetTileBounds(ivec2 coord, float border) const {

            auto min = vec2(coord) * tileWorldSize - border;
            auto max = vec2(coord + 1) * tileWorldSize + border;
            // The height covers every walkable floor of a world
            return Volume::AABB(vec3(min.x, -100000.0f, min.y), vec3(max.x, 100000.0f, max.y));

        }

        int64_t NavigationMesh::GetTileKey(ivec2 coord) {

            return (int64_t(coord.x) << 32) | int64_t(uint32_t(coord.y));

        }

    }

}
