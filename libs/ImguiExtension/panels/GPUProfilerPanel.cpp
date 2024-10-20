#include "GPUProfilerPanel.h"

#include "graphics/Profiler.h"
#include "Clock.h"

namespace Atlas::ImguiExtension {

    void GPUProfilerPanel::Render() {

        ImGui::PushID(GetNameID());

        bool enabled = Graphics::Profiler::enable;
        ImGui::Checkbox("Enable##Profiler", &enabled);
        ImGui::Checkbox("Show graph##Profiler", &showGraph);
        Graphics::Profiler::enable = enabled;

        if (showGraph)
            perfGraphPanel.Render();
        else
            RenderTable();

        ImGui::PopID();

    }

    void GPUProfilerPanel::RenderTable() {

        const char* items[] = { "Chronologically", "Max time", "Min time" };
        static int item = 0;
        ImGui::Combo("Sort##Performance", &item, items, IM_ARRAYSIZE(items));

        Graphics::Profiler::OrderBy order;
        switch (item) {
            case 1: order = Graphics::Profiler::OrderBy::MAX_TIME; break;
            case 2: order = Graphics::Profiler::OrderBy::MIN_TIME; break;
            default: order = Graphics::Profiler::OrderBy::CHRONO; break;
        }

        std::function<void(Graphics::Profiler::Query&)> displayQuery;
        displayQuery = [&displayQuery](Graphics::Profiler::Query& query) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            ImGuiTreeNodeFlags expandable = 0;
            if (!query.children.size()) expandable = ImGuiTreeNodeFlags_NoTreePushOnOpen |
                ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet;

            bool open = ImGui::TreeNodeEx(query.name.c_str(), expandable | ImGuiTreeNodeFlags_SpanFullWidth);
            ImGui::TableNextColumn();
            ImGui::Text("%f", double(query.timer.elapsedTime) / 1000000.0);

            if (open && query.children.size()) {
                for (auto& child : query.children)
                    displayQuery(child);
                ImGui::TreePop();
            }

        };

        static ImGuiTableFlags flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH |
            ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody;

        if (ImGui::BeginTable("PerfTable", 2, flags)) {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);
            ImGui::TableSetupColumn("Elapsed (ms)", ImGuiTableColumnFlags_NoHide);
            ImGui::TableHeadersRow();

            auto threadData = Graphics::Profiler::GetQueriesAverage(64, order);
            for (auto& thread : threadData) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                ImGuiTreeNodeFlags expandable = 0;
                if (!thread.queries.size()) expandable = ImGuiTreeNodeFlags_NoTreePushOnOpen |
                    ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet;

                bool open = ImGui::TreeNodeEx(thread.name.c_str(), expandable | ImGuiTreeNodeFlags_SpanFullWidth);

                if (open && thread.queries.size()) {
                    for (auto &query: thread.queries)
                        displayQuery(query);
                    ImGui::TreePop();
                }
            }


            ImGui::EndTable();
        }

    }

}