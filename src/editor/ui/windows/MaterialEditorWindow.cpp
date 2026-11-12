#include "MaterialEditorWindow.h"

#include "../../DataCreator.h"
#include "../../Notifications.h"
#include "../../Singletons.h"
#include "../../Theme.h"
#include "../Widgets.h"

#include "loader/AssetLoader.h"
#include "loader/MaterialGraphLoader.h"
#include "materialgraph/Baker.h"
#include "materialgraph/MaterialGraphSerializer.h"
#include "mesh/MeshSerializer.h"
#include "loader/MeshLoader.h"
#include "loader/MaterialLoader.h"

#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace Atlas::Editor::UI {

    static const char* previewMeshNames[] = { "Detailed sphere", "Sphere", "Capsule", "Wall" };
    static const char* previewMeshPaths[] = { "", "meshes/chromesphere.aemesh", "meshes/capsule.aemesh", "meshes/metallicwall.aemesh" };

    MaterialEditorWindow::MaterialEditorWindow(ResourceHandle<Material> material, bool show) :
        AssetEditorWindow(material.IsValid() ? material.GetResource()->GetFileName() : "No material", show), material(material) {

        Init();

    }

    MaterialEditorWindow::MaterialEditorWindow(ResourceHandle<MaterialGraph::Graph> graph, bool show) :
        AssetEditorWindow(graph.IsValid() ? graph.GetResource()->GetFileName() : "No graph", show),
        material(ResourceHandle<Material>(CreateRef<Material>())), editedGraph(graph) {

        material->graph = graph;
        Init();

    }

    MaterialEditorWindow::~MaterialEditorWindow() {

        JobSystem::Wait(graphUsersJob);

        if (material.IsLoaded() && material->HasGraph())
            material->graph->blockingPipelineUsers--;

    }

    void MaterialEditorWindow::Init() {

        LoadPreviewMesh();

        if (material.IsLoaded() && material->HasGraph()) {
            material->graph->blockingPipelineUsers++;
            FindOtherUsers();
        }

        if (material.IsLoaded() && !material->graphBake.meshPath.empty()) {
            bakeMesh = ResourceManager<Mesh::Mesh>::GetOrLoadResourceWithLoaderAsync(material->graphBake.meshPath, ResourceOrigin::User,
                Loader::MeshLoader::LoadMesh, true);
        }

    }

    void MaterialEditorWindow::LoadPreviewMesh() {

        previewMesh = ResourceHandle<Mesh::Mesh>(previewMeshIdx == 0 ? DataCreator::CreateSphere(material, 128, 192) :
            Loader::MeshLoader::LoadMesh(previewMeshPaths[previewMeshIdx], true));

        previewMesh->data.materials = { material };
        for (auto& subData : previewMesh->data.subData) {
            subData.material = material;
            subData.materialIdx = 0;
        }

    }

    void MaterialEditorWindow::Save() {

        if (!material.IsLoaded())
            return;

        if (material->HasGraph() && !material->graph.IsGenerated()) {
            Loader::MaterialGraphLoader::SaveGraph(material->graph.Get(), material->graph.GetResource()->path);
            graphPanel.modified = false;
        }

        if (material.IsGenerated()) {
            Notifications::Push({ .message = "Saved material graph " + editedGraph.GetResource()->GetFileName() });
            return;
        }

        Loader::MaterialLoader::SaveMaterial(material.Get(), material.GetResource()->path);
        Notifications::Push({ .message = "Saved material " + material.GetResource()->GetFileName() });

    }

    std::string MaterialEditorWindow::GetAssetPath() const {

        if (editedGraph.IsValid())
            return editedGraph.GetResource()->path;

        return material.IsValid() ? material.GetResource()->path : "";

    }

    bool MaterialEditorWindow::IsReady() const {

        return material.IsLoaded() && (!editedGraph.IsValid() || editedGraph.IsLoaded());

    }

    Scene::Entity MaterialEditorWindow::CreatePreviewEntity() {

        return scene->CreatePrefab<Scene::Prefabs::MeshInstance>(previewMesh, false);

    }

    Volume::AABB MaterialEditorWindow::GetBounds() const {

        return previewMesh->data.aabb;

    }

    void MaterialEditorWindow::RenderProperties() {

        RenderHeading("Saves the material and its graph (Ctrl+S)");

        // Path tracing doesn't run graphs, the preview would only show the values for ray tracing
        viewportPanel.allowPathTracing = !material->HasGraph();

        if (Widgets::SectionHeader("Material graph", ImGuiTreeNodeFlags_DefaultOpen))
            RenderGraphSection();

        auto compiled = material->HasGraph() ? material->graph->GetCompiled() : nullptr;
        auto replacesSurface = compiled && compiled->valid && compiled->hasSurface;

        if (replacesSurface && Widgets::SectionHeader("Bake for ray tracing", ImGuiTreeNodeFlags_DefaultOpen))
            RenderBakeSection(*compiled);

        if (Widgets::SectionHeader(replacesSurface ? "Properties for ray tracing" : "Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (replacesSurface) {
                Widgets::Callout("The graph describes the surface. Ray traced reflections, global illumination and shadows "
                    "don't run graphs, they use these values instead.", Theme::Accent());
            }

            materialPanel.Render(Singletons::imguiWrapper, material.Get(), [&](ResourceHandle<Texture::Texture2D> texture) {
                return textureSelectionPanel.Render(texture);
                });
            textureSelectionPanel.Reset();
        }

        // Just update the pipelines regardless of any change, as in the mesh component panel
        previewMesh->UpdatePipelines();

    }

    void MaterialEditorWindow::RenderPanels() {

        ImGui::Begin(graphPanelWindow.GetNameID());

        graphPanelWindow.isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

        if (graphPanel.maximized && material->HasGraph()) {
            ImGui::TextDisabled("The graph is maximized");
        }
        else if (material->HasGraph()) {
            RenderGraphCanvas();
        }
        else {
            ImGui::TextDisabled("This material has no graph, its surface comes from the properties");
            if (!editedGraph.IsValid()) {
                ImGui::Spacing();
                Theme::PushPrimaryButton();
                if (ImGui::Button("Create material graph"))
                    CreateGraph();
                Theme::PopPrimaryButton();
            }
        }

        ImGui::End();

        if (graphPanel.maximized && material->HasGraph()) {
            auto viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);

            auto name = "Material graph##Maximized" + std::string(graphPanelWindow.GetNameID());
            ImGui::Begin(name.c_str(), nullptr, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);

            graphPanelWindow.isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
            RenderGraphCanvas();

            ImGui::End();
        }

    }

    void MaterialEditorWindow::RenderGraphCanvas() {

        RenderSharedGraphWarning();

        if (graphPanel.Render(material->graph, editedGraph.IsValid() ? nullptr : material.Get().get()))
            UpdateMeshesOfGraph();

    }

    void MaterialEditorWindow::RenderSharedGraphWarning() {

        if (!graphUsers || !graphUsers->finished)
            return;

        // The button below replaces graphUsers
        auto currentUsers = graphUsers;

        const auto& users = currentUsers->materials;
        if (users.empty())
            return;

        std::string names;
        const size_t maxNames = 3;
        for (size_t i = 0; i < std::min(users.size(), maxNames); i++)
            names += (i ? ", " : "") + std::filesystem::path(users[i]).stem().string();
        if (users.size() > maxNames)
            names += " and " + std::to_string(users.size() - maxNames) + " more";

        auto text = (editedGraph.IsValid() ? "Used by " + std::to_string(users.size()) + (users.size() == 1 ? " material: " : " materials: ") :
            std::to_string(users.size()) + (users.size() == 1 ? " other material uses" : " other materials use") + " this graph: ") + names +
            ". Changing nodes changes all of them. Parameter values belong to each material.";

        Widgets::Callout(text.c_str(), Theme::Warning());
        if (ImGui::IsItemHovered() && users.size() > maxNames) {
            ImGui::BeginTooltip();
            for (const auto& user : users)
                ImGui::TextUnformatted(user.c_str());
            ImGui::EndTooltip();
        }

        if (!editedGraph.IsValid()) {
            if (ImGui::Button("Make a copy for this material"))
                MakeGraphUnique();
            ImGui::SetItemTooltip("This material gets its own graph file, the other materials keep the current one");
        }

    }

    void MaterialEditorWindow::FindOtherUsers() {

        JobSystem::Wait(graphUsersJob);

        graphUsers = CreateRef<GraphUsers>();

        if (!material.IsLoaded() || !material->graph.IsValid() || material->graph.IsGenerated())
            return;

        auto graphPath = material->graph.GetResource()->path;
        auto ownPath = material.IsGenerated() ? std::string() : material.GetResource()->path;

        // Materials which are changed and aren't saved yet only know the current state
        std::vector<std::string> loaded;
        for (const auto& loadedMaterial : ResourceManager<Material>::GetResources()) {
            if (!loadedMaterial.IsLoaded() || loadedMaterial.IsGenerated() || loadedMaterial.GetResource()->path == ownPath)
                continue;
            if (loadedMaterial->graph.IsValid() && !loadedMaterial->graph.IsGenerated() && loadedMaterial->graph.GetResource()->path == graphPath)
                loaded.push_back(loadedMaterial.GetResource()->path);
        }

        JobSystem::Execute(graphUsersJob, [users = graphUsers, graphPath, ownPath, loaded = std::move(loaded)](JobData&) {
            namespace fs = std::filesystem;

            auto root = fs::path(Loader::AssetLoader::GetAssetDirectory());
            auto fileName = fs::path(graphPath).filename().string();
            auto materials = loaded;

            std::error_code error;
            for (auto iter = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, error);
                !error && iter != fs::recursive_directory_iterator(); iter.increment(error)) {
                const auto& path = iter->path();

                if (iter->is_directory(error)) {
                    auto name = path.filename().string();
                    if (!name.empty() && name[0] == '.')
                        iter.disable_recursion_pending();
                    continue;
                }

                if (path.extension() != ".aematerial")
                    continue;

                std::ifstream file(path);
                std::stringstream content;
                content << file.rdbuf();
                auto text = content.str();

                // Most materials don't have a graph, parsing them isn't needed
                if (text.find(fileName) == std::string::npos)
                    continue;

                auto relative = fs::relative(path, root, error).generic_string();
                if (relative == ownPath || std::find(materials.begin(), materials.end(), relative) != materials.end())
                    continue;

                auto j = json::parse(text, nullptr, false);
                if (j.is_discarded() || !j.contains("graphPath"))
                    continue;

                auto reference = j["graphPath"].get<std::string>();
                // References inside of packages are relative to the file
                auto resolved = reference.starts_with(".") ? fs::path(relative).parent_path() / reference : fs::path(reference);
                if (resolved.lexically_normal().generic_string() == graphPath)
                    materials.push_back(relative);
            }

            users->materials = std::move(materials);
            users->finished = true;
            });

    }

    void MaterialEditorWindow::MakeGraphUnique() {

        auto directory = std::filesystem::path(material.GetResource()->path).parent_path();
        auto stem = std::filesystem::path(material.GetResource()->path).stem().string();

        auto path = (directory / (stem + ".aematgraph")).generic_string();
        int32_t counter = 1;
        while (std::filesystem::exists(Loader::AssetLoader::GetFullPath(path)))
            path = (directory / (stem + "_" + std::to_string(counter++) + ".aematgraph")).generic_string();

        auto graph = CreateRef<MaterialGraph::Graph>(*material->graph.Get());
        graph->name = stem;
        graph->Compile();
        Loader::MaterialGraphLoader::SaveGraph(graph, path);

        AttachGraph(ResourceManager<MaterialGraph::Graph>::AddResource(path, ResourceOrigin::User, graph));

        Notifications::Push({ .message = "This material now has its own graph " + std::filesystem::path(path).filename().string() +
            ", save the material to keep that" });

    }

    void MaterialEditorWindow::DockPanels(ImGuiID side, ImGuiID bottom) {

        if (bottom)
            ImGui::DockBuilderDockWindow(graphPanelWindow.GetNameID(), bottom);

    }

    bool MaterialEditorWindow::IsPanelFocused() const {

        return graphPanelWindow.isFocused;

    }

    bool MaterialEditorWindow::AllowsViewportShortcuts() const {

        return !graphPanelWindow.isFocused;

    }

    void MaterialEditorWindow::RenderViewportSettings() {

        ImGui::SeparatorText("Preview");
        if (ImGui::Combo("Mesh", &previewMeshIdx, previewMeshNames, IM_ARRAYSIZE(previewMeshNames))) {
            LoadPreviewMesh();
            if (scene) {
                scene->DestroyEntity(entity);
                entity = CreatePreviewEntity();
                FocusAsset();
            }
        }
        ImGui::SetItemTooltip("Moving vertices need a mesh with enough vertices");

    }

    void MaterialEditorWindow::CreateGraph() {

        auto directory = std::filesystem::path(material.GetResource()->path).parent_path();
        auto stem = std::filesystem::path(material.GetResource()->path).stem().string();

        // Graphs can be shared, so an existing one of another material stays untouched
        auto path = (directory / (stem + ".aematgraph")).generic_string();
        int32_t counter = 1;
        while (std::filesystem::exists(Loader::AssetLoader::GetFullPath(path)))
            path = (directory / (stem + "_" + std::to_string(counter++) + ".aematgraph")).generic_string();

        auto graph = MaterialGraph::Graph::CreateDefault();
        graph->name = stem;
        Loader::MaterialGraphLoader::SaveGraph(graph, path);

        AttachGraph(ResourceManager<MaterialGraph::Graph>::AddResource(path, ResourceOrigin::User, graph));
        graphPanel.FrameAll();

    }

    json MaterialEditorWindow::SaveState() const {

        json j;
        j["material"] = *material.Get();
        if (material->HasGraph())
            j["graph"] = *material->graph.Get();
        return j;

    }

    void MaterialEditorWindow::RestoreState(const json& j) {

        auto graph = material->graph;
        j["material"].get_to(*material.Get());

        // Graphs without a file aren't part of the material state, the attached one stays
        auto restoredGraph = j["material"].contains("graphPath") ? material->graph :
            j.contains("graph") ? graph : ResourceHandle<MaterialGraph::Graph>();
        material->graph = graph;
        if (restoredGraph.GetResource() != graph.GetResource())
            AttachGraph(restoredGraph);

        if (j.contains("graph") && material->HasGraph()) {
            j["graph"].get_to(*material->graph.Get());
            graphPanel.OnGraphRestored(*material->graph.Get());
        }

    }

    void MaterialEditorWindow::AttachGraph(const ResourceHandle<MaterialGraph::Graph>& graph) {

        if (material->HasGraph())
            material->graph->blockingPipelineUsers--;

        material->graph = graph;

        if (material->HasGraph())
            material->graph->blockingPipelineUsers++;

        graphPanel.Reset();

        UpdateMeshesOfGraph();
        FindOtherUsers();

    }

    void MaterialEditorWindow::RenderGraphSection() {

        if (!editedGraph.IsValid()) {
            bool changed = false;
            auto graph = graphSelectionPanel.Render(material->graph, changed);
            graphSelectionPanel.Reset();
            if (changed) {
                AttachGraph(graph);
                graphPanel.FrameAll();
            }

            if (!material->graph.IsValid()) {
                Theme::PushPrimaryButton();
                if (ImGui::Button("Create material graph"))
                    CreateGraph();
                Theme::PopPrimaryButton();
            }
        }

        if (!material->HasGraph())
            return;

        auto compiled = material->graph->GetCompiled();
        if (!compiled)
            return;

        for (const auto& error : compiled->errors)
            Widgets::Callout(error.message.c_str(), Theme::Error());

        if (!compiled->parameters.empty()) {
            ImGui::SeparatorText("Parameters");
            RenderParameters(*compiled);
        }

    }

    void MaterialEditorWindow::RenderParameters(const MaterialGraph::CompiledGraph& compiled) {

        using namespace MaterialGraph;

        // Values come before textures, which are larger. Within that the graph decides
        std::vector<const Parameter*> sorted;
        for (const auto& parameter : compiled.parameters)
            sorted.push_back(&parameter);

        std::map<std::string, int32_t> groupOrder;
        for (auto parameter : sorted) {
            auto [iter, inserted] = groupOrder.try_emplace(parameter->group, parameter->order);
            if (!inserted)
                iter->second = std::min(iter->second, parameter->order);
        }

        auto getRank = [](const Parameter& parameter) {
            return parameter.isColor ? 0 : parameter.type == ValueType::Texture ? 2 : 1;
            };

        std::stable_sort(sorted.begin(), sorted.end(), [&](const Parameter* a, const Parameter* b) {
            // Parameters without a group are always on top
            auto aKey = std::tuple(!a->group.empty(), groupOrder[a->group], a->group, a->order, getRank(*a), a->nodeId);
            auto bKey = std::tuple(!b->group.empty(), groupOrder[b->group], b->group, b->order, getRank(*b), b->nodeId);
            return aKey < bKey;
            });

        auto toLower = [](std::string text) {
            std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return char(std::tolower(c)); });
            return text;
            };

        if (sorted.size() > maxParametersWithoutSearch) {
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputTextWithHint("##Search parameters", "Search parameters...", &parameterSearch);
        }
        else {
            parameterSearch.clear();
        }

        auto search = toLower(parameterSearch);
        auto matches = [&](const Parameter& parameter) {
            return toLower(parameter.name).find(search) != std::string::npos || toLower(parameter.group).find(search) != std::string::npos;
            };

        const auto iconColumnWidth = ImGui::GetTextLineHeight() + 2.0f * ImGui::GetStyle().FramePadding.x;

        for (size_t start = 0; start < sorted.size();) {
            const auto& group = sorted[start]->group;
            auto end = std::find_if(sorted.begin() + start, sorted.end(), [&](const Parameter* parameter) { return parameter->group != group; });
            auto count = std::count_if(sorted.begin() + start, end, [&](const Parameter* parameter) { return matches(*parameter); });

            auto isOpen = count > 0;
            if (count && !group.empty()) {
                // A search shows all results, without changing which groups stay open afterwards
                if (!search.empty()) {
                    ImGui::SeparatorText(group.c_str());
                }
                else {
                    auto label = group + " (" + std::to_string(count) + ")###" + group;
                    auto flags = ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth |
                        (sorted.size() <= maxParametersWithoutSearch ? ImGuiTreeNodeFlags_DefaultOpen : 0);
                    isOpen = ImGui::TreeNodeEx(label.c_str(), flags);
                }
            }

            if (isOpen && ImGui::BeginTable(("##Parameters" + group).c_str(), 3, ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.35f);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.65f);
                ImGui::TableSetupColumn("Reset", ImGuiTableColumnFlags_WidthFixed, iconColumnWidth);

                for (auto iter = sorted.begin() + start; iter != end; iter++) {
                    if (matches(**iter))
                        RenderParameter(**iter);
                }

                ImGui::EndTable();
            }

            start = size_t(end - sorted.begin());
        }

    }

    void MaterialEditorWindow::RenderParameter(const MaterialGraph::Parameter& parameter) {

        using namespace MaterialGraph;

        ImGui::PushID(parameter.name.c_str());
        ImGui::TableNextRow();

        auto isTexture = parameter.type == ValueType::Texture;
        auto isOverridden = isTexture ? material->graphTextures.contains(parameter.name) : material->graphValues.contains(parameter.name);

        // Names of parameters often are one word per capital letter
        std::string name;
        for (size_t i = 0; i < parameter.name.size(); i++) {
            auto character = parameter.name[i];
            if (i > 0 && std::isupper(static_cast<unsigned char>(character)) && std::islower(static_cast<unsigned char>(parameter.name[i - 1])))
                name += ' ';
            name += character;
        }

        // Values of the graph are muted, the ones of this material stand out
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        if (isOverridden)
            ImGui::TextUnformatted(name.c_str());
        else
            ImGui::TextColored(Theme::TextMuted(), "%s", name.c_str());
        ImGui::SetItemTooltip("%s\n%s", parameter.name.c_str(), isOverridden ? "This material has its own value" : "Uses the default of the graph");

        ImGui::TableNextColumn();
        if (isTexture) {
            auto changed = false;
            auto selected = textureSelectionPanel.Render(material->GetGraphTexture(parameter), changed);
            if (changed)
                material->SetGraphTexture(parameter.name, selected);
        }
        else {
            auto value = material->GetGraphValue(parameter);
            auto components = int32_t(parameter.type) + 1;

            auto changed = false;
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (parameter.isColor)
                changed = ImGui::ColorEdit3("##Value", &value.x);
            else if (parameter.HasRange())
                changed = ImGui::SliderScalarN("##Value", ImGuiDataType_Float, &value.x, components, &parameter.range.x, &parameter.range.y);
            else
                changed = ImGui::DragScalarN("##Value", ImGuiDataType_Float, &value.x, components, 0.01f);

            if (changed)
                material->SetGraphParameter(parameter.name, value);
        }

        ImGui::TableNextColumn();
        if (isOverridden && Widgets::IconButton("Reset", Singletons::icons->Get(IconType::Reset), ImGui::GetTextLineHeight(), "Back to the default of the graph")) {
            material->ResetGraphParameter(parameter.name);
        }

        ImGui::PopID();

    }

    void MaterialEditorWindow::RenderBakeSection(const MaterialGraph::CompiledGraph& compiled) {

        using namespace MaterialGraph;

        auto& settings = material->graphBake;

        if (material.IsGenerated()) {
            ImGui::TextDisabled("Only materials can be baked, this window edits a graph on its own");
            return;
        }

        ImGui::PushTextWrapPos();
        ImGui::TextColored(Theme::TextMuted(), "Ray traced reflections, global illumination and shadows don't run graphs. Baking writes the surface "
            "of the graph into the properties below, which they use.");
        ImGui::PopTextWrapPos();

        if (Widgets::BeginProperties("Bake settings")) {
            const char* modes[] = { "Textures: one moment on one mesh", "Average: one value per property" };
            auto mode = int32_t(settings.mode);
            Widgets::PropertyLabel("Mode", "Textures keep the details, but are frozen at one point in time and only fit meshes "
                "with the same positions and texture coordinates. Averages lose all details and work for every mesh");
            if (ImGui::Combo("##Mode", &mode, modes, IM_ARRAYSIZE(modes)))
                settings.mode = BakeMode(mode);

            if (settings.mode == BakeMode::Textures) {
                const int32_t resolutions[] = { 256, 512, 1024, 2048, 4096 };
                Widgets::PropertyLabel("Resolution");
                if (ImGui::BeginCombo("##Resolution", std::to_string(settings.resolution).c_str())) {
                    for (auto resolution : resolutions) {
                        if (ImGui::Selectable(std::to_string(resolution).c_str(), resolution == settings.resolution))
                            settings.resolution = resolution;
                    }
                    ImGui::EndCombo();
                }
            }

            Widgets::PropertyLabel("Time", "The moment which gets baked, for graphs which change over time");
            ImGui::DragFloat("##Time", &settings.time, 0.05f, 0.0f, 3600.0f, "%.2f s");

            Widgets::PropertyLabel("Mesh", "Positions and texture coordinates come from this mesh. Without one the sphere of "
                "this window is used");
            bool meshChanged = false;
            auto selected = meshSelectionPanel.Render(bakeMesh, meshChanged);
            meshSelectionPanel.Reset();
            if (meshChanged) {
                bakeMesh = selected;
                settings.meshPath = bakeMesh.IsValid() ? bakeMesh.GetResource()->path : "";
            }

            Widgets::EndProperties();
        }

        Theme::PushPrimaryButton();
        auto bake = ImGui::Button("Bake");
        Theme::PopPrimaryButton();

        ImGui::SameLine();
        ImGui::BeginDisabled(settings.bakedState == 0);
        if (Widgets::DangerButton("Remove bake", ImVec2(0.0f, 0.0f))) {
            Baker::Clear(*material.Get());
            UpdateMeshesOfGraph();
            previewMesh->UpdatePipelines();
        }
        ImGui::EndDisabled();
        ImGui::SetItemTooltip("Brings back the properties from before the first bake. Changes made to them since then are lost");

        if (bake) {
            auto mesh = bakeMesh.IsValid() ? bakeMesh : previewMesh;
            auto result = Baker::Bake(material, mesh);
            bakeError = result.error;
            if (result.success) {
                UpdateMeshesOfGraph();
                previewMesh->UpdatePipelines();
                Notifications::Push({ .message = settings.mode == BakeMode::Average ? "Baked the averages of " + material.GetResource()->GetFileName() :
                    "Baked " + std::to_string(result.maps) + " maps of " + material.GetResource()->GetFileName() + ", save the material to keep them" });
            }
        }

        if (!bakeError.empty())
            Widgets::Callout(bakeError.c_str(), Theme::Error());

        if (settings.bakedState != 0 && settings.bakedState != Baker::GetState(*material.Get()))
            Widgets::Callout("The graph, its values or these settings changed since the last bake, ray tracing shows the old state", Theme::Warning());
        else if (settings.bakedState == 0)
            ImGui::TextDisabled("Not baked yet, ray tracing uses the properties as they are");
        else
            ImGui::TextColored(Theme::Success(), "Baked, ray tracing is up to date");

    }

    void MaterialEditorWindow::UpdateMeshesOfGraph() {

        for (const auto& mesh : ResourceManager<Mesh::Mesh>::GetResources()) {
            if (!mesh.IsLoaded())
                continue;

            auto usesGraph = false;
            for (const auto& meshMaterial : mesh->data.materials)
                usesGraph |= meshMaterial.IsLoaded() && meshMaterial->graph == material->graph;

            if (usesGraph)
                mesh->UpdatePipelines();
        }

    }

}
