#include "Checks.h"

#include <stdexcept>

namespace Atlas::Capture {

    using namespace Scene::Components;

    // Separating axis test against the box faces, triangle normal and edge cross products
    static bool TriangleIntersectsBox(vec3 center, vec3 halfSize, vec3 a, vec3 b, vec3 c) {

        auto v0 = a - center, v1 = b - center, v2 = c - center;
        vec3 edges[3] = { v1 - v0, v2 - v1, v0 - v2 };

        auto separated = [&](vec3 axis) {

            if (glm::dot(axis, axis) < 1e-12f)
                return false;
            auto p0 = glm::dot(v0, axis), p1 = glm::dot(v1, axis), p2 = glm::dot(v2, axis);
            auto radius = glm::dot(halfSize, glm::abs(axis));
            return std::min({ p0, p1, p2 }) > radius || std::max({ p0, p1, p2 }) < -radius;

        };

        for (int32_t i = 0; i < 3; i++) {
            vec3 boxAxis(0.0f);
            boxAxis[i] = 1.0f;
            if (separated(boxAxis))
                return false;
            for (const auto& edge : edges)
                if (separated(glm::cross(boxAxis, edge)))
                    return false;
        }

        return !separated(glm::cross(edges[0], edges[1]));

    }

    // Counts the triangles of a mesh that intersect the given world space box
    static int32_t CountIntrudingTriangles(const MeshComponent& meshComponent, const TransformComponent& transform,
        const Volume::AABB& box) {

        if (!meshComponent.mesh.IsLoaded())
            return 0;

        auto& data = meshComponent.mesh->data;
        auto& vertices = data.vertices.Get();
        auto& indices = data.indices.Get();

        // Cheap rejection in local space with the conservatively transformed box
        auto localBox = box.Transform(glm::inverse(transform.globalMatrix));
        auto center = box.GetCenter(), halfSize = box.GetSize() * 0.5f;

        int32_t count = 0;
        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            const auto& a = vertices[indices[i]];
            const auto& b = vertices[indices[i + 1]];
            const auto& c = vertices[indices[i + 2]];

            auto min = glm::min(a, glm::min(b, c)), max = glm::max(a, glm::max(b, c));
            if (glm::any(glm::lessThan(max, localBox.min)) || glm::any(glm::greaterThan(min, localBox.max)))
                continue;

            auto worldA = vec3(transform.globalMatrix * vec4(a, 1.0f));
            auto worldB = vec3(transform.globalMatrix * vec4(b, 1.0f));
            auto worldC = vec3(transform.globalMatrix * vec4(c, 1.0f));
            if (TriangleIntersectsBox(center, halfSize, worldA, worldB, worldC))
                count++;
        }

        return count;

    }

    static bool MatchesSelfOrAncestor(Scene::Scene& scene, Scene::Entity entity,
        const std::vector<std::string>& patterns) {

        while (entity.IsValid()) {
            if (MatchAnyPattern(patterns, GetEntityName(entity)))
                return true;
            entity = scene.GetParentEntity(entity);
        }

        return false;

    }

    static bool Compare(float actual, const std::string& op, float expected, float tolerance) {

        if (op == ">")
            return actual > expected;
        if (op == ">=")
            return actual >= expected;
        if (op == "<")
            return actual < expected;
        if (op == "<=")
            return actual <= expected;
        if (op == "==")
            return std::abs(actual - expected) <= tolerance;
        if (op == "!=")
            return std::abs(actual - expected) > tolerance;
        throw std::runtime_error("Unknown comparison operator '" + op + "'");

    }

    std::vector<Scene::Entity> RemoveDescendants(Scene::Scene& scene, const std::vector<Scene::Entity>& entities) {

        EntitySet contained;
        for (auto entity : entities)
            contained.insert(entity);

        std::vector<Scene::Entity> result;
        for (auto entity : entities) {
            bool ancestorContained = false;
            auto parent = scene.GetParentEntity(entity);
            while (parent.IsValid() && !ancestorContained) {
                ancestorContained = contained.contains(parent);
                parent = scene.GetParentEntity(parent);
            }

            if (!ancestorContained)
                result.push_back(entity);
        }

        return result;

    }

    void CheckRunner::Run(Scene::Scene& scene, float time) {

        if (spec.grounded)
            CheckGrounded(scene, time);
        if (spec.penetration)
            CheckPenetration(scene, time);
        if (spec.invalidTransforms)
            CheckInvalidTransforms(scene, time);

    }

    void CheckRunner::AddFailure(const Failure& failure) {

        auto key = failure.check + "|" + failure.entity;
        if (failureCounts[key]++ == 0)
            failures.push_back(failure);

    }

    void CheckRunner::CheckGrounded(Scene::Scene& scene, float time) {

        auto entities = RemoveDescendants(scene, FindEntities(scene, spec.grounded->entities));
        runCounts["grounded"] += int32_t(entities.size());

        for (auto entity : entities) {
            EntitySet ignored;
            CollectHierarchy(entity, ignored);

            // Skip entities whose hierarchy has no loaded mesh bounds
            bool hasMesh = false;
            auto bounds = GetEntityBounds(entity, &hasMesh);
            if (!hasMesh)
                continue;

            auto center = bounds.GetCenter();
            // Start just above the entity so roofs or arches above it are not hit
            auto ground = GroundBelow(scene, vec3(center.x, bounds.max.y + 0.5f, center.z), ignored);

            if (!ground.valid) {
                AddFailure({ "grounded", GetEntityLabel(entity), time,
                    "no ground below the entity at " + FormatVec3(center) });
                continue;
            }

            // Standing on a prop (e.g. walking over hay bales) counts as a failure if surfaces are given
            const auto& surfaces = spec.grounded->surfaces;
            if (!surfaces.empty() && ground.entity.IsValid() &&
                !MatchesSelfOrAncestor(scene, ground.entity, surfaces)) {

                AddFailure({ "grounded", GetEntityLabel(entity), time,
                    "stands on " + GetEntityLabel(ground.entity) + ", which doesn't match the allowed surfaces" });
                continue;

            }

            auto gap = bounds.min.y - ground.position.y;
            if (gap > spec.grounded->maxGap) {
                AddFailure({ "grounded", GetEntityLabel(entity), time,
                    "floats " + FormatFloat(gap) + " m above " +
                        (ground.entity.IsValid() ? GetEntityLabel(ground.entity) : ground.kind) + " (bottom y " +
                        FormatFloat(bounds.min.y) + ", ground y " + FormatFloat(ground.position.y) + ")" });
            }
            else if (gap < -spec.grounded->maxSink) {
                AddFailure({ "grounded", GetEntityLabel(entity), time,
                    "sinks " + FormatFloat(-gap) + " m into " +
                        (ground.entity.IsValid() ? GetEntityLabel(ground.entity) : ground.kind) + " (bottom y " +
                        FormatFloat(bounds.min.y) + ", ground y " + FormatFloat(ground.position.y) + ")" });
            }
        }

    }

    void CheckRunner::CheckPenetration(Scene::Scene& scene, float time) {

        auto& penetration = *spec.penetration;
        auto entities = RemoveDescendants(scene, FindEntities(scene, penetration.entities));
        runCounts["penetration"] += int32_t(entities.size());

        for (auto entity : entities) {
            EntitySet hierarchy;
            CollectHierarchy(entity, hierarchy);

            bool hasMesh = false;
            auto bounds = GetEntityBounds(entity, &hasMesh);
            if (!hasMesh)
                continue;

            // Shrink the bounds such that touching neighbors (e.g. resting on the floor) don't count
            auto margin = glm::min(vec3(penetration.margin), bounds.GetSize() * 0.45f);
            auto box = Volume::AABB(bounds.min + margin, bounds.max - margin);

            for (auto other : scene.QueryAABB(box)) {
                if (hierarchy.contains(other))
                    continue;
                if (!MatchesSelfOrAncestor(scene, other, penetration.against))
                    continue;

                auto meshComponent = other.TryGetComponent<MeshComponent>();
                auto transform = other.TryGetComponent<TransformComponent>();
                if (!meshComponent || !transform || !meshComponent->visible)
                    continue;

                auto intruding = CountIntrudingTriangles(*meshComponent, *transform, box);

                // Count only additional intersections relative to the first observation
                auto key = std::make_pair(ECS::Entity(entity), ECS::Entity(other));
                auto [it, inserted] = baselineIntrusions.try_emplace(key, intruding);
                auto increase = intruding - it->second;

                if (increase >= penetration.minTriangles) {
                    AddFailure({ "penetration", GetEntityLabel(entity), time,
                        std::to_string(increase) + " more triangles of " + GetEntityLabel(other) +
                            " intrude into its bounds than at the start (" + std::to_string(intruding) + " vs " +
                            std::to_string(it->second) + ", bounds " + FormatVec3(bounds.min) + " - " +
                            FormatVec3(bounds.max) + ")" });
                }
            }
        }

    }

    void CheckRunner::CheckInvalidTransforms(Scene::Scene& scene, float time) {

        auto transformSubset = scene.GetSubset<TransformComponent>();
        for (auto entity : transformSubset) {
            const auto& transform = transformSubset.Get(entity);

            bool valid = true;
            for (int32_t column = 0; column < 4; column++)
                for (int32_t row = 0; row < 4; row++)
                    valid &= std::isfinite(transform.globalMatrix[column][row]);

            if (!valid)
                AddFailure({ "invalidTransforms", GetEntityLabel({ entity, &scene.entityManager }), time,
                    "transform contains NaN or infinite values" });
        }

    }

    json CheckRunner::GetReport() const {

        json j;
        j["passed"] = failures.empty();

        json checks = json::object();
        if (spec.grounded)
            checks["grounded"] = { { "evaluations", runCounts.contains("grounded") ? runCounts.at("grounded") : 0 } };
        if (spec.penetration)
            checks["penetration"] = { { "evaluations",
                runCounts.contains("penetration") ? runCounts.at("penetration") : 0 } };
        j["checks"] = checks;

        json failureList = json::array();
        for (const auto& failure : failures) {
            auto key = failure.check + "|" + failure.entity;
            failureList.push_back(
                { { "check", failure.check }, { "entity", failure.entity }, { "firstTime", failure.time },
                    { "message", failure.message }, { "occurrences", failureCounts.at(key) } });
        }
        j["failures"] = failureList;

        return j;
    }

    std::vector<std::string> CheckRunner::GetSummary() const {

        std::vector<std::string> lines;
        for (const auto& failure : failures) {
            auto key = failure.check + "|" + failure.entity;
            lines.push_back("[" + failure.check + "] t=" + FormatFloat(failure.time, 2) + "s " + failure.entity + ": " +
                failure.message + " (" + std::to_string(failureCounts.at(key)) + "x)");
        }

        return lines;
    }

    void Tracker::Record(Scene::Scene& scene, float time) {

        auto entities = RemoveDescendants(scene, FindEntities(scene, spec.entities));
        for (auto entity : entities) {
            auto bounds = GetEntityBounds(entity);
            samples[GetEntityLabel(entity)].push_back(
                { time, GetEntityPosition(entity), glm::degrees(GetEntityYaw(entity)), bounds.min.y });
        }

    }

    json Tracker::GetTrace() const {

        json j;
        j["interval"] = spec.interval;

        json summary = json::object();
        json entities = json::object();
        for (const auto& [label, list] : samples) {
            json entitySamples = json::array();
            float travelled = 0.0f;
            float minY = std::numeric_limits<float>::max(), maxY = -std::numeric_limits<float>::max();
            for (size_t i = 0; i < list.size(); i++) {
                const auto& sample = list[i];
                entitySamples.push_back({ { "t", std::round(sample.time * 1000.0f) / 1000.0f },
                    { "position", FromVec3(sample.position) }, { "yaw", std::round(sample.yaw * 10.0f) / 10.0f },
                    { "bottomY", std::round(sample.minY * 1000.0f) / 1000.0f } });
                if (i > 0)
                    travelled += glm::distance(sample.position, list[i - 1].position);
                minY = std::min(minY, sample.position.y);
                maxY = std::max(maxY, sample.position.y);
            }
            entities[label] = entitySamples;

            if (!list.empty()) {
                summary[label] = { { "start", FromVec3(list.front().position) },
                    { "end", FromVec3(list.back().position) }, { "distanceTravelled", travelled }, { "minY", minY },
                    { "maxY", maxY }, { "firstSeen", list.front().time }, { "lastSeen", list.back().time } };
            }
        }

        j["summary"] = summary;
        j["entities"] = entities;
        return j;
    }

    json EvaluateExpectation(Scene::Scene& scene, const json& expectation, bool& passed) {

        json result;
        result["expect"] = expectation;
        passed = false;

        auto entityPattern = expectation.at("entity").get<std::string>();
        auto property = expectation.at("property").get<std::string>();
        auto entities = RemoveDescendants(scene, FindEntities(scene, entityPattern));

        if (property == "count") {
            auto op = expectation.value("op", std::string("=="));
            auto expected = expectation.at("value").get<float>();
            passed = Compare(float(entities.size()), op, expected, 0.5f);
            result["actual"] = entities.size();
            return result;
        }

        if (property == "exists") {
            auto expected = expectation.value("value", true);
            passed = entities.empty() != expected;
            result["actual"] = !entities.empty();
            return result;
        }

        if (entities.empty()) {
            result["error"] = "No entity matches '" + entityPattern + "'";
            return result;
        }

        auto op = expectation.value("op", std::string(">"));
        auto expected = expectation.at("value").get<float>();
        auto tolerance = expectation.value("tolerance", 0.01f);

        // All matching entities have to fulfill the expectation
        passed = true;
        json actualValues = json::object();
        for (auto entity : entities) {
            auto position = GetEntityPosition(entity);
            bool hasMesh = false;
            auto bounds = GetEntityBounds(entity, &hasMesh);

            // Like the grounded check, skip entities whose hierarchy has no loaded mesh bounds
            if (property == "gapToGround" && !hasMesh)
                continue;

            float actual = 0.0f;
            if (property == "position.x")
                actual = position.x;
            else if (property == "position.y")
                actual = position.y;
            else if (property == "position.z")
                actual = position.z;
            else if (property == "bottom.y")
                actual = bounds.min.y;
            else if (property == "top.y")
                actual = bounds.max.y;
            else if (property == "yaw")
                actual = glm::degrees(GetEntityYaw(entity));
            else if (property == "distanceTo")
                actual = glm::distance(position, ToVec3(expectation.at("point")));
            else if (property == "gapToGround") {
                EntitySet ignored;
                CollectHierarchy(entity, ignored);
                auto center = bounds.GetCenter();
                auto ground = GroundBelow(scene, vec3(center.x, bounds.max.y + 0.5f, center.z), ignored);
                actual = ground.valid ? bounds.min.y - ground.position.y : std::numeric_limits<float>::infinity();
            }
            else {
                throw std::runtime_error("Unknown expectation property '" + property +
                    "' (exists, position.x/y/z, " "bottom.y, top.y, yaw, distanceTo, gapToGround)");
            }

            actualValues[GetEntityLabel(entity)] = actual;
            passed &= Compare(actual, op, expected, tolerance);
        }

        result["actual"] = actualValues;
        return result;

    }

}
