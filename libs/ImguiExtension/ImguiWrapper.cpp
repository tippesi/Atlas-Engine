// Based on SDL3 Imgui backend: https://github.com/ocornut/imgui/blob/master/backends/imgui_impl_sdl3.cpp

#include "ImguiWrapper.h"

#ifndef AE_OS_APPLE_MOBILE
#define VK_NO_PROTOTYPES
#endif
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_clipboard.h>
#include <graphics/Instance.h>
#include <ImguiVulkan.h>

namespace Atlas::ImguiExtension {

    void ImguiWrapper::Load(Atlas::Window *window) {

        ImPlot::CreateContext();

        // Setup back-end capabilities flags
        ImGuiIO &io = ImGui::GetIO();
        io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;       // We can honor GetMouseCursor() values (optional)
        io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;        // We can honor io.WantSetMousePos requests (optional, rarely used)
        io.BackendPlatformName = "imgui_impl_atlas";

        ImGuiPlatformIO &platformIO = ImGui::GetPlatformIO();
        platformIO.Platform_ClipboardUserData = this;
        platformIO.Platform_SetClipboardTextFn = [](ImGuiContext*, const char* text) {
            SDL_SetClipboardText(text);
        };
        platformIO.Platform_GetClipboardTextFn = [](ImGuiContext*) -> const char* {
            auto wrapper = static_cast<ImguiWrapper*>(ImGui::GetPlatformIO().Platform_ClipboardUserData);
            // SDL hands out a copy that has to stay valid until ImGui asks again
            SDL_free(wrapper->clipboardText);
            wrapper->clipboardText = SDL_GetClipboardText();
            return wrapper->clipboardText;
        };

        mouseCursors[ImGuiMouseCursor_Arrow] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
        mouseCursors[ImGuiMouseCursor_TextInput] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_TEXT);
        mouseCursors[ImGuiMouseCursor_ResizeAll] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_MOVE);
        mouseCursors[ImGuiMouseCursor_ResizeNS] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NS_RESIZE);
        mouseCursors[ImGuiMouseCursor_ResizeEW] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_EW_RESIZE);
        mouseCursors[ImGuiMouseCursor_ResizeNESW] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NESW_RESIZE);
        mouseCursors[ImGuiMouseCursor_ResizeNWSE] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NWSE_RESIZE);
        mouseCursors[ImGuiMouseCursor_Hand] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_POINTER);

        // Subscribe to events
        auto windowEventHandler = std::bind(&ImguiWrapper::WindowHandler, this, std::placeholders::_1);
        windowID = window->windowEventDelegate.Subscribe(windowEventHandler);

        auto mouseButtonEventHandler = std::bind(&ImguiWrapper::MouseButtonHandler, this, std::placeholders::_1);
        mouseButtonID = window->mouseButtonEventDelegate.Subscribe(mouseButtonEventHandler);

        auto mouseMotionEventHandler = std::bind(&ImguiWrapper::MouseMotionHandler, this, std::placeholders::_1);
        mouseMotionID = window->mouseMotionEventDelegate.Subscribe(mouseMotionEventHandler);

        auto mouseWheelEventHandler = std::bind(&ImguiWrapper::MouseWheelHandler, this, std::placeholders::_1);
        mouseWheelID = window->mouseWheelEventDelegate.Subscribe(mouseWheelEventHandler);

        auto keyboardEventHandler = std::bind(&ImguiWrapper::KeyboardHandler, this, std::placeholders::_1);
        keyboardID = Atlas::Events::EventManager::KeyboardEventDelegate.Subscribe(keyboardEventHandler);

        auto textInputEventHandler = std::bind(&ImguiWrapper::TextInputHandler, this, std::placeholders::_1);
        textInputID = Atlas::Events::EventManager::TextInputEventDelegate.Subscribe(textInputEventHandler);

        RecreateImGuiResources();

    }

    void ImguiWrapper::Unload() {

        auto instance = Atlas::Graphics::Instance::DefaultInstance;
        auto device = instance->GetGraphicsDevice();

        device->WaitForIdle();

        // Unsubscribe from all events
        ImGui_ImplVulkan_Shutdown();

        ImPlot::DestroyContext();

        pool.reset();

        SDL_free(clipboardText);
        clipboardText = nullptr;

        initialized = false;

    }

    void ImguiWrapper::Update(Atlas::Window *window, float deltaTime) {

        ImGuiIO &io = ImGui::GetIO();

        io.DeltaTime = deltaTime;

        auto w = (float) window->GetWidth();
        auto h = (float) window->GetHeight();
        auto size = glm::vec2(window->GetDrawableSize());
        io.DisplaySize = ImVec2(w, h);
        if (w > 0.0f && h > 0.0f)
            io.DisplayFramebufferScale = ImVec2(size.x / w, size.y / h);

        if (io.WantTextInput)
            Atlas::Events::EventManager::EnableTextInput();
        else
            Atlas::Events::EventManager::DisableTextInput();

        UpdateMouseCursor();

        // Need to delete old descriptor set, especially when for each frame
        // there are new textures to be rendered
        std::vector<VkImageView> deletedKeys;
        for (auto &[key, handle]: imageViewToDescriptorSetMap) {
            if (handle.lastAccess <= FRAME_DATA_COUNT) {
                handle.lastAccess++;
                continue;
            }

            ImGui_ImplVulkan_RemoveTexture(handle.set);
            deletedKeys.push_back(key);
        }

        for (auto key: deletedKeys)
            imageViewToDescriptorSetMap.erase(key);

    }

    void ImguiWrapper::Render(bool clearSwapChain) {

        ImGuiIO &io = ImGui::GetIO();

        ImGui_ImplVulkan_NewFrame();

        auto instance = Atlas::Graphics::Instance::DefaultInstance;
        auto device = instance->GetGraphicsDevice();
        auto swapChain = device->swapChain;

        if (!device->swapChain->isComplete) return;

        auto commandList = device->GetCommandList(Atlas::Graphics::GraphicsQueue);

        commandList->BeginCommands();

        Atlas::Graphics::Profiler::BeginThread("ImGui thread", commandList);
        Atlas::Graphics::Profiler::BeginQuery("ImGui");

        commandList->BeginRenderPass(swapChain, clearSwapChain);
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandList->commandBuffer);
        commandList->EndRenderPass();

        Atlas::Graphics::Profiler::EndQuery();
        Atlas::Graphics::Profiler::EndThread();

        commandList->EndCommands();

        device->SubmitCommandList(commandList);

    }

    void ImguiWrapper::RecreateImGuiResources() {

        if (initialized) {
            ImGui_ImplVulkan_Shutdown();
        }

        auto instance = Atlas::Graphics::Instance::DefaultInstance;
        auto device = instance->GetGraphicsDevice();

        auto desc = Atlas::Graphics::DescriptorPoolDesc{
            .freeDescriptorSets = true
        };
        pool = device->CreateDescriptorPool(desc);

        auto queue = device->GetAndLockQueue(Atlas::Graphics::GraphicsQueue);

        //this initializes imgui for Vulkan
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = instance->GetNativeInstance();
        init_info.PhysicalDevice = device->physicalDevice;
        init_info.Device = device->device;
        init_info.Queue = queue.queue;
        init_info.DescriptorPool = pool->GetNativePool();
        init_info.MinImageCount = 3;
        init_info.ImageCount = 3;
        init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

        ImGui_ImplVulkan_Init(&init_info, device->swapChain->renderPass);

        //execute a gpu command to upload imgui font textures
        device->memoryManager->transferManager->ImmediateSubmit(Atlas::Graphics::GraphicsQueue,
            [&](Atlas::Graphics::CommandList *commandList) {
                ImGui_ImplVulkan_CreateFontsTexture(commandList->commandBuffer);
            });

        //clear font textures from cpu data
        ImGui_ImplVulkan_DestroyFontUploadObjects();

        imageViewToDescriptorSetMap.clear();

        initialized = true;

    }

    ImTextureID ImguiWrapper::GetTextureId(const Atlas::Texture::Texture* texture,
        VkImageLayout layout) {

        if (!imageViewToDescriptorSetMap.contains(texture->image->view)) {
            auto set = ImGui_ImplVulkan_AddTexture(texture->sampler->sampler, texture->image->view, layout);
            imageViewToDescriptorSetMap[texture->image->view] = {
                .set = set,
                .lastAccess = 0,
            };
        }

        auto &handle = imageViewToDescriptorSetMap[texture->image->view];
        handle.lastAccess = 0;

        return (unsigned long long)handle.set;

    }

    void ImguiWrapper::WindowHandler(Atlas::Events::WindowEvent event) {

        ImGuiIO &io = ImGui::GetIO();

        if (event.type == AE_WINDOWEVENT_FOCUS_LOST) {
            io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
        }

    }

    void ImguiWrapper::MouseButtonHandler(Atlas::Events::MouseButtonEvent event) {

        ImGuiIO &io = ImGui::GetIO();

        int mouseButton = -1;
        if (event.button == AE_MOUSEBUTTON_LEFT) { mouseButton = 0; }
        if (event.button == AE_MOUSEBUTTON_RIGHT) { mouseButton = 1; }
        if (event.button == AE_MOUSEBUTTON_MIDDLE) { mouseButton = 2; }

        if (mouseButton < 0) return;

        io.AddMouseButtonEvent(mouseButton, event.down);

    }

    void ImguiWrapper::MouseMotionHandler(Atlas::Events::MouseMotionEvent event) {

        ImGuiIO &io = ImGui::GetIO();

        io.MousePos = ImVec2(event.x, event.y);

    }

    void ImguiWrapper::MouseWheelHandler(Atlas::Events::MouseWheelEvent event) {

        ImGuiIO &io = ImGui::GetIO();

        io.MouseWheelH += event.x;
        io.MouseWheel += event.y;

    }

    void ImguiWrapper::KeyboardHandler(Atlas::Events::KeyboardEvent event) {

        ImGuiIO &io = ImGui::GetIO();

        UpdateKeyModifiers(event.keyModifiers);
        ImGuiKey key = KeycodeToImGuiKey(static_cast<int32_t>(event.keyCode));
        io.AddKeyEvent(key, event.down);

    }

    void ImguiWrapper::TextInputHandler(Atlas::Events::TextInputEvent event) {

        ImGuiIO &io = ImGui::GetIO();

        io.AddInputCharactersUTF8(event.character.c_str());

    }

    void ImguiWrapper::UpdateMouseCursor() {

        ImGuiIO &io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange)
            return;

        ImGuiMouseCursor imgui_cursor = ImGui::GetMouseCursor();
        if (io.MouseDrawCursor || imgui_cursor == ImGuiMouseCursor_None) {
            // Hide OS mouse cursor if imgui is drawing it or if it wants no cursor
            SDL_HideCursor();
        } else {
            // Show OS mouse cursor
            SDL_SetCursor(
                mouseCursors[imgui_cursor] ? mouseCursors[imgui_cursor] : mouseCursors[ImGuiMouseCursor_Arrow]);
            SDL_ShowCursor();
        }

    }

    ImGuiKey ImguiWrapper::KeycodeToImGuiKey(int keycode) {

        switch (keycode) {
            case SDLK_TAB:
                return ImGuiKey_Tab;
            case SDLK_LEFT:
                return ImGuiKey_LeftArrow;
            case SDLK_RIGHT:
                return ImGuiKey_RightArrow;
            case SDLK_UP:
                return ImGuiKey_UpArrow;
            case SDLK_DOWN:
                return ImGuiKey_DownArrow;
            case SDLK_PAGEUP:
                return ImGuiKey_PageUp;
            case SDLK_PAGEDOWN:
                return ImGuiKey_PageDown;
            case SDLK_HOME:
                return ImGuiKey_Home;
            case SDLK_END:
                return ImGuiKey_End;
            case SDLK_INSERT:
                return ImGuiKey_Insert;
            case SDLK_DELETE:
                return ImGuiKey_Delete;
            case SDLK_BACKSPACE:
                return ImGuiKey_Backspace;
            case SDLK_SPACE:
                return ImGuiKey_Space;
            case SDLK_RETURN:
                return ImGuiKey_Enter;
            case SDLK_ESCAPE:
                return ImGuiKey_Escape;
            case SDLK_APOSTROPHE:
                return ImGuiKey_Apostrophe;
            case SDLK_COMMA:
                return ImGuiKey_Comma;
            case SDLK_MINUS:
                return ImGuiKey_Minus;
            case SDLK_PERIOD:
                return ImGuiKey_Period;
            case SDLK_SLASH:
                return ImGuiKey_Slash;
            case SDLK_SEMICOLON:
                return ImGuiKey_Semicolon;
            case SDLK_EQUALS:
                return ImGuiKey_Equal;
            case SDLK_LEFTBRACKET:
                return ImGuiKey_LeftBracket;
            case SDLK_BACKSLASH:
                return ImGuiKey_Backslash;
            case SDLK_RIGHTBRACKET:
                return ImGuiKey_RightBracket;
            case SDLK_GRAVE:
                return ImGuiKey_GraveAccent;
            case SDLK_CAPSLOCK:
                return ImGuiKey_CapsLock;
            case SDLK_SCROLLLOCK:
                return ImGuiKey_ScrollLock;
            case SDLK_NUMLOCKCLEAR:
                return ImGuiKey_NumLock;
            case SDLK_PRINTSCREEN:
                return ImGuiKey_PrintScreen;
            case SDLK_PAUSE:
                return ImGuiKey_Pause;
            case SDLK_KP_0:
                return ImGuiKey_Keypad0;
            case SDLK_KP_1:
                return ImGuiKey_Keypad1;
            case SDLK_KP_2:
                return ImGuiKey_Keypad2;
            case SDLK_KP_3:
                return ImGuiKey_Keypad3;
            case SDLK_KP_4:
                return ImGuiKey_Keypad4;
            case SDLK_KP_5:
                return ImGuiKey_Keypad5;
            case SDLK_KP_6:
                return ImGuiKey_Keypad6;
            case SDLK_KP_7:
                return ImGuiKey_Keypad7;
            case SDLK_KP_8:
                return ImGuiKey_Keypad8;
            case SDLK_KP_9:
                return ImGuiKey_Keypad9;
            case SDLK_KP_PERIOD:
                return ImGuiKey_KeypadDecimal;
            case SDLK_KP_DIVIDE:
                return ImGuiKey_KeypadDivide;
            case SDLK_KP_MULTIPLY:
                return ImGuiKey_KeypadMultiply;
            case SDLK_KP_MINUS:
                return ImGuiKey_KeypadSubtract;
            case SDLK_KP_PLUS:
                return ImGuiKey_KeypadAdd;
            case SDLK_KP_ENTER:
                return ImGuiKey_KeypadEnter;
            case SDLK_KP_EQUALS:
                return ImGuiKey_KeypadEqual;
            case SDLK_LCTRL:
                return ImGuiKey_LeftCtrl;
            case SDLK_LSHIFT:
                return ImGuiKey_LeftShift;
            case SDLK_LALT:
                return ImGuiKey_LeftAlt;
            case SDLK_LGUI:
                return ImGuiKey_LeftSuper;
            case SDLK_RCTRL:
                return ImGuiKey_RightCtrl;
            case SDLK_RSHIFT:
                return ImGuiKey_RightShift;
            case SDLK_RALT:
                return ImGuiKey_RightAlt;
            case SDLK_RGUI:
                return ImGuiKey_RightSuper;
            case SDLK_APPLICATION:
                return ImGuiKey_Menu;
            case SDLK_0:
                return ImGuiKey_0;
            case SDLK_1:
                return ImGuiKey_1;
            case SDLK_2:
                return ImGuiKey_2;
            case SDLK_3:
                return ImGuiKey_3;
            case SDLK_4:
                return ImGuiKey_4;
            case SDLK_5:
                return ImGuiKey_5;
            case SDLK_6:
                return ImGuiKey_6;
            case SDLK_7:
                return ImGuiKey_7;
            case SDLK_8:
                return ImGuiKey_8;
            case SDLK_9:
                return ImGuiKey_9;
            case SDLK_A:
                return ImGuiKey_A;
            case SDLK_B:
                return ImGuiKey_B;
            case SDLK_C:
                return ImGuiKey_C;
            case SDLK_D:
                return ImGuiKey_D;
            case SDLK_E:
                return ImGuiKey_E;
            case SDLK_F:
                return ImGuiKey_F;
            case SDLK_G:
                return ImGuiKey_G;
            case SDLK_H:
                return ImGuiKey_H;
            case SDLK_I:
                return ImGuiKey_I;
            case SDLK_J:
                return ImGuiKey_J;
            case SDLK_K:
                return ImGuiKey_K;
            case SDLK_L:
                return ImGuiKey_L;
            case SDLK_M:
                return ImGuiKey_M;
            case SDLK_N:
                return ImGuiKey_N;
            case SDLK_O:
                return ImGuiKey_O;
            case SDLK_P:
                return ImGuiKey_P;
            case SDLK_Q:
                return ImGuiKey_Q;
            case SDLK_R:
                return ImGuiKey_R;
            case SDLK_S:
                return ImGuiKey_S;
            case SDLK_T:
                return ImGuiKey_T;
            case SDLK_U:
                return ImGuiKey_U;
            case SDLK_V:
                return ImGuiKey_V;
            case SDLK_W:
                return ImGuiKey_W;
            case SDLK_X:
                return ImGuiKey_X;
            case SDLK_Y:
                return ImGuiKey_Y;
            case SDLK_Z:
                return ImGuiKey_Z;
            case SDLK_F1:
                return ImGuiKey_F1;
            case SDLK_F2:
                return ImGuiKey_F2;
            case SDLK_F3:
                return ImGuiKey_F3;
            case SDLK_F4:
                return ImGuiKey_F4;
            case SDLK_F5:
                return ImGuiKey_F5;
            case SDLK_F6:
                return ImGuiKey_F6;
            case SDLK_F7:
                return ImGuiKey_F7;
            case SDLK_F8:
                return ImGuiKey_F8;
            case SDLK_F9:
                return ImGuiKey_F9;
            case SDLK_F10:
                return ImGuiKey_F10;
            case SDLK_F11:
                return ImGuiKey_F11;
            case SDLK_F12:
                return ImGuiKey_F12;
        }

        return ImGuiKey_None;

    }

    void ImguiWrapper::UpdateKeyModifiers(uint16_t keyModifier) {

        ImGuiIO &io = ImGui::GetIO();
        io.AddKeyEvent(ImGuiMod_Ctrl, (keyModifier & SDL_KMOD_CTRL) != 0);
        io.AddKeyEvent(ImGuiMod_Shift, (keyModifier & SDL_KMOD_SHIFT) != 0);
        io.AddKeyEvent(ImGuiMod_Alt, (keyModifier & SDL_KMOD_ALT) != 0);
        io.AddKeyEvent(ImGuiMod_Super, (keyModifier & SDL_KMOD_GUI) != 0);

    }

}