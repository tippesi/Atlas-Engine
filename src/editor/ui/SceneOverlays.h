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

        // Sections of the viewport menu
        enum class Group {
            Scene,
            Entities,
            Lighting
        };

        struct Kind {
            KindBits bit;
            Group group;
            const char* label;
            const char* tooltip;
        };

        // In the order of the viewport menu and the commands
        static constexpr Kind kinds[] = {
            { EntityIconsBit, Group::Scene, "Entity icons", "Lights, cameras, players, audio, fog volumes, texts and bodies or scripts without a mesh, a right click selects them" },
            { LightRangesBit, Group::Lighting, "Light ranges", "Point light ranges, spot light cones and directions of directional lights, in the color of the light" },
            { CameraFrustumsBit, Group::Entities, "Camera frustums", nullptr },
            { AudioRangesBit, Group::Entities, "Audio ranges and volumes", "Where audio sources are cut off and the bounds of audio volumes" },
            { FogVolumesBit, Group::Entities, "Fog volumes", nullptr },
            { IrradianceVolumeBit, Group::Lighting, "Irradiance volume", "The cascades of the DDGI volume" },
            { SplinesBit, Group::Entities, "Splines", nullptr },
            { TextRectanglesBit, Group::Entities, "Text rectangles", nullptr },
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

        // All shapes of the selected entities, also of their bodies, with their bounds and shadow frustums
        void RenderSelection(Scene::Scene& scene, ViewportPanel& viewportPanel, vec3 viewLocation,
            const std::unordered_set<ECS::Entity>& selection);

        // The icon under the mouse is highlighted, without one it's none. Draws the name of its entity and the number
        // of the icons merged into others
        void RenderIconLabels(Scene::Scene& scene, bool isHovered, vec2 mousePosition);

        // The entity of the icon under the mouse, the closest one where icons overlap
        Scene::Entity GetIconAt(Scene::Scene& scene, vec2 mousePosition) const;

        // Nothing is shown, e.g. while playing
        void ClearIcons() { icons.clear(); hoveredEntity = ECS::EntityConfig::InvalidEntity; }

        // Checkboxes of the kinds in a group
        void RenderKindSettings(Group group);

        void RenderSettings(ViewportPanel& viewportPanel);

        Kinds shownKinds = EntityIconsBit | CameraFrustumsBit | FogVolumesBit | AudioRangesBit | SplinesBit;
        float distance = 150.0f;
        // In interface pixels, icons further away get smaller
        float iconSize = 40.0f;
        // Otherwise lines are drawn over everything
        bool fadeHiddenLines = true;

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

        // Markers like the points of splines keep their size on the screen, measured from the view location
        void RenderEntity(Scene::Scene& scene, Renderer::PrimitiveBatch& batch, Scene::Entity entity, Kinds entityKinds,
            bool selected, vec3 viewLocation) const;

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

        // Of the icons and the lines of a kind
        static const vec3 playerColor, cameraColor, audioColor, fogColor, textColor, bodyColor, splineColor,
            shadowColor, irradianceColor;

        // The ones of the last frame, sorted from back to front
        std::vector<Icon> icons;
        ECS::Entity hoveredEntity = ECS::EntityConfig::InvalidEntity;

        std::unordered_set<ECS::Entity> bodiesWithoutMesh;
        size_t searchedBodyIdx = 0;
        static constexpr size_t searchedBodiesPerFrame = 1024;

    };

}
