#pragma once

// ImguiExtension includes
#include <imgui.h>
#include <imgui_internal.h>

// Implot
#include <implot.h>

// Atlas engine includes
#include <Window.h>
#include <graphics/DescriptorPool.h>

namespace Atlas::ImguiExtension {

    class ImguiWrapper {

    public:
        ImguiWrapper() = default;

        void Load(Atlas::Window *window);

        void Unload();

        void Update(Atlas::Window *window, float deltaTime);

        void Render(bool clearSwapChain = false);

        // Copies the draw data after ImGui::Render(), Render() then draws the copy. A render thread can draw it
        // while the next frame is built, but must not render while it is copied
        void CopyDrawData();

        void RecreateImGuiResources();

        // Scales the whole interface, ImGui works with a smaller display and a larger framebuffer scale
        void SetScale(float scale);

        float GetScale() const;

        ImTextureID GetTextureId(const Atlas::Texture::Texture* texture,
            VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    private:
        struct DescriptorSetHandle {
            VkDescriptorSet set;
            int32_t lastAccess = 0;
        };

        void MouseMotionHandler(Atlas::Events::MouseMotionEvent event);

        void MouseButtonHandler(Atlas::Events::MouseButtonEvent event);

        void MouseWheelHandler(Atlas::Events::MouseWheelEvent event);

        void KeyboardHandler(Atlas::Events::KeyboardEvent event);

        void TextInputHandler(Atlas::Events::TextInputEvent event);

        void WindowHandler(Atlas::Events::WindowEvent event);

        ImGuiKey KeycodeToImGuiKey(int keyCode);

        void UpdateKeyModifiers(uint16_t keyModifier);

        void UpdateMouseCursor();

        SDL_Cursor *mouseCursors[ImGuiMouseCursor_COUNT];
        char *clipboardText = nullptr;

        int32_t windowID = 0;
        int32_t mouseMotionID = 0;
        int32_t mouseButtonID = 0;
        int32_t mouseWheelID = 0;
        int32_t keyboardID = 0;
        int32_t textInputID = 0;

        Atlas::Window *window = nullptr;
        float scale = 1.0f;
        Atlas::Ref<Atlas::Graphics::DescriptorPool> pool = nullptr;

        std::unordered_map<VkImageView, DescriptorSetHandle> imageViewToDescriptorSetMap;

        ImDrawData drawDataCopy;
        // Reused by the copies, such that their buffers aren't allocated every frame
        ImVector<ImDrawList*> drawListCopies;
        bool hasDrawDataCopy = false;

        bool initialized = false;

    };

}