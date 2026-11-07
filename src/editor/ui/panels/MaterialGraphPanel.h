#pragma once

#include "Panel.h"
#include "MaterialGraphPreviews.h"
#include "ResourceSelectionPanel.h"

#include "materialgraph/Compiler.h"
#include "materialgraph/MaterialGraph.h"
#include "materialgraph/NodeLibrary.h"
#include "resource/Resource.h"

#include <imgui.h>

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Atlas::Editor::UI {

    // Canvas to edit the nodes and links of a material graph. Generates the shaders again after each change
    class MaterialGraphPanel : public Panel {

    public:
        MaterialGraphPanel() : Panel("Graph") {}

        // Renders the canvas into the current window, returns whether the graph was compiled. The previews of
        // nodes show the values of the material, or the defaults of the graph without one
        bool Render(const ResourceHandle<MaterialGraph::Graph>& graph, const Material* material = nullptr);

        // Forgets everything about the last graph, needed when another graph is shown from now on
        void Reset();

        void FrameAll();

        void Arrange(MaterialGraph::Graph& graph);

        void OpenAddMenu();

        void ShowGeneratedCode();

        void Undo(const ResourceHandle<MaterialGraph::Graph>& graph);

        void Redo(const ResourceHandle<MaterialGraph::Graph>& graph);

        void SelectAll(const MaterialGraph::Graph& graph);

        void DeleteSelection(MaterialGraph::Graph& graph);

        void DuplicateSelection(MaterialGraph::Graph& graph);

        // A frame around the selected nodes
        void FrameSelection(MaterialGraph::Graph& graph);

        void CopySelection(const MaterialGraph::Graph& graph);

        void Paste(MaterialGraph::Graph& graph);

        void ToggleMaximized();

        // Whether nodes changed since the last save
        bool modified = false;

        // The canvas takes the whole editor, the window hosting the panel decides how
        bool maximized = false;

    private:
        struct PinRef {
            uint32_t node = 0;
            std::string pin;
            bool isOutput = false;

            bool IsValid() const { return node != 0; }
        };

        enum class RowKind {
            Output = 0,
            Input,
            Value,
            Text,
            Preview
        };

        struct Row {
            RowKind kind = RowKind::Output;
            const MaterialGraph::OutputDefinition* output = nullptr;
            const MaterialGraph::InputDefinition* input = nullptr;
            float height = 0.0f;
        };

        // Positions of a node on the screen, updated every frame
        struct NodeLayout {
            ImVec2 min;
            ImVec2 max;
            float headerHeight = 0.0f;
            std::vector<Row> rows;
            std::unordered_map<std::string, ImVec2> outputPins;
            std::unordered_map<std::string, ImVec2> inputPins;
        };

        enum class Interaction {
            None = 0,
            Panning,
            MovingNodes,
            BoxSelection,
            DraggingLink,
            ResizingFrame
        };

        enum class PreviewMode {
            None = 0,
            // Nodes where a picture says more than the values
            Key,
            All
        };

        ImVec2 ToScreen(vec2 position) const;

        vec2 ToCanvas(ImVec2 position) const;

        void UpdateLayouts(MaterialGraph::Graph& graph);

        void DrawGrid(ImDrawList* drawList, ImVec2 min, ImVec2 max);

        void DrawFrames(ImDrawList* drawList, MaterialGraph::Graph& graph);

        void DrawLinks(ImDrawList* drawList, MaterialGraph::Graph& graph);

        void DrawNodes(ImDrawList* drawList, MaterialGraph::Graph& graph, const MaterialGraph::CompiledGraph* compiled);

        bool DrawNodeWidgets(MaterialGraph::Graph& graph, MaterialGraph::Node& node, const NodeLayout& layout);

        bool DrawInputWidget(MaterialGraph::Node& node, const MaterialGraph::InputDefinition& input, ImVec2 position, float width);

        bool DrawValueWidget(MaterialGraph::Node& node, const MaterialGraph::NodeDefinition& definition, ImVec2 position, float width);

        void DrawToolbar(MaterialGraph::Graph& graph, const MaterialGraph::CompiledGraph* compiled);

        void DrawMinimap(ImDrawList* drawList, MaterialGraph::Graph& graph, ImVec2 canvasMin, ImVec2 canvasMax);

        void DrawCodeWindow(const MaterialGraph::CompiledGraph* compiled);

        // The properties of the selected node and the settings of the graph
        void DrawInspector(MaterialGraph::Graph& graph);

        bool HasPreview(const MaterialGraph::NodeDefinition& definition) const;

        void HandleInteractions(MaterialGraph::Graph& graph);

        void HandleShortcuts(const ResourceHandle<MaterialGraph::Graph>& graph);

        void RenderNodeMenu(MaterialGraph::Graph& graph);

        void RenderAddMenu(MaterialGraph::Graph& graph);

        PinRef FindPin(const MaterialGraph::Graph& graph, ImVec2 position) const;

        uint32_t FindNode(const MaterialGraph::Graph& graph, ImVec2 position) const;

        const MaterialGraph::Link* FindLink(const MaterialGraph::Graph& graph, ImVec2 position) const;

        void AddNode(MaterialGraph::Graph& graph, const MaterialGraph::NodeDefinition& definition);

        // The pin of a new node the dropped link connects to, null if it can't connect to this kind of node
        const std::string* FindPendingPartner(const MaterialGraph::Graph& graph, const MaterialGraph::NodeDefinition& definition) const;

        void InsertReroute(MaterialGraph::Graph& graph, MaterialGraph::Link link, vec2 position);

        std::vector<uint32_t> GetNodesInFrame(const MaterialGraph::Graph& graph, const MaterialGraph::Node& frame) const;

        void PushUndo(const MaterialGraph::Graph& graph);

        void RestoreState(MaterialGraph::Graph& graph, const std::string& state);

        std::string Serialize(const MaterialGraph::Graph& graph) const;

        void Frame(const MaterialGraph::Graph& graph);

        vec2 GetNodeSize(const MaterialGraph::Node& node) const;

        static ImVec4 GetValueColor(MaterialGraph::ValueType type);

        static const char* GetValueName(MaterialGraph::ValueType type);

        // The graph while it renders
        MaterialGraph::Graph* currentGraph = nullptr;

        vec2 pan = vec2(0.0f);
        float zoom = 1.0f;

        ImVec2 canvasOrigin = ImVec2(0.0f, 0.0f);
        ImVec2 canvasSize = ImVec2(0.0f, 0.0f);
        ImVec2 lastCanvasSize = ImVec2(0.0f, 0.0f);
        ImVec2 minimapMin = ImVec2(0.0f, 0.0f);
        ImVec2 minimapMax = ImVec2(0.0f, 0.0f);
        int32_t stableCanvasFrames = 0;

        std::unordered_map<uint32_t, NodeLayout> layouts;
        std::unordered_set<uint32_t> selection;

        Interaction interaction = Interaction::None;
        PinRef draggedPin;
        ImVec2 boxStart = ImVec2(0.0f, 0.0f);
        bool nodesMoved = false;

        // A link dropped on the canvas connects to the node added from the menu
        PinRef pendingPin;
        vec2 addPosition = vec2(0.0f);
        std::string addSearch;
        bool focusAddSearch = false;
        int32_t addRow = 0;
        std::unordered_set<std::string> openAddCategories;

        bool openAddMenu = false;
        bool frameRequested = true;
        bool frameOnlySelection = false;
        bool showCode = false;
        bool showInspector = true;

        PreviewMode previewMode = PreviewMode::Key;
        MaterialGraphPreviews previews;
        ResourceSelectionPanel<Texture::Texture2D> textureSelectionPanel;

        uint32_t resizedFrame = 0;
        // Nodes in moved frames move along with them
        std::unordered_set<uint32_t> movedWithFrames;

        // Generated code when a value changes, the mouse release decides for values baked into the shaders
        bool compileRequested = false;
        // The picker of a color is a popup, its item doesn't tell when the value is final
        bool colorEdited = false;

        std::vector<std::string> undoStack;
        std::vector<std::string> redoStack;

        static std::string clipboard;

        static constexpr float minZoom = 0.02f;
        static constexpr float nodeWidth = 230.0f;
        static constexpr float rowHeight = 24.0f;
        static constexpr float pinRadius = 6.0f;
        static constexpr float rerouteSize = 22.0f;
        static constexpr float previewHeight = 128.0f;
        static constexpr float frameHeaderHeight = 34.0f;
        static constexpr float inspectorWidth = 320.0f;
        // Below this zoom only the titles and previews of nodes are shown
        static constexpr float detailZoom = 0.45f;

    };

}
