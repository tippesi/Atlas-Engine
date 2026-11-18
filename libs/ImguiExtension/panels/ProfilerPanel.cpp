#include "ProfilerPanel.h"
#include "common/MathHelper.h"

#include "profiler/CPUProfiler.h"
#include "profiler/GPUProfiler.h"

#include <algorithm>
#include <cmath>
#include <functional>

namespace Atlas::ImguiExtension {

    void ProfilerPanel::Render() {

        ImGui::PushID(GetNameID());

        if (ImGui::BeginTabBar("Profilers")) {
            if (ImGui::BeginTabItem("CPU")) {
                if (RenderControls(CPUProfiler::enable, false)) {
                    // The main and the render thread come first and are open, the workers are listed collapsed
                    auto threadData = CPUProfiler::GetQueriesAverage(64, GetOrder());
                    std::stable_partition(threadData.begin(), threadData.end(), [](const auto& thread) {
                        return thread.name == "Main" || thread.name == "Render";
                        });
                    RenderTable(threadData, false);
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("GPU")) {
                if (RenderControls(GPUProfiler::enable, true)) {
                    if (showGraph)
                        perfGraphPanel.Render();
                    else
                        RenderTable(GPUProfiler::GetQueriesAverage(64, GetOrder()), true);
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Hitches")) {
                RenderHitches();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        ImGui::PopID();

    }

    bool ProfilerPanel::RenderControls(std::atomic_bool& enable, bool graph) {

        bool enabled = enable;
        ImGui::Checkbox("Enable##Profiler", &enabled);
        enable = enabled;

        if (graph) {
            ImGui::SameLine();
            ImGui::Checkbox("Show graph##Profiler", &showGraph);
        }

        if (!graph || !showGraph) {
            const char* items[] = { "Chronologically", "Max time", "Min time", "Alphabetically" };
            ImGui::SameLine();
            ImGui::SetNextItemWidth(std::min(ImGui::GetContentRegionAvail().x * 0.5f, ImGui::GetFontSize() * 10.0f));
            ImGui::Combo("Sort##Profiler", &sortItem, items, IM_ARRAYSIZE(items));
        }

        if (!enabled)
            ImGui::TextDisabled("The profiler is disabled, enable it to collect timings");

        return enabled;

    }

    void ProfilerPanel::RenderHitches() {

        auto settings = HitchRecorder::GetSettings();
        auto settingsChanged = false;

        // The revision is read first, a hitch recorded meanwhile is copied in the next frame
        auto revision = HitchRecorder::GetRevision();
        if (revision != hitchRevision) {
            hitchRevision = revision;
            hitches = HitchRecorder::GetHitches();
            hitchSummary = HitchRecorder::Evaluate(hitches);
        }

        settingsChanged |= ImGui::Checkbox("Record##Hitches", &settings.enable);
        ImGui::SameLine();
        if (ImGui::Button("Clear##Hitches"))
            HitchRecorder::Clear();
        ImGui::SameLine();
        settingsChanged |= ImGui::Checkbox("Log##Hitches", &settings.log);
        ImGui::SameLine();
        ImGui::TextDisabled("%d hitches", int32_t(hitches.size()));

        // Wide panels show the hitches and their causes next to the profile of the selected hitch
        auto sideBySide = ImGui::GetContentRegionAvail().x >= ImGui::GetFontSize() * 50.0f;

        if (ImGui::CollapsingHeader("Detection##Hitches"))
            settingsChanged |= RenderHitchSettings(settings, sideBySide);

        if (settingsChanged)
            HitchRecorder::SetSettings(settings);

        if (!sideBySide) {
            if (ImGui::CollapsingHeader("Causes##Hitches", ImGuiTreeNodeFlags_DefaultOpen))
                RenderHitchSummary(0.0f);
            RenderHitchList(0.0f, settings.enable);
            RenderHitchDetails();
            return;
        }

        ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp;
        if (ImGui::BeginTable("HitchLayout", 2, flags, ImVec2(0.0f, ImGui::GetContentRegionAvail().y))) {
            ImGui::TableSetupColumn("Hitches", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("Profile", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            auto height = ImGui::GetContentRegionAvail().y;
            ImGui::BeginChild("Hitches", ImVec2(0.0f, height));
            auto listHeight = std::floor(ImGui::GetContentRegionAvail().y * 0.55f);
            RenderHitchList(listHeight, settings.enable);
            ImGui::SeparatorText("Causes");
            RenderHitchSummary(ImGui::GetContentRegionAvail().y);
            ImGui::EndChild();

            ImGui::TableNextColumn();
            ImGui::BeginChild("Profile", ImVec2(0.0f, height));
            RenderHitchDetails();
            ImGui::EndChild();

            ImGui::EndTable();
        }

    }

    bool ProfilerPanel::RenderHitchSettings(HitchRecorder::Settings& settings, bool sideBySide) {

        auto changed = false;
        // The render thread metric needs a render thread
        auto hasRenderThread = CPUProfiler::GetThreadTime("Render") > 0.0f;

        const char* triggers[] = { "Above minimum", "Above average", "Above minimum and average" };

        auto renderDetector = [&](const char* label, HitchRecorder::Detector& detector, bool gpu) {
            ImGui::PushID(label);
            ImGui::SeparatorText(label);

            changed |= ImGui::Checkbox("Detect", &detector.enable);

            // The CPU detector measures the CPU metrics, the GPU detector the GPU ones
            auto firstMetric = gpu ? int32_t(HitchRecorder::Metric::GPU_BUSY) : int32_t(HitchRecorder::Metric::FRAME_TIME);
            auto metricCount = gpu ? 2 : 3;
            auto metricIdx = std::clamp(int32_t(detector.metric) - firstMetric, 0, metricCount - 1);
            if (ImGui::BeginCombo("Metric", HitchRecorder::GetMetricName(HitchRecorder::Metric(firstMetric + metricIdx)))) {
                for (int32_t i = 0; i < metricCount; i++) {
                    auto metric = HitchRecorder::Metric(firstMetric + i);
                    auto selectableFlags = metric == HitchRecorder::Metric::RENDER_THREAD && !hasRenderThread ?
                        ImGuiSelectableFlags_Disabled : ImGuiSelectableFlags_None;
                    if (ImGui::Selectable(HitchRecorder::GetMetricName(metric), i == metricIdx, selectableFlags)) {
                        detector.metric = metric;
                        changed = true;
                    }
                }
                ImGui::EndCombo();
            }

            auto triggerIdx = int32_t(detector.trigger);
            if (ImGui::Combo("Trigger", &triggerIdx, triggers, IM_ARRAYSIZE(triggers))) {
                detector.trigger = HitchRecorder::Trigger(triggerIdx);
                changed = true;
            }

            if (detector.trigger != HitchRecorder::Trigger::RELATIVE)
                changed |= ImGui::DragFloat("Minimum time", &detector.minimumTime, 0.1f, 0.1f, 1000.0f, "%.1f ms",
                    ImGuiSliderFlags_AlwaysClamp);
            if (detector.trigger != HitchRecorder::Trigger::ABSOLUTE)
                changed |= ImGui::DragFloat("Relative to average", &detector.relativeThreshold, 0.01f, 1.1f, 20.0f, "%.2fx",
                    ImGuiSliderFlags_AlwaysClamp);
            if (detector.metric == HitchRecorder::Metric::RENDER_THREAD && !hasRenderThread)
                ImGui::TextDisabled("There is no render thread");

            auto profilerEnabled = gpu ? GPUProfiler::enable.load() : CPUProfiler::enable.load();
            if (detector.enable && !profilerEnabled) {
                if (gpu || detector.metric != HitchRecorder::Metric::FRAME_TIME)
                    ImGui::TextDisabled("Needs the %s profiler, which is disabled", gpu ? "GPU" : "CPU");
                else
                    ImGui::TextDisabled("The CPU profiler is disabled, hitches are recorded without a profile");
            }

            ImGui::PopID();
            };

        // The labels are on the right of the widgets and keep their room
        if (sideBySide && ImGui::BeginTable("HitchDetectors", 2, ImGuiTableFlags_SizingStretchSame)) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::PushItemWidth(-ImGui::GetFontSize() * 9.0f);
            renderDetector("CPU", settings.cpu, false);
            ImGui::PopItemWidth();
            ImGui::TableNextColumn();
            ImGui::PushItemWidth(-ImGui::GetFontSize() * 9.0f);
            renderDetector("GPU", settings.gpu, true);
            ImGui::PopItemWidth();
            ImGui::EndTable();
        }
        else {
            renderDetector("CPU", settings.cpu, false);
            renderDetector("GPU", settings.gpu, true);
        }

        ImGui::Separator();
        uint32_t minHitches = 1, maxHitches = 4096;
        changed |= ImGui::DragScalar("Kept hitches", ImGuiDataType_U32, &settings.maxHitches, 1.0f, &minHitches, &maxHitches,
            nullptr, ImGuiSliderFlags_AlwaysClamp);

        return changed;

    }

    void ProfilerPanel::RenderHitchSummary(float height) {

        if (hitchSummary.empty()) {
            ImGui::TextWrapped("No causes yet, a cause is a query which took much longer than on average");
            return;
        }

        if (height <= 0.0f) {
            auto rows = std::min(int32_t(hitchSummary.size()), 8) + 1;
            height = ImGui::GetTextLineHeightWithSpacing() * float(rows) + ImGui::GetStyle().CellPadding.y * 2.0f * float(rows);
        }

        ImGuiTableFlags flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_Resizable |
            ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY;
        if (ImGui::BeginTable("HitchCauses", 5, flags, ImVec2(0.0f, height))) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Thread", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 8.0f);
            ImGui::TableSetupColumn("Query", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Hitches", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 4.0f);
            ImGui::TableSetupColumn("Excess (ms)", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 6.0f);
            ImGui::TableSetupColumn("Max (ms)", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 5.0f);
            ImGui::TableHeadersRow();

            for (const auto& summary : hitchSummary) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(summary.thread.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(summary.path.c_str());
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", summary.path.c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%d", int32_t(summary.hitches));
                ImGui::TableNextColumn();
                ImGui::Text("%.1f", summary.totalExcess);
                ImGui::TableNextColumn();
                ImGui::Text("%.1f", summary.maxExcess);
            }

            ImGui::EndTable();
        }

    }

    void ProfilerPanel::RenderHitchList(float height, bool recording) {

        if (hitches.empty()) {
            ImGui::TextDisabled(recording ? "No hitches recorded" : "Recording is disabled");
            return;
        }

        auto selected = GetSelectedHitch();

        if (height <= 0.0f) {
            auto rows = std::min(int32_t(hitches.size()), 8) + 1;
            height = ImGui::GetTextLineHeightWithSpacing() * float(rows) + ImGui::GetStyle().CellPadding.y * 2.0f * float(rows);
        }

        ImGuiTableFlags flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_Resizable |
            ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY;
        if (ImGui::BeginTable("HitchList", 6, flags, ImVec2(0.0f, height))) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Time (s)", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 5.0f);
            ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 9.0f);
            ImGui::TableSetupColumn("ms", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 4.0f);
            ImGui::TableSetupColumn("Average", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 4.0f);
            ImGui::TableSetupColumn("Factor", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 4.0f);
            ImGui::TableSetupColumn("Main cause", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            // The latest first, only the visible rows are drawn
            ImGuiListClipper clipper;
            clipper.Begin(int32_t(hitches.size()));
            while (clipper.Step()) {
                for (auto row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                    const auto& hitch = *hitches[hitches.size() - 1 - size_t(row)];

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    char label[48];
                    std::snprintf(label, sizeof(label), "%.1f##%llu", hitch.time, static_cast<unsigned long long>(hitch.id));
                    if (ImGui::Selectable(label, hitch.id == selected->id, ImGuiSelectableFlags_SpanAllColumns))
                        selectedHitchId = hitch.id;
                    ImGui::TableNextColumn();
                    ImGui::Text("%s %s", hitch.source == HitchRecorder::Source::GPU ? "GPU" : "CPU",
                        HitchRecorder::GetMetricName(hitch.metric));
                    ImGui::TableNextColumn();
                    ImGui::Text("%.1f", hitch.frameTime);
                    ImGui::TableNextColumn();
                    ImGui::Text("%.1f", hitch.averageFrameTime);
                    ImGui::TableNextColumn();
                    if (hitch.averageFrameTime > 0.0f)
                        ImGui::Text("%.1fx", hitch.frameTime / hitch.averageFrameTime);
                    ImGui::TableNextColumn();
                    if (!hitch.causes.empty())
                        ImGui::Text("%s: %s", hitch.causes.front().thread.c_str(), hitch.causes.front().path.c_str());
                    else
                        ImGui::TextDisabled("-");
                }
            }

            ImGui::EndTable();
        }

    }

    void ProfilerPanel::RenderHitchDetails() {

        auto selected = GetSelectedHitch();
        if (!selected)
            return;

        const auto& hitch = *selected;
        auto sourceName = hitch.source == HitchRecorder::Source::GPU ? "GPU" : "CPU";
        ImGui::TextWrapped("%s hitch at %.1f s: %.1f ms %s, average %.1f ms", sourceName, hitch.time, hitch.frameTime,
            HitchRecorder::GetMetricName(hitch.metric), hitch.averageFrameTime);
        for (const auto& cause : hitch.causes)
            ImGui::BulletText("%s: %s  %.1f ms (average %.1f ms)", cause.thread.c_str(), cause.path.c_str(),
                cause.time, cause.averageTime);

        if (hitch.threads.empty())
            ImGui::TextDisabled("No profile, the %s profiler was disabled", sourceName);
        else
            RenderTable(hitch.threads, hitch.source == HitchRecorder::Source::GPU);

    }

    const HitchRecorder::Hitch* ProfilerPanel::GetSelectedHitch() const {

        if (hitches.empty())
            return nullptr;

        // Shows the latest hitch until another one is selected
        auto selected = std::find_if(hitches.begin(), hitches.end(),
            [&](const Ref<const HitchRecorder::Hitch>& hitch) { return hitch->id == selectedHitchId; });
        return selected != hitches.end() ? selected->get() : hitches.back().get();

    }

    void ProfilerPanel::RenderTable(const std::vector<Profiler::ThreadData>& threadData, bool openAllThreads) {

        // Fraction of the thread time is drawn as a bar behind the time, such that expensive queries stand out
        auto barColor = ImGui::GetStyleColorVec4(ImGuiCol_PlotHistogram);
        barColor.w *= 0.35f;
        auto barColorU32 = ImGui::GetColorU32(barColor);

        auto renderRightAligned = [](const char* text) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(text).x);
            ImGui::TextUnformatted(text);
            };

        auto renderTime = [&](double elapsedNs, double totalNs) {
            auto fraction = totalNs > 0.0 ? float(Helper::Math::Saturate(elapsedNs / totalNs)) : 0.0f;

            auto cellMin = ImGui::GetCursorScreenPos();
            auto cellWidth = ImGui::GetContentRegionAvail().x;
            auto lineHeight = ImGui::GetTextLineHeight();
            ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(cellMin.x, cellMin.y + lineHeight - 3.0f),
                ImVec2(cellMin.x + cellWidth * fraction, cellMin.y + lineHeight), barColorU32, 1.5f);

            char text[32];
            std::snprintf(text, sizeof(text), "%.3f", elapsedNs / 1000000.0);
            renderRightAligned(text);
            };

        std::function<void(const Profiler::Query&, double)> displayQuery;
        displayQuery = [&](const Profiler::Query& query, double totalNs) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            ImGuiTreeNodeFlags expandable = 0;
            if (!query.children.size()) expandable = ImGuiTreeNodeFlags_NoTreePushOnOpen |
                ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet;

            bool open = ImGui::TreeNodeEx(query.name.c_str(), expandable | ImGuiTreeNodeFlags_SpanFullWidth);
            ImGui::TableNextColumn();
            renderRightAligned(std::to_string(query.calls).c_str());
            ImGui::TableNextColumn();
            renderTime(double(query.elapsedTime), totalNs);

            if (open && query.children.size()) {
                for (const auto& child : query.children)
                    displayQuery(child, totalNs);
                ImGui::TreePop();
            }
            };

        ImGuiTableFlags flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH |
            ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody |
            ImGuiTableFlags_ScrollY;

        if (ImGui::BeginTable("PerfTable", 3, flags)) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Calls", ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_WidthFixed,
                ImGui::GetFontSize() * 4.0f);
            ImGui::TableSetupColumn("Time (ms)", ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_WidthFixed,
                ImGui::GetFontSize() * 10.0f);
            ImGui::TableHeadersRow();

            for (const auto& thread : threadData) {
                double threadTime = 0.0;
                for (const auto& query : thread.queries)
                    threadTime += double(query.elapsedTime);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                auto open = openAllThreads || thread.name == "Main" || thread.name == "Render";
                ImGuiTreeNodeFlags expandable = open ? ImGuiTreeNodeFlags_DefaultOpen : 0;
                if (!thread.queries.size()) expandable = ImGuiTreeNodeFlags_NoTreePushOnOpen |
                    ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet;

                bool isOpen = ImGui::TreeNodeEx(thread.name.c_str(), expandable | ImGuiTreeNodeFlags_SpanFullWidth);
                ImGui::TableNextColumn();
                ImGui::TableNextColumn();
                renderTime(threadTime, 0.0);

                if (isOpen && thread.queries.size()) {
                    for (const auto& query : thread.queries)
                        displayQuery(query, threadTime);
                    ImGui::TreePop();
                }
            }

            ImGui::EndTable();
        }

    }

    Profiler::OrderBy ProfilerPanel::GetOrder() const {

        switch (sortItem) {
            case 1: return Profiler::OrderBy::MAX_TIME;
            case 2: return Profiler::OrderBy::MIN_TIME;
            case 3: return Profiler::OrderBy::NAME;
            default: return Profiler::OrderBy::CHRONO;
        }

    }

}
