#pragma once

#include "Common.h"

#include <functional>
#include <map>
#include <optional>

namespace Atlas::Capture {

    // A sequence of edit jobs that builds a scene step by step. After each step the files of the
    // state (the saved scenes, edited materials and the files listed by the pipeline) are stored
    // as a snapshot. A step whose job and inputs didn't change since its snapshot is skipped, the
    // first changed step starts from the snapshot of the step before it. See README.md
    class Pipeline {

    public:
        struct Step {

            std::string name;
            fs::path jobFile;
            // Additional files the result depends on, e.g. scripts or prefabs used by the job
            std::vector<fs::path> inputs;
            // Hash of the job, its inputs and the key of the previous step
            std::string key;

        };

        // Parses the pipeline file, --from <step> and --force are options of the command line
        static Pipeline Load(const fs::path& file, const std::string& from, bool force);

        // Determines the first step to run and restores the state it starts from. All steps are finished
        // if all snapshots are up to date
        void Prepare(const std::function<void(const std::string&)>& print);

        // Stores the state after the current step finished successfully and continues with the next one. Scenes and
        // terrains saved by the job and materials it changed become part of the state of this and all following steps
        void CompleteStep(const json& report);

        // A failed step and all following ones have no valid snapshot anymore, the pipeline stops with the exit code
        void FailStep(int32_t exitCode);

        bool IsFinished() const;

        // Writes pipeline.json with the result of each step next to the snapshots and returns its path
        fs::path WriteSummary() const;

        json GetSummary() const;

        std::string name;
        std::vector<Step> steps;
        fs::path snapshotDirectory;

        size_t currentStep = 0;
        int32_t exitCode = 0;

    private:
        struct Snapshot {

            std::string key;
            // Asset path -> hash of the stored content, empty if the file didn't exist
            std::map<std::string, std::string> files;

        };

        fs::path GetManifestPath(size_t index) const;

        std::optional<Snapshot> ReadSnapshot(size_t index) const;

        // Copies the stored files of a snapshot back into the data directory
        void Restore(const Snapshot& snapshot, const std::function<void(const std::string&)>& print);

        std::string StoreFile(const std::string& assetPath);

        void Complete(size_t index, const json& report);

        void Fail(size_t index);

        void Invalidate(size_t index);

        // Asset paths that make up the state, the files listed by the pipeline come first
        std::vector<std::string> stateFiles;
        std::string from;
        bool force = false;

        std::vector<std::string> results;

    };

}
