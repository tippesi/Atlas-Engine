#include "Summary.h"

#include <map>

namespace Atlas::Capture {

    // The first line, e.g. of a Lua error without its stack traceback
    static std::string Shorten(const std::string& text, size_t length = 160) {

        auto line = text.substr(0, text.find('\n'));
        if (line.size() > length)
            return line.substr(0, length - 3) + "...";
        return line.size() < text.size() ? line + " ..." : line;

    }

    // Equal messages once with their count, in the order they first appeared
    static std::vector<std::string> CountDuplicates(const json& messages) {

        std::vector<std::string> order;
        std::map<std::string, int32_t> counts;
        for (const auto& message : messages) {
            auto text = message.is_string() ? message.get<std::string>() : message.dump();
            if (counts[text]++ == 0)
                order.push_back(text);
        }

        std::vector<std::string> lines;
        for (const auto& text : order)
            lines.push_back(Shorten(text) + (counts[text] > 1 ? " (" + std::to_string(counts[text]) + "x)" : ""));
        return lines;

    }

    std::vector<std::string> FormatSummary(const json& report, const fs::path& outputDirectory) {

        std::vector<std::string> lines;
        auto add = [&](const std::string& line) { lines.push_back(line); };

        auto status = report.value("status", std::string("unknown"));
        std::string head = report.value("job", std::string("job")) + ": " + status;
        if (report.contains("totalSeconds"))
            head += " in " + FormatFloat(report["totalSeconds"].get<float>(), 1) + " s";
        std::vector<std::string> parts;
        if (report.value("loadSeconds", 0.0f) > 0.05f)
            parts.push_back("load " + FormatFloat(report["loadSeconds"].get<float>(), 1) + " s");
        if (report.contains("playedSeconds"))
            parts.push_back("played " + FormatFloat(report["playedSeconds"].get<float>(), 1) + " s");
        if (!parts.empty()) {
            head += " (";
            for (size_t i = 0; i < parts.size(); i++)
                head += (i ? ", " : "") + parts[i];
            head += ")";
        }
        if (report.contains("reason"))
            head += ": " + report["reason"].get<std::string>();
        add(head);

        constexpr size_t maxItems = 6;
        auto list = [&](const std::string& title, const std::vector<std::string>& items) {

            if (items.empty())
                return;
            add("  " + title + ":");
            for (size_t i = 0; i < std::min(items.size(), maxItems); i++)
                add("    " + items[i]);
            if (items.size() > maxItems)
                add("    ... " + std::to_string(items.size() - maxItems) + " more in report.json");

        };

        list("errors", CountDuplicates(report.value("errors", json::array())));

        // Failures of a check are listed by their message, entities with the same failure are counted
        if (report.contains("checks")) {
            std::vector<std::string> failures;
            for (const auto& failure : report["checks"].value("failures", json::array())) {
                // Log failures are named by their message
                auto check = failure.value("check", std::string());
                auto entity = check == "log" ? std::string() : failure.value("entity", std::string()) + ": ";
                failures.push_back("[" + check + "] " + entity + Shorten(failure.value("message", std::string()), 140) + " (t=" +
                    FormatFloat(failure.value("firstTime", 0.0f), 2) + "s, " + std::to_string(failure.value("occurrences", 1)) + "x)");
            }
            list("failed checks", failures);
        }

        if (report.contains("expectations")) {
            std::vector<std::string> failed;
            int32_t passed = 0;
            for (const auto& expectation : report["expectations"]) {
                if (expectation.value("passed", false)) {
                    passed++;
                    continue;
                }
                auto expect = expectation.value("expect", json());
                auto what = expect.contains("lua") ? expect["lua"].get<std::string>() : expect.dump();
                if (expect.contains("op"))
                    what += " " + expect["op"].get<std::string>() + " " + expect.value("value", json()).dump();
                failed.push_back("t=" + FormatFloat(expectation.value("t", 0.0f), 2) + "s " + Shorten(what, 100) + ", actual " +
                    Shorten(expectation.contains("error") ? expectation["error"].dump() : expectation.value("actual", json()).dump(), 100));
            }
            add("  expectations: " + std::to_string(passed) + " passed" + (failed.empty() ? "" : ", " + std::to_string(failed.size()) + " failed"));
            list("failed expectations", failed);
        }

        if (report.contains("comparisons")) {
            std::vector<std::string> differing;
            size_t same = 0;
            for (const auto& comparison : report["comparisons"]) {
                if (comparison.value("status", std::string()) == "same")
                    same++;
                else
                    differing.push_back(comparison.value("image", std::string()) + ": " + comparison.value("status", std::string()) +
                        (comparison.contains("percent") ? " (" + FormatFloat(comparison["percent"].get<float>(), 2) + "% of the pixels)" : ""));
            }
            add("  reference images: " + std::to_string(same) + " same" + (differing.empty() ? "" : ", " + std::to_string(differing.size()) + " differ"));
            list("differing images", differing);
        }

        if (report.contains("conditionalEvents")) {
            std::vector<std::string> missed;
            for (const auto& trigger : report["conditionalEvents"])
                if (!trigger.value("fired", false))
                    missed.push_back(Shorten(trigger.value("event", json()).dump(), 140));
            list("conditions that never happened", missed);
        }

        if (report.contains("results") && !report["results"].empty()) {
            std::vector<std::string> results;
            for (const auto& [key, value] : report["results"].items())
                results.push_back(key + " = " + Shorten(value.dump(), 200));
            add("  results:");
            for (const auto& result : results)
                add("    " + result);
        }

        auto warnings = CountDuplicates(report.value("warnings", json::array()));
        if (!warnings.empty())
            add("  warnings: " + std::to_string(warnings.size()) + ", first: " + warnings.front());
        if (report.value("engineLogErrors", 0) > 0)
            add("  engine log errors: " + std::to_string(report["engineLogErrors"].get<int32_t>()) + " (log.txt)");

        // Images by count, the sheet first, other files by name
        int32_t images = 0;
        std::vector<std::string> files;
        for (const auto& output : report.value("outputs", json::array())) {
            auto path = output.get<std::string>();
            auto extension = fs::path(path).extension().string();
            if (path == "sheet.jpg")
                files.insert(files.begin(), path);
            else if (extension == ".png" || extension == ".jpg")
                images++;
            else
                files.push_back(path);
        }
        std::string outputs = "  outputs in " + outputDirectory.string() + ":";
        for (const auto& file : files)
            outputs += " " + file;
        if (images > 0)
            outputs += " +" + std::to_string(images) + " images";
        add(outputs);

        return lines;

    }

}
