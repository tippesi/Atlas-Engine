#pragma once

// ImguiExtension includes
#include <imgui.h>
#include <imgui_internal.h>

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

        void RecreateImGuiResources();

        VkDescriptorSet GetTextureDescriptorSet(const Atlas::Texture::Texture* texture,
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

        int32_t windowID = 0;
        int32_t mouseMotionID = 0;
        int32_t mouseButtonID = 0;
        int32_t mouseWheelID = 0;
        int32_t keyboardID = 0;
        int32_t textInputID = 0;

        Atlas::Window *window = nullptr;
        Atlas::Ref<Atlas::Graphics::DescriptorPool> pool = nullptr;

        std::unordered_map<VkImageView, DescriptorSetHandle> imageViewToDescriptorSetMap;

        bool initialized = false;

    };

}