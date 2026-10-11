#include "GPUProfilerPanel.h"
#include "common/MathHelper.h"

#include "graphics/Profiler.h"
#include "Clock.h"

namespace Atlas::ImguiExtension {

    void GPUProfilerPanel::Render() {

        ImGui::PushID(GetNameID());

        // All controls in one row
        bool enabled = Graphics::Profiler::enable;
        ImGui::Checkbox("Enable##Profiler", &enabled);
        Graphics::Profiler::enable = enabled;

        ImGui::SameLine();
        ImGui::Checkbox("Show graph##Profiler", &showGraph);

        if (!showGraph) {
            const char* items[] = { "Chronologically", "Max time", "Min time" };
            ImGui::SameLine();
            ImGui::SetNextItemWidth(std::min(ImGui::GetContentRegionAvail().x * 0.5f, ImGui::GetFontSize() * 10.0f));
            ImGui::Combo("Sort##Performance", &sortItem, items, IM_ARRAYSIZE(items));
        }

        if (!enabled)
            ImGui::TextDisabled("The profiler is disabled, enable it to collect GPU timings");
        else if (showGraph)
            perfGraphPanel.Render();
        else
            RenderTable();

        ImGui::PopID();

    }

    void GPUProfilerPanel::RenderTable() {

        Graphics::Profiler::OrderBy order;
        switch (sortItem) {
            case 1: order = Graphics::Profiler::OrderBy::MAX_TIME; break;
            case 2: order = Graphics::Profiler::OrderBy::MIN_TIME; break;
            default: order = Graphics::Profiler::OrderBy::CHRONO; break;
        }

        // Fraction of the thread time is drawn as a bar behind the time, such that expensive passes stand out
        auto barColor = ImGui::GetStyleColorVec4(ImGuiCol_PlotHistogram);
        barColor.w *= 0.35f;
        auto barColorU32 = ImGui::GetColorU32(barColor);

        auto renderTime = [&](double elapsedNs, double totalNs) {
            auto ms = elapsedNs / 1000000.0;
            auto fraction = totalNs > 0.0 ? float(Helper::Math::Saturate(elapsedNs / totalNs)) : 0.0f;

            auto cellMin = ImGui::GetCursorScreenPos();
            auto cellWidth = ImGui::GetContentRegionAvail().x;
            auto lineHeight = ImGui::GetTextLineHeight();
            ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(cellMin.x, cellMin.y + lineHeight - 3.0f),
                ImVec2(cellMin.x + cellWidth * fraction, cellMin.y + lineHeight), barColorU32, 1.5f);

            char text[32];
            std::snprintf(text, sizeof(text), "%.3f", ms);
            auto textWidth = ImGui::CalcTextSize(text).x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + cellWidth - textWidth);
            ImGui::TextUnformatted(text);
            };

        std::function<void(Graphics::Profiler::Query&, double)> displayQuery;
        displayQuery = [&](Graphics::Profiler::Query& query, double totalNs) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            ImGuiTreeNodeFlags expandable = 0;
            if (!query.children.size()) expandable = ImGuiTreeNodeFlags_NoTreePushOnOpen |
                ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet;

            bool open = ImGui::TreeNodeEx(query.name.c_str(), expandable | ImGuiTreeNodeFlags_SpanFullWidth);
            ImGui::TableNextColumn();
            renderTime(double(query.timer.elapsedTime), totalNs);

            if (open && query.children.size()) {
                for (auto& child : query.children)
                    displayQuery(child, totalNs);
                ImGui::TreePop();
            }

        };

        ImGuiTableFlags flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH |
            ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody |
            ImGuiTableFlags_ScrollY;

        if (ImGui::BeginTable("PerfTable", 2, flags)) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Time (ms)", ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_WidthFixed,
                ImGui::GetFontSize() * 14.0f);
            ImGui::TableHeadersRow();

            auto threadData = Graphics::Profiler::GetQueriesAverage(64, order);
            for (auto& thread : threadData) {
                double threadTime = 0.0;
                for (const auto& query : thread.queries)
                    threadTime += double(query.timer.elapsedTime);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                ImGuiTreeNodeFlags expandable = ImGuiTreeNodeFlags_DefaultOpen;
                if (!thread.queries.size()) expandable = ImGuiTreeNodeFlags_NoTreePushOnOpen |
                    ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet;

                bool open = ImGui::TreeNodeEx(thread.name.c_str(), expandable | ImGuiTreeNodeFlags_SpanFullWidth);
                ImGui::TableNextColumn();
                renderTime(threadTime, 0.0);

                if (open && thread.queries.size()) {
                    for (auto &query: thread.queries)
                        displayQuery(query, threadTime);
                    ImGui::TreePop();
                }
            }

            ImGui::EndTable();
        }

    }

}
