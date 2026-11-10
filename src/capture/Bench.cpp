#include "Bench.h"
#include "Client.h"
#include "Image.h"
#include "Process.h"

#include "InstanceRegistry.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>

namespace Atlas::Capture {

    static void Print(const std::string& line) {

        std::printf("%s\n", line.c_str());
        std::fflush(stdout);

    }

    struct Variant {

        std::string name;
        fs::path executable;
        // Relative to the data directory, empty for the default shaders
        std::string shaderDirectory;
        // Shaders that differ between the variants, touched before a run so the SPIR-V cache doesn't serve the other one
        std::vector<fs::path> changedShaders;

    };

    struct Statistics {

        double mean = 0.0;
        double min = 0.0;
        double max = 0.0;
        double variance = 0.0;

    };

    static Statistics GetStatistics(const std::vector<double>& values) {

        Statistics statistics;
        if (values.empty())
            return statistics;
        statistics.min = *std::min_element(values.begin(), values.end());
        statistics.max = *std::max_element(values.begin(), values.end());
        for (auto value : values)
            statistics.mean += value;
        statistics.mean /= double(values.size());
        for (auto value : values)
            statistics.variance += (value - statistics.mean) * (value - statistics.mean);
        statistics.variance /= double(std::max(values.size(), size_t(2)) - 1);
        return statistics;

    }

    // GPU passes of the profile up to the second level by their path, e.g. "Render scene / Opaque"
    static std::map<std::string, double> ReadProfile(const fs::path& path) {

        std::map<std::string, double> passes;
        std::ifstream stream(path);
        auto profile = json::parse(stream, nullptr, false);
        if (!profile.is_object())
            return passes;

        for (const auto& thread : profile.value("threads", json::array()))
            for (const auto& query : thread.value("queries", json::array()))
                if (query.value("depth", 0) <= 1)
                    passes[query.value("query", std::string())] += query.value("ms", 0.0);
        if (profile.contains("cpu"))
            passes["CPU frame"] = profile["cpu"].value("frameMs", 0.0);
        return passes;

    }

    static fs::path FirstImage(const fs::path& directory) {

        std::vector<fs::path> images;
        std::error_code error;
        for (const auto& entry : fs::directory_iterator(directory, error))
            if (entry.path().extension() == ".png")
                images.push_back(entry.path());
        std::sort(images.begin(), images.end());
        return images.empty() ? fs::path() : images.front();

    }

    static std::vector<Variant> GetVariants(const json& bench, const fs::path& benchDirectory) {

        auto resolve = [&](const std::string& path) {

            fs::path value(path);
            if (value.is_absolute())
                return value;
            auto inBench = (benchDirectory / value).lexically_normal();
            return fs::exists(inBench) ? inBench : ResolveUserPath(path);

        };

        std::vector<Variant> variants;
        if (bench.contains("builds")) {
            for (const auto& [name, executable] : bench["builds"].items())
                variants.push_back({ name, resolve(executable.get<std::string>()) });
            for (const auto& variant : variants)
                if (!fs::exists(variant.executable))
                    throw std::runtime_error("The build " + variant.name + " doesn't exist: " + variant.executable.string());
            return variants;
        }
        if (!bench.contains("shaders"))
            throw std::runtime_error("A bench needs builds or shaders");

        // Shader directories are given to the engine relative to the data directory
        auto data = GetAssetDirectory();
        std::vector<fs::path> directories;
        for (const auto& [name, directory] : bench["shaders"].items()) {
            fs::path path(directory.get<std::string>());
            auto absolute = path.is_absolute() ? path : fs::exists(data / path) ? data / path : resolve(path.string());
            if (!fs::is_directory(absolute))
                throw std::runtime_error("The shader directory " + name + " doesn't exist: " + absolute.string());
            directories.push_back(fs::weakly_canonical(absolute));
            variants.push_back({ name, GetExecutablePath(), fs::relative(directories.back(), data).generic_string() });
        }

        // Only the files that differ are touched, the shaders of other applications are reloaded when they change
        auto readFile = [](const fs::path& path) {

            std::ifstream stream(path, std::ios::binary);
            return std::string(std::istreambuf_iterator<char>(stream), {});

        };
        std::error_code error;
        for (const auto& entry : fs::recursive_directory_iterator(directories.front(), error)) {
            if (!entry.is_regular_file())
                continue;
            auto relative = fs::relative(entry.path(), directories.front());
            auto content = readFile(entry.path());
            bool differs = false;
            for (size_t i = 1; i < directories.size(); i++)
                differs |= readFile(directories[i] / relative) != content;
            if (!differs)
                continue;
            for (size_t i = 0; i < directories.size(); i++)
                if (fs::exists(directories[i] / relative))
                    variants[i].changedShaders.push_back(directories[i] / relative);
        }
        Print(std::to_string(variants.front().changedShaders.size()) + " shader files differ between the directories");
        return variants;

    }

    int32_t RunBench(const std::vector<std::string>& args) {

        if (args.size() < 3)
            throw std::runtime_error("bench expects a bench file");

        auto benchFile = ResolveUserPath(args[2]);
        std::ifstream stream(benchFile);
        auto bench = json::parse(stream, nullptr, false, true);
        if (!bench.is_object())
            throw std::runtime_error("Couldn't parse " + benchFile.string());

        auto variants = GetVariants(bench, benchFile.parent_path());
        if (variants.size() < 2)
            throw std::runtime_error("A bench needs at least two builds or shader directories");

        auto rounds = std::max(bench.value("rounds", 2), 2);
        auto maxRounds = std::max(bench.value("maxRounds", 5), rounds);
        auto frames = bench.value("frames", 240);
        auto passFilter = bench.value("passes", std::vector<std::string>());
        auto extraArgs = bench.value("args", std::vector<std::string>());
        auto output = (GetExecutablePath().parent_path() / ".." / "capture" / "bench" / benchFile.stem()).lexically_normal();
        std::error_code error;
        fs::remove_all(output, error);

        for (const auto& instance : InstanceRegistry::GetInstances())
            if (instance.pid != CurrentProcessId())
                Print("Note: " + instance.executable + " (process " + std::to_string(instance.pid) + ") runs, " + instance.activity +
                    ". Its GPU work shifts the timings");

        json result = json::object();
        int32_t exitCode = 0;
        for (const auto& jobEntry : bench.at("jobs")) {
            auto jobFile = fs::path(jobEntry.get<std::string>());
            if (!jobFile.is_absolute())
                jobFile = (benchFile.parent_path() / jobFile).lexically_normal();
            auto jobName = jobFile.stem().string();

            std::map<std::string, std::map<std::string, std::vector<double>>> measurements;
            std::map<std::string, std::vector<fs::path>> runDirectories;
            int32_t round = 0;
            bool decided = false;
            while (round < maxRounds && !decided) {
                round++;
                // Alternating the order cancels a drift over the rounds, e.g. a GPU that heats up
                auto order = variants;
                if (round % 2 == 0)
                    std::reverse(order.begin(), order.end());

                for (const auto& variant : order) {
                    for (const auto& shader : variant.changedShaders)
                        fs::last_write_time(shader, fs::file_time_type::clock::now(), error);
                    WaitForGpu(600.0f);

                    auto directory = output / jobName / (variant.name + "_" + std::to_string(round));
                    std::vector<std::string> command = { variant.executable.string(), jobFile.string(), "--out", directory.string(),
                        "--resolution", "1920x1080", "--profile", "64", "--settle", std::to_string(frames),
                        "--set", "postProcessing.autoExposure.enable=false", "--pause" };
                    if (!variant.shaderDirectory.empty())
                        command.insert(command.end(), { "--shader-dir", variant.shaderDirectory });
                    command.insert(command.end(), extraArgs.begin(), extraArgs.end());

                    fs::create_directories(directory, error);
                    auto start = std::chrono::steady_clock::now();
                    auto code = RunProcess(command, variant.executable.parent_path(), directory / "bench_run.log", 900.0f);
                    auto passes = ReadProfile(directory / "profile.json");
                    if (code != 0 || passes.empty()) {
                        Print(jobName + " " + variant.name + " round " + std::to_string(round) + " failed with exit code " +
                            std::to_string(code) + ", see " + (directory / "bench_run.log").string());
                        return 2;
                    }
                    for (const auto& [pass, milliseconds] : passes)
                        measurements[variant.name][pass].push_back(milliseconds);
                    runDirectories[variant.name].push_back(directory);
                    Print(jobName + " " + variant.name + " round " + std::to_string(round) + ": Render scene " +
                        FormatFloat(float(passes["Render scene"]), 3) + " ms (" + FormatFloat(SecondsSince(start), 0) + " s)");
                }

                if (round < rounds)
                    continue;

                // More rounds only help while the difference of the frame is within its uncertainty and above 1 %
                auto base = GetStatistics(measurements[variants[0].name]["Render scene"]);
                auto change = GetStatistics(measurements[variants[1].name]["Render scene"]);
                auto standardError = std::sqrt(base.variance / double(round) + change.variance / double(round));
                auto delta = std::abs(change.mean - base.mean);
                decided = delta > 2.0 * standardError || 2.0 * standardError < 0.01 * base.mean;
            }

            // Passes of the frame total and the largest ones unless the bench names them
            std::vector<std::string> passes = passFilter;
            if (passes.empty()) {
                std::vector<std::pair<double, std::string>> sorted;
                for (const auto& [pass, values] : measurements[variants[0].name])
                    sorted.emplace_back(GetStatistics(values).mean, pass);
                std::sort(sorted.rbegin(), sorted.rend());
                for (size_t i = 0; i < std::min(sorted.size(), size_t(14)); i++)
                    passes.push_back(sorted[i].second);
            }

            Print("");
            Print(jobName + ", " + std::to_string(round) + " rounds, ms as mean (min - max):");
            json jobResult = json::object();
            for (const auto& pass : passes) {
                std::string line = "  " + pass + ":";
                json passResult = json::object();
                std::vector<Statistics> statistics;
                for (const auto& variant : variants) {
                    auto values = measurements[variant.name][pass];
                    statistics.push_back(GetStatistics(values));
                    const auto& s = statistics.back();
                    line += " " + variant.name + " " + FormatFloat(float(s.mean), 3) + " (" + FormatFloat(float(s.min), 3) + " - " +
                        FormatFloat(float(s.max), 3) + ")";
                    passResult[variant.name] = { { "mean", s.mean }, { "min", s.min }, { "max", s.max }, { "values", values } };
                }
                auto standardError = std::sqrt(statistics[0].variance / double(round) + statistics[1].variance / double(round));
                auto delta = statistics[1].mean - statistics[0].mean;
                auto verdict = std::abs(delta) > 2.0 * standardError ? (delta < 0.0 ? "faster" : "slower") : "inconclusive";
                line += ", " + std::string(delta >= 0.0 ? "+" : "") + FormatFloat(float(delta), 3) + " ms " + verdict;
                passResult["delta"] = delta;
                passResult["verdict"] = verdict;
                jobResult[pass] = passResult;
                Print(line);
            }

            // Pixel differences between the builds next to those between two runs of one build
            auto noiseA = FirstImage(runDirectories[variants[0].name][0]), noiseB = FirstImage(runDirectories[variants[0].name][1]);
            auto changed = FirstImage(runDirectories[variants[1].name][0]);
            if (!noiseA.empty() && !noiseB.empty() && !changed.empty()) {
                auto a = LoadImageFile(noiseA), b = LoadImageFile(noiseB), c = LoadImageFile(changed);
                if (a && b && c) {
                    auto noise = CompareImages(*a, *b, 8);
                    auto difference = CompareImages(*a, *c, 8);
                    Print("  pixels: " + variants[0].name + " to " + variants[1].name + " " + FormatFloat(difference.percent, 2) + "% differ (mean " +
                        FormatFloat(difference.mean, 2) + "), two runs of " + variants[0].name + " " + FormatFloat(noise.percent, 2) +
                        "% (mean " + FormatFloat(noise.mean, 2) + ")");
                    jobResult["pixels"] = { { "builds", difference.percent }, { "noise", noise.percent } };
                }
            }
            jobResult["rounds"] = round;
            result[jobName] = jobResult;
        }

        WriteFileAtomic(output / "bench.json", result.dump(2));
        Print("Results in " + (output / "bench.json").string());
        return exitCode;

    }

}
