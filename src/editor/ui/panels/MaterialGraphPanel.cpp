#include "MaterialGraphPanel.h"

#include "../../Theme.h"
#include "../../Notifications.h"
#include "../../Singletons.h"
#include "../Widgets.h"

#include "materialgraph/MaterialGraphSerializer.h"

#include <imgui_stdlib.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cctype>
#include <map>

namespace Atlas::Editor::UI {

    using namespace MaterialGraph;

    std::string MaterialGraphPanel::clipboard;

    bool MaterialGraphPanel::Render(const ResourceHandle<Graph>& graphHandle, const Material* material) {

        if (!graphHandle.IsLoaded())
            return false;

        auto& graph = *graphHandle.Get();
        auto compiled = graph.GetCompiled();
        currentGraph = &graph;

        ImGui::PushID(GetNameID());

        DrawToolbar(graph, compiled.get());

        auto inspectorSize = showInspector ? std::min(inspectorWidth, ImGui::GetContentRegionAvail().x * 0.4f) : 0.0f;

        canvasOrigin = ImGui::GetCursorScreenPos();
        canvasSize = ImGui::GetContentRegionAvail();
        // A narrow strip between canvas and inspector holds the arrow that shows and hides the inspector
        const float toggleGap = 2.0f;
        const float togglePadding = 3.0f;
        auto toggleIconSize = std::floor(ImGui::GetTextLineHeight());
        auto toggleWidth = toggleIconSize + 2.0f * togglePadding;
        canvasSize.x -= inspectorSize > 0.0f ? inspectorSize + toggleGap : 0.0f;
        canvasSize.x -= toggleWidth + toggleGap;
        if (canvasSize.x < 32.0f || canvasSize.y < 32.0f) {
            ImGui::PopID();
            return false;
        }

        ImGui::BeginChild("Canvas", canvasSize, ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground);

        UpdateLayouts(graph);

        // New windows start tiny and the docking takes a few frames, framing during that would miss the final size
        auto isCanvasStable = canvasSize.x == lastCanvasSize.x && canvasSize.y == lastCanvasSize.y;
        stableCanvasFrames = isCanvasStable ? stableCanvasFrames + 1 : 0;
        lastCanvasSize = canvasSize;
        if (frameRequested && stableCanvasFrames >= 3) {
            Frame(graph);
            frameRequested = false;
            UpdateLayouts(graph);
        }

        auto drawList = ImGui::GetWindowDrawList();
        auto canvasMax = ImVec2(canvasOrigin.x + canvasSize.x, canvasOrigin.y + canvasSize.y);

        drawList->PushClipRect(canvasOrigin, canvasMax, true);

        DrawGrid(drawList, canvasOrigin, canvasMax);
        DrawFrames(drawList, graph);
        DrawLinks(drawList, graph);
        DrawNodes(drawList, graph, compiled.get());

        HandleInteractions(graph);

        // The selection box and the dragged link are on top of everything
        if (interaction == Interaction::BoxSelection) {
            auto mouse = ImGui::GetMousePos();
            drawList->AddRectFilled(boxStart, mouse, ImGui::ColorConvertFloat4ToU32(Theme::Accent(0.15f)));
            drawList->AddRect(boxStart, mouse, ImGui::ColorConvertFloat4ToU32(Theme::Accent(0.8f)));
        }

        DrawMinimap(drawList, graph, canvasOrigin, canvasMax);

        drawList->PopClipRect();

        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::GetIO().WantTextInput)
            HandleShortcuts(graphHandle);

        RenderNodeMenu(graph);
        RenderAddMenu(graph);

        ImGui::EndChild();

        ImGui::SameLine(0.0f, toggleGap);
        ImGui::BeginChild("Inspector toggle", ImVec2(toggleWidth, canvasSize.y), ImGuiChildFlags_None,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground);
        {
            auto buttonHeight = toggleIconSize + 2.0f * togglePadding;
            ImGui::SetCursorPos(ImVec2(0.0f, std::max(0.0f, (canvasSize.y - buttonHeight) * 0.5f)));

            // The arrow points where the inspector goes
            auto& arrow = Singletons::icons->Get(showInspector ? IconType::ArrowRight : IconType::ArrowLeft);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(togglePadding, togglePadding));
            if (ImGui::ImageButton("Toggle inspector", Singletons::imguiWrapper->GetTextureId(&arrow),
                ImVec2(toggleIconSize, toggleIconSize), ImVec2(0.1f, 0.1f), ImVec2(0.9f, 0.9f)))
                showInspector = !showInspector;
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor();
            ImGui::SetItemTooltip(showInspector ? "Hide the inspector" : "Show the inspector");
        }
        ImGui::EndChild();

        if (inspectorSize > 0.0f) {
            ImGui::SameLine(0.0f, toggleGap);
            ImGui::BeginChild("Inspector", ImVec2(inspectorSize, canvasSize.y), ImGuiChildFlags_Borders);
            DrawInspector(graph);
            ImGui::EndChild();
        }

        DrawCodeWindow(compiled.get());

        ImGui::PopID();

        previews.Render(graph, material);

        if (colorEdited && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            colorEdited = false;
            compileRequested = true;
        }

        if (compileRequested) {
            graph.Compile();
            previews.Invalidate();
            compileRequested = false;
            modified = true;
            return true;
        }

        return false;

    }

    namespace {

        // A rounded label in the color of what it stands for. Pills follow each other and wrap at the end of the line
        void Pill(const char* text, ImVec4 color, const char* tooltip, bool& first) {

            const ImVec2 padding(8.0f, 3.0f);
            auto textSize = ImGui::CalcTextSize(text);
            auto size = ImVec2(textSize.x + 2.0f * padding.x, textSize.y + 2.0f * padding.y);

            if (!first) {
                ImGui::SameLine(0.0f, 6.0f);
                if (ImGui::GetCursorPosX() + size.x > ImGui::GetContentRegionMax().x)
                    ImGui::NewLine();
            }
            first = false;

            auto position = ImGui::GetCursorScreenPos();
            ImGui::PushID(text);
            ImGui::InvisibleButton("##Pill", size);
            ImGui::PopID();
            ImGui::SetItemTooltip("%s", tooltip);

            auto drawList = ImGui::GetWindowDrawList();
            auto max = ImVec2(position.x + size.x, position.y + size.y);
            auto rounding = size.y * 0.5f;
            auto fill = color;
            fill.w *= ImGui::IsItemHovered() ? 0.3f : 0.18f;
            auto border = color;
            border.w *= 0.6f;
            drawList->AddRectFilled(position, max, ImGui::GetColorU32(fill), rounding);
            drawList->AddRect(position, max, ImGui::GetColorU32(border), rounding);
            drawList->AddText(ImVec2(position.x + padding.x, position.y + padding.y), ImGui::GetColorU32(color), text);

        }

        // Transparent icon button with the frame padding of the row, so it is as high as the text next to it
        bool InspectorIconButton(const char* id, IconType iconType, const char* tooltip) {

            auto& icon = Singletons::icons->Get(iconType);
            auto size = ImGui::GetTextLineHeight();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.15f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 1.0f, 1.0f, 0.25f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
            auto pressed = ImGui::ImageButton(id, Singletons::imguiWrapper->GetTextureId(&icon), ImVec2(size, size),
                ImVec2(0.1f, 0.1f), ImVec2(0.9f, 0.9f));
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);
            ImGui::SetItemTooltip("%s", tooltip);
            return pressed;

        }

        // The default of a parameter or the value of a constant in short, for the list of parameters
        std::string FormatValue(const Node& node, const NodeDefinition& definition) {

            char buffer[96];
            switch (definition.valueType) {
            case ValueType::Float: snprintf(buffer, sizeof(buffer), "%.3g", node.value.x); break;
            case ValueType::Vec2: snprintf(buffer, sizeof(buffer), "%.3g, %.3g", node.value.x, node.value.y); break;
            case ValueType::Vec3: snprintf(buffer, sizeof(buffer), "%.3g, %.3g, %.3g", node.value.x, node.value.y, node.value.z); break;
            case ValueType::Vec4: snprintf(buffer, sizeof(buffer), "%.2g, %.2g, %.2g, %.2g", node.value.x, node.value.y,
                node.value.z, node.value.w); break;
            case ValueType::Texture:
                return node.texture.IsValid() ? node.texture.GetResource()->GetFileName() : "No texture";
            default: buffer[0] = 0; break;
            }
            return buffer;

        }

    }

    void MaterialGraphPanel::DrawInspector(Graph& graph) {

        Node* selected = nullptr;
        if (selection.size() == 1)
            selected = graph.GetNode(*selection.begin());

        if (!selected) {
            DrawGraphOverview(graph);
            return;
        }

        auto definition = NodeLibrary::Find(selected->type);
        if (!definition)
            return;

        // Header in the color of the node on the canvas: back to the overview, the name and the type of the node
        auto isNamed = (definition->kind == NodeKind::Parameter || definition->kind == NodeKind::Frame) && !selected->name.empty();
        auto title = isNamed ? selected->name : selected->type;
        auto subtitle = isNamed ? selected->type : definition->category;
        {
            auto drawList = ImGui::GetWindowDrawList();
            const float inset = 4.0f;
            auto start = ImGui::GetCursorScreenPos();
            auto width = ImGui::GetContentRegionAvail().x;
            auto height = ImGui::GetFrameHeight() + 2.0f * inset;
            drawList->AddRectFilled(start, ImVec2(start.x + width, start.y + height),
                ImGui::GetColorU32(Theme::GraphNodeHeader(definition->category.c_str())), 6.0f);

            ImGui::SetCursorScreenPos(ImVec2(start.x + inset, start.y + inset));
            if (InspectorIconButton("Back to overview", IconType::ArrowLeft, "Back to the overview of the graph")) {
                selection.clear();
                return;
            }
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(title.c_str());
            auto subtitleWidth = ImGui::CalcTextSize(subtitle.c_str()).x;
            auto subtitleX = start.x + width - inset - ImGui::GetStyle().FramePadding.x - subtitleWidth;
            if (subtitleX > ImGui::GetItemRectMax().x + 8.0f) {
                ImGui::SameLine();
                ImGui::SetCursorScreenPos(ImVec2(subtitleX, ImGui::GetCursorScreenPos().y));
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.65f), "%s", subtitle.c_str());
            }
            ImGui::SetCursorScreenPos(ImVec2(start.x, start.y + height + ImGui::GetStyle().ItemSpacing.y));
        }

        ImGui::PushTextWrapPos();
        ImGui::TextColored(Theme::TextMuted(), "%s", definition->tooltip.c_str());
        ImGui::PopTextWrapPos();
        ImGui::Spacing();

        if (definition->kind == NodeKind::Comment) {
            auto edited = false;
            Widgets::TextArea("Comment text", selected->name, 14.0f, 0, &edited);
            if (edited)
                modified = true;
            return;
        }

        if (!Widgets::BeginProperties("Node properties"))
            return;

        if (definition->kind == NodeKind::Frame) {
            Widgets::PropertyLabel("Title");
            ImGui::InputText("##Title", &selected->name);
            if (ImGui::IsItemDeactivatedAfterEdit())
                modified = true;
        }

        auto compiled = definition->kind != NodeKind::Frame ? graph.GetCompiled() : nullptr;
        if (compiled) {
            auto iter = compiled->nodeStages.find(selected->id);
            auto stages = iter != compiled->nodeStages.end() ? iter->second : 0u;
            Widgets::PropertyLabel("Stages", "The shaders this node ends up in");
            if (!stages) {
                ImGui::AlignTextToFramePadding();
                ImGui::TextColored(Theme::TextMuted(), "Not used");
                ImGui::SetItemTooltip("The node doesn't lead to an output, it costs nothing");
            }
            auto first = true;
            if (stages & VertexStage)
                Pill("Vertex", Theme::Success(), "Evaluated for every vertex", first);
            if (stages & SurfaceStage)
                Pill("Fragment", Theme::Accent(), "Evaluated for every fragment", first);
        }

        if (definition->kind == NodeKind::Parameter) {
            Widgets::PropertyLabel("Name", "Materials show the parameter with this name");
            ImGui::InputText("##Name", &selected->name);
            if (ImGui::IsItemDeactivatedAfterEdit())
                compileRequested = true;
        }

        // Defaults of parameters aren't part of the code and apply while dragging, constants compile once the value is final
        auto hasValue = (definition->kind == NodeKind::Parameter || definition->kind == NodeKind::Constant) &&
            definition->valueType != ValueType::Texture;
        if (hasValue) {
            auto isParameter = definition->kind == NodeKind::Parameter;
            Widgets::PropertyLabel(isParameter ? "Default" : "Value", isParameter ? "Materials use this value until they set their own" :
                "Baked into the shaders, changing it compiles them again");
            int32_t components = definition->valueType == ValueType::Vec2 ? 2 : definition->valueType == ValueType::Vec3 ? 3 :
                definition->valueType == ValueType::Vec4 ? 4 : 1;
            auto changed = false;
            if (definition->isColor)
                changed = ImGui::ColorEdit3("##Value", &selected->value.x, ImGuiColorEditFlags_Float);
            else if (selected->range.x < selected->range.y)
                changed = ImGui::DragScalarN("##Value", ImGuiDataType_Float, &selected->value.x, components, 0.01f,
                    &selected->range.x, &selected->range.y, "%.3g", ImGuiSliderFlags_AlwaysClamp);
            else
                changed = ImGui::DragScalarN("##Value", ImGuiDataType_Float, &selected->value.x, components, 0.01f,
                    nullptr, nullptr, "%.3g");
            if (isParameter ? changed : ImGui::IsItemDeactivatedAfterEdit())
                compileRequested = true;
            else
                colorEdited |= definition->isColor && changed;
        }

        if (definition->kind == NodeKind::StaticSwitch) {
            auto isOn = selected->value.x > 0.0f;
            Widgets::PropertyLabel("Selected", "Only the selected input is compiled. Changing it applies to all materials with the graph");
            if (ImGui::Checkbox("On##Switch", &isOn)) {
                selected->value.x = isOn ? 1.0f : 0.0f;
                compileRequested = true;
            }
        }

        if (definition->kind == NodeKind::Parameter) {
            if (definition->valueType != ValueType::Texture && !definition->isColor) {
                Widgets::PropertyLabel("Range", "Materials get a slider within these limits. Without a range the values are free");
                ImGui::DragFloatRange2("##Range", &selected->range.x, &selected->range.y, 0.01f, 0.0f, 0.0f, "Min %.3g", "Max %.3g");
                if (ImGui::IsItemDeactivatedAfterEdit())
                    compileRequested = true;
            }

            Widgets::PropertyLabel("Group", "Materials show parameters of the same group below a heading");
            ImGui::InputTextWithHint("##Group", "No group", &selected->group);
            if (ImGui::IsItemDeactivatedAfterEdit())
                compileRequested = true;

            Widgets::PropertyLabel("Order", "Materials show smaller numbers first");
            ImGui::DragInt("##Order", &selected->order, 0.1f);
            if (ImGui::IsItemDeactivatedAfterEdit())
                compileRequested = true;
        }

        if (definition->kind == NodeKind::Parameter && definition->valueType == ValueType::Texture) {
            Widgets::PropertyLabel("Default", "Materials use this texture until they select their own");
            auto textureChanged = false;
            auto texture = textureSelectionPanel.Render(selected->texture, textureChanged);
            textureSelectionPanel.Reset();
            if (textureChanged) {
                selected->texture = texture;
                compileRequested = true;
            }

            const char* fallbacks[] = { "White", "Black", "Flat normal" };
            auto fallback = int32_t(selected->value.y);
            Widgets::PropertyLabel("Fallback", "What the shaders read while neither the material nor the default has a texture");
            if (ImGui::Combo("##Fallback", &fallback, fallbacks, IM_ARRAYSIZE(fallbacks))) {
                selected->value.y = float(fallback);
                compileRequested = true;
            }

            Widgets::PropertyLabel("Coordinate scale", "How much the graph scales the texture coordinates. The texture streaming "
                "loads more detail for textures which repeat more often");
            ImGui::DragFloat("##Coordinate scale", &selected->value.x, 0.1f, 0.01f, 1000.0f);
            if (ImGui::IsItemDeactivatedAfterEdit())
                compileRequested = true;
        }

        if (definition->kind == NodeKind::VertexOutput) {
            auto derive = selected->value.x > 0.0f;
            Widgets::PropertyLabel("Derive normals", "Evaluates the displacement next to each vertex to find the normal of the moved "
                "surface. This works for displacement from positions, not from texture coordinates");
            if (ImGui::Checkbox("##Derive normals", &derive)) {
                selected->value.x = derive ? 1.0f : 0.0f;
                compileRequested = true;
            }

            Widgets::PropertyLabel("Sampling distance", "How far next to each vertex the displacement is evaluated for the normals");
            ImGui::BeginDisabled(!derive);
            ImGui::DragFloat("##Sampling distance", &selected->value.y, 0.001f, 0.001f, 10.0f);
            if (ImGui::IsItemDeactivatedAfterEdit())
                compileRequested = true;
            ImGui::EndDisabled();
        }

        Widgets::EndProperties();

    }

    void MaterialGraphPanel::DrawGraphOverview(Graph& graph) {

        auto compiled = graph.GetCompiled();

        if (selection.size() > 1) {
            ImGui::TextColored(Theme::TextMuted(), "%d nodes selected", int32_t(selection.size()));
            ImGui::Spacing();
        }

        // A click selects the node and moves it into the middle of the canvas, the zoom stays
        auto showNode = [&](uint32_t nodeId) {
            auto node = graph.GetNode(nodeId);
            if (!node)
                return;
            selection.clear();
            selection.insert(nodeId);
            auto center = node->position + GetNodeSize(*node) * 0.5f;
            pan = vec2(canvasSize.x, canvasSize.y) * 0.5f / zoom - center;
            };

        // What the compiled graph does, the toolbar only names the kind of graph
        if (Widgets::SectionHeader("Output", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::PushTextWrapPos();
            if (!compiled)
                ImGui::TextColored(Theme::TextMuted(), "Not compiled yet");
            else if (!compiled->valid)
                ImGui::TextColored(Theme::Error(), "The graph has errors, materials use their regular properties");
            else if (!compiled->hasVertexOffset && !compiled->hasSurface)
                ImGui::TextColored(Theme::TextMuted(), "No output is connected, materials use their regular properties");
            ImGui::PopTextWrapPos();

            // Only what the graph does, aspects it doesn't change aren't shown
            if (compiled && compiled->valid && (compiled->hasVertexOffset || compiled->hasSurface)) {
                auto first = true;
                if (compiled->hasVertexOffset)
                    Pill("Moves vertices", Theme::Success(), "The vertex output moves the vertices of the mesh. "
                        "Set the largest displacement, culling needs it", first);
                if (compiled->hasSurface)
                    Pill("Surface", Theme::Accent(), "The surface output describes the surface. Ray tracing uses the regular "
                        "properties or a bake of the graph", first);
                if (compiled->hasUniformOpacity)
                    Pill("Cutout per draw", Theme::Accent(), "Opacity only depends on parameters and constants: it is evaluated "
                        "per vertex and hides the whole draw, fragments stay as fast as opaque ones", first);
                else if (compiled->hasOpacity)
                    Pill("Cutout per fragment", Theme::Warning(), "Opacity varies over the surface: fragments are cut out, which "
                        "turns off early depth tests and costs time even where nothing is cut out", first);
                if (compiled->UsesShadowPipeline())
                    Pill("Own shadows", Theme::Accent(), "Moved vertices or cut out fragments need their own shadow shaders, "
                        "otherwise shadows use the regular pipeline", first);
            }

            // Only graphs moving vertices need it, a value from before stays editable to remove it
            auto showDisplacement = (compiled && compiled->hasVertexOffset) || graph.maxDisplacement > 0.0f;
            if (showDisplacement && Widgets::BeginProperties("Output")) {
                Widgets::PropertyLabel("Displacement", "Largest distance a vertex moves in the space of the mesh. Culling adds this to "
                    "the bounds, otherwise moved vertices disappear at the edges of the screen");
                ImGui::DragFloat("##Largest displacement", &graph.maxDisplacement, 0.01f, 0.0f, 1000.0f, "%.3f");
                if (ImGui::IsItemDeactivatedAfterEdit())
                    compileRequested = true;
                Widgets::EndProperties();
            }
            ImGui::Spacing();
        }

        if (compiled && !compiled->errors.empty()) {
            auto errorsTitle = "Errors (" + std::to_string(compiled->errors.size()) + ")###Errors";
            if (Widgets::SectionHeader(errorsTitle.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::PushTextWrapPos();
                for (const auto& error : compiled->errors) {
                    ImGui::TextColored(Theme::Error(), "%s", error.message.c_str());
                    // Errors of the whole graph have no node to show
                    if (error.nodeId == 0 || !graph.GetNode(error.nodeId))
                        continue;
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                        ImGui::SetTooltip("Show the node");
                    }
                    if (ImGui::IsItemClicked())
                        showNode(error.nodeId);
                }
                ImGui::PopTextWrapPos();
                ImGui::Spacing();
            }
        }

        // Parameters in the order materials show them: ungrouped first like in the material, then by group and order
        std::vector<const Node*> parameters;
        for (const auto& node : graph.nodes) {
            auto definition = NodeLibrary::Find(node.type);
            if (definition && definition->kind == NodeKind::Parameter)
                parameters.push_back(&node);
        }
        std::stable_sort(parameters.begin(), parameters.end(), [](const Node* a, const Node* b) {
            if (a->group != b->group)
                return a->group < b->group;
            return a->order < b->order;
            });

        auto parametersTitle = "Parameters (" + std::to_string(parameters.size()) + ")###Parameters";
        if (Widgets::SectionHeader(parametersTitle.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            if (parameters.empty()) {
                ImGui::PushTextWrapPos();
                ImGui::TextColored(Theme::TextMuted(), "No parameters, materials can't change anything without compiling");
                ImGui::PopTextWrapPos();
            }

            auto drawList = ImGui::GetWindowDrawList();
            auto rowHeight = ImGui::GetFrameHeight();
            for (size_t start = 0; start < parameters.size();) {
                const auto& group = parameters[start]->group;
                auto end = start;
                while (end < parameters.size() && parameters[end]->group == group)
                    end++;

                auto isOpen = true;
                if (!group.empty()) {
                    auto label = group + " (" + std::to_string(end - start) + ")###Group" + group;
                    isOpen = ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_NoTreePushOnOpen |
                        ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen);
                }

                // Rows of a group start below its title, tighter than the default spacing
                auto indent = group.empty() ? 0.0f : ImGui::GetTreeNodeToLabelSpacing() - 12.0f;
                if (isOpen && indent > 0.0f)
                    ImGui::Indent(indent);
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 1.0f));
                for (auto i = start; isOpen && i < end; i++) {
                    auto parameter = parameters[i];
                    auto definition = NodeLibrary::Find(parameter->type);
                    auto isUsed = !compiled || compiled->nodeStages.contains(parameter->id);
                    auto alpha = isUsed ? 1.0f : 0.45f;

                    ImGui::PushID(int32_t(parameter->id));
                    auto position = ImGui::GetCursorScreenPos();
                    auto width = ImGui::GetContentRegionAvail().x;
                    if (ImGui::Selectable("##Parameter", false, ImGuiSelectableFlags_SpanAvailWidth, ImVec2(0.0f, rowHeight)))
                        showNode(parameter->id);
                    ImGui::SetItemTooltip(isUsed ? "Show the node" : "Not connected to an output, materials don't show it");
                    ImGui::PopID();

                    // Type as a dot in the color of its pins, the name and the default on the right
                    auto centerY = position.y + rowHeight * 0.5f;
                    auto textY = centerY - ImGui::GetTextLineHeight() * 0.5f;
                    auto typeColor = GetValueColor(definition->valueType);
                    typeColor.w *= alpha;
                    drawList->AddCircleFilled(ImVec2(position.x + 10.0f, centerY), 4.0f, ImGui::GetColorU32(typeColor));
                    auto name = parameter->name.empty() ? std::string("Unnamed") : parameter->name;
                    auto textColor = ImGui::GetStyleColorVec4(ImGuiCol_Text);
                    textColor.w *= alpha;
                    drawList->AddText(ImVec2(position.x + 22.0f, textY), ImGui::GetColorU32(textColor), name.c_str());

                    auto right = position.x + width - 6.0f;
                    if (definition->isColor) {
                        auto swatch = rowHeight - 10.0f;
                        auto color = ImVec4(parameter->value.x, parameter->value.y, parameter->value.z, alpha);
                        auto min = ImVec2(right - swatch * 1.6f, centerY - swatch * 0.5f);
                        auto max = ImVec2(right, centerY + swatch * 0.5f);
                        drawList->AddRectFilled(min, max, ImGui::GetColorU32(color), 3.0f);
                        drawList->AddRect(min, max, ImGui::GetColorU32(Theme::TextMuted(0.4f * alpha)), 3.0f);
                    }
                    else {
                        auto value = FormatValue(*parameter, *definition);
                        auto valueWidth = ImGui::CalcTextSize(value.c_str()).x;
                        drawList->AddText(ImVec2(right - valueWidth, textY), ImGui::GetColorU32(Theme::TextMuted(alpha)), value.c_str());
                    }
                }

                ImGui::PopStyleVar();
                if (isOpen && indent > 0.0f)
                    ImGui::Unindent(indent);

                start = end;
            }
        }

        ImGui::Spacing();
        ImGui::TextColored(Theme::TextMuted(0.7f), "Select a node to see its properties");

    }

    bool MaterialGraphPanel::HasPreview(const NodeDefinition& definition) const {

        if (previewMode == PreviewMode::None || definition.kind == NodeKind::Comment || definition.kind == NodeKind::Frame ||
            definition.kind == NodeKind::Reroute)
            return false;

        if (previewMode == PreviewMode::All)
            return true;

        if (definition.kind == NodeKind::Parameter)
            return definition.valueType == ValueType::Texture;

        return definition.kind == NodeKind::SurfaceOutput || definition.category == "Texture" || definition.category == "Procedural" ||
            definition.category == "Normal" || definition.category == "Coordinates" || definition.category == "Logic";

    }

    void MaterialGraphPanel::Reset() {

        selection.clear();
        layouts.clear();
        movedWithFrames.clear();

        interaction = Interaction::None;
        draggedPin = PinRef();
        pendingPin = PinRef();
        resizedFrame = 0;
        nodesMoved = false;

        compileRequested = false;
        colorEdited = false;
        modified = false;

        previews.Reset();

    }

    void MaterialGraphPanel::FrameAll() {

        frameRequested = true;

    }

    void MaterialGraphPanel::OpenAddMenu() {

        openAddMenu = true;
        pendingPin = PinRef();
        addPosition = ToCanvas(ImVec2(canvasOrigin.x + canvasSize.x * 0.5f, canvasOrigin.y + canvasSize.y * 0.5f));

    }

    void MaterialGraphPanel::ShowGeneratedCode() {

        showCode = true;

    }

    void MaterialGraphPanel::OnGraphRestored(const Graph& graph) {

        std::erase_if(selection, [&](uint32_t id) { return graph.GetNode(id) == nullptr; });
        compileRequested = true;
        modified = true;

    }

    ImVec2 MaterialGraphPanel::ToScreen(vec2 position) const {

        auto screen = (position + pan) * zoom;
        return ImVec2(canvasOrigin.x + screen.x, canvasOrigin.y + screen.y);

    }

    vec2 MaterialGraphPanel::ToCanvas(ImVec2 position) const {

        return vec2(position.x - canvasOrigin.x, position.y - canvasOrigin.y) / zoom - pan;

    }

    void MaterialGraphPanel::UpdateLayouts(Graph& graph) {

        std::erase_if(layouts, [&](const auto& item) { return graph.GetNode(item.first) == nullptr; });

        for (const auto& node : graph.nodes) {
            auto definition = NodeLibrary::Find(node.type);
            auto& layout = layouts[node.id];

            layout.rows.clear();
            layout.outputPins.clear();
            layout.inputPins.clear();

            if (definition && definition->kind == NodeKind::Frame) {
                layout.min = ToScreen(node.position);
                layout.max = ImVec2(layout.min.x + node.size.x * zoom, layout.min.y + node.size.y * zoom);
                layout.headerHeight = frameHeaderHeight * zoom;
                continue;
            }

            if (definition && definition->kind == NodeKind::Reroute) {
                layout.min = ToScreen(node.position);
                layout.max = ImVec2(layout.min.x + rerouteSize * zoom, layout.min.y + rerouteSize * zoom);
                layout.headerHeight = 0.0f;
                auto centerY = 0.5f * (layout.min.y + layout.max.y);
                layout.outputPins[definition->outputs.front().name + std::string()] = ImVec2(layout.max.x, centerY);
                layout.inputPins[definition->inputs.front().name] = ImVec2(layout.min.x, centerY);
                continue;
            }

            auto scaledRow = rowHeight * zoom;
            layout.headerHeight = scaledRow * (definition && definition->kind == NodeKind::Parameter ? 1.75f : 1.15f);

            if (definition) {
                if (definition->kind == NodeKind::Comment)
                    layout.rows.push_back({ .kind = RowKind::Text, .height = scaledRow * 3.0f });
                if (definition->kind == NodeKind::Constant || definition->kind == NodeKind::Parameter) {
                    if (definition->valueType != ValueType::Texture)
                        layout.rows.push_back({ .kind = RowKind::Value, .height = scaledRow });
                }
                if (definition->kind == NodeKind::StaticSwitch)
                    layout.rows.push_back({ .kind = RowKind::Value, .height = scaledRow });
                for (const auto& output : definition->outputs)
                    layout.rows.push_back({ .kind = RowKind::Output, .output = &output, .height = scaledRow });
                for (const auto& input : definition->inputs) {
                    auto hasWideWidget = (input.type == ValueType::Vec2 || input.type == ValueType::Vec3 || input.type == ValueType::Vec4) &&
                        input.defaultExpression.empty() && !graph.GetInputLink(node.id, input.name);
                    layout.rows.push_back({ .kind = RowKind::Input, .input = &input, .height = scaledRow * (hasWideWidget ? 2.0f : 1.0f) });
                }
                if (HasPreview(*definition))
                    layout.rows.push_back({ .kind = RowKind::Preview, .height = previewHeight * zoom });
            }

            auto height = layout.headerHeight + 8.0f * zoom;
            for (const auto& row : layout.rows)
                height += row.height;

            layout.min = ToScreen(node.position);
            layout.max = ImVec2(layout.min.x + nodeWidth * zoom, layout.min.y + height);

            auto y = layout.min.y + layout.headerHeight + 4.0f * zoom;
            for (const auto& row : layout.rows) {
                auto center = y + std::min(row.height, scaledRow) * 0.5f;
                if (row.kind == RowKind::Output)
                    layout.outputPins[row.output->name] = ImVec2(layout.max.x, center);
                else if (row.kind == RowKind::Input)
                    layout.inputPins[row.input->name] = ImVec2(layout.min.x, center);
                y += row.height;
            }
        }

    }

    void MaterialGraphPanel::DrawGrid(ImDrawList* drawList, ImVec2 min, ImVec2 max) {

        drawList->AddRectFilled(min, max, ImGui::ColorConvertFloat4ToU32(Theme::GraphBackground()));

        // Coarser lines when zoomed out, otherwise they fill the whole canvas
        auto baseStep = 32.0f;
        while (baseStep * zoom < 16.0f)
            baseStep *= 4.0f;
        auto step = baseStep * zoom;
        auto minor = ImGui::ColorConvertFloat4ToU32(Theme::GraphGrid(0.5f));
        auto major = ImGui::ColorConvertFloat4ToU32(Theme::GraphGrid(1.0f));

        auto start = ToScreen(vec2(0.0f));
        auto firstX = std::fmod(start.x - min.x, step);
        auto firstY = std::fmod(start.y - min.y, step);

        for (float x = firstX; x < max.x - min.x; x += step) {
            auto index = int32_t(std::round((min.x + x - start.x) / step));
            drawList->AddLine(ImVec2(min.x + x, min.y), ImVec2(min.x + x, max.y), index % 4 == 0 ? major : minor);
        }
        for (float y = firstY; y < max.y - min.y; y += step) {
            auto index = int32_t(std::round((min.y + y - start.y) / step));
            drawList->AddLine(ImVec2(min.x, min.y + y), ImVec2(max.x, min.y + y), index % 4 == 0 ? major : minor);
        }

    }

    static void DrawCurve(ImDrawList* drawList, ImVec2 from, ImVec2 to, ImU32 color, float thickness, float zoom) {

        auto distance = std::max(std::abs(to.x - from.x) * 0.5f, 40.0f * zoom);
        drawList->AddBezierCubic(from, ImVec2(from.x + distance, from.y), ImVec2(to.x - distance, to.y), to, color, thickness);

    }

    void MaterialGraphPanel::DrawFrames(ImDrawList* drawList, Graph& graph) {

        auto font = ImGui::GetFont();

        for (const auto& node : graph.nodes) {
            auto definition = NodeLibrary::Find(node.type);
            if (!definition || definition->kind != NodeKind::Frame)
                continue;

            const auto& layout = layouts[node.id];
            auto rounding = 8.0f * zoom;
            auto isSelected = selection.contains(node.id);

            drawList->AddRectFilled(layout.min, layout.max, ImGui::ColorConvertFloat4ToU32(Theme::Accent(0.06f)), rounding);
            drawList->AddRectFilled(layout.min, ImVec2(layout.max.x, layout.min.y + layout.headerHeight),
                ImGui::ColorConvertFloat4ToU32(Theme::Accent(0.18f)), rounding, ImDrawFlags_RoundCornersTop);
            drawList->AddRect(layout.min, layout.max, ImGui::ColorConvertFloat4ToU32(isSelected ? Theme::Accent() : Theme::Accent(0.35f)), rounding, 0,
                isSelected ? 2.0f : 1.0f);

            // Titles stay readable when zoomed out, they are what is left to read then
            auto fontSize = std::max(ImGui::GetFontSize() * 1.4f * zoom, 15.0f);
            auto clip = ImVec4(layout.min.x, layout.min.y, layout.max.x, layout.max.y);
            drawList->AddText(font, fontSize, ImVec2(layout.min.x + 8.0f * zoom, layout.min.y + 4.0f * zoom),
                ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(ImGuiCol_Text)), node.name.empty() ? "Frame" : node.name.c_str(), nullptr, 0.0f, &clip);

            auto corner = 12.0f * zoom;
            drawList->AddTriangleFilled(ImVec2(layout.max.x, layout.max.y - corner), layout.max, ImVec2(layout.max.x - corner, layout.max.y),
                ImGui::ColorConvertFloat4ToU32(Theme::Accent(0.5f)));
        }

    }

    void MaterialGraphPanel::DrawLinks(ImDrawList* drawList, Graph& graph) {

        auto mouse = ImGui::GetMousePos();
        auto hoveredLink = interaction == Interaction::None && ImGui::IsWindowHovered() ? FindLink(graph, mouse) : nullptr;

        for (const auto& link : graph.links) {
            auto from = layouts.find(link.fromNode);
            auto to = layouts.find(link.toNode);
            if (from == layouts.end() || to == layouts.end())
                continue;

            auto fromPin = from->second.outputPins.find(link.fromPin);
            auto toPin = to->second.inputPins.find(link.toPin);
            if (fromPin == from->second.outputPins.end() || toPin == to->second.inputPins.end())
                continue;

            auto fromNode = graph.GetNode(link.fromNode);
            auto output = NodeLibrary::Find(fromNode->type)->FindOutput(link.fromPin);

            auto highlight = &link == hoveredLink || selection.contains(link.fromNode) || selection.contains(link.toNode);
            auto color = GetValueColor(output->type);
            color.w = highlight ? 1.0f : 0.75f;
            DrawCurve(drawList, fromPin->second, toPin->second, ImGui::ColorConvertFloat4ToU32(color),
                (highlight ? 3.0f : 2.0f) * std::max(zoom, 0.6f), zoom);
        }

        if (interaction == Interaction::DraggingLink && draggedPin.IsValid()) {
            auto layout = layouts.find(draggedPin.node);
            if (layout != layouts.end()) {
                const auto& pins = draggedPin.isOutput ? layout->second.outputPins : layout->second.inputPins;
                auto pin = pins.find(draggedPin.pin);
                if (pin != pins.end()) {
                    auto color = ImGui::ColorConvertFloat4ToU32(Theme::Accent());
                    if (draggedPin.isOutput)
                        DrawCurve(drawList, pin->second, mouse, color, 2.5f, zoom);
                    else
                        DrawCurve(drawList, mouse, pin->second, color, 2.5f, zoom);
                }
            }
        }

    }

    void MaterialGraphPanel::DrawNodes(ImDrawList* drawList, Graph& graph, const CompiledGraph* compiled) {

        auto hoveredPin = FindPin(graph, ImGui::GetMousePos());

        auto showDetails = zoom >= detailZoom;

        for (auto& node : graph.nodes) {
            auto& layout = layouts[node.id];
            auto definition = NodeLibrary::Find(node.type);

            if (definition && definition->kind == NodeKind::Frame)
                continue;

            // Nodes outside of the canvas don't need any draw calls or widgets
            if (layout.max.x < canvasOrigin.x || layout.max.y < canvasOrigin.y ||
                layout.min.x > canvasOrigin.x + canvasSize.x || layout.min.y > canvasOrigin.y + canvasSize.y)
                continue;

            if (definition && definition->kind == NodeKind::Reroute) {
                auto center = ImVec2(0.5f * (layout.min.x + layout.max.x), 0.5f * (layout.min.y + layout.max.y));
                auto isSelected = selection.contains(node.id);
                drawList->AddCircleFilled(center, 0.5f * rerouteSize * zoom, ImGui::ColorConvertFloat4ToU32(Theme::GraphNode()));
                drawList->AddCircle(center, 0.5f * rerouteSize * zoom, ImGui::ColorConvertFloat4ToU32(isSelected ? Theme::Accent() : GetValueColor(ValueType::Generic)),
                    0, 2.0f);
                continue;
            }

            auto rounding = 6.0f * zoom;
            auto isSelected = selection.contains(node.id);

            const std::string* error = nullptr;
            if (compiled) {
                for (const auto& compileError : compiled->errors) {
                    if (compileError.nodeId == node.id) {
                        error = &compileError.message;
                        break;
                    }
                }
            }

            auto usedStages = 0u;
            if (compiled) {
                auto iter = compiled->nodeStages.find(node.id);
                usedStages = iter != compiled->nodeStages.end() ? iter->second : 0u;
            }

            // Nodes that don't contribute to anything are faded, their value isn't part of the shaders
            auto isUsed = usedStages != 0u || (definition && (definition->kind == NodeKind::Comment ||
                definition->kind == NodeKind::VertexOutput || definition->kind == NodeKind::SurfaceOutput));
            auto alpha = isUsed ? 1.0f : 0.55f;

            drawList->AddRectFilled(layout.min, layout.max, ImGui::ColorConvertFloat4ToU32(Theme::GraphNode(alpha)), rounding);

            auto headerColor = Theme::GraphNodeHeader(definition ? definition->category.c_str() : "", alpha);
            auto headerMax = ImVec2(layout.max.x, layout.min.y + layout.headerHeight);
            drawList->AddRectFilled(layout.min, headerMax, ImGui::ColorConvertFloat4ToU32(headerColor), rounding, ImDrawFlags_RoundCornersTop);

            auto border = error ? Theme::Error() : isSelected ? Theme::Accent() : Theme::GraphNodeBorder();
            drawList->AddRect(layout.min, layout.max, ImGui::ColorConvertFloat4ToU32(border), rounding, 0, (isSelected || error ? 2.0f : 1.0f));

            auto font = ImGui::GetFont();
            auto fontSize = ImGui::GetFontSize() * zoom;
            auto textColor = ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(ImGuiCol_Text));
            auto mutedColor = ImGui::ColorConvertFloat4ToU32(Theme::TextMuted());

            auto textX = layout.min.x + 8.0f * zoom;
            if (showDetails) {
                drawList->AddText(font, fontSize, ImVec2(textX, layout.min.y + 3.0f * zoom), textColor, node.type.c_str());

                if (definition && definition->kind == NodeKind::Parameter)
                    drawList->AddText(font, fontSize, ImVec2(textX, layout.min.y + rowHeight * 0.8f * zoom), mutedColor, node.name.c_str());
            }
            else {
                // Parameters are told apart by their name
                auto clip = ImVec4(layout.min.x, layout.min.y, layout.max.x, layout.max.y);
                const auto& title = definition && definition->kind == NodeKind::Parameter && !node.name.empty() ? node.name : node.type;
                drawList->AddText(font, std::max(fontSize, 11.0f), ImVec2(layout.min.x + 3.0f, layout.min.y + 1.0f), textColor, title.c_str(), nullptr, 0.0f, &clip);
                if (ImGui::IsMouseHoveringRect(layout.min, layout.max) && ImGui::IsWindowHovered())
                    ImGui::SetTooltip("%s", title == node.type ? node.type.c_str() : (node.type + ": " + title).c_str());
            }

            // Which stage the node ends up in
            auto chipX = layout.max.x - 8.0f * zoom;
            auto drawChip = [&](const char* text, ImVec4 color) {
                auto size = font->CalcTextSizeA(fontSize * 0.85f, FLT_MAX, 0.0f, text);
                chipX -= size.x;
                drawList->AddText(font, fontSize * 0.85f, ImVec2(chipX, layout.min.y + 4.0f * zoom), ImGui::ColorConvertFloat4ToU32(color), text);
                chipX -= 5.0f * zoom;
                };
            if (showDetails) {
                if (usedStages & SurfaceStage)
                    drawChip("F", Theme::Warning());
                if (usedStages & VertexStage)
                    drawChip("V", Theme::Success());
            }

            auto y = layout.min.y + layout.headerHeight + 4.0f * zoom;
            for (const auto& row : layout.rows) {
                if (row.kind == RowKind::Output && showDetails) {
                    auto size = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, row.output->name.c_str());
                    drawList->AddText(font, fontSize, ImVec2(layout.max.x - size.x - 14.0f * zoom, y + 3.0f * zoom), textColor, row.output->name.c_str());
                }
                else if (row.kind == RowKind::Input && showDetails) {
                    drawList->AddText(font, fontSize, ImVec2(layout.min.x + 14.0f * zoom, y + 3.0f * zoom), textColor, row.input->name.c_str());
                }
                else if (row.kind == RowKind::Preview) {
                    previews.Request(node.id);

                    // Square in the middle of the node
                    auto size = previewHeight * zoom - 6.0f * zoom;
                    auto centerX = 0.5f * (layout.min.x + layout.max.x);
                    auto imageMin = ImVec2(centerX - 0.5f * size, y + 3.0f * zoom);
                    auto imageMax = ImVec2(centerX + 0.5f * size, imageMin.y + size);
                    ImTextureID texture;
                    ImVec2 uvMin, uvMax;
                    if (previews.GetImage(node.id, texture, uvMin, uvMax))
                        drawList->AddImage(texture, imageMin, imageMax, uvMin, uvMax);
                    else
                        drawList->AddRectFilled(imageMin, imageMax, ImGui::ColorConvertFloat4ToU32(Theme::GraphBackground()));
                }
                y += row.height;
            }

            auto drawPin = [&](ImVec2 position, ValueType type, const std::string& pin, bool isOutput, bool isConnected) {
                auto isHovered = hoveredPin.node == node.id && hoveredPin.pin == pin && hoveredPin.isOutput == isOutput;
                auto radius = pinRadius * zoom * (isHovered ? 1.35f : 1.0f);
                auto color = ImGui::ColorConvertFloat4ToU32(GetValueColor(type));
                if (isConnected)
                    drawList->AddCircleFilled(position, radius, color);
                else
                    drawList->AddCircleFilled(position, radius, ImGui::ColorConvertFloat4ToU32(Theme::GraphNode()));
                drawList->AddCircle(position, radius, color, 0, 2.0f * zoom);
                if (isHovered)
                    ImGui::SetTooltip("%s: %s", pin.c_str(), GetValueName(type));
                };

            if (definition) {
                for (const auto& output : definition->outputs) {
                    auto isConnected = std::any_of(graph.links.begin(), graph.links.end(),
                        [&](const Link& link) { return link.fromNode == node.id && link.fromPin == output.name; });
                    drawPin(layout.outputPins[output.name], output.type, output.name, true, isConnected);
                }
                for (const auto& input : definition->inputs) {
                    drawPin(layout.inputPins[input.name], input.type, input.name, false, graph.GetInputLink(node.id, input.name) != nullptr);
                }
            }

            ImGui::PushID(int32_t(node.id));
            if (DrawNodeWidgets(graph, node, layout))
                modified = true;
            ImGui::PopID();

            if (error && ImGui::IsMouseHoveringRect(layout.min, layout.max))
                ImGui::SetTooltip("%s", error->c_str());
        }

    }

    bool MaterialGraphPanel::DrawNodeWidgets(Graph& graph, Node& node, const NodeLayout& layout) {

        // Widgets get too small to be used before the text gets unreadable
        if (zoom < detailZoom)
            return false;

        auto definition = NodeLibrary::Find(node.type);
        if (!definition)
            return false;

        auto changed = false;

        ImGui::SetWindowFontScale(zoom);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(3.0f * zoom, 1.0f * zoom));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.0f * zoom, 2.0f * zoom));

        auto y = layout.min.y + layout.headerHeight + 4.0f * zoom;
        auto padding = 8.0f * zoom;
        for (const auto& row : layout.rows) {
            if (row.kind == RowKind::Preview) {
            }
            else if (row.kind == RowKind::Value) {
                changed |= DrawValueWidget(node, *definition, ImVec2(layout.min.x + padding, y + 1.0f * zoom),
                    layout.max.x - layout.min.x - 2.0f * padding);
            }
            else if (row.kind == RowKind::Text) {
                ImGui::SetCursorScreenPos(ImVec2(layout.min.x + padding, y));
                changed |= ImGui::InputTextMultiline("##Comment", &node.name,
                    ImVec2(layout.max.x - layout.min.x - 2.0f * padding, row.height - 4.0f * zoom));
            }
            else if (row.kind == RowKind::Input && !graph.GetInputLink(node.id, row.input->name)) {
                auto textWidth = ImGui::CalcTextSize(row.input->name.c_str()).x;
                auto isWide = row.height > rowHeight * zoom * 1.5f;
                auto startX = isWide ? layout.min.x + 14.0f * zoom : layout.min.x + 14.0f * zoom + textWidth + 6.0f * zoom;
                auto startY = isWide ? y + rowHeight * zoom : y + 1.0f * zoom;
                auto width = layout.max.x - startX - padding;
                if (width > 16.0f * zoom)
                    changed |= DrawInputWidget(node, *row.input, ImVec2(startX, startY), width);
            }
            y += row.height;
        }

        ImGui::PopStyleVar(2);
        ImGui::SetWindowFontScale(1.0f);

        return changed;

    }

    bool MaterialGraphPanel::DrawInputWidget(Node& node, const InputDefinition& input, ImVec2 position, float width) {

        ImGui::SetCursorScreenPos(position);
        ImGui::PushID(input.name.c_str());

        if (input.type == ValueType::Texture) {
            ImGui::TextColored(Theme::Warning(), "connect a texture");
            ImGui::PopID();
            return false;
        }

        if (!input.defaultExpression.empty()) {
            ImGui::TextColored(Theme::TextMuted(), "of the mesh");
            ImGui::PopID();
            return false;
        }

        auto value = input.defaultValue;
        auto iter = node.inputValues.find(input.name);
        if (iter != node.inputValues.end())
            value = iter->second;

        auto isColor = input.type == ValueType::Vec3 && (input.name.find("olor") != std::string::npos ||
            input.name == NodeLibrary::emissivePin);

        int32_t components = 1;
        switch (input.type) {
        case ValueType::Vec2: components = 2; break;
        case ValueType::Vec3: components = 3; break;
        case ValueType::Vec4: components = 4; break;
        default: break;
        }

        auto changed = false;
        ImGui::SetNextItemWidth(width);
        if (isColor) {
            changed = ImGui::ColorEdit3("##Value", &value.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs |
                ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_HDR);
        }
        else {
            changed = ImGui::DragScalarN("##Value", ImGuiDataType_Float, &value.x, components, 0.01f, nullptr, nullptr, "%.3g");
        }

        if (changed)
            node.inputValues[input.name] = value;
        // Values are part of the generated code, so they wait until the value is final
        if (ImGui::IsItemDeactivatedAfterEdit())
            compileRequested = true;
        colorEdited |= isColor && changed;

        ImGui::PopID();

        return changed;

    }

    bool MaterialGraphPanel::DrawValueWidget(Node& node, const NodeDefinition& definition, ImVec2 position, float width) {

        ImGui::SetCursorScreenPos(position);

        int32_t components = 1;
        switch (definition.valueType) {
        case ValueType::Vec2: components = 2; break;
        case ValueType::Vec3: components = 3; break;
        case ValueType::Vec4: components = 4; break;
        default: break;
        }

        if (definition.kind == NodeKind::StaticSwitch) {
            auto isOn = node.value.x > 0.0f;
            auto changed = ImGui::Checkbox("On##Value", &isOn);
            if (changed) {
                node.value.x = isOn ? 1.0f : 0.0f;
                compileRequested = true;
            }
            return changed;
        }

        auto changed = false;
        ImGui::SetNextItemWidth(width);
        if (definition.isColor) {
            changed = ImGui::ColorEdit3("##Value", &node.value.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
        }
        else if (node.range.x < node.range.y) {
            changed = ImGui::DragScalarN("##Value", ImGuiDataType_Float, &node.value.x, components, 0.01f, &node.range.x, &node.range.y, "%.3g",
                ImGuiSliderFlags_AlwaysClamp);
        }
        else {
            changed = ImGui::DragScalarN("##Value", ImGuiDataType_Float, &node.value.x, components, 0.01f, nullptr, nullptr, "%.3g");
        }

        // The default of a parameter isn't part of the code, so it can be applied while dragging
        if (definition.kind == NodeKind::Parameter && changed)
            compileRequested = true;
        else if (ImGui::IsItemDeactivatedAfterEdit())
            compileRequested = true;
        else
            colorEdited |= definition.isColor && changed;

        return changed;

    }

    void MaterialGraphPanel::DrawToolbar(Graph& graph, const CompiledGraph* compiled) {

        if (ImGui::Button("Add node"))
            OpenAddMenu();
        ImGui::SetItemTooltip("Adds a node (Space or right click on the canvas)");
        ImGui::SameLine();
        if (ImGui::Button("Frame"))
            FrameAll();
        ImGui::SetItemTooltip("Shows all nodes (F)");
        ImGui::SameLine();
        if (ImGui::Button("Arrange"))
            Arrange(graph);
        ImGui::SetItemTooltip("Places all nodes next to the nodes they connect to");
        ImGui::SameLine();
        if (ImGui::Button("Generated code"))
            showCode = true;
        ImGui::SameLine();
        const char* previewModes[] = { "No previews", "Key previews", "All previews" };
        auto previewModeIdx = int32_t(previewMode);
        ImGui::SetNextItemWidth(130.0f);
        if (ImGui::Combo("##Previews", &previewModeIdx, previewModes, IM_ARRAYSIZE(previewModes)))
            previewMode = PreviewMode(previewModeIdx);
        ImGui::SetItemTooltip("Which nodes show a picture of their value. Pictures make nodes larger, Arrange takes that into account");
        ImGui::SameLine();
        // As high as the text buttons next to it: the icon is one line high with the same frame padding
        auto& maximizeIcon = Singletons::icons->Get(maximized ? IconType::ScaleInverse : IconType::Scale);
        auto lineHeight = ImGui::GetTextLineHeight();
        if (ImGui::ImageButton("Maximize", Singletons::imguiWrapper->GetTextureId(&maximizeIcon), ImVec2(lineHeight, lineHeight),
            ImVec2(0.1f, 0.1f), ImVec2(0.9f, 0.9f))) {
            maximized = !maximized;
            frameRequested = true;
            // The canvas gets its new size in the next frames
            stableCanvasFrames = -2;
        }
        ImGui::SetItemTooltip(maximized ? "Shows the graph in the material editor again (F11)" :
            "Shows the graph over the whole editor (F11)");
        ImGui::SameLine();
        if (modified) {
            ImGui::TextColored(Theme::Warning(), "Unsaved");
            ImGui::SetItemTooltip("Saving the material saves the graph as well (Ctrl+S)");
            ImGui::SameLine();
        }
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        if (!compiled) {
            ImGui::TextDisabled("Not compiled");
            return;
        }

        if (!compiled->valid) {
            ImGui::TextColored(Theme::Error(), "%d %s", int32_t(compiled->errors.size()), compiled->errors.size() == 1 ? "error" : "errors");
            ImGui::SameLine();
            ImGui::TextColored(Theme::TextMuted(), "%s", compiled->errors.empty() ? "" : compiled->errors.front().message.c_str());
            return;
        }

        if (!compiled->hasVertexOffset && !compiled->hasSurface) {
            ImGui::TextColored(Theme::TextMuted(), "Connect an output to use the graph, until then the regular material properties are used");
            return;
        }

        // The kind of graph only, the output section of the inspector explains what it does
        auto kind = compiled->hasVertexOffset && compiled->hasSurface ? "Vertex and surface graph" :
            compiled->hasVertexOffset ? "Vertex graph" : "Surface graph";
        auto textureCount = std::count_if(compiled->parameters.begin(), compiled->parameters.end(),
            [](const Parameter& parameter) { return parameter.type == ValueType::Texture; });
        ImGui::TextUnformatted(kind);
        ImGui::SetItemTooltip("The inspector shows what the graph does without a selected node");
        ImGui::SameLine();
        ImGui::TextColored(Theme::TextMuted(), "· %d parameters, %d textures", int32_t(compiled->parameters.size()), int32_t(textureCount));

    }

    void MaterialGraphPanel::DrawMinimap(ImDrawList* drawList, Graph& graph, ImVec2 canvasMin, ImVec2 canvasMax) {

        if (graph.nodes.empty())
            return;

        vec2 boundsMin(FLT_MAX), boundsMax(-FLT_MAX);
        for (const auto& node : graph.nodes) {
            const auto& layout = layouts[node.id];
            boundsMin = glm::min(boundsMin, node.position);
            boundsMax = glm::max(boundsMax, node.position + GetNodeSize(node));
        }

        auto viewMin = ToCanvas(canvasMin);
        auto viewMax = ToCanvas(canvasMax);
        boundsMin = glm::min(boundsMin, viewMin);
        boundsMax = glm::max(boundsMax, viewMax);

        const auto maxMapSize = vec2(180.0f, 120.0f);
        const auto margin = 8.0f;
        auto extent = glm::max(boundsMax - boundsMin, vec2(1.0f));
        auto scale = glm::min(maxMapSize.x / extent.x, maxMapSize.y / extent.y);

        // The map only covers what it shows, a click anywhere on it stays near the graph
        auto mapSize = extent * scale;
        auto mapMax = ImVec2(canvasMax.x - margin, canvasMax.y - margin);
        auto mapMin = ImVec2(mapMax.x - mapSize.x, mapMax.y - mapSize.y);
        minimapMin = mapMin;
        minimapMax = mapMax;

        auto toMap = [&](vec2 position) {
            auto local = (position - boundsMin) * scale;
            return ImVec2(mapMin.x + local.x, mapMin.y + local.y);
            };

        drawList->AddRectFilled(mapMin, mapMax, ImGui::ColorConvertFloat4ToU32(Theme::OverlayBackground()), 4.0f);
        drawList->AddRect(mapMin, mapMax, ImGui::ColorConvertFloat4ToU32(Theme::OverlayBorder()), 4.0f);

        for (const auto& node : graph.nodes) {
            auto definition = NodeLibrary::Find(node.type);
            auto isFrame = definition && definition->kind == NodeKind::Frame;
            auto color = selection.contains(node.id) ? Theme::Accent() : isFrame ? Theme::Accent(0.2f) : Theme::TextMuted();
            drawList->AddRectFilled(toMap(node.position), toMap(node.position + GetNodeSize(node)),
                ImGui::ColorConvertFloat4ToU32(color));
        }

        drawList->AddRect(toMap(viewMin), toMap(viewMax), ImGui::ColorConvertFloat4ToU32(Theme::Accent()));

        // Clicking the map moves the view there
        if (ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(mapMin, mapMax) && ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
            interaction == Interaction::None) {
            auto mouse = ImGui::GetMousePos();
            auto target = boundsMin + vec2(mouse.x - mapMin.x, mouse.y - mapMin.y) / scale;
            pan = vec2(canvasSize.x, canvasSize.y) * 0.5f / zoom - target;
        }

    }

    void MaterialGraphPanel::DrawCodeWindow(const CompiledGraph* compiled) {

        if (!showCode)
            return;

        ImGui::SetNextWindowSize(ImVec2(760.0f, 620.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Generated code##MaterialGraph", &showCode)) {
            if (!compiled || !compiled->valid) {
                ImGui::TextColored(Theme::Warning(), "The graph has errors, there is no code");
            }
            else if (compiled->mainVertexCode.empty()) {
                ImGui::TextDisabled("The graph doesn't generate any code yet");
            }
            else if (ImGui::BeginTabBar("Stages")) {
                auto tab = [](const char* name, const std::string& code) {
                    if (code.empty() || !ImGui::BeginTabItem(name))
                        return;
                    auto text = code;
                    ImGui::InputTextMultiline("##Code", &text, ImGui::GetContentRegionAvail(), ImGuiInputTextFlags_ReadOnly);
                    ImGui::EndTabItem();
                    };
                tab("Vertex", compiled->mainVertexCode);
                tab("Fragment", compiled->mainFragmentCode);
                tab("Shadow vertex", compiled->shadowVertexCode);
                tab("Shadow fragment", compiled->shadowFragmentCode);
                ImGui::EndTabBar();
            }
        }
        ImGui::End();

    }

    void MaterialGraphPanel::HandleInteractions(Graph& graph) {

        auto& io = ImGui::GetIO();
        auto mouse = ImGui::GetMousePos();
        auto isCanvasHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) &&
            ImGui::IsMouseHoveringRect(canvasOrigin, ImVec2(canvasOrigin.x + canvasSize.x, canvasOrigin.y + canvasSize.y));

        // A widget of a node has the mouse, e.g. a value that is dragged
        auto isWidgetBusy = ImGui::IsAnyItemActive() || ImGui::IsAnyItemHovered();

        // Only left clicks focus windows, the shortcuts should also work after using the canvas with the other buttons
        if (isCanvasHovered && (ImGui::IsMouseClicked(ImGuiMouseButton_Right) || ImGui::IsMouseClicked(ImGuiMouseButton_Middle)))
            ImGui::SetWindowFocus();

        if (isCanvasHovered && io.MouseWheel != 0.0f) {
            auto before = ToCanvas(mouse);
            zoom = std::clamp(zoom * std::pow(1.15f, io.MouseWheel), minZoom, 2.5f);
            auto after = ToCanvas(mouse);
            pan += after - before;
        }

        // Panning works from everywhere, also on top of nodes
        if (isCanvasHovered && interaction == Interaction::None && ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
            interaction = Interaction::Panning;
        if (isCanvasHovered && interaction == Interaction::None && ImGui::IsMouseDragging(ImGuiMouseButton_Right, 4.0f))
            interaction = Interaction::Panning;

        if (interaction == Interaction::Panning) {
            pan += vec2(io.MouseDelta.x, io.MouseDelta.y) / zoom;
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Middle) && !ImGui::IsMouseDown(ImGuiMouseButton_Right))
                interaction = Interaction::None;
            return;
        }

        if (interaction == Interaction::MovingNodes) {
            if (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f) {
                nodesMoved = true;
                for (auto id : selection) {
                    if (auto node = graph.GetNode(id))
                        node->position += vec2(io.MouseDelta.x, io.MouseDelta.y) / zoom;
                }
                for (auto id : movedWithFrames) {
                    if (selection.contains(id))
                        continue;
                    if (auto node = graph.GetNode(id))
                        node->position += vec2(io.MouseDelta.x, io.MouseDelta.y) / zoom;
                }
            }
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                interaction = Interaction::None;
                modified |= nodesMoved;
            }
            return;
        }

        if (interaction == Interaction::ResizingFrame) {
            if (auto node = graph.GetNode(resizedFrame)) {
                node->size = glm::max(node->size + vec2(io.MouseDelta.x, io.MouseDelta.y) / zoom, vec2(120.0f, 80.0f));
            }
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                interaction = Interaction::None;
                modified = true;
            }
            return;
        }

        if (interaction == Interaction::BoxSelection) {
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                auto boxMin = ImVec2(std::min(boxStart.x, mouse.x), std::min(boxStart.y, mouse.y));
                auto boxMax = ImVec2(std::max(boxStart.x, mouse.x), std::max(boxStart.y, mouse.y));
                if (!io.KeyShift && !io.KeyCtrl)
                    selection.clear();
                for (const auto& node : graph.nodes) {
                    const auto& layout = layouts[node.id];
                    // A box inside of a frame selects the nodes in there, not the frame
                    auto definition = NodeLibrary::Find(node.type);
                    if (definition && definition->kind == NodeKind::Frame) {
                        if (layout.min.x >= boxMin.x && layout.max.x <= boxMax.x && layout.min.y >= boxMin.y && layout.max.y <= boxMax.y)
                            selection.insert(node.id);
                        continue;
                    }
                    if (layout.min.x < boxMax.x && layout.max.x > boxMin.x && layout.min.y < boxMax.y && layout.max.y > boxMin.y)
                        selection.insert(node.id);
                }
                interaction = Interaction::None;
            }
            return;
        }

        if (interaction == Interaction::DraggingLink) {
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                auto target = FindPin(graph, mouse);
                interaction = Interaction::None;

                if (target.IsValid() && target.isOutput != draggedPin.isOutput) {
                    const auto& output = draggedPin.isOutput ? draggedPin : target;
                    const auto& input = draggedPin.isOutput ? target : draggedPin;
                    if (graph.Connect(output.node, output.pin, input.node, input.pin))
                        compileRequested = true;
                }
                else if (!target.IsValid() && isCanvasHovered) {
                    // Dropped on the canvas: choose the node for the other end
                    pendingPin = draggedPin;
                    addPosition = ToCanvas(mouse);
                    openAddMenu = true;
                }
                draggedPin = PinRef();
            }
            return;
        }

        if (!isCanvasHovered)
            return;

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) && !isWidgetBusy) {
            auto node = FindNode(graph, mouse);
            if (node) {
                if (!selection.contains(node)) {
                    selection.clear();
                    selection.insert(node);
                }
                ImGui::OpenPopup("NodeMenu");
            }
            else {
                pendingPin = PinRef();
                addPosition = ToCanvas(mouse);
                openAddMenu = true;
            }
        }

        if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            return;

        // The minimap handles the mouse on its own
        if (ImGui::IsMouseHoveringRect(minimapMin, minimapMax))
            return;

        auto pin = FindPin(graph, mouse);
        if (pin.IsValid()) {
            // An input with a link gets picked up, the link can then be dropped somewhere else
            if (!pin.isOutput) {
                if (auto link = graph.GetInputLink(pin.node, pin.pin)) {
                    draggedPin = { link->fromNode, link->fromPin, true };
                    graph.Disconnect(pin.node, pin.pin);
                    compileRequested = true;
                    interaction = Interaction::DraggingLink;
                    return;
                }
            }
            draggedPin = pin;
            interaction = Interaction::DraggingLink;
            return;
        }

        if (isWidgetBusy)
            return;

        // A double click on a link adds a point to lead it around other nodes
        if (ImGui::GetMouseClickedCount(ImGuiMouseButton_Left) == 2) {
            if (auto link = FindLink(graph, mouse)) {
                InsertReroute(graph, *link, ToCanvas(mouse) - vec2(rerouteSize * 0.5f));
                return;
            }
        }

        // Alt click removes a link
        if (io.KeyAlt) {
            if (auto link = FindLink(graph, mouse)) {
                graph.Disconnect(link->toNode, link->toPin);
                compileRequested = true;
                return;
            }
        }

        auto node = FindNode(graph, mouse);
        if (node) {
            auto frameDefinition = NodeLibrary::Find(graph.GetNode(node)->type);
            auto isFrame = frameDefinition && frameDefinition->kind == NodeKind::Frame;

            if (isFrame) {
                const auto& layout = layouts[node];
                auto corner = 14.0f;
                if (mouse.x > layout.max.x - corner && mouse.y > layout.max.y - corner) {
                    resizedFrame = node;
                    interaction = Interaction::ResizingFrame;
                    return;
                }
            }

            if (io.KeyCtrl || io.KeyShift) {
                if (selection.contains(node))
                    selection.erase(node);
                else
                    selection.insert(node);
            }
            else if (!selection.contains(node)) {
                selection.clear();
                selection.insert(node);
            }
            nodesMoved = false;
            interaction = Interaction::MovingNodes;

            movedWithFrames.clear();
            for (auto id : selection) {
                auto selectedNode = graph.GetNode(id);
                auto selectedDefinition = selectedNode ? NodeLibrary::Find(selectedNode->type) : nullptr;
                if (selectedDefinition && selectedDefinition->kind == NodeKind::Frame) {
                    for (auto inFrame : GetNodesInFrame(graph, *selectedNode))
                        movedWithFrames.insert(inFrame);
                }
            }

            // The node ends up in front of the others
            auto iter = std::find_if(graph.nodes.begin(), graph.nodes.end(), [&](const Node& item) { return item.id == node; });
            std::rotate(iter, iter + 1, graph.nodes.end());
            return;
        }

        boxStart = mouse;
        interaction = Interaction::BoxSelection;

    }

    void MaterialGraphPanel::HandleShortcuts(const ResourceHandle<Graph>& graphHandle) {

        auto& graph = *graphHandle.Get();
        auto& io = ImGui::GetIO();

#ifdef AE_OS_MACOS
        auto command = io.KeySuper;
#else
        auto command = io.KeyCtrl;
#endif

        if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) || ImGui::IsKeyPressed(ImGuiKey_Backspace, false))
            DeleteSelection(graph);
        if (ImGui::IsKeyPressed(ImGuiKey_F, false) && !command) {
            frameOnlySelection = !selection.empty();
            FrameAll();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_F11, false))
            ToggleMaximized();
        if (ImGui::IsKeyPressed(ImGuiKey_Space, false) && ImGui::IsWindowHovered()) {
            pendingPin = PinRef();
            addPosition = ToCanvas(ImGui::GetMousePos());
            openAddMenu = true;
        }

        if (!command)
            return;

        if (ImGui::IsKeyPressed(ImGuiKey_A, false))
            SelectAll(graph);
        if (ImGui::IsKeyPressed(ImGuiKey_C, false))
            CopySelection(graph);
        if (ImGui::IsKeyPressed(ImGuiKey_V, false))
            Paste(graph);
        if (ImGui::IsKeyPressed(ImGuiKey_D, false))
            DuplicateSelection(graph);
        if (ImGui::IsKeyPressed(ImGuiKey_G, false))
            FrameSelection(graph);

    }

    void MaterialGraphPanel::RenderNodeMenu(Graph& graph) {

        if (!ImGui::BeginPopup("NodeMenu"))
            return;

        if (ImGui::MenuItem("Duplicate", "Ctrl+D"))
            DuplicateSelection(graph);
        if (ImGui::MenuItem("Copy", "Ctrl+C"))
            CopySelection(graph);
        if (ImGui::MenuItem("Frame selection", "Ctrl+G"))
            FrameSelection(graph);
        if (ImGui::MenuItem("Disconnect")) {
            std::erase_if(graph.links, [&](const Link& link) {
                return selection.contains(link.fromNode) || selection.contains(link.toNode);
                });
            compileRequested = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Delete", "Del"))
            DeleteSelection(graph);

        ImGui::EndPopup();

    }

    void MaterialGraphPanel::RenderAddMenu(Graph& graph) {

        if (openAddMenu) {
            ImGui::OpenPopup("AddNodeMenu");
            addSearch.clear();
            addRow = 0;
            focusAddSearch = true;
            openAddMenu = false;

            // A dropped link leaves few nodes, they shouldn't hide in closed categories
            openAddCategories.clear();
            if (pendingPin.IsValid()) {
                for (const auto& definition : NodeLibrary::GetDefinitions())
                    openAddCategories.insert(definition.category);
            }
        }

        ImGui::SetNextWindowSizeConstraints(ImVec2(280.0f, 0.0f), ImVec2(320.0f, 460.0f));
        if (!ImGui::BeginPopup("AddNodeMenu")) {
            return;
        }

        // A popup can only take the focus once it is shown
        if (focusAddSearch) {
            ImGui::SetKeyboardFocusHere();
            focusAddSearch = ImGui::IsWindowAppearing();
        }
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputTextWithHint("##Search", "Search nodes...", &addSearch))
            addRow = 0;

        auto toLower = [](std::string text) {
            std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return char(std::tolower(c)); });
            return text;
            };
        auto search = toLower(addSearch);

        auto canAdd = [&](const NodeDefinition& definition) {
            // The outputs exist in every graph, there can only be one of them
            if (definition.kind == NodeKind::VertexOutput || definition.kind == NodeKind::SurfaceOutput) {
                auto exists = std::any_of(graph.nodes.begin(), graph.nodes.end(), [&](const Node& node) { return node.type == definition.type; });
                if (exists)
                    return false;
            }
            // A dropped link only offers nodes the link can connect to
            return !pendingPin.IsValid() || FindPendingPartner(graph, definition);
            };

        // Either a category or a node. Searching lists the nodes without categories
        struct Row {
            std::string category;
            const NodeDefinition* definition = nullptr;
        };

        std::vector<Row> rows;
        std::string category;
        for (const auto& definition : NodeLibrary::GetDefinitions()) {
            if (!canAdd(definition))
                continue;

            if (!search.empty()) {
                if (toLower(definition.type).find(search) != std::string::npos || toLower(definition.tooltip).find(search) != std::string::npos)
                    rows.push_back({ definition.category, &definition });
                continue;
            }

            if (definition.category != category) {
                category = definition.category;
                rows.push_back({ category });
            }
            if (openAddCategories.contains(category))
                rows.push_back({ category, &definition });
        }

        const NodeDefinition* selected = nullptr;
        auto rowMoved = false;
        if (!rows.empty()) {
            addRow = std::clamp(addRow, 0, int32_t(rows.size()) - 1);

            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) || ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
                auto step = ImGui::IsKeyPressed(ImGuiKey_DownArrow) ? 1 : int32_t(rows.size()) - 1;
                addRow = (addRow + step) % int32_t(rows.size());
                rowMoved = true;
            }

            const auto& row = rows[addRow];
            auto isCategoryOpen = openAddCategories.contains(row.category);
            if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) {
                if (row.definition)
                    selected = row.definition;
                else if (isCategoryOpen)
                    openAddCategories.erase(row.category);
                else
                    openAddCategories.insert(row.category);
            }

            // The arrows move the cursor of the search text as long as there is one
            if (search.empty() && ImGui::IsKeyPressed(ImGuiKey_RightArrow) && !row.definition)
                openAddCategories.insert(row.category);
            if (search.empty() && ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
                if (row.definition) {
                    while (rows[addRow].definition)
                        addRow--;
                    rowMoved = true;
                }
                else {
                    openAddCategories.erase(row.category);
                }
            }
        }

        ImGui::BeginChild("Nodes", ImVec2(0.0f, 360.0f));

        for (int32_t i = 0; i < int32_t(rows.size()); i++) {
            const auto& row = rows[i];
            auto isCurrent = i == addRow;

            if (row.definition) {
                if (search.empty())
                    ImGui::Indent();
                if (ImGui::Selectable(row.definition->type.c_str(), isCurrent))
                    selected = row.definition;
                ImGui::SetItemTooltip("%s", row.definition->tooltip.c_str());
                if (search.empty())
                    ImGui::Unindent();
            }
            else {
                auto isOpen = openAddCategories.contains(row.category);
                ImGui::SetNextItemOpen(isOpen);
                auto flags = ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth | (isCurrent ? ImGuiTreeNodeFlags_Selected : 0);
                if (ImGui::TreeNodeEx(row.category.c_str(), flags) != isOpen) {
                    if (isOpen)
                        openAddCategories.erase(row.category);
                    else
                        openAddCategories.insert(row.category);
                }
            }

            if (isCurrent && rowMoved)
                ImGui::SetScrollHereY();
        }

        ImGui::EndChild();

        if (selected) {
            AddNode(graph, *selected);
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();

    }

    MaterialGraphPanel::PinRef MaterialGraphPanel::FindPin(const Graph& graph, ImVec2 position) const {

        auto radius = std::max(pinRadius * zoom * 1.6f, 7.0f);
        auto isClose = [&](ImVec2 pin) {
            auto dx = pin.x - position.x;
            auto dy = pin.y - position.y;
            return dx * dx + dy * dy <= radius * radius;
            };

        // From front to back, as nodes are drawn
        for (auto node = graph.nodes.rbegin(); node != graph.nodes.rend(); node++) {
            auto layout = layouts.find(node->id);
            if (layout == layouts.end())
                continue;
            for (const auto& [name, pin] : layout->second.outputPins) {
                if (isClose(pin))
                    return { node->id, name, true };
            }
            for (const auto& [name, pin] : layout->second.inputPins) {
                if (isClose(pin))
                    return { node->id, name, false };
            }
        }

        return {};

    }

    uint32_t MaterialGraphPanel::FindNode(const Graph& graph, ImVec2 position) const {

        uint32_t frame = 0;
        for (auto node = graph.nodes.rbegin(); node != graph.nodes.rend(); node++) {
            auto layout = layouts.find(node->id);
            if (layout == layouts.end())
                continue;
            const auto& min = layout->second.min;
            const auto& max = layout->second.max;
            if (position.x < min.x || position.x > max.x || position.y < min.y || position.y > max.y)
                continue;

            // Frames lie behind all nodes, inside of them only the header and the corner can be grabbed
            auto definition = NodeLibrary::Find(node->type);
            if (definition && definition->kind == NodeKind::Frame) {
                auto isCorner = position.x > max.x - 14.0f && position.y > max.y - 14.0f;
                if (!frame && (position.y <= min.y + layout->second.headerHeight || isCorner))
                    frame = node->id;
                continue;
            }

            return node->id;
        }

        return frame;

    }

    const Link* MaterialGraphPanel::FindLink(const Graph& graph, ImVec2 position) const {

        if (FindNode(graph, position))
            return nullptr;

        for (const auto& link : graph.links) {
            auto from = layouts.find(link.fromNode);
            auto to = layouts.find(link.toNode);
            if (from == layouts.end() || to == layouts.end())
                continue;

            auto fromPin = from->second.outputPins.find(link.fromPin);
            auto toPin = to->second.inputPins.find(link.toPin);
            if (fromPin == from->second.outputPins.end() || toPin == to->second.inputPins.end())
                continue;

            auto a = fromPin->second;
            auto d = toPin->second;
            auto distance = std::max(std::abs(d.x - a.x) * 0.5f, 40.0f * zoom);
            auto b = ImVec2(a.x + distance, a.y);
            auto c = ImVec2(d.x - distance, d.y);

            auto closest = ImBezierCubicClosestPointCasteljau(a, b, c, d, position, ImGui::GetStyle().CurveTessellationTol);
            auto dx = closest.x - position.x;
            auto dy = closest.y - position.y;
            if (dx * dx + dy * dy < 36.0f)
                return &link;
        }

        return nullptr;

    }

    void MaterialGraphPanel::AddNode(Graph& graph, const NodeDefinition& definition) {

        auto& node = graph.AddNode(definition.type, addPosition);
        auto nodeId = node.id;

        if (definition.kind == NodeKind::Frame) {
            node.name = "Frame";
            node.size = vec2(520.0f, 360.0f);
        }

        // Parameters need a name, which is unique for new nodes
        if (definition.kind == NodeKind::Parameter) {
            int32_t index = 1;
            std::string name;
            do {
                name = "Parameter " + std::to_string(index++);
            } while (std::any_of(graph.nodes.begin(), graph.nodes.end(), [&](const Node& other) { return other.name == name; }));
            graph.GetNode(nodeId)->name = name;
        }

        if (pendingPin.IsValid()) {
            if (auto pin = FindPendingPartner(graph, definition)) {
                if (pendingPin.isOutput)
                    graph.Connect(pendingPin.node, pendingPin.pin, nodeId, *pin);
                else
                    graph.Connect(nodeId, *pin, pendingPin.node, pendingPin.pin);
            }
            pendingPin = PinRef();
        }

        selection.clear();
        selection.insert(nodeId);

        compileRequested = true;

    }

    const std::string* MaterialGraphPanel::FindPendingPartner(const Graph& graph, const NodeDefinition& definition) const {

        auto node = graph.GetNode(pendingPin.node);
        auto pendingDefinition = node ? NodeLibrary::Find(node->type) : nullptr;
        if (!pendingDefinition)
            return nullptr;

        // A varying reads in the vertex stage and hands the value to the surface stage
        auto getStages = [](const NodeDefinition& nodeDefinition, bool isOutput) -> uint32_t {
            switch (nodeDefinition.kind) {
            case NodeKind::Varying: return isOutput ? SurfaceStage : VertexStage;
            case NodeKind::VertexOutput: return VertexStage;
            case NodeKind::SurfaceOutput: return SurfaceStage;
            default: return nodeDefinition.stages;
            }
            };

        if (!(getStages(*pendingDefinition, pendingPin.isOutput) & getStages(definition, !pendingPin.isOutput)))
            return nullptr;

        auto pendingType = pendingPin.isOutput ? pendingDefinition->FindOutput(pendingPin.pin)->type :
            pendingDefinition->FindInput(pendingPin.pin)->type;

        // Pins of the same type go first, other values would only convert
        const std::string* partner = nullptr;
        auto check = [&](const auto& pins) {
            for (const auto& pin : pins) {
                if ((pin.type == ValueType::Texture) != (pendingType == ValueType::Texture))
                    continue;
                if (pin.type == pendingType || pin.type == ValueType::Generic || pendingType == ValueType::Generic)
                    return &pin.name;
                if (!partner)
                    partner = &pin.name;
            }
            return partner;
            };

        return pendingPin.isOutput ? check(definition.inputs) : check(definition.outputs);

    }

    void MaterialGraphPanel::SelectAll(const Graph& graph) {

        for (const auto& node : graph.nodes)
            selection.insert(node.id);

    }

    void MaterialGraphPanel::ToggleMaximized() {

        maximized = !maximized;
        frameRequested = true;
        stableCanvasFrames = -2;

    }

    void MaterialGraphPanel::DeleteSelection(Graph& graph) {

        if (selection.empty())
            return;

        for (auto id : selection) {
            auto node = graph.GetNode(id);
            if (!node)
                continue;

            // The outputs stay, otherwise there is nothing to connect the graph to
            auto definition = NodeLibrary::Find(node->type);
            if (definition && (definition->kind == NodeKind::VertexOutput || definition->kind == NodeKind::SurfaceOutput)) {
                std::erase_if(graph.links, [&](const Link& link) { return link.toNode == id; });
                continue;
            }

            graph.RemoveNode(id);
        }

        selection.clear();
        compileRequested = true;

    }

    void MaterialGraphPanel::DuplicateSelection(Graph& graph) {

        auto previousClipboard = clipboard;
        CopySelection(graph);
        Paste(graph);
        clipboard = previousClipboard;

    }

    void MaterialGraphPanel::FrameSelection(Graph& graph) {

        if (selection.empty())
            return;

        vec2 boundsMin(FLT_MAX), boundsMax(-FLT_MAX);
        for (auto id : selection) {
            auto node = graph.GetNode(id);
            if (!node)
                continue;
            boundsMin = glm::min(boundsMin, node->position);
            boundsMax = glm::max(boundsMax, node->position + GetNodeSize(*node));
        }

        const float padding = 24.0f;
        auto& frame = graph.AddNode("Frame", boundsMin - vec2(padding, padding + frameHeaderHeight));
        frame.name = "Frame";
        frame.size = boundsMax - boundsMin + vec2(2.0f * padding, 2.0f * padding + frameHeaderHeight);

        selection.clear();
        selection.insert(frame.id);

        modified = true;

    }

    void MaterialGraphPanel::InsertReroute(Graph& graph, Link link, vec2 position) {

        auto nodeId = graph.AddNode("Reroute", position).id;

        graph.Connect(link.fromNode, link.fromPin, nodeId, "Value");
        graph.Connect(nodeId, "Value", link.toNode, link.toPin);

        selection.clear();
        selection.insert(nodeId);

        compileRequested = true;

    }

    std::vector<uint32_t> MaterialGraphPanel::GetNodesInFrame(const Graph& graph, const Node& frame) const {

        std::vector<uint32_t> nodes;
        for (const auto& node : graph.nodes) {
            if (node.id == frame.id)
                continue;

            auto center = node.position + 0.5f * GetNodeSize(node);
            if (glm::all(glm::greaterThanEqual(center, frame.position)) && glm::all(glm::lessThanEqual(center, frame.position + frame.size)))
                nodes.push_back(node.id);
        }

        return nodes;

    }

    vec2 MaterialGraphPanel::GetNodeSize(const Node& node) const {

        auto layout = layouts.find(node.id);
        if (layout == layouts.end() || zoom <= 0.0f)
            return vec2(nodeWidth, 200.0f);

        return vec2(layout->second.max.x - layout->second.min.x, layout->second.max.y - layout->second.min.y) / zoom;

    }

    void MaterialGraphPanel::CopySelection(const Graph& graph) {

        std::unordered_set<uint32_t> copied;
        json nodes = json::array();
        for (const auto& node : graph.nodes) {
            auto definition = NodeLibrary::Find(node.type);
            if (!selection.contains(node.id) || (definition && (definition->kind == NodeKind::VertexOutput || definition->kind == NodeKind::SurfaceOutput)))
                continue;
            nodes.push_back(node);
            copied.insert(node.id);
        }

        json links = json::array();
        for (const auto& link : graph.links) {
            if (copied.contains(link.fromNode) && copied.contains(link.toNode))
                links.push_back(link);
        }

        clipboard = json{ {"nodes", nodes}, {"links", links} }.dump();

    }

    void MaterialGraphPanel::Paste(Graph& graph) {

        if (clipboard.empty())
            return;

        auto j = json::parse(clipboard);

        std::vector<Node> nodes = j["nodes"];
        std::vector<Link> links = j["links"];

        vec2 center(0.0f);
        for (const auto& node : nodes)
            center += node.position / float(std::max(nodes.size(), size_t(1)));

        // Near the mouse, a duplicate shouldn't hide the original
        auto target = ImGui::IsWindowHovered() ? ToCanvas(ImGui::GetMousePos()) : center + vec2(40.0f);

        std::unordered_map<uint32_t, uint32_t> idMap;
        selection.clear();
        for (auto& node : nodes) {
            auto oldId = node.id;
            node.id = graph.nextNodeId++;
            node.position += target - center;
            idMap[oldId] = node.id;
            selection.insert(node.id);
            graph.nodes.push_back(node);
        }

        for (auto link : links) {
            link.fromNode = idMap[link.fromNode];
            link.toNode = idMap[link.toNode];
            graph.links.push_back(link);
        }

        compileRequested = true;

    }

    void MaterialGraphPanel::Arrange(Graph& graph) {

        auto isFrame = [](const Node& node) {
            auto definition = NodeLibrary::Find(node.type);
            return definition && definition->kind == NodeKind::Frame;
            };

        // Frames keep the nodes they hold and are fitted around them afterwards
        std::vector<std::pair<Node*, std::vector<uint32_t>>> frames;
        for (auto& node : graph.nodes) {
            if (isFrame(node))
                frames.emplace_back(&node, GetNodesInFrame(graph, node));
        }

        // Columns by the longest way to an output, from right to left
        std::unordered_map<uint32_t, int32_t> depths;
        for (const auto& node : graph.nodes)
            depths[node.id] = 0;
        for (size_t i = 0; i < graph.nodes.size(); i++) {
            auto changed = false;
            for (const auto& link : graph.links) {
                auto depth = depths[link.toNode] + 1;
                if (depth > depths[link.fromNode]) {
                    depths[link.fromNode] = depth;
                    changed = true;
                }
            }
            if (!changed)
                break;
        }

        std::map<int32_t, std::vector<Node*>> columns;
        for (auto& node : graph.nodes) {
            if (!isFrame(node))
                columns[depths[node.id]].push_back(&node);
        }

        const float columnSpacing = nodeWidth + 80.0f;
        const float rowSpacing = 24.0f;

        auto getHeight = [&](const Node& node) {
            return GetNodeSize(node).y;
            };

        std::unordered_map<uint32_t, float> centers;

        // Nodes go where their links want them. Nodes in each other's way push each other apart evenly, pushing them
        // only downwards would stack long chains into a staircase
        auto placeColumn = [&](int32_t depth, std::vector<Node*>& nodes, const std::unordered_map<uint32_t, float>& wanted) {
            std::stable_sort(nodes.begin(), nodes.end(), [&](const Node* a, const Node* b) { return wanted.at(a->id) < wanted.at(b->id); });

            struct Block {
                std::vector<Node*> nodes;
                float offsetSum = 0.0f;
                float height = 0.0f;
                float top = 0.0f;
            };
            std::vector<Block> blocks;
            for (auto node : nodes) {
                auto height = getHeight(*node);
                Block block = { .nodes = { node }, .offsetSum = wanted.at(node->id) - 0.5f * height, .height = height };
                block.top = block.offsetSum;
                blocks.push_back(block);

                while (blocks.size() > 1) {
                    auto& upper = blocks[blocks.size() - 2];
                    auto& lower = blocks.back();
                    if (upper.top + upper.height + rowSpacing <= lower.top)
                        break;

                    auto shift = upper.height + rowSpacing;
                    upper.offsetSum += lower.offsetSum - shift * float(lower.nodes.size());
                    upper.nodes.insert(upper.nodes.end(), lower.nodes.begin(), lower.nodes.end());
                    upper.height += rowSpacing + lower.height;
                    upper.top = upper.offsetSum / float(upper.nodes.size());
                    blocks.pop_back();
                }
            }

            for (const auto& block : blocks) {
                auto top = block.top;
                for (auto node : block.nodes) {
                    auto height = getHeight(*node);
                    node->position = vec2(-float(depth) * columnSpacing, top);
                    centers[node->id] = top + 0.5f * height;
                    top += height + rowSpacing;
                }
            }
            };

        // Follow the outputs first, then a few rounds from both sides settle the rest
        auto sweep = [&](bool towardsInputs) {
            auto apply = [&](int32_t depth, std::vector<Node*>& nodes) {
                std::unordered_map<uint32_t, float> wanted;
                float nextFree = 0.0f;
                for (auto node : nodes) {
                    float sum = 0.0f, count = 0.0f;
                    for (const auto& link : graph.links) {
                        auto isTarget = towardsInputs ? link.toNode == node->id : link.fromNode == node->id;
                        auto other = towardsInputs ? link.fromNode : link.toNode;
                        if (isTarget && centers.contains(other)) {
                            sum += centers[other];
                            count += 1.0f;
                        }
                    }
                    if (count > 0.0f) {
                        wanted[node->id] = sum / count;
                    }
                    else {
                        // Nothing to follow, the node stays where it is or below the others
                        wanted[node->id] = centers.contains(node->id) ? centers[node->id] : nextFree;
                        nextFree += getHeight(*node) + rowSpacing;
                    }
                }
                placeColumn(depth, nodes, wanted);
                };

            if (towardsInputs) {
                for (auto iter = columns.rbegin(); iter != columns.rend(); iter++)
                    apply(iter->first, iter->second);
            }
            else {
                for (auto iter = columns.begin(); iter != columns.end(); iter++)
                    apply(iter->first, iter->second);
            }
            };

        sweep(false);
        for (int32_t round = 0; round < 3; round++) {
            sweep(true);
            sweep(false);
        }

        for (auto& [frame, nodes] : frames) {
            vec2 boundsMin(FLT_MAX), boundsMax(-FLT_MAX);
            for (auto id : nodes) {
                auto node = graph.GetNode(id);
                if (!node)
                    continue;
                boundsMin = glm::min(boundsMin, node->position);
                boundsMax = glm::max(boundsMax, node->position + GetNodeSize(*node));
            }
            if (nodes.empty())
                continue;

            const float padding = 24.0f;
            frame->position = boundsMin - vec2(padding, padding + frameHeaderHeight);
            frame->size = boundsMax - boundsMin + vec2(2.0f * padding, 2.0f * padding + frameHeaderHeight);
        }

        modified = true;
        frameRequested = true;

    }

    void MaterialGraphPanel::Frame(const Graph& graph) {

        if (graph.nodes.empty() || canvasSize.x <= 0.0f || canvasSize.y <= 0.0f)
            return;

        vec2 boundsMin(FLT_MAX), boundsMax(-FLT_MAX);
        for (const auto& node : graph.nodes) {
            if (frameOnlySelection && !selection.contains(node.id))
                continue;
            boundsMin = glm::min(boundsMin, node.position);
            boundsMax = glm::max(boundsMax, node.position + GetNodeSize(node));
        }
        frameOnlySelection = false;

        const float margin = 40.0f;
        auto extent = glm::max(boundsMax - boundsMin, vec2(1.0f));
        zoom = std::clamp(std::min((canvasSize.x - 2.0f * margin) / extent.x, (canvasSize.y - 2.0f * margin) / extent.y), minZoom, 1.25f);
        pan = vec2(canvasSize.x, canvasSize.y) * 0.5f / zoom - (boundsMin + boundsMax) * 0.5f;

    }

    ImVec4 MaterialGraphPanel::GetValueColor(ValueType type) {

        return Theme::GraphValue(int32_t(type));

    }

    const char* MaterialGraphPanel::GetValueName(ValueType type) {

        switch (type) {
        case ValueType::Float: return "Number";
        case ValueType::Vec2: return "Vector 2";
        case ValueType::Vec3: return "Vector 3";
        case ValueType::Vec4: return "Vector 4";
        case ValueType::Texture: return "Texture";
        default: return "Any";
        }

    }

}
