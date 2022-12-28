#include "App.h"

const std::string Atlas::EngineInstance::assetDirectory = "../data";
const std::string Atlas::EngineInstance::shaderDirectory = "shader";

void App::LoadContent() {

    testRenderer.Init(Atlas::Graphics::GraphicsDevice::DefaultDevice);

    camera = Atlas::Camera(47.0f, 2.0f, 1.0f, 400.0f,
        glm::vec3(30.0f, 25.0f, 0.0f), glm::vec2(-3.14f / 2.0f, 0.0f));

    mouseHandler = Atlas::Input::MouseHandler(&camera, 1.5f, 6.0f);
    keyboardHandler = Atlas::Input::KeyboardHandler(&camera, 7.0f, 6.0f);

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    imguiWrapper.Load(window);

    /*
    io.Fonts->AddFontFromFileTTF(
        Atlas::Loader::AssetLoader::GetFullPath("font/roboto.ttf").c_str(),
        20.0f);
    */

    ImGui::StyleColorsDark();
    for (auto& color : ImGui::GetStyle().Colors) {
        float gamma = 2.2f;
        color.x = powf(color.x, gamma);
        color.y = powf(color.y, gamma);
        color.z = powf(color.z, gamma);
    }

}

void App::UnloadContent() {

    imguiWrapper.Unload();

}

void App::Update(float deltaTime) {

    mouseHandler.Update(&camera, deltaTime);
    keyboardHandler.Update(&camera, deltaTime);

    camera.Update();
    imguiWrapper.Update(window, deltaTime);

}

void App::Render(float deltaTime) {

    testRenderer.Render(&camera);

    ImGui::NewFrame();

    if (ImGui::Begin("Settings", (bool*)0, ImGuiWindowFlags_HorizontalScrollbar)) {
        if (ImGui::CollapsingHeader("Controls")) {
            ImGui::Text("Use WASD for movement");
            ImGui::Text("Use left mouse click + mouse movement to look around");
            ImGui::Text("Use F11 to hide/unhide the UI");
        }
        if (ImGui::CollapsingHeader("Profiler")) {

            const char* items[] = { "Chronologically", "Max time", "Min time" };
            static int item = 0;
            ImGui::Combo("Sort##Performance", &item, items, IM_ARRAYSIZE(items));

            Atlas::Graphics::Profiler::OrderBy order;
            switch (item) {
                case 1: order = Atlas::Graphics::Profiler::OrderBy::MAX_TIME; break;
                case 2: order = Atlas::Graphics::Profiler::OrderBy::MIN_TIME; break;
                default: order = Atlas::Graphics::Profiler::OrderBy::CHRONO; break;
            }

            std::function<void(Atlas::Graphics::Profiler::Query&)> displayQuery;
            displayQuery = [&displayQuery](Atlas::Graphics::Profiler::Query& query) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                ImGuiTreeNodeFlags expandable = 0;
                if (!query.children.size()) expandable = ImGuiTreeNodeFlags_NoTreePushOnOpen |
                                                         ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet;

                bool open = ImGui::TreeNodeEx(query.name.c_str(), expandable | ImGuiTreeNodeFlags_SpanFullWidth);
                ImGui::TableNextColumn();
                ImGui::Text("%f", double(query.timer.elapsedTime) / 1000000.0);
                // ImGui::TableNextColumn();
                // ImGui::TextUnformatted(node->Type);

                if (open && query.children.size()) {
                    for (auto& child : query.children)
                        displayQuery(child);
                    ImGui::TreePop();
                }

            };

            static ImGuiTableFlags flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody;

            if (ImGui::BeginTable("PerfTable", 2, flags))
            {
                // The first column will use the default _WidthStretch when ScrollX is Off and _WidthFixed when ScrollX is On
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);
                ImGui::TableSetupColumn("Elapsed (ms)", ImGuiTableColumnFlags_NoHide);
                //ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_NoHide);
                ImGui::TableHeadersRow();

                auto threadData = Atlas::Graphics::Profiler::GetQueriesAverage(64, order);
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

        ImGui::End();
    }

    ImGui::Render();
    imguiWrapper.Render();

}

Atlas::EngineInstance* GetEngineInstance() {

    return new App();

}