#include "Pipeline.h"

#include <cstdio>
#include <fstream>
#include <stdexcept>

namespace Atlas::Capture {

    static constexpr uint64_t fnvOffset = 14695981039346656037ull;
    static constexpr uint64_t fnvPrime = 1099511628211ull;

    static uint64_t HashBytes(uint64_t hash, const char* data, size_t size) {

        for (size_t i = 0; i < size; i++) {
            hash ^= uint8_t(data[i]);
            hash *= fnvPrime;
        }
        return hash;

    }

    static uint64_t HashFile(uint64_t hash, const fs::path& path) {

        std::ifstream stream(path, std::ios::binary);
        if (!stream.is_open())
            throw std::runtime_error("Couldn't read " + path.string());

        std::vector<char> buffer(1 << 20);
        while (stream) {
            stream.read(buffer.data(), std::streamsize(buffer.size()));
            hash = HashBytes(hash, buffer.data(), size_t(stream.gcount()));
        }
        return hash;

    }

    static std::string ToHex(uint64_t value) {

        char text[17];
        std::snprintf(text, sizeof(text), "%016llx", static_cast<unsigned long long>(value));
        return text;

    }

    // Empty for files that don't exist
    static std::string HashAsset(const std::string& assetPath) {

        auto path = GetAssetDirectory() / assetPath;
        return fs::exists(path) ? ToHex(HashFile(fnvOffset, path)) : std::string();

    }

    Pipeline Pipeline::Load(const fs::path& file, const std::string& from, bool force) {

        std::ifstream stream(file);
        if (!stream.is_open())
            throw std::runtime_error("Couldn't open pipeline file " + file.string());

        json j;
        try {
            j = json::parse(stream, nullptr, true, true);
        }
        catch (const json::exception& e) {
            throw std::runtime_error("Couldn't parse pipeline file " + file.string() + ": " + e.what());
        }

        Pipeline pipeline;
        pipeline.name = j.value("name", file.stem().string());
        pipeline.from = from;
        pipeline.force = force;

        auto baseDirectory = file.parent_path();
        for (const auto& path : j.value("files", std::vector<std::string>()))
            pipeline.stateFiles.push_back(ToAssetPath(path));

        if (!j.contains("steps") || j["steps"].empty())
            throw std::runtime_error("The pipeline " + file.string() + " has no steps");

        for (const auto& stepJson : j["steps"]) {
            // A step is a job file or { "job": ..., "name": ..., "inputs": [...] }
            Step step;
            auto jobPath = stepJson.is_string() ? stepJson.get<std::string>() : stepJson.at("job").get<std::string>();
            step.jobFile = (baseDirectory / jobPath).lexically_normal();
            step.name = stepJson.is_object() ? stepJson.value("name", step.jobFile.stem().string()) : step.jobFile.stem().string();
            if (stepJson.is_object())
                for (const auto& input : stepJson.value("inputs", std::vector<std::string>()))
                    step.inputs.push_back((baseDirectory / input).lexically_normal());
            pipeline.steps.push_back(step);
        }

        if (!from.empty() && std::none_of(pipeline.steps.begin(), pipeline.steps.end(), [&](const Step& step) { return step.name == from; }))
            throw std::runtime_error("--from: the pipeline has no step named '" + from + "'");

        if (j.contains("snapshots")) {
            fs::path snapshots(j["snapshots"].get<std::string>());
            pipeline.snapshotDirectory = snapshots.is_absolute() ? snapshots : (baseDirectory / snapshots).lexically_normal();
        }
        else {
            // Next to the job outputs, the bin directory is ignored by git
            pipeline.snapshotDirectory =
                (fs::current_path() / ".." / "capture" / SanitizeFileName(pipeline.name) / "snapshots").lexically_normal();
        }

        // Each key covers the steps before it, a change invalidates all following steps
        uint64_t key = fnvOffset;
        for (auto& step : pipeline.steps) {
            key = HashFile(key, step.jobFile);
            for (const auto& input : step.inputs)
                key = HashFile(key, input);
            step.key = ToHex(key);
        }

        pipeline.results.resize(pipeline.steps.size(), "pending");
        return pipeline;

    }

    size_t Pipeline::Prepare(const std::function<void(const std::string&)>& print) {

        size_t first = 0;
        for (; first < steps.size(); first++) {
            if (force || steps[first].name == from)
                break;
            auto snapshot = ReadSnapshot(first);
            if (!snapshot || snapshot->key != steps[first].key)
                break;
            results[first] = "up to date";
        }

        if (first > 0) {
            // Also when everything is up to date, the files may have been changed since the last run
            auto snapshot = ReadSnapshot(first - 1);
            Restore(*snapshot, print);
            stateFiles.clear();
            for (const auto& [path, hash] : snapshot->files)
                stateFiles.push_back(path);
        }

        Invalidate(first);
        return first;

    }

    void Pipeline::Complete(size_t index, const json& report) {

        if (report.contains("edit")) {
            const auto& edit = report["edit"];
            auto add = [&](const std::string& path) {

                if (std::find(stateFiles.begin(), stateFiles.end(), path) == stateFiles.end())
                    stateFiles.push_back(path);

            };
            if (edit.value("saved", false))
                add(edit["output"].get<std::string>());
            if (edit.contains("terrain"))
                add(edit["terrain"].get<std::string>());
            for (const auto& material : edit.value("materials", json::array()))
                add(material["material"].get<std::string>());
        }

        json manifest = { { "step", steps[index].name }, { "job", steps[index].jobFile.string() }, { "key", steps[index].key },
            { "files", json::object() } };
        for (const auto& path : stateFiles)
            manifest["files"][path] = StoreFile(path);

        fs::create_directories(snapshotDirectory);
        std::ofstream stream(GetManifestPath(index));
        stream << manifest.dump(2);
        results[index] = "ran";

    }

    void Pipeline::Fail(size_t index) {

        results[index] = "failed";
        Invalidate(index);

    }

    void Pipeline::Invalidate(size_t index) {

        std::error_code error;
        for (size_t i = index; i < steps.size(); i++)
            fs::remove(GetManifestPath(i), error);

    }

    json Pipeline::GetSummary() const {

        json summary = json::array();
        for (size_t i = 0; i < steps.size(); i++)
            summary.push_back({ { "step", steps[i].name }, { "job", steps[i].jobFile.string() }, { "result", results[i] } });
        return summary;

    }

    fs::path Pipeline::GetManifestPath(size_t index) const {

        return snapshotDirectory / (std::to_string(index + 1) + "_" + SanitizeFileName(steps[index].name) + ".json");

    }

    std::optional<Pipeline::Snapshot> Pipeline::ReadSnapshot(size_t index) const {

        std::ifstream stream(GetManifestPath(index));
        if (!stream.is_open())
            return std::nullopt;

        auto manifest = json::parse(stream, nullptr, false);
        if (manifest.is_discarded())
            return std::nullopt;

        Snapshot snapshot;
        snapshot.key = manifest.value("key", std::string());
        for (const auto& [path, hash] : manifest.value("files", json::object()).items()) {
            // A snapshot whose stored files were removed can't be restored
            if (!hash.get<std::string>().empty() && !fs::exists(snapshotDirectory / "files" / hash.get<std::string>()))
                return std::nullopt;
            snapshot.files[path] = hash.get<std::string>();
        }
        return snapshot;

    }

    void Pipeline::Restore(const Snapshot& snapshot, const std::function<void(const std::string&)>& print) {

        for (const auto& [path, hash] : snapshot.files) {
            if (HashAsset(path) == hash)
                continue;

            auto target = GetAssetDirectory() / path;
            if (hash.empty()) {
                fs::remove(target);
                print("Restored " + path + " (removed, it didn't exist at this step)");
                continue;
            }
            fs::create_directories(target.parent_path());
            fs::copy_file(snapshotDirectory / "files" / hash, target, fs::copy_options::overwrite_existing);
            print("Restored " + path);
        }

    }

    std::string Pipeline::StoreFile(const std::string& assetPath) {

        auto hash = HashAsset(assetPath);
        if (hash.empty())
            return hash;

        // Content addressed, steps that didn't change a file share its copy
        auto stored = snapshotDirectory / "files" / hash;
        if (!fs::exists(stored)) {
            fs::create_directories(stored.parent_path());
            fs::copy_file(GetAssetDirectory() / assetPath, stored);
        }
        return hash;

    }

}
