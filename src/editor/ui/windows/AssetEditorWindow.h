#pragma once

#include "Window.h"

#include "../panels/ViewportPanel.h"

#include "scene/Scene.h"
#include "../../UndoHistory.h"
#include "common/SerializationHelper.h"

namespace Atlas::Editor::UI {

    // Shows one asset in a preview scene with an editor camera, a viewport and a properties panel
    class AssetEditorWindow : public Window {

    public:
        AssetEditorWindow(const std::string& name, bool show);

        virtual ~AssetEditorWindow() = default;

        void Render();

        virtual void Save() = 0;

        // Path of the edited asset, an asset is only opened once
        virtual std::string GetAssetPath() const = 0;

        // Moves the camera such that the whole asset is in view
        void FocusAsset();

        void Undo();

        void Redo();

        Ref<Scene::Scene> scene;
        Scene::Entity entity;
        Scene::Entity cameraEntity;

        ViewportPanel viewportPanel;

        // Clock time when the window was last focused, e.g. animations opened from the content browser play in the latest
        float lastFocusTime = 0.0f;
        bool requestFocus = false;
        // Only the active window renders its viewport
        bool isActiveWindow = false;

        float cameraMovementSpeed = 1.0f;

        bool showGrid = true;
        bool showBounds = false;

        UndoHistory history;

    protected:
        // Whether the asset is loaded and the preview can be created
        virtual bool IsReady() const = 0;

        // Creates the entity showing the asset in the preview scene
        virtual Scene::Entity CreatePreviewEntity() = 0;

        virtual Volume::AABB GetBounds() const = 0;

        // Shown below the heading of the properties panel
        virtual void RenderProperties() = 0;

        // Additional panels left of and below the viewport, docked by DockPanels
        virtual void RenderPanels() {}

        virtual bool HasSidePanel() const { return false; }

        virtual bool HasBottomPanel() const { return false; }

        // Part of the height of the viewport area which the bottom panel takes
        virtual float GetBottomPanelSize() const { return 0.35f; }

        // Panels with their own keyboard shortcuts keep the keys from the viewport while they are focused
        virtual bool AllowsViewportShortcuts() const { return true; }

        virtual void DockPanels(ImGuiID side, ImGuiID bottom) {}

        virtual bool IsPanelFocused() const { return false; }

        // Additional entries of the viewport settings popup
        virtual void RenderViewportSettings() {}

        // Lines drawn into the scene each frame
        virtual void RenderOverlays() {}

        // Called after the viewport rendered, e.g. to accept resources dropped onto it
        virtual void AcceptViewportDrop() {}

        // Heading with the asset name, its path and the save button
        void RenderHeading(const char* saveTooltip);

        // State of the asset for the undo history, it is compared once an interaction finished
        virtual json SaveState() const { return json(); }

        virtual void RestoreState(const json& j) {}

        Panel propertiesPanel = Panel("Properties");

    private:
        void CreatePreview();

        void RegisterViewportOverlay();

        void UpdateHistory();

        bool focusRequested = false;

        json lastState;

    };

}
