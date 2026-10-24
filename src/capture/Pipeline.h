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

        // Determines the first step to run and restores the state it starts from. Returns
        // the number of steps, i.e. nothing to run, if all snapshots are up to date
        size_t Prepare(const std::function<void(const std::string&)>& print);

        // Stores the state after a successfully finished step. Scenes and terrains saved by the job and
        // materials it changed become part of the state of this and all following steps
        void Complete(size_t index, const json& report);

        // A failed step and all following ones have no valid snapshot anymore
        void Fail(size_t index);

        json GetSummary() const;

        std::string name;
        std::vector<Step> steps;
        fs::path snapshotDirectory;

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

        void Invalidate(size_t index);

        // Asset paths that make up the state, the files listed by the pipeline come first
        std::vector<std::string> stateFiles;
        std::string from;
        bool force = false;

        std::vector<std::string> results;

    };

}
