#pragma once

#include "panels/ViewportPanel.h"

#include "scene/Scene.h"
#include "renderer/PrimitiveBatch.h"

#include <unordered_set>
#include <vector>

namespace Atlas::Editor::UI {

    // What the viewport of a scene shows of the entities around the camera: ranges, frustums, volumes and their icons
    class SceneOverlays {

    public:
        // Persisted as bits: new kinds only at the end
        typedef uint32_t Kinds;
        typedef enum KindBits {
            EntityIconsBit = (1 << 0),
            LightRangesBit = (1 << 1),
            CameraFrustumsBit = (1 << 2),
            AudioRangesBit = (1 << 3),
            FogVolumesBit = (1 << 4),
            IrradianceVolumeBit = (1 << 5),
            SplinesBit = (1 << 6),
            TextRectanglesBit = (1 << 7)
        } KindBits;

        struct Kind {
            KindBits bit;
            const char* label;
            const char* tooltip;
        };

        // In the order of the viewport menu and the commands
        static constexpr Kind kinds[] = {
            { EntityIconsBit, "Entity icons", "Lights, cameras, players, audio, fog volumes, texts and bodies or scripts without a mesh, a right click selects them" },
            { LightRangesBit, "Light ranges", "Point light ranges, spot light cones and directions of directional lights, in the color of the light" },
            { CameraFrustumsBit, "Camera frustums", nullptr },
            { AudioRangesBit, "Audio ranges and volumes", "Where audio sources are cut off and the bounds of audio volumes" },
            { FogVolumesBit, "Fog volumes", nullptr },
            { IrradianceVolumeBit, "Irradiance volume", "The cascades of the DDGI volume" },
            { SplinesBit, "Splines", nullptr },
            { TextRectanglesBit, "Text rectangles", nullptr },
        };

        struct View {
            Scene::Scene& scene;
            ViewportPanel& viewportPanel;
            const CameraComponent& camera;
            // Has no overlays of its own
            Scene::Entity cameraEntity;
            // Distances are measured from here
            vec3 location;
            const std::unordered_set<ECS::Entity>& selection;
        };

        // The shown kinds of the entities around the camera
        void Render(const View& view);

        // All shapes of a selected entity with its bounds and shadow frustums
        void RenderSelected(Scene::Scene& scene, ViewportPanel& viewportPanel, Scene::Entity entity);

        // The icon under the mouse is highlighted, without one it's none. Draws the name of its entity and the number
        // of the icons merged into others
        void RenderIconLabels(Scene::Scene& scene, bool isHovered, vec2 mousePosition);

        // The entity of the icon under the mouse, the closest one where icons overlap
        Scene::Entity GetIconAt(Scene::Scene& scene, vec2 mousePosition) const;

        // Nothing is shown, e.g. while playing
        void ClearIcons() { icons.clear(); hoveredEntity = ECS::EntityConfig::InvalidEntity; }

        // Checkboxes of the kinds
        void RenderKindSettings();

        void RenderSettings(ViewportPanel& viewportPanel);

        Kinds shownKinds = EntityIconsBit | CameraFrustumsBit | FogVolumesBit | AudioRangesBit | SplinesBit;
        float distance = 150.0f;
        // In interface pixels, icons further away get smaller
        float iconSize = 40.0f;
        bool depthTest = false;

    private:
        struct Icon {
            ECS::Entity entity;
            vec3 position;
            float distance;
            // In interface pixels of the window
            vec2 screenPosition;
            float size;
            float alpha;
            // Icons of the entities behind it which it covers
            int32_t hiddenCount = 0;
        };

        void RenderEntity(Scene::Scene& scene, Renderer::PrimitiveBatch& batch, Scene::Entity entity, Kinds entityKinds, bool selected) const;

        // Where the icon of an entity is, ranges are measured to it as well
        static vec3 GetPosition(Scene::Scene& scene, Scene::Entity entity);

        // Directional lights are everywhere
        bool IsInRange(const View& view, Scene::Entity entity, vec3 position) const;

        // A part of the bodies each frame, worlds have many of them and nearly all have a mesh
        void FindBodiesWithoutMesh(Scene::Scene& scene);

        // The icons in view
        void CollectIcons(const View& view);

        // Icons covered by a closer one are merged into it
        void MergeIcons(const View& view);

        void AddSprites(const View& view) const;

        static std::pair<IconType, vec3> GetIcon(Scene::Entity entity);

        // Ranges and icons of lights show their color
        static vec3 GetLightColor(const LightComponent& light);

        // The ones of the last frame, sorted from back to front
        std::vector<Icon> icons;
        ECS::Entity hoveredEntity = ECS::EntityConfig::InvalidEntity;

        std::unordered_set<ECS::Entity> bodiesWithoutMesh;
        size_t searchedBodyIdx = 0;
        static constexpr size_t searchedBodiesPerFrame = 1024;

    };

}
