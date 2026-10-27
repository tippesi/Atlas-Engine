#include "GeometryBrushWindow.h"

#include "common/RandomHelper.h"
#include "../../Notifications.h"
#include "../../Theme.h"
#include "../Widgets.h"

#include <glm/gtx/quaternion.hpp>

namespace Atlas::Editor::UI {

    void GeometryBrushWindow::Render(const Ref<SceneWindow>& activeSceneWindow) {

        if (!Begin())
            return;

        RenderBrushSettings(activeSceneWindow);

        // There might be no scene window at all, e.g. when every scene was closed
        if (brushEnabled && activeSceneWindow) {
            activeSceneWindow->lockSelection = true;

            auto dropTarget = CalculateBrushTarget(activeSceneWindow);

            if (dropTarget.valid) {
                switch (brushType) {
                case BrushType::Create:
                    ApplyCreateBrush(activeSceneWindow, dropTarget);
                    break;
                case BrushType::Delete:
                    ApplyDeleteBrush(activeSceneWindow, dropTarget);
                    break;
                default: break;
                }

            }
        }

        End();

    }

    void GeometryBrushWindow::RenderBrushSettings(const Ref<SceneWindow>& activeSceneWindow) { 
        
        if (!activeSceneWindow) {
            Widgets::Callout("Open a scene to use the geometry brush.", Theme::Warning());
        }
        else {
            auto scene = activeSceneWindow->scene;
            ImGui::TextDisabled("Target scene");
            ImGui::SameLine();
            ImGui::TextUnformatted(scene->name.c_str());
        }

        ImGui::Checkbox("Enable", &brushEnabled);

        const char* brushTypes[] = { "Create", "Delete" };
        int32_t brushSelection = static_cast<int32_t>(brushType);
        Widgets::SegmentedControl("Type", brushSelection, brushTypes, IM_ARRAYSIZE(brushTypes));
        brushType = static_cast<BrushType>(brushSelection);

        ImGui::DragFloat("Brush radius", &brushRadius, 0.5f, 0.5f, 1000.0f);
        ImGui::Checkbox("Only query terrain", &brushOnlyQueryTerrain);

        // The delete brush has no further settings
        if (brushType == BrushType::Create)
            RenderCreateBrushSettings();

    }

    void GeometryBrushWindow::RenderCreateBrushSettings() {

        ImGui::SeparatorText("Entities");

        bool hasBrushEntity = brushEntity.IsValid() && brushEntity.HasComponent<NameComponent>();
        std::string buttonName = hasBrushEntity ? brushEntity.GetComponent<NameComponent>().name :
            "Drop entity with transform component here";
        Widgets::DropSlot((buttonName + "##Brush entity").c_str(), !hasBrushEntity);
        ImGui::SetItemTooltip("Entity that is placed by the brush");

        Scene::Entity dropEntity;
        if (ImGui::BeginDragDropTarget()) {
            auto dropPayload = ImGui::GetDragDropPayload();
            if (dropPayload->IsDataType(typeid(Scene::Entity).name())) {
                std::memcpy(&dropEntity, dropPayload->Data, dropPayload->DataSize);
                bool validEntity = dropEntity.HasComponent<TransformComponent>();

                if (validEntity && ImGui::AcceptDragDropPayload(typeid(Scene::Entity).name())) {
                    brushEntity = dropEntity;
                }
            }

            ImGui::EndDragDropTarget();
        }

        bool hasParentEntity = parentEntity.IsValid() && parentEntity.HasComponent<NameComponent>();
        buttonName = hasParentEntity ? parentEntity.GetComponent<NameComponent>().name : "Drop parent entity here";
        Widgets::DropSlot((buttonName + "##Parent entity").c_str(), !hasParentEntity);
        ImGui::SetItemTooltip("New entities are added as children of this entity");

        if (ImGui::BeginDragDropTarget()) {
            auto dropPayload = ImGui::GetDragDropPayload();
            if (dropPayload->IsDataType(typeid(Scene::Entity).name())) {
                std::memcpy(&dropEntity, dropPayload->Data, dropPayload->DataSize);

                if (ImGui::AcceptDragDropPayload(typeid(Scene::Entity).name())) {
                    parentEntity = dropEntity;
                }
            }

            ImGui::EndDragDropTarget();
        }

        ImGui::SeparatorText("Placement");

        ImGui::DragFloat("Brush ray length", &brushRayLength, 0.5f, 0.5f, 1000.0f);
        ImGui::DragInt("Brush drop rate (per second)", &brushDropRate, 1.0f, 1, 100000);

        ImGui::Checkbox("Align to surface", &brushAlignToSurface);

    }

    void GeometryBrushWindow::RenderDeleteBrushSettings() {



    }

    GeometryBrushWindow::BrushTarget GeometryBrushWindow::CalculateBrushTarget(const Ref<SceneWindow>& activeSceneWindow) {

        BrushTarget target;

        auto& scene = activeSceneWindow->scene;

        // We need to have a match to insert the entity again
        if (brushEntity.IsValid() && scene.Get().get() != brushEntity.GetScene() && brushType == BrushType::Create)
            return target;

        if (!activeSceneWindow->cameraEntity.IsValid())
            return target;

        auto& viewport = activeSceneWindow->viewportPanel.viewport;
        auto& camera = activeSceneWindow->cameraEntity.GetComponent<CameraComponent>();

        const auto& io = ImGui::GetIO();

        auto windowPos = ImGui::GetWindowPos();
        auto mousePos = vec2(io.MousePos.x, io.MousePos.y);

        bool inViewport = mousePos.x > float(viewport->x)
            && mousePos.y > float(viewport->y)
            && mousePos.x < float(viewport->x + viewport->width)
            && mousePos.y < float(viewport->y + viewport->height);

        if (inViewport) {

            auto nearPoint = viewport->Unproject(vec3(mousePos, 0.0f), camera);
            auto farPoint = viewport->Unproject(vec3(mousePos, 1.0f), camera);

            Atlas::Volume::Ray ray(camera.GetLocation(), glm::normalize(farPoint - nearPoint));

            Scene::SceneQueryComponents queryComponent = Scene::SceneQueryComponentBits::AllComponentsBit;
            if (brushOnlyQueryTerrain)
                queryComponent = Scene::SceneQueryComponentBits::TerrainComponentBit;

            auto rayCastResult = scene->CastRay(ray, queryComponent);

            const auto& primitiveBatchWrapper = activeSceneWindow->viewportPanel.primitiveBatchWrapper;
            vec3 hitPosition = ray.Get(rayCastResult.hitDistance);

            if (rayCastResult.valid && rayCastResult.IsNormalValid()) {
                if (io.MouseDown[ImGuiMouseButton_Right]) {
                    target.center = ray.Get(rayCastResult.hitDistance);
                    target.normal = rayCastResult.normal;

                    target.tangent = vec3(1.0f, 0.0f, 0.0f);
                    target.bitangent = normalize(cross(target.tangent, target.normal));
                    target.valid = true;
                }

                activeSceneWindow->viewportPanel.primitiveBatchWrapper.GetBatch(true)->AddLineSphere(hitPosition,
                    brushRadius, vec3(1.0f));
            }
            else if (rayCastResult.valid && !rayCastResult.IsNormalValid()) {
                activeSceneWindow->viewportPanel.primitiveBatchWrapper.GetBatch(true)->AddLineSphere(hitPosition,
                    brushRadius, vec3(1.0f, 0.0f, 0.0f));
            }
        }

        return target;

    }

    void GeometryBrushWindow::ApplyCreateBrush(const Ref<SceneWindow>& activeSceneWindow, const BrushTarget& dropTarget) {

        if (!brushEntity.IsValid() || !parentEntity.IsValid() || activeSceneWindow == nullptr
            || activeSceneWindow->scene.Get().get() != brushEntity.GetScene()
            || activeSceneWindow->scene.Get().get() != parentEntity.GetScene()) {
            return;
        }

        // This whole approach isn't very accurate but should still be good enough for most cases
        int32_t dropsPerFrame = int32_t(glm::round(brushDropRate * Clock::GetDelta()));

        if (dropsPerFrame == 0) {
            if (1.0f / float(brushDropRate) < Clock::Get() - brushLastApplied)
                dropsPerFrame = 1;
        }

        if (!dropsPerFrame)
            return;

        brushLastApplied = Clock::Get();

        auto scene = brushEntity.GetScene();

        auto decomposition = brushEntity.GetComponent<TransformComponent>().Decompose();

        Scene::SceneQueryComponents queryComponent = Scene::SceneQueryComponentBits::AllComponentsBit;
        if (brushOnlyQueryTerrain)
            queryComponent = Scene::SceneQueryComponentBits::TerrainComponentBit;

        for (int32_t i = 0; i < dropsPerFrame; i++) {
            auto deg = Common::Random::SampleFastUniformFloat() * 2.0f * 3.14159f;
            auto dist = Common::Random::SampleFastUniformFloat() + Common::Random::SampleFastUniformFloat();
            dist = dist > 1.0f ? 2.0f - dist : dist;

            auto pos = vec3(cosf(deg), 0.0f, sinf(deg)) * dist * brushRadius;

            pos += dropTarget.center;

            Atlas::Volume::Ray ray(pos + dropTarget.normal * brushRayLength, -dropTarget.normal);
            auto rayCastResult = scene->CastRay(ray, queryComponent);

            if (!rayCastResult.valid || !rayCastResult.IsNormalValid() || rayCastResult.hitDistance > 2.0f * brushRayLength) {
                continue;
            }

            mat4 rot{ 1.0f };
            if (brushAlignToSurface) {
                vec3 N = rayCastResult.normal;

                glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
                glm::quat rotation = glm::rotation(up, glm::normalize(N));
                rot = glm::toMat4(rotation);
            }

            auto translation = ray.Get(rayCastResult.hitDistance);

            mat4 matrix(1.0f);
            matrix = glm::translate(matrix, translation);
            matrix *= rot;
            matrix = glm::scale(matrix, decomposition.scale);

            auto entity = scene->DuplicateEntity(brushEntity);
            entity.SetParent(parentEntity);

            // Ray cast result is in global space
            entity.GetComponent<TransformComponent>().SetGlobal(matrix, parentEntity);
        }

    }

    void GeometryBrushWindow::ApplyDeleteBrush(const Ref<SceneWindow>& activeSceneWindow, const BrushTarget& dropTarget) {

        if (activeSceneWindow == nullptr) {
            return;
        }

        auto& scene = activeSceneWindow->scene;

        Volume::AABB aabb(vec3(-1.0f), vec3(1.0f));
        aabb = aabb.Scale(brushRadius).Translate(dropTarget.center);

        auto entities = scene->QueryAABB(aabb);
        for (auto entity : entities) {
            // Shouldn't really happen, but check anyway
            if (!entity.HasComponent<TransformComponent>())
                continue;
            auto& transform = entity.GetComponent<TransformComponent>();
            auto position = vec3(transform.globalMatrix[3]);
            // Check distance such that brush is circular
            if (glm::distance(position, dropTarget.center) > brushRadius)
                continue;

            scene->DestroyEntity(entity, false);
        }

    }

}