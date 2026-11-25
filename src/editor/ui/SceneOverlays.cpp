#include "SceneOverlays.h"
#include "../Singletons.h"
#include "../Theme.h"
#include "common/MathHelper.h"
#include "scene/components/LuaScriptComponent.h"

#include <imgui.h>
#include <glm/gtx/component_wise.hpp>

#include <algorithm>

namespace Atlas::Editor::UI {

    void SceneOverlays::Render(const View& view) {

        auto batch = view.viewportPanel.primitiveBatchWrapper.GetBatch(depthTest);

        auto renderSubset = [&](auto subset, Kinds entityKinds) {
            if (!(shownKinds & entityKinds))
                return;
            for (auto ecsEntity : subset) {
                Scene::Entity entity(ecsEntity, &view.scene.entityManager);
                if (IsInRange(view, entity, GetPosition(view.scene, entity)))
                    RenderEntity(view.scene, *batch, entity, entityKinds, false);
            }
        };

        renderSubset(view.scene.GetSubset<LightComponent>(), LightRangesBit);
        renderSubset(view.scene.GetSubset<CameraComponent>(), CameraFrustumsBit);
        renderSubset(view.scene.GetSubset<AudioComponent>(), AudioRangesBit);
        renderSubset(view.scene.GetSubset<AudioVolumeComponent>(), AudioRangesBit);
        renderSubset(view.scene.GetSubset<FogVolumeComponent>(), FogVolumesBit);
        renderSubset(view.scene.GetSubset<SplineComponent>(), SplinesBit);
        renderSubset(view.scene.GetSubset<TextComponent>(), TextRectanglesBit);

        const auto& volume = view.scene.irradianceVolume;
        if ((shownKinds & IrradianceVolumeBit) && volume && volume->enable) {
            for (int32_t i = 0; i < volume->cascadeCount; i++)
                batch->AddLineAABB(volume->cascades[i].aabb, vec3(0.3f, 1.0f, 0.6f) * (1.0f - 0.25f * float(i)));
        }

        icons.clear();
        if (!(shownKinds & EntityIconsBit))
            return;

        FindBodiesWithoutMesh(view.scene);
        CollectIcons(view);
        MergeIcons(view);
        AddSprites(view);

    }

    void SceneOverlays::RenderSelected(Scene::Scene& scene, ViewportPanel& viewportPanel, Scene::Entity entity) {

        if (entity.IsValid())
            RenderEntity(scene, *viewportPanel.primitiveBatchWrapper.GetBatch(depthTest), entity, ~0u, true);

    }

    void SceneOverlays::RenderEntity(Scene::Scene& scene, Renderer::PrimitiveBatch& batch, Scene::Entity entity,
        Kinds entityKinds, bool selected) const {

        // Ranges of entities which aren't selected stay in the background, only their circle on the ground is drawn
        auto brightness = selected ? 1.0f : 0.55f;
        auto addRange = [&](vec3 center, float radius, vec3 color) {
            if (selected)
                batch.AddLineSphereRings(center, radius, color);
            else
                batch.AddLineCircle(center, radius, color);
        };

        if (selected && entity.HasComponent<MeshComponent>()) {
            const auto& meshComponent = entity.GetComponent<MeshComponent>();
            batch.AddLineAABB(meshComponent.aabb, vec3(1.0f, 1.0f, 0.0f));
        }
        if ((entityKinds & AudioRangesBit) && entity.HasComponent<AudioComponent>()) {
            const auto& audioComponent = entity.GetComponent<AudioComponent>();
            auto position = vec3(scene.GetGlobalMatrix(entity)[3]);

            // After this the audio will be cutoff
            float radius = powf(audioComponent.falloffFactor / audioComponent.cutoff, 1.0f / audioComponent.falloffPower);
            addRange(position, radius, vec3(0.0f, 1.0f, 0.0f) * brightness);
        }
        if ((entityKinds & AudioRangesBit) && entity.HasComponent<AudioVolumeComponent>()) {
            const auto& audioVolumeComponent = entity.GetComponent<AudioVolumeComponent>();
            batch.AddLineAABB(audioVolumeComponent.GetTransformedAABB(), vec3(0.0f, 1.0f, 0.0f) * brightness);
        }
        if ((entityKinds & FogVolumesBit) && entity.HasComponent<FogVolumeComponent>()) {
            auto aabb = Volume::AABB(vec3(-1.0f), vec3(1.0f)).Transform(scene.GetGlobalMatrix(entity));
            batch.AddLineAABB(aabb, vec3(0.6f, 0.8f, 1.0f) * brightness);
        }
        if ((entityKinds & CameraFrustumsBit) && entity.HasComponent<CameraComponent>()) {
            const auto& cameraComponent = entity.GetComponent<CameraComponent>();
            // The far plane of other cameras would cross the whole view, their direction and field of view are enough
            auto frustum = selected ? cameraComponent.frustum : Volume::Frustum(cameraComponent.GetFrustumCorners(
                cameraComponent.nearPlane, std::min(cameraComponent.farPlane, cameraComponent.nearPlane + 2.0f)));
            batch.AddLineFrustum(frustum, vec3(1.0f, 0.0f, 1.0f) * brightness);
        }
        if (entity.HasComponent<LightComponent>()) {
            const auto& lightComponent = entity.GetComponent<LightComponent>();
            const auto& properties = lightComponent.transformedProperties;
            const auto lightColor = GetLightColor(lightComponent) * brightness;

            if (entityKinds & LightRangesBit) {
                if (lightComponent.type == LightType::PointLight) {
                    addRange(properties.point.position, properties.point.radius, lightColor);
                }
                else if (lightComponent.type == LightType::SpotLight) {
                    batch.AddLineCone(properties.spot.position, properties.spot.direction, properties.spot.radius,
                        properties.spot.outerConeAngle, lightColor);
                    batch.AddLineCone(properties.spot.position, properties.spot.direction, properties.spot.radius,
                        properties.spot.innerConeAngle, lightColor * 0.6f);
                }
                else {
                    auto position = vec3(scene.GetGlobalMatrix(entity)[3]);
                    batch.AddLine(position, position + properties.directional.direction * 3.0f, lightColor, lightColor);
                }
            }

            if (selected && lightComponent.shadow) {
                for (const auto& component : lightComponent.shadow->views)
                    batch.AddLineFrustum(Volume::Frustum(component.frustumMatrix), vec3(1.0f, 0.0f, 0.0f));
            }
        }
        if ((entityKinds & TextRectanglesBit) && entity.HasComponent<TextComponent>()) {
            const auto& textComponent = entity.GetComponent<TextComponent>();
            batch.AddLineRectangle(textComponent.GetRectangle(), vec3(0.0f, 0.0f, 1.0f));
        }
        if ((entityKinds & SplinesBit) && entity.HasComponent<SplineComponent>()) {
            const auto& splineComponent = entity.GetComponent<SplineComponent>();

            mat4 globalMatrix = scene.GetGlobalMatrix(entity);

            SplinePoint lastSplinePoint = splineComponent.GetInterpolatedFromIndex(0, 0.0f);
            lastSplinePoint.position = vec3(globalMatrix * vec4(lastSplinePoint.position, 1.0f));

            if (!splineComponent.controlPoints.empty())
                batch.AddLineSphere(lastSplinePoint.position, 0.025f, vec3(1.0f, 0.15f, 0.0f));

            const int32_t linesPerSegment = 20;
            for (int32_t i = 0; i < int32_t(splineComponent.controlPoints.size()) - 1; i++) {
                for (int32_t j = 0; j < linesPerSegment; j++) {
                    auto t = float(j + 1) / float(linesPerSegment);
                    auto splinePoint = splineComponent.GetInterpolatedFromIndex(i, t);
                    splinePoint.position = vec3(globalMatrix * vec4(splinePoint.position, 1.0f));

                    batch.AddLine(lastSplinePoint.position, splinePoint.position,
                        vec3(1.0f, 0.15f, 0.0f), vec3(1.0f, 0.15f, 0.0f));

                    lastSplinePoint = splinePoint;
                }

                batch.AddLineSphere(lastSplinePoint.position, 0.025f, vec3(1.0f, 0.15f, 0.0f));
            }
        }

    }

    vec3 SceneOverlays::GetPosition(Scene::Scene& scene, Scene::Entity entity) {

        auto light = entity.TryGetComponent<LightComponent>();
        if (light && light->type == LightType::PointLight)
            return light->transformedProperties.point.position;
        if (light && light->type == LightType::SpotLight)
            return light->transformedProperties.spot.position;
        // Above the head of a player instead of at its feet
        auto bounds = entity.HasComponent<PlayerComponent>() ? scene.GetHierarchyBounds(entity) : std::nullopt;
        if (bounds)
            return vec3(0.5f * (bounds->min.x + bounds->max.x), bounds->max.y + 0.4f, 0.5f * (bounds->min.z + bounds->max.z));
        return vec3(scene.GetGlobalMatrix(entity)[3]);

    }

    bool SceneOverlays::IsInRange(const View& view, Scene::Entity entity, vec3 position) const {

        auto light = entity.TryGetComponent<LightComponent>();
        return entity != view.cameraEntity && ((light && light->type == LightType::DirectionalLight) ||
            glm::distance(position, view.location) <= distance);

    }

    void SceneOverlays::FindBodiesWithoutMesh(Scene::Scene& scene) {

        const auto& bodies = scene.entityManager.GetPool<RigidBodyComponent>();
        for (size_t i = 0; i < std::min(bodies.Size(), searchedBodiesPerFrame); i++) {
            searchedBodyIdx = (searchedBodyIdx + 1) % bodies.Size();
            if (!scene.entityManager.Contains<MeshComponent>(bodies[searchedBodyIdx]))
                bodiesWithoutMesh.insert(bodies[searchedBodyIdx]);
        }
        std::erase_if(bodiesWithoutMesh, [&](ECS::Entity ecsEntity) {
            Scene::Entity entity(ecsEntity, &scene.entityManager);
            return !entity.IsValid() || !entity.HasComponent<RigidBodyComponent>();
            });

    }

    void SceneOverlays::CollectIcons(const View& view) {

        auto viewport = view.viewportPanel.viewport;
        auto viewportSize = vec2(float(viewport->width), float(viewport->height));
        auto projection = view.camera.unjitterdProjection * view.camera.viewMatrix;

        // Entities which show something invisible or have no mesh to click on
        auto collect = [&](const auto& entities, bool withoutMesh) {
            for (auto ecsEntity : entities) {
                Scene::Entity entity(ecsEntity, &view.scene.entityManager);
                if (withoutMesh && entity.HasComponent<MeshComponent>())
                    continue;

                auto position = GetPosition(view.scene, entity);
                if (!IsInRange(view, entity, position))
                    continue;

                auto clip = projection * vec4(position, 1.0f);
                if (clip.w <= view.camera.nearPlane)
                    continue;
                auto viewportPosition = (vec2(clip) / clip.w * 0.5f + 0.5f) * viewportSize;
                if (glm::any(glm::lessThan(viewportPosition, vec2(-iconSize))) ||
                    glm::any(glm::greaterThan(viewportPosition, viewportSize + iconSize)))
                    continue;

                // Icons fade out before the overlay distance
                auto iconDistance = glm::distance(position, view.location);
                auto light = entity.TryGetComponent<LightComponent>();
                auto alpha = light && light->type == LightType::DirectionalLight ? 1.0f :
                    Helper::Math::Saturate((distance - iconDistance) / (0.2f * distance));

                icons.push_back(Icon {
                    .entity = entity,
                    .position = position,
                    .distance = iconDistance,
                    .screenPosition = vec2(float(viewport->x), float(viewport->y)) + viewportPosition,
                    .size = iconSize * glm::clamp(std::sqrt(15.0f / std::max(iconDistance, 1.0f)), 0.6f, 1.0f),
                    .alpha = alpha });
            }
        };

        collect(view.scene.GetSubset<LightComponent>(), false);
        collect(view.scene.GetSubset<CameraComponent>(), false);
        collect(view.scene.GetSubset<PlayerComponent>(), false);
        collect(view.scene.GetSubset<AudioComponent>(), false);
        collect(view.scene.GetSubset<AudioVolumeComponent>(), false);
        collect(view.scene.GetSubset<FogVolumeComponent>(), false);
        collect(view.scene.GetSubset<TextComponent>(), false);
        collect(bodiesWithoutMesh, true);
        collect(view.scene.GetSubset<LuaScriptComponent>(), true);

        // An entity has one icon
        std::sort(icons.begin(), icons.end(), [](const Icon& a, const Icon& b) { return a.entity < b.entity; });
        icons.erase(std::unique(icons.begin(), icons.end(), [](const Icon& a, const Icon& b) { return a.entity == b.entity; }),
            icons.end());

    }

    void SceneOverlays::MergeIcons(const View& view) {

        std::sort(icons.begin(), icons.end(), [](const Icon& a, const Icon& b) { return a.distance < b.distance; });

        std::vector<Icon> mergedIcons;
        for (const auto& icon : icons) {
            auto covering = std::find_if(mergedIcons.begin(), mergedIcons.end(), [&](const Icon& closer) {
                auto offset = closer.screenPosition - icon.screenPosition;
                return glm::dot(offset, offset) < 0.25f * closer.size * closer.size; });
            if (covering != mergedIcons.end() && !view.selection.contains(icon.entity))
                covering->hiddenCount++;
            else
                mergedIcons.push_back(icon);
        }

        // The further ones are drawn first
        icons.assign(mergedIcons.rbegin(), mergedIcons.rend());

    }

    void SceneOverlays::AddSprites(const View& view) const {

        // Selected icons get the accent as outline, the hovered one is white and slightly larger
        auto scale = Singletons::imguiWrapper->GetScale();
        auto accent = Theme::Accent();
        for (const auto& icon : icons) {
            auto [type, color] = GetIcon(Scene::Entity(icon.entity, &view.scene.entityManager));
            auto selected = view.selection.contains(icon.entity);
            auto hovered = icon.entity == hoveredEntity;

            auto outlineColor = selected ? vec4(accent.x, accent.y, accent.z, 1.0f) : vec4(0.0f, 0.0f, 0.0f, 0.85f);
            view.viewportPanel.sprites.push_back(Renderer::Sprite {
                .position = icon.position,
                .size = vec2(icon.size * scale * (hovered || selected ? 1.15f : 1.0f)),
                .screenSpaceSize = true,
                .color = vec4(hovered ? vec3(1.0f) : color, icon.alpha),
                .outlineColor = outlineColor * vec4(1.0f, 1.0f, 1.0f, icon.alpha),
                .outlineWidth = (selected ? 3.0f : 2.0f) * scale,
                .texture = &Singletons::icons->GetTintable(type) });
        }

    }

    std::pair<IconType, vec3> SceneOverlays::GetIcon(Scene::Entity entity) {

        if (auto light = entity.TryGetComponent<LightComponent>()) {
            auto lightIcon = light->type == LightType::DirectionalLight ? IconType::Sky :
                light->type == LightType::SpotLight ? IconType::SpotLight : IconType::GlobalIllumination;
            return { lightIcon, GetLightColor(*light) };
        }
        // Players often carry their camera
        if (entity.HasComponent<PlayerComponent>())
            return { IconType::Player, vec3(0.4f, 0.9f, 1.0f) };
        if (entity.HasComponent<CameraComponent>())
            return { IconType::Camera, vec3(1.0f, 0.5f, 1.0f) };
        if (entity.HasComponent<AudioComponent>() || entity.HasComponent<AudioVolumeComponent>())
            return { IconType::Audio, vec3(0.4f, 1.0f, 0.4f) };
        if (entity.HasComponent<FogVolumeComponent>())
            return { IconType::Fog, vec3(0.6f, 0.8f, 1.0f) };
        if (entity.HasComponent<TextComponent>())
            return { IconType::Font, vec3(0.6f, 0.7f, 1.0f) };
        if (entity.HasComponent<RigidBodyComponent>())
            return { IconType::Trigger, vec3(0.0f, 1.0f, 1.0f) };
        return { IconType::Document, vec3(0.9f) };

    }

    vec3 SceneOverlays::GetLightColor(const LightComponent& light) {

        // The hue of the light at full brightness, black lights stay visible
        auto maxComponent = glm::compMax(light.color);
        return maxComponent > 0.0f ? light.color / maxComponent : vec3(1.0f);

    }

    Scene::Entity SceneOverlays::GetIconAt(Scene::Scene& scene, vec2 mousePosition) const {

        // Icons are sorted from back to front, the last one under the mouse is in front
        auto icon = std::find_if(icons.rbegin(), icons.rend(), [&](const Icon& icon) {
            return icon.alpha > 0.0f && glm::all(glm::lessThanEqual(glm::abs(mousePosition - icon.screenPosition), vec2(icon.size * 0.5f))); });
        return icon != icons.rend() ? Scene::Entity(icon->entity, &scene.entityManager) : Scene::Entity();

    }

    void SceneOverlays::RenderIconLabels(Scene::Scene& scene, bool isHovered, vec2 mousePosition) {

        auto hovered = isHovered ? GetIconAt(scene, mousePosition) : Scene::Entity();
        hoveredEntity = hovered.IsValid() ? ECS::Entity(hovered) : ECS::EntityConfig::InvalidEntity;

        auto drawList = ImGui::GetWindowDrawList();
        auto& style = ImGui::GetStyle();

        // Number of the merged icons at the lower right of their icon
        for (const auto& icon : icons) {
            if (!icon.hiddenCount || icon.alpha <= 0.0f)
                continue;
            auto text = "+" + std::to_string(icon.hiddenCount);
            auto textSize = ImGui::CalcTextSize(text.c_str());
            auto position = ImVec2(icon.screenPosition.x + 0.3f * icon.size, icon.screenPosition.y + 0.15f * icon.size);
            drawList->AddRectFilled(ImVec2(position.x - 3.0f, position.y - 1.0f),
                ImVec2(position.x + textSize.x + 3.0f, position.y + textSize.y + 1.0f),
                ImGui::GetColorU32(Theme::OverlayBackground(0.8f * icon.alpha)), style.FrameRounding);
            drawList->AddText(position, ImGui::GetColorU32(ImGuiCol_Text, icon.alpha), text.c_str());
        }

        if (!hovered.IsValid() || ImGui::IsMouseDown(ImGuiMouseButton_Right))
            return;

        auto name = hovered.TryGetComponent<NameComponent>();
        auto icon = std::find_if(icons.begin(), icons.end(), [&](const Icon& icon) { return icon.entity == hoveredEntity; });
        auto label = name ? name->name : "Entity " + std::to_string(hovered.GetID());
        if (icon->hiddenCount)
            label += " and " + std::to_string(icon->hiddenCount) + " more behind it";
        ImGui::SetTooltip("%s", label.c_str());

    }

    void SceneOverlays::RenderKindSettings() {

        for (const auto& kind : kinds) {
            ImGui::CheckboxFlags(kind.label, &shownKinds, kind.bit);
            if (kind.tooltip)
                ImGui::SetItemTooltip("%s", kind.tooltip);
        }

    }

    void SceneOverlays::RenderSettings(ViewportPanel& viewportPanel) {

        ImGui::Checkbox("Depth test bounding volumes", &depthTest);
        ImGui::DragFloat("Overlay distance", &distance, 1.0f, 1.0f, 10000.0f, "%.0f m");
        ImGui::SetItemTooltip("Selected entities show their overlays at any distance");
        ImGui::BeginDisabled(!(shownKinds & EntityIconsBit));
        ImGui::DragFloat("Icon size", &iconSize, 0.5f, 8.0f, 128.0f, "%.0f px");
        ImGui::SliderFloat("Hidden icon opacity", &viewportPanel.occludedSpriteAlpha, 0.0f, 1.0f);
        ImGui::SetItemTooltip("Icons behind surfaces, e.g. lights inside lamps, are faded");
        ImGui::EndDisabled();

    }

}
