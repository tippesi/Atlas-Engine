#pragma once

#include "Job.h"
#include "SceneTools.h"

#include <map>

namespace Atlas::Capture {

    struct Failure {

        std::string check;
        std::string entity;
        float time = 0.0f;
        std::string message;

    };

    // Runs the plausibility checks of a job, either once (static) or repeatedly while playing
    class CheckRunner {

    public:
        explicit CheckRunner(const ChecksSpec& spec) : spec(spec) {}

        void Run(Scene::Scene& scene, float time);

        void AddFailure(const Failure& failure);

        bool HasFailures() const { return !failures.empty(); }

        json GetReport() const;

        std::vector<std::string> GetSummary() const;

        const ChecksSpec spec;

    private:
        void CheckGrounded(Scene::Scene& scene, float time);

        void CheckPenetration(Scene::Scene& scene, float time);

        void CheckInvalidTransforms(Scene::Scene& scene, float time);

        std::vector<Failure> failures;
        // Only the first failure per check and entity is kept in detail, the rest is counted
        std::map<std::string, int32_t> failureCounts;
        std::map<std::string, int32_t> runCounts;

        std::map<std::pair<ECS::Entity, ECS::Entity>, int32_t> baselineIntrusions;

    };

    // Records positions of entities over time
    class Tracker {

    public:
        explicit Tracker(const TrackSpec& spec) : spec(spec) {}

        void Record(Scene::Scene& scene, float time);

        json GetTrace() const;

        struct Sample {

            float time;
            vec3 position;
            float yaw;
            float minY;

        };

        const std::map<std::string, std::vector<Sample>>& GetSamples() const { return samples; }

        const TrackSpec spec;

    private:
        std::map<std::string, std::vector<Sample>> samples;

    };

    // Evaluates a timeline expectation, e.g. { "entity": "Crate", "property": "position.y", "op": ">", "value": 0 }
    json EvaluateExpectation(Scene::Scene& scene, const json& expectation, bool& passed);

}
