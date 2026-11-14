#pragma once

#include "AssetEditorWindow.h"

#include "../panels/MaterialGraphPanel.h"
#include "../panels/ResourceSelectionPanel.h"

#include <ImguiExtension/panels/MaterialPanel.h>

#include <atomic>
#include <string>
#include <vector>

namespace Atlas::Editor::UI {

    // Edits a material and its graph on a preview mesh
    class MaterialEditorWindow : public AssetEditorWindow {

    public:
        explicit MaterialEditorWindow(ResourceHandle<Material> material, bool show);

        // Edits a graph with a material which only exists for the preview
        explicit MaterialEditorWindow(ResourceHandle<MaterialGraph::Graph> graph, bool show);

        ~MaterialEditorWindow() override;

        void Save() override;

        std::string GetAssetPath() const override;

        ResourceHandle<Material> material;

        MaterialGraphPanel graphPanel;

    protected:
        bool IsReady() const override;

        Scene::Entity CreatePreviewEntity() override;

        Volume::AABB GetBounds() const override;

        void RenderProperties() override;

        void RenderPanels() override;

        bool HasBottomPanel() const override { return true; }

        float GetBottomPanelSize() const override { return 0.6f; }

        void DockPanels(ImGuiID side, ImGuiID bottom) override;

        bool IsPanelFocused() const override;

        bool AllowsViewportShortcuts() const override;

        void RenderViewportSettings() override;

        json SaveState() const override;

        void RestoreState(const json& j) override;

    private:
        void Init();

        void LoadPreviewMesh();

        void CreateGraph();

        void AttachGraph(const ResourceHandle<MaterialGraph::Graph>& graph);

        void RenderGraphSection();

        void RenderParameters(const MaterialGraph::CompiledGraph& compiled);

        void RenderParameter(const MaterialGraph::Parameter& parameter);

        void RenderBakeSection(const MaterialGraph::CompiledGraph& compiled);

        void RenderGraphCanvas();

        void RenderSharedGraphWarning();

        // Finds the other materials with the graph, also the ones which aren't loaded
        void FindOtherUsers();

        void MakeGraphUnique();

        struct GraphUsers {
            // Only valid when finished
            std::vector<std::string> materials;
            std::atomic_bool finished = false;
        };

        // Other meshes with this graph get the new pipelines as well
        void UpdateMeshesOfGraph();

        // Own copy of a mesh, the material replaces the one of the file
        ResourceHandle<Mesh::Mesh> previewMesh;
        int32_t previewMeshIdx = 0;

        // Editing a graph without a material of its own, which isn't saved then
        ResourceHandle<MaterialGraph::Graph> editedGraph;

        Panel graphPanelWindow = Panel("Material graph");

        Ref<GraphUsers> graphUsers;
        JobGroup graphUsersJob { "Material graph users", JobPriority::Low };

        ResourceHandle<Mesh::Mesh> bakeMesh;
        ResourceSelectionPanel<Mesh::Mesh> meshSelectionPanel;
        std::string bakeError;

        std::string parameterSearch;
        // Short lists stay open and don't need a search
        static constexpr size_t maxParametersWithoutSearch = 12;

        ImguiExtension::MaterialPanel materialPanel;
        ResourceSelectionPanel<Texture::Texture2D> textureSelectionPanel;
        ResourceSelectionPanel<MaterialGraph::Graph> graphSelectionPanel;

    };

}
