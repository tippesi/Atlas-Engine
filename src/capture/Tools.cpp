#include "Tools.h"
#include "Bench.h"
#include "Check.h"
#include "Client.h"
#include "Image.h"
#include "Inspect.h"
#include "Process.h"

#include "InstanceRegistry.h"

#include "Log.h"
#include "loader/AssetLoader.h"

#include <algorithm>
#include <cstdio>
#include <map>

namespace Atlas::Capture {

    static void Print(const std::string& line) {

        std::printf("%s\n", line.c_str());
        std::fflush(stdout);

    }

    static bool IsImage(const fs::path& path) {

        auto extension = path.extension().string();
        return extension == ".png" || extension == ".jpg" || extension == ".jpeg";

    }

    // A file, a directory (its images) or a pattern with * and ? in the file name, sorted by name
    static std::vector<fs::path> ExpandImages(const std::string& item) {

        auto path = ResolveUserPath(item);
        std::error_code error;
        std::vector<fs::path> paths;
        if (fs::is_directory(path, error)) {
            for (const auto& entry : fs::directory_iterator(path, error))
                if (entry.is_regular_file() && IsImage(entry.path()))
                    paths.push_back(entry.path());
        }
        else if (path.filename().string().find_first_of("*?") != std::string::npos) {
            auto pattern = path.filename().string();
            for (const auto& entry : fs::directory_iterator(path.parent_path(), error))
                if (entry.is_regular_file() && Helper::String::MatchWildcard(pattern, entry.path().filename().string()))
                    paths.push_back(entry.path());
        }
        else {
            paths.push_back(path);
        }
        std::sort(paths.begin(), paths.end());
        return paths;

    }

    // compose <out> [--title <text>] [--columns <n>] [--width <pixels>] <image | label=image | directory | pattern> ...
    static int32_t Compose(const std::vector<std::string>& args) {

        if (args.size() < 4)
            throw std::runtime_error("compose expects an output file and images");

        auto output = ResolveUserPath(args[2]);
        std::string title;
        int32_t columns = 0;
        int32_t width = 1536;
        std::vector<Ref<Image>> images;
        std::vector<std::string> labels;

        for (size_t i = 3; i < args.size(); i++) {
            if (args[i] == "--title" && i + 1 < args.size()) {
                title = args[++i];
                continue;
            }
            if (args[i] == "--columns" && i + 1 < args.size()) {
                columns = std::stoi(args[++i]);
                continue;
            }
            if (args[i] == "--width" && i + 1 < args.size()) {
                width = std::stoi(args[++i]);
                continue;
            }

            auto separator = args[i].find('=');
            auto label = separator == std::string::npos ? std::string() : args[i].substr(0, separator);
            for (const auto& path : ExpandImages(separator == std::string::npos ? args[i] : args[i].substr(separator + 1))) {
                auto image = LoadImageFile(path);
                if (!image) {
                    Print("Couldn't load " + path.string());
                    return 2;
                }
                images.push_back(image);
                labels.push_back(label.empty() ? path.stem().string() : label);
            }
        }
        if (images.empty()) {
            Print("No images to compose");
            return 2;
        }

        auto columnCount = columns > 0 ? columns : int32_t(std::ceil(std::sqrt(float(images.size()))));
        columnCount = std::min(columnCount, int32_t(images.size()));
        auto cellWidth = std::max((width - 4 * (columnCount + 1)) / columnCount, 32);
        auto sheet = ComposeSheet(images, labels, columnCount, title, cellWidth);
        if (!WriteImage(sheet, output))
            return 2;
        Print("Composed " + std::to_string(images.size()) + " images into " + output.string() + " (" + std::to_string(sheet->width) + "x" +
            std::to_string(sheet->height) + ")");
        return 0;

    }

    // diff <a> <b> [--threshold <0-255>] [--out <sheet>]: two images or two directories with images of the same names
    static int32_t Diff(const std::vector<std::string>& args) {

        if (args.size() < 4)
            throw std::runtime_error("diff expects two images or two directories");

        int32_t threshold = 8;
        fs::path output;
        for (size_t i = 4; i < args.size(); i++) {
            if (args[i] == "--threshold" && i + 1 < args.size())
                threshold = std::stoi(args[++i]);
            else if (args[i] == "--out" && i + 1 < args.size())
                output = ResolveUserPath(args[++i]);
            else
                throw std::runtime_error("Unknown diff option '" + args[i] + "' (--threshold <0-255>, --out <sheet>)");
        }

        auto a = ResolveUserPath(args[2]), b = ResolveUserPath(args[3]);
        std::vector<std::pair<std::string, std::pair<fs::path, fs::path>>> pairs;
        std::vector<std::string> unmatched;
        std::error_code error;
        if (fs::is_directory(a, error) && fs::is_directory(b, error)) {
            std::map<std::string, fs::path> inB;
            for (const auto& entry : fs::recursive_directory_iterator(b, error))
                if (entry.is_regular_file() && IsImage(entry.path()))
                    inB[fs::relative(entry.path(), b).generic_string()] = entry.path();
            for (const auto& entry : fs::recursive_directory_iterator(a, error)) {
                if (!entry.is_regular_file() || !IsImage(entry.path()))
                    continue;
                auto relative = fs::relative(entry.path(), a).generic_string();
                if (auto it = inB.find(relative); it != inB.end()) {
                    pairs.push_back({ relative, { entry.path(), it->second } });
                    inB.erase(it);
                }
                else {
                    unmatched.push_back(relative + ": only in the first");
                }
            }
            for (const auto& [relative, path] : inB)
                unmatched.push_back(relative + ": only in the second");
        }
        else {
            pairs.push_back({ a.filename().string(), { a, b } });
        }

        struct Result {

            std::string name;
            ImageDifference difference;
            Ref<Image> a, b, heat;

        };
        std::vector<Result> results;
        for (const auto& [name, paths] : pairs) {
            auto imageA = LoadImageFile(paths.first), imageB = LoadImageFile(paths.second);
            if (!imageA || !imageB) {
                unmatched.push_back(name + ": couldn't be read");
                continue;
            }
            Result result = { name, {}, imageA, imageB, nullptr };
            result.difference = CompareImages(*imageA, *imageB, threshold, output.empty() ? nullptr : &result.heat);
            results.push_back(result);
        }
        std::sort(results.begin(), results.end(), [](const Result& x, const Result& y) { return x.difference.percent > y.difference.percent; });

        size_t differing = 0;
        for (const auto& result : results) {
            const auto& difference = result.difference;
            if (difference.sizeMismatch) {
                differing++;
                Print("  " + result.name + ": sizes differ (" + std::to_string(result.a->width) + "x" + std::to_string(result.a->height) +
                    ", " + std::to_string(result.b->width) + "x" + std::to_string(result.b->height) + ")");
                continue;
            }
            if (difference.percent > 0.0f)
                differing++;
            Print("  " + result.name + ": " + FormatFloat(difference.percent, 2) + "% of the pixels differ by more than " +
                std::to_string(threshold) + ", mean " + FormatFloat(difference.mean, 2) + ", max " + FormatFloat(difference.max, 0));
        }
        for (const auto& line : unmatched)
            Print("  " + line);
        Print(std::to_string(results.size()) + " images compared, " + std::to_string(differing) + " differ" +
            (unmatched.empty() ? "" : ", " + std::to_string(unmatched.size()) + " without a partner"));

        if (!output.empty()) {
            std::vector<Ref<Image>> images;
            std::vector<std::string> labels;
            for (const auto& result : results) {
                if (result.difference.percent <= 0.0f || result.difference.sizeMismatch || images.size() >= 24)
                    continue;
                images.insert(images.end(), { result.a, result.b, result.heat });
                labels.insert(labels.end(), { result.name, "second", FormatFloat(result.difference.percent, 2) + "% differ" });
            }
            if (!images.empty()) {
                auto sheet = ComposeSheet(images, labels, 3, "", 504);
                if (WriteImage(sheet, output))
                    Print("Differences in " + output.string());
            }
        }

        return differing > 0 || !unmatched.empty() ? 1 : 0;

    }

    static int32_t Check(const std::vector<std::string>& args) {

        if (args.size() < 3)
            throw std::runtime_error("check expects job files");

        bool failed = false;
        for (size_t i = 2; i < args.size(); i++) {
            auto path = ResolveUserPath(args[i]);
            auto issues = CheckJobFile(path, {});
            auto errors = std::count_if(issues.begin(), issues.end(), [](const std::string& issue) { return issue.starts_with("ERROR"); });
            failed |= errors > 0;
            Print(path.filename().string() + ": " + (errors > 0 ? std::to_string(errors) + " errors" : std::string("ok")));
            for (const auto& issue : issues)
                Print("  " + issue);
        }
        return failed ? 2 : 0;

    }

    static std::string GetToolUsage() {

        return R"(Commands without the GPU:
  run <job.json | inline job> [job options]   Run a job on the server of this executable and print its summary
  eval <lua> [--play <s>] [--scene <scene>]   Evaluate Lua in the loaded scene, after playing it for a while
  wait <command> | status | cancel | stop     The server: wait for a command, show what runs, cancel the job, stop it
      --server <dir> --detach --timeout <s> --wait-gpu --streaming --validation
  check <job.json> ...                        Find mistakes in jobs without loading their scenes
  kill <pid | captures>                       End a process and the processes it started
  compose <out> [--title t] [--columns n] [--width px] <image | label=image | directory | pattern> ...
  diff <a> <b> [--threshold 0-255] [--out sheet.jpg]   Compare two images or directories of images
  inspect <scene | prefab | mesh | model> [--filter pattern] [--assembly pattern] [--limit n]
  bench <bench.json>                          Compare builds or shader directories in interleaved rounds
)";

    }

    std::optional<int32_t> RunCommandLineTool(const std::vector<std::string>& args, const std::string& assetDirectory) {

        // Jobs and scenes are found relative to the data directory of the engine
        Loader::AssetLoader::SetAssetDirectory(assetDirectory);
        // The engine log goes into log.txt and its errors are printed, --verbose prints all of it
        Log::printToConsole = std::find(args.begin(), args.end(), "--verbose") != args.end();

        if (args.size() < 2)
            return std::nullopt;

        const auto& command = args[1];
        try {
            if (auto exitCode = RunClient(args))
                return exitCode;
            if (command == "check")
                return Check(args);
            if (command == "kill") {
                if (args.size() < 3)
                    throw std::runtime_error("kill expects a process id or captures");
                return Kill(args[2]);
            }
            if (command == "compose" || command == "--compose")
                return Compose(args);
            if (command == "diff")
                return Diff(args);
            if (command == "inspect")
                return Inspect(args);
            if (command == "bench")
                return RunBench(args);
            if (command == "help" || command == "--help" || command == "-h") {
                Print(GetUsage() + "\n" + GetToolUsage());
                return 0;
            }
        }
        catch (const std::exception& e) {
            Print(std::string("ERROR: ") + e.what());
            return 2;
        }

        // A direct run of a job is checked before the engine starts, servers and pipelines check their jobs themselves
        auto isRendering = std::any_of(args.begin(), args.end(), [](const std::string& arg) { return arg == "--serve" || arg == "--pipeline"; });
        auto jobFile = std::find_if(args.begin() + 1, args.end(), [](const std::string& arg) { return !arg.starts_with("--") && arg.ends_with(".json"); });
        if (!isRendering && jobFile != args.end()) {
            std::vector<std::string> issues;
            try {
                issues = CheckJob(ParseCommandLine(args));
            }
            catch (const std::exception& e) {
                std::string message = e.what();
                Print(message.starts_with("Usage:") ? message + "\n" + GetToolUsage() : "ERROR: " + message);
                return message.starts_with("Usage:") ? 0 : 2;
            }
            for (const auto& issue : issues)
                Print(issue);
            if (!issues.empty() && issues.front().starts_with("ERROR"))
                return 2;
        }

        if (std::find(args.begin(), args.end(), "--wait-gpu") != args.end()) {
            if (!WaitForGpu(0.0f))
                return 4;
        }
        else if (!isRendering || std::find(args.begin(), args.end(), "--serve") == args.end()) {
            for (const auto& instance : InstanceRegistry::GetInstances())
                if (instance.busy && instance.pid != CurrentProcessId())
                    Print("Note: " + instance.executable + " (process " + std::to_string(instance.pid) + ") is busy: " + instance.activity);
        }

        return std::nullopt;

    }

}
