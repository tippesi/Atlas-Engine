#include "MeshEditorWindow.h"

#include "../../Notifications.h"
#include "../../Theme.h"
#include "../../tools/ResourcePayloadHelper.h"
#include "../Widgets.h"

#include "loader/MeshLoader.h"
#include "loader/MaterialLoader.h"
#include "mesh/MeshSerializer.h"
#include "physics/ShapesManager.h"

#include <imgui_stdlib.h>

namespace Atlas::Editor::UI {

    MeshEditorWindow::MeshEditorWindow(ResourceHandle<Mesh::Mesh> mesh, bool show) :
        AssetEditorWindow(mesh.IsValid() ? mesh.GetResource()->GetFileName() : "No mesh", show), mesh(mesh) {}

    void MeshEditorWindow::Save() {

        if (!mesh.IsLoaded())
            return;

        Loader::MeshLoader::SaveMesh(mesh.Get(), mesh.GetResource()->path, true);
        Notifications::Push({ .message = "Saved mesh " + mesh.GetResource()->GetFileName() });

    }

    json MeshEditorWindow::SaveState() const {

        if (!mesh.IsLoaded())
            return json();

        json j = {
            {"mobility", int32_t(mesh->mobility)},
            {"invertUVs", mesh->invertUVs},
            {"cullBackFaces", mesh->cullBackFaces},
            {"rayTrace", mesh->rayTrace},
            {"vegetation", mesh->vegetation},
            {"castShadow", mesh->castShadow},
            {"allowedShadowCascades", mesh->allowedShadowCascades},
            {"distanceCulling", mesh->distanceCulling},
            {"shadowDistanceCulling", mesh->shadowDistanceCulling},
            {"rayTraceDistanceCulling", mesh->rayTraceDistanceCulling},
            {"minScreenSize", mesh->minScreenSize},
            {"animationLodDistance", mesh->animationLodDistance},
            {"animationMaxInterval", mesh->animationMaxInterval},
            {"impostorDistance", mesh->impostorDistance},
            {"impostorShadowDistance", mesh->impostorShadowDistance},
            {"windNoiseTextureLod", mesh->windNoiseTextureLod},
            {"windBendScale", mesh->windBendScale},
            {"windWiggleScale", mesh->windWiggleScale},
        };

        auto materials = json::array();
        for (const auto& material : mesh->data.materials) {
            json materialJson;
            if (material.IsLoaded())
                materialJson["material"] = *material.Get();
            if (material.IsValid() && !material.IsGenerated())
                materialJson["path"] = material.GetResource()->path;
            materials.push_back(materialJson);
        }
        j["materials"] = materials;

        return j;

    }

    void MeshEditorWindow::RestoreState(const json& j) {

        if (!mesh.IsLoaded() || j.is_null())
            return;

        mesh->mobility = Mesh::MeshMobility(j["mobility"].get<int32_t>());
        j.at("invertUVs").get_to(mesh->invertUVs);
        j.at("cullBackFaces").get_to(mesh->cullBackFaces);
        j.at("rayTrace").get_to(mesh->rayTrace);
        j.at("vegetation").get_to(mesh->vegetation);
        j.at("castShadow").get_to(mesh->castShadow);
        j.at("allowedShadowCascades").get_to(mesh->allowedShadowCascades);
        j.at("distanceCulling").get_to(mesh->distanceCulling);
        j.at("shadowDistanceCulling").get_to(mesh->shadowDistanceCulling);
        j.at("rayTraceDistanceCulling").get_to(mesh->rayTraceDistanceCulling);
        j.at("minScreenSize").get_to(mesh->minScreenSize);
        j.at("animationLodDistance").get_to(mesh->animationLodDistance);
        j.at("animationMaxInterval").get_to(mesh->animationMaxInterval);
        j.at("impostorDistance").get_to(mesh->impostorDistance);
        j.at("impostorShadowDistance").get_to(mesh->impostorShadowDistance);
        j.at("windNoiseTextureLod").get_to(mesh->windNoiseTextureLod);
        j.at("windBendScale").get_to(mesh->windBendScale);
        j.at("windWiggleScale").get_to(mesh->windWiggleScale);

        const auto& materials = j["materials"];
        for (size_t i = 0; i < std::min(materials.size(), mesh->data.materials.size()); i++) {
            const auto& materialJson = materials[i];
            auto& material = mesh->data.materials[i];

            // Another material was selected for the slot. Embedded materials only exist in the mesh,
            // a new one gets their values, such that a material file used by the slot isn't overwritten
            auto isFileMaterial = materialJson.contains("path");
            auto slotChanged = isFileMaterial ? !material.IsValid() || material.IsGenerated() ||
                material.GetResource()->path != materialJson["path"].get<std::string>() :
                material.IsValid() && !material.IsGenerated();
            if (slotChanged) {
                if (isFileMaterial)
                    material = ResourceManager<Material>::GetOrLoadResourceWithLoader(materialJson["path"],
                        ResourceOrigin::User, Loader::MaterialLoader::LoadMaterial, false);
                else
                    material = ResourceHandle<Material>(CreateRef<Material>());
                for (auto& subData : mesh->data.subData)
                    if (subData.materialIdx == int32_t(i))
                        subData.material = material;
            }

            if (materialJson.contains("material") && material.IsLoaded())
                materialJson["material"].get_to(*material.Get());
        }

        mesh->UpdatePipelines();

    }

    std::string MeshEditorWindow::GetAssetPath() const {

        return mesh.IsValid() ? mesh.GetResource()->path : "";

    }

    bool MeshEditorWindow::Play(const ResourceHandle<Animation::Animation>& animation) {

        if (!scene || !IsSkinned())
            return false;

        auto& animationComponent = entity.HasComponent<AnimationComponent>() ? entity.GetComponent<AnimationComponent>() :
            entity.AddComponent<AnimationComponent>();
        animationComponent.Play(animation);

        return true;

    }

    void MeshEditorWindow::StatisticsRow(const char* label, const char* format, ...) {

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(Theme::TextMuted(), "%s", label);
        ImGui::TableNextColumn();

        va_list args;
        va_start(args, format);
        ImGui::TextV(format, args);
        va_end(args);

    }

    bool MeshEditorWindow::IsReady() const {

        return mesh.IsLoaded();

    }

    Scene::Entity MeshEditorWindow::CreatePreviewEntity() {

        // Not static, the mesh can be animated
        auto meshEntity = scene->CreatePrefab<Scene::Prefabs::MeshInstance>(mesh, false);
        if (IsSkinned())
            meshEntity.AddComponent<AnimationComponent>();

        return meshEntity;

    }

    Volume::AABB MeshEditorWindow::GetBounds() const {

        return mesh->data.aabb;

    }

    void MeshEditorWindow::RenderProperties() {

        RenderHeading("Saves the mesh and its materials (Ctrl+S)");

        const auto& data = mesh->data;
        if (ImGui::BeginTable("Statistics", 2, ImGuiTableFlags_SizingStretchProp)) {
            auto size = data.aabb.max - data.aabb.min;
            StatisticsRow("Vertices", "%d", data.GetVertexCount());
            StatisticsRow("Triangles", "%d", data.GetIndexCount() / 3);
            StatisticsRow("Sub meshes", "%d", int32_t(data.subData.size()));
            StatisticsRow("Materials", "%d", int32_t(data.materials.size()));
            StatisticsRow("Size", "%.2f x %.2f x %.2f m", size.x, size.y, size.z);
            if (data.IsSkinned())
                StatisticsRow("Joints", "%d", data.skeleton.GetJointCount());
            ImGui::EndTable();
            ImGui::Spacing();
        }

        if (Widgets::SectionHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& meshComponent = entity.GetComponent<MeshComponent>();
            auto wasSkinned = IsSkinned();
            if (meshComponentPanel.Render(scene, entity, meshComponent) && meshComponent.mesh.IsValid()) {
                // Another mesh was selected, edit that one instead
                mesh = meshComponent.mesh;
                selectedJoint = -1;
                if (IsSkinned() && !entity.HasComponent<AnimationComponent>())
                    entity.AddComponent<AnimationComponent>();
                if (IsSkinned() != wasSkinned)
                    resetDockingLayout = true;
            }
        }

        if (entity.HasComponent<AnimationComponent>() && Widgets::SectionHeader("Animation", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& animationComponent = entity.GetComponent<AnimationComponent>();
            animationComponentPanel.Render(scene, entity, animationComponent);
        }

        // The clips the mesh references, a click plays one
        if (!mesh->data.animations.empty() && Widgets::SectionHeader("Animations", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto animationComponent = entity.TryGetComponent<AnimationComponent>();
            for (const auto& animation : mesh->data.animations) {
                if (!animation.IsLoaded())
                    continue;
                auto playing = animationComponent && animationComponent->animation == animation;
                if (ImGui::Selectable(animation->name.c_str(), playing))
                    Play(animation);
            }
        }

        if (Widgets::SectionHeader("Physics"))
            RenderPhysics();

    }

    void MeshEditorWindow::RenderPanels() {

        if (!IsSkinned())
            return;

        RenderSkeleton();

        ImGui::Begin(timelinePanelWindow.GetNameID());
        timelinePanelWindow.isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
        if (auto animationComponent = entity.TryGetComponent<AnimationComponent>())
            timelinePanel.Render(*animationComponent, mesh->data.skeleton, selectedJoint);
        ImGui::End();

    }

    bool MeshEditorWindow::HasSidePanel() const {

        return IsSkinned();

    }

    bool MeshEditorWindow::HasBottomPanel() const {

        return IsSkinned();

    }

    void MeshEditorWindow::DockPanels(ImGuiID side, ImGuiID bottom) {

        if (side)
            ImGui::DockBuilderDockWindow(skeletonPanel.GetNameID(), side);
        if (bottom)
            ImGui::DockBuilderDockWindow(timelinePanelWindow.GetNameID(), bottom);

    }

    bool MeshEditorWindow::IsPanelFocused() const {

        return skeletonPanel.isFocused || timelinePanelWindow.isFocused;

    }

    void MeshEditorWindow::RenderViewportSettings() {

        ImGui::Checkbox("Show physics", &showPhysics);
        ImGui::BeginDisabled(!IsSkinned());
        ImGui::Checkbox("Show skeleton", &showSkeleton);
        ImGui::EndDisabled();

    }

    void MeshEditorWindow::AcceptViewportDrop() {

        auto animation = ResourcePayloadHelper::AcceptDropResource<Animation::Animation>();
        if (animation.IsValid() && !Play(animation))
            Notifications::Push({ .message = "The mesh has no skeleton to play the animation on", .color = vec3(1.0f, 0.0f, 0.0f) });

    }

    void MeshEditorWindow::RenderPhysics() {

        if (!entity.HasComponent<RigidBodyComponent>()) {
            ImGui::TextWrapped("Bodies are part of entities. Add one to see how a shape fits the mesh");
            if (ImGui::Button("Add body", ImVec2(-FLT_MIN, 0.0f))) {
                auto shape = Physics::ShapesManager::CreateShape(Physics::BoundingBoxShapeSettings { .scale = vec3(1.0f) });
                entity.AddComponent<RigidBodyComponent>(Physics::BodyCreationSettings {
                    .objectLayer = Physics::Layers::Movable, .shape = shape });
                showPhysics = true;
            }
            return;
        }

        // Same as in the entity properties, changed settings recreate the body
        auto comp = entity.GetComponent<RigidBodyComponent>();
        rigidBodyComponentPanel.Render(scene, entity, comp);
        if (comp.creationSettings) {
            entity.RemoveComponent<RigidBodyComponent>();
            entity.AddComponent<RigidBodyComponent>(*comp.creationSettings);
        }

        if (ImGui::Button("Remove body", ImVec2(-FLT_MIN, 0.0f)))
            entity.RemoveComponent<RigidBodyComponent>();

    }

    void MeshEditorWindow::RenderSkeleton() {

        ImGui::Begin(skeletonPanel.GetNameID());

        skeletonPanel.isFocused = ImGui::IsWindowFocused();

        const auto& joints = mesh->data.skeleton.joints;
        const auto& style = ImGui::GetStyle();

        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##Search", "Search joints...", &jointSearch);

        auto toLower = [](std::string text) {
            std::transform(text.begin(), text.end(), text.begin(), ::tolower);
            return text;
            };
        auto search = toLower(jointSearch);

        std::vector<std::vector<int32_t>> children(joints.size());
        for (int32_t i = 0; i < int32_t(joints.size()); i++)
            if (joints[i].parentIdx >= 0)
                children[joints[i].parentIdx].push_back(i);

        std::function<void(int32_t, int32_t)> renderJoint = [&](int32_t jointIdx, int32_t depth) {
            auto flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick |
                ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;
            // While searching the matches are listed flat
            bool flat = !search.empty();
            if (children[jointIdx].empty() || flat)
                flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            if (jointIdx == selectedJoint)
                flags |= ImGuiTreeNodeFlags_Selected;

            if (!flat || toLower(joints[jointIdx].name).find(search) != std::string::npos) {
                if (!flat && depth < 3)
                    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
                bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>(intptr_t(jointIdx)), flags, "%s", joints[jointIdx].name.c_str());
                if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
                    selectedJoint = jointIdx == selectedJoint ? -1 : jointIdx;

                if (!flat && open && !(flags & ImGuiTreeNodeFlags_Leaf)) {
                    for (auto child : children[jointIdx])
                        renderJoint(child, depth + 1);
                    ImGui::TreePop();
                }
            }

            if (flat)
                for (auto child : children[jointIdx])
                    renderJoint(child, depth + 1);
            };

        auto detailsHeight = selectedJoint >= 0 ? 5.0f * ImGui::GetTextLineHeightWithSpacing() : 0.0f;
        ImGui::BeginChild("Joints", ImVec2(0.0f, -detailsHeight));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x, 2.0f));
        for (int32_t i = 0; i < int32_t(joints.size()); i++)
            if (joints[i].parentIdx < 0)
                renderJoint(i, 0);
        ImGui::PopStyleVar();
        ImGui::EndChild();

        if (selectedJoint >= 0 && selectedJoint < int32_t(joints.size())) {
            const auto& joint = joints[selectedJoint];
            ImGui::SeparatorText(joint.name.c_str());
            ImGui::TextColored(Theme::TextMuted(), "Parent");
            ImGui::SameLine();
            ImGui::TextUnformatted(joint.parentIdx >= 0 ? joints[joint.parentIdx].name.c_str() : "None");
            if (!joint.hasVertices)
                ImGui::TextColored(Theme::TextMuted(), "Doesn't influence any vertices");

            // Keys of the joint in the current animation
            auto animationComponent = entity.TryGetComponent<AnimationComponent>();
            if (animationComponent && animationComponent->animation.IsLoaded()) {
                const auto& channels = animationComponent->animation->channels;
                auto channel = std::find_if(channels.begin(), channels.end(),
                    [&](const auto& channel) { return channel.jointName == joint.name; });
                if (channel != channels.end())
                    ImGui::Text("%d position, %d rotation, %d scale keys", int32_t(channel->positionTimes.size()),
                        int32_t(channel->rotationTimes.size()), int32_t(channel->scaleTimes.size()));
                else
                    ImGui::TextColored(Theme::TextMuted(), "Not animated by %s", animationComponent->animation->name.c_str());
            }
        }

        ImGui::End();

    }

    void MeshEditorWindow::RenderOverlays() {

        auto& wrapper = viewportPanel.primitiveBatchWrapper;

        if (showPhysics && scene->physicsWorld) {
            physicsDebugDrawSettings.center = cameraEntity.GetComponent<CameraComponent>().GetLocation();
            scene->physicsWorld->DebugDraw(wrapper.GetBatch(false).get(), physicsDebugDrawSettings);
        }

        if (!showSkeleton || !IsSkinned())
            return;

        // The pose of the component includes its layers, inverse kinematics and joint offsets, before its first
        // update the joints are shown in their rest pose
        const auto& skeleton = mesh->data.skeleton;
        auto animationComponent = entity.TryGetComponent<AnimationComponent>();
        auto animated = animationComponent && !animationComponent->jointMatrices.empty();
        if (!animated) {
            skeleton.GetRestPose(restPose);
            skeleton.GetGlobalMatrices(restPose, restMatrices);
        }

        auto globalMatrix = scene->GetGlobalMatrix(entity);
        auto getPosition = [&](int32_t jointIdx) {
            auto jointMatrix = animated ? animationComponent->GetJointMatrix(jointIdx) : restMatrices[jointIdx];
            return vec3(globalMatrix * jointMatrix[3]);
            };

        auto extent = glm::length(mesh->data.aabb.max - mesh->data.aabb.min);
        for (int32_t i = 0; i < skeleton.GetJointCount(); i++) {
            auto position = getPosition(i);
            auto selected = i == selectedJoint;
            auto color = selected ? vec3(1.0f, 0.6f, 0.1f) : vec3(0.2f, 0.8f, 1.0f);

            auto parentIdx = skeleton.joints[i].parentIdx;
            if (parentIdx >= 0)
                wrapper.GetBatch(false)->AddLine(getPosition(parentIdx), position, color, color);
            if (selected)
                wrapper.GetBatch(false)->AddLineSphere(position, 0.02f * extent, color);
        }

    }

    bool MeshEditorWindow::IsSkinned() const {

        return mesh.IsLoaded() && mesh->data.IsSkinned();

    }

}
