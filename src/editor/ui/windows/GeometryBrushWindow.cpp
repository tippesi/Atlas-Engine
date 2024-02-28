#include "GeometryBrushWindow.h"

#include "common/RandomHelper.h"
#include "../../Notifications.h"

namespace Atlas::Editor::UI {

    void GeometryBrushWindow::Render(const Ref<SceneWindow>& activeSceneWindow) {

        if (!Begin())
            return;

        std::string buttonName = "Drop entity with transform component here";

        if (brushEntity.IsValid() && brushEntity.HasComponent<NameComponent>())
            buttonName = brushEntity.GetComponent<NameComponent>().name;
        ImGui::Button(buttonName.c_str(), {-FLT_MIN, 0});
        
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

        buttonName = "Drop hierarchy entity here";

        if (parentEntity.IsValid() && parentEntity.HasComponent<NameComponent>())
            buttonName = parentEntity.GetComponent<NameComponent>().name;
        ImGui::Button(buttonName.c_str(), {-FLT_MIN, 0});

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

        if (brushEntity.IsValid() && parentEntity.IsValid() && activeSceneWindow != nullptr
            && activeSceneWindow->scene.Get().get() == brushEntity.GetScene() 
            && activeSceneWindow->scene.Get().get() == parentEntity.GetScene()) {
            RenderBrushSettings();

            if (brushEnabled) {
                activeSceneWindow->lockSelection = true;

                auto dropTarget = CalculateDropTarget(activeSceneWindow);            

                if (dropTarget.valid)
                    ApplyBrush(activeSceneWindow, dropTarget);
            }
        }

        End();

    }

    void GeometryBrushWindow::RenderBrushSettings() {

        auto scene = brushEntity.GetScene();

        ImGui::Separator();

        ImGui::Text("Brushing into scene %s", scene->name.c_str());

        ImGui::Checkbox("Enable", &brushEnabled);

        ImGui::DragFloat("Brush radius", &brushRadius, 0.5f, 0.5f, 1000.0f);
        ImGui::DragFloat("Brush ray length", &brushRayLength, 0.5f, 0.5f, 1000.0f);
        ImGui::DragInt("Brush drop rate (per second)", &brushDropRate, 1.0f, 1, 100000);

        ImGui::Checkbox("Align to surface", &brushAlignToSurface);

        ImGui::Separator();

        ImGui::Text("Drop randomization");


    }

    GeometryBrushWindow::DropTarget GeometryBrushWindow::CalculateDropTarget(const Ref<SceneWindow>& activeSceneWindow) {

        DropTarget target;

        auto scene = brushEntity.GetScene();

        // We need to have a match to insert the entity again
        if (activeSceneWindow->scene.Get().get() != scene)
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

        if (io.MouseDown[ImGuiMouseButton_Right] && inViewport) {

            auto nearPoint = viewport->Unproject(vec3(mousePos, 0.0f), camera);
            auto farPoint = viewport->Unproject(vec3(mousePos, 1.0f), camera);

            Atlas::Volume::Ray ray(camera.GetLocation(), glm::normalize(farPoint - nearPoint));

            auto rayCastResult = scene->CastRay(ray);
            if (rayCastResult.valid && rayCastResult.IsNormalValid()) {
                target.center = ray.Get(rayCastResult.hitDistance);
                target.normal = rayCastResult.normal;

                target.tangent = vec3(1.0f, 0.0f, 0.0f);
                target.bitangent = normalize(cross(target.tangent, target.normal));
                target.valid = true;
            }
        }

        return target;

    }

    void GeometryBrushWindow::ApplyBrush(const Ref<SceneWindow>& activeSceneWindow, const DropTarget& dropTarget) {

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

        if (!parentEntity.HasComponent<HierarchyComponent>())
            parentEntity.AddComponent<HierarchyComponent>();

        auto decomposition = brushEntity.GetComponent<TransformComponent>().Decompose();

        for (int32_t i = 0; i < dropsPerFrame; i++) {
            auto deg = Common::Random::SampleFastUniformFloat() * 2.0f * 3.14159f;
            auto dist = Common::Random::SampleFastUniformFloat() + Common::Random::SampleFastUniformFloat();
            dist = dist > 1.0f ? 2.0f - dist : dist;

            auto pos = vec3(cosf(deg), 0.0f, sinf(deg)) * dist * brushRadius;

            pos += dropTarget.center;

            Atlas::Volume::Ray ray(pos + dropTarget.normal * brushRayLength, -dropTarget.normal);
            auto rayCastResult = scene->CastRay(ray);

            if (!rayCastResult.valid || !rayCastResult.IsNormalValid() || rayCastResult.hitDistance > 2.0f * brushRayLength) {
                return;
            }                

            mat4 rot { 1.0f };
            if (brushAlignToSurface) {
                vec3 N = rayCastResult.normal;
                vec3 up = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
                vec3 tangent = normalize(cross(up, N));
                vec3 bitangent = cross(N, tangent);

                rot = mat4(mat3(tangent, N, bitangent));
            }

            decomposition.translation = ray.Get(rayCastResult.hitDistance);

            auto transform = decomposition.Compose() * rot;
            auto entity = scene->DuplicateEntity(brushEntity);

            auto& transformComponent = entity.GetComponent<TransformComponent>();
            transformComponent.Set(transform);

            parentEntity.GetComponent<HierarchyComponent>().AddChild(entity);

            // Ray cast result is in global space, so need to bring transform to valid local one
            transformComponent.globalMatrix = transform;
            transformComponent.ReconstructLocalMatrix(parentEntity);
        }

    }

}