#include "ProfilerPanel.h"
#include "common/MathHelper.h"

#include "profiler/CPUProfiler.h"
#include "profiler/GPUProfiler.h"

#include <algorithm>
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
