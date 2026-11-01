#pragma once

#include "../System.h"
#include "../volume/AABB.h"
#include "../jobsystem/JobGroup.h"

#include <optional>
#include <unordered_map>

class dtNavMesh;
class dtNavMeshQuery;
class dtQueryFilter;

namespace Atlas {

    namespace Scene {

        class Scene;

    }

    namespace Navigation {

        struct NavigationSettings {

            bool operator==(const NavigationSettings&) const = default;

            float agentRadius = 0.3f;
            float agentHeight = 1.8f;
            float agentMaxClimb = 0.4f;
            // In degrees
            float agentMaxSlope = 50.0f;

            float cellSize = 0.15f;
            float cellHeight = 0.1f;
            float tileSize = 48.0f;

        };

        /**
         * A tiled navigation mesh, built from the static bodies of a scene around a center, a terrain through its
         * height field body. Tiles are built with the job system, all queries are made on the main thread. Tiles
         * are rebuilt where static bodies change.
         */
        class NavigationMesh {

        public:
            explicit NavigationMesh(const NavigationSettings& settings = {});

            NavigationMesh(const NavigationMesh&) = delete;

            ~NavigationMesh();

            // Builds the missing tiles within the radius around the center, tiles further away are removed
            void Update(Scene::Scene* scene, vec3 center, float radius);

            // Rebuilds all tiles
            void Invalidate();

            // Rebuilds the tiles overlapping the bounds, e.g. after static geometry changed
            void Invalidate(const Volume::AABB& aabb);

            // Whether the tile at the position is built (it can still be empty)
            bool IsBuilt(vec3 position) const;

            // Corners of the path, empty if the end can't be reached on the built tiles
            std::vector<vec3> FindPath(vec3 start, vec3 end) const;

            std::optional<vec3> FindNearestPoint(vec3 position, vec3 halfExtents) const;

            // Moves from a point on the mesh towards the target and slides along walls and edges
            std::optional<vec3> MoveAlongSurface(vec3 from, vec3 to) const;

            // A straight walk on the mesh from one point to the other, on the same floor
            bool IsWalkable(vec3 from, vec3 to) const;

            std::optional<vec3> FindRandomPoint(vec3 center, float radius) const;

            int32_t GetTileCount() const;

            int32_t GetPendingTileCount() const;

            // The largest radius whose tiles fit into the mesh, larger ones are reduced to it
            float GetMaxRadius() const;

            // Pairs of points, the edges of the polygons within the bounds, each shared edge once
            std::vector<vec3> GetPolygonEdges(const Volume::AABB& aabb) const;

            // Changes when tiles are added or removed, e.g. to keep debug geometry until the mesh changes
            uint64_t GetRevision() const { return revision; }

            const NavigationSettings settings;

            // Half extents used to find the polygon of a position
            vec3 searchExtents = vec3(1.0f, 2.0f, 1.0f);

            // Tiles whose geometry is collected on the main thread per update and tiles built at the same time
            int32_t maxTileStartsPerUpdate = 2;
            int32_t maxConcurrentBuilds = 8;
            // The worker pool of the job system the tiles are built in
            JobPriority buildPriority = JobPriority::Medium;

        private:
            struct TileBuild;

            struct Tile {
                ivec2 coord;
                uint64_t ref = 0;
                // Id of the running build, a tile that was invalidated meanwhile ignores its result
                int64_t buildId = 0;
                bool built = false;
            };

            void InvalidateChanges(Scene::Scene* scene);

            void StartBuild(Scene::Scene* scene, Tile& tile);

            void BuildTile(TileBuild& build) const;

            void FinishBuild(TileBuild& build);

            void RemoveTile(Tile& tile);

            Volume::AABB GetTileBounds(ivec2 coord, float border) const;

            static int64_t GetTileKey(ivec2 coord);

            dtNavMesh* navMesh = nullptr;
            dtNavMeshQuery* query = nullptr;
            dtQueryFilter* filter = nullptr;

            float tileWorldSize;
            int32_t tileCellCount;
            int32_t maxPolysPerTile;

            std::unordered_map<int64_t, Tile> tiles;
            std::vector<Ref<TileBuild>> builds;
            int64_t buildCounter = 0;

            // What the tiles were built from, a different physics world rebuilds everything
            const void* builtPhysicsWorld = nullptr;
            uint64_t physicsChangeCount = 0;

            uint64_t revision = 0;

            JobGroup jobGroup { "Navigation tile build", JobPriority::Medium };

        };

    }

}
