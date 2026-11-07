#pragma once

#include "AssetEditorWindow.h"

#include "../panels/AnimationTimelinePanel.h"
#include "../panels/components/MeshComponentPanel.h"
#include "../panels/components/AnimationComponentPanel.h"
#include "../panels/components/RigidBodyComponentPanel.h"

#include "resource/Resource.h"

namespace Atlas::Editor::UI {

    // Edits the settings and materials of a mesh, plays and edits animations on its skeleton
    class MeshEditorWindow : public AssetEditorWindow {

    public:
        explicit MeshEditorWindow(ResourceHandle<Mesh::Mesh> mesh, bool show);

        void Save() override;

        std::string GetAssetPath() const override;

        // Plays the animation on the skeleton of the mesh, fails for meshes without one
        bool Play(const ResourceHandle<Animation::Animation>& animation);

        ResourceHandle<Mesh::Mesh> mesh;

        bool showSkeleton = true;
        bool showPhysics = true;

        AnimationTimelinePanel timelinePanel;

    protected:
        bool IsReady() const override;

        Scene::Entity CreatePreviewEntity() override;

        Volume::AABB GetBounds() const override;

        void RenderProperties() override;

        void RenderPanels() override;

        bool HasSidePanel() const override;

        bool HasBottomPanel() const override;

        void DockPanels(ImGuiID side, ImGuiID bottom) override;

        bool IsPanelFocused() const override;

        void RenderViewportSettings() override;

        void RenderOverlays() override;

        void AcceptViewportDrop() override;

    private:
        void RenderSkeleton();

        // Bodies belong to entities, the preview entity gets one to try shapes for the mesh
        void RenderPhysics();

        // A row of a table with two columns of muted labels and values
        static void StatisticsRow(const char* label, const char* format, ...);

        bool IsSkinned() const;

        Panel skeletonPanel = Panel("Skeleton");
        Panel timelinePanelWindow = Panel("Timeline");

        MeshComponentPanel meshComponentPanel;
        AnimationComponentPanel animationComponentPanel;
        RigidBodyComponentPanel rigidBodyComponentPanel;

        Physics::DebugDrawSettings physicsDebugDrawSettings;

        std::vector<Mesh::JointPose> restPose;
        std::vector<mat4> restMatrices;

        std::string jointSearch;
        int32_t selectedJoint = -1;

    };

}
