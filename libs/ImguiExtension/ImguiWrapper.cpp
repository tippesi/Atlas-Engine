#include "ImguiWrapper.h"

#define VK_NO_PROTOTYPES
#include <SDL2/SDL_mouse.h>
#include <graphics/Instance.h>
#include <ImguiVulkan.h>

void ImguiWrapper::Load(Atlas::Window* window) {

	 // Setup back-end capabilities flags
    ImGuiIO& io = ImGui::GetIO();
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;       // We can honor GetMouseCursor() values (optional)
    io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;        // We can honor io.WantSetMousePos requests (optional, rarely used)
    io.BackendPlatformName = "imgui_impl_atlas";

    // Keyboard mapping. ImGui will use those indices to peek into the io.KeysDown[] array.
    io.KeyMap[ImGuiKey_Tab] = KEYCODE_TO_SCANCODE(AE_KEY_TAB);
    io.KeyMap[ImGuiKey_LeftArrow] = KEYCODE_TO_SCANCODE(AE_KEY_LEFT);
    io.KeyMap[ImGuiKey_RightArrow] = KEYCODE_TO_SCANCODE(AE_KEY_RIGHT);
    io.KeyMap[ImGuiKey_UpArrow] = KEYCODE_TO_SCANCODE(AE_KEY_UP);
    io.KeyMap[ImGuiKey_DownArrow] = KEYCODE_TO_SCANCODE(AE_KEY_DOWN);
    io.KeyMap[ImGuiKey_PageUp] = KEYCODE_TO_SCANCODE(AE_KEY_PAGEUP);
    io.KeyMap[ImGuiKey_PageDown] = KEYCODE_TO_SCANCODE(AE_KEY_PAGEDOWN);
    io.KeyMap[ImGuiKey_Home] = KEYCODE_TO_SCANCODE(AE_KEY_HOME);
    io.KeyMap[ImGuiKey_End] = KEYCODE_TO_SCANCODE(AE_KEY_END);
    io.KeyMap[ImGuiKey_Insert] = KEYCODE_TO_SCANCODE(AE_KEY_INSERT);
    io.KeyMap[ImGuiKey_Delete] = KEYCODE_TO_SCANCODE(AE_KEY_DELETE);
    io.KeyMap[ImGuiKey_Backspace] = KEYCODE_TO_SCANCODE(AE_KEY_BACKSPACE);
    io.KeyMap[ImGuiKey_Space] = KEYCODE_TO_SCANCODE(AE_KEY_SPACE);
    io.KeyMap[ImGuiKey_Enter] = KEYCODE_TO_SCANCODE(AE_KEY_ENTER);
    io.KeyMap[ImGuiKey_Escape] = KEYCODE_TO_SCANCODE(AE_KEY_ESCAPE);
    io.KeyMap[ImGuiKey_KeyPadEnter] = KEYCODE_TO_SCANCODE(AE_KEY_KEYPAD_ENTER);
    io.KeyMap[ImGuiKey_A] = AE_KEY_A;
    io.KeyMap[ImGuiKey_C] = AE_KEY_C;
    io.KeyMap[ImGuiKey_V] = AE_KEY_V;
    io.KeyMap[ImGuiKey_X] = AE_KEY_X;
    io.KeyMap[ImGuiKey_Y] = AE_KEY_Y;
    io.KeyMap[ImGuiKey_Z] = AE_KEY_Z;

	/*
    io.SetClipboardTextFn = ImGui_ImplSDL2_SetClipboardText;
    io.GetClipboardTextFn = ImGui_ImplSDL2_GetClipboardText;
    io.ClipboardUserData = NULL;
	*/

	mouseCursors[ImGuiMouseCursor_Arrow] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
	mouseCursors[ImGuiMouseCursor_TextInput] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_IBEAM);
	mouseCursors[ImGuiMouseCursor_ResizeAll] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEALL);
	mouseCursors[ImGuiMouseCursor_ResizeNS] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENS);
	mouseCursors[ImGuiMouseCursor_ResizeEW] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEWE);
	mouseCursors[ImGuiMouseCursor_ResizeNESW] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENESW);
	mouseCursors[ImGuiMouseCursor_ResizeNWSE] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENWSE);
	mouseCursors[ImGuiMouseCursor_Hand] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);

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

    auto instance = Atlas::Graphics::Instance::DefaultInstance;
	auto device = instance->GetGraphicsDevice();
    pool = device->CreateDescriptorPool();

    //this initializes imgui for Vulkan
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = instance->GetNativeInstance();
    init_info.PhysicalDevice = device->physicalDevice;
    init_info.Device = device->device;
    init_info.Queue = device->GetQueue(Atlas::Graphics::GraphicsQueue);
    init_info.DescriptorPool = pool->GetNativePool();
    init_info.MinImageCount = 3;
    init_info.ImageCount = 3;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info, device->swapChain->renderPass);

    //execute a gpu command to upload imgui font textures
    device->memoryManager->transferManager->ImmediateSubmit(
        [&](VkCommandBuffer cmd) {
        ImGui_ImplVulkan_CreateFontsTexture(cmd);
    });

    //clear font textures from cpu data
    ImGui_ImplVulkan_DestroyFontUploadObjects();

}

void ImguiWrapper::Unload() {

    auto instance = Atlas::Graphics::Instance::DefaultInstance;
    auto device = instance->GetGraphicsDevice();

    device->WaitForIdle();

    // Unsubscribe from all events
    ImGui_ImplVulkan_Shutdown();

}

void ImguiWrapper::Update(Atlas::Window* window, float deltaTime) {

	ImGuiIO& io = ImGui::GetIO();

	io.DeltaTime = deltaTime;

	auto w = (float)window->GetWidth();
	auto h = (float)window->GetHeight();
	auto size = glm::vec2(window->GetDrawableSize());
	io.DisplaySize = ImVec2(w, h);
	if (w > 0.0f && h > 0.0f)
		io.DisplayFramebufferScale = ImVec2(size.x / w, size.y / h);

	if (io.WantTextInput)
		Atlas::Events::EventManager::EnableTextInput();
	else
		Atlas::Events::EventManager::DisableTextInput();

	UpdateMouseCursor();
    ImGui_ImplVulkan_NewFrame();

}

void ImguiWrapper::Render() {

	ImGuiIO& io = ImGui::GetIO();

    auto instance = Atlas::Graphics::Instance::DefaultInstance;
    auto device = instance->GetGraphicsDevice();

    auto commandList = device->GetCommandList(Atlas::Graphics::GraphicsQueue);
    auto swapChain = device->swapChain;

    commandList->BeginCommands();

    Atlas::Graphics::Profiler::BeginThread("ImGui thread", commandList);
    Atlas::Graphics::Profiler::BeginQuery("ImGui");

    commandList->BeginRenderPass(swapChain, false);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandList->commandBuffer);
    commandList->EndRenderPass();

    Atlas::Graphics::Profiler::EndQuery();
    Atlas::Graphics::Profiler::EndThread();

    commandList->EndCommands();

    device->SubmitCommandList(commandList, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

}

void ImguiWrapper::WindowHandler(Atlas::Events::WindowEvent event) {

	ImGuiIO& io = ImGui::GetIO();

	if (event.type == AE_WINDOWEVENT_FOCUS_LOST) {
		io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
	}

}

void ImguiWrapper::MouseButtonHandler(Atlas::Events::MouseButtonEvent event) {

	ImGuiIO& io = ImGui::GetIO();

	switch (event.button) {
		case AE_MOUSEBUTTON_LEFT: io.MouseDown[0] = (event.state == AE_BUTTON_PRESSED); break;
		case AE_MOUSEBUTTON_RIGHT: io.MouseDown[1] = (event.state == AE_BUTTON_PRESSED); break;
		case AE_MOUSEBUTTON_MIDDLE: io.MouseDown[2] = (event.state == AE_BUTTON_PRESSED); break;
	}


}

void ImguiWrapper::MouseMotionHandler(Atlas::Events::MouseMotionEvent event) {

	ImGuiIO& io = ImGui::GetIO();

	io.MousePos = ImVec2((float)event.x, (float)event.y);

}

void ImguiWrapper::MouseWheelHandler(Atlas::Events::MouseWheelEvent event) {

	ImGuiIO& io = ImGui::GetIO();

	if (event.x > 0) io.MouseWheelH += 1;
	if (event.x < 0) io.MouseWheelH -= 1;
	if (event.y > 0) io.MouseWheel += 1;
	if (event.y < 0) io.MouseWheel -= 1;

}

void ImguiWrapper::KeyboardHandler(Atlas::Events::KeyboardEvent event) {

	ImGuiIO& io = ImGui::GetIO();

	io.KeysDown[KEYCODE_TO_SCANCODE(event.keycode)] = (event.state == AE_BUTTON_PRESSED);
	
	if (event.keycode == AE_KEY_LSHIFT || event.keycode == AE_KEY_RSHIFT)
		io.KeyShift = (event.state == AE_BUTTON_PRESSED);
	if (event.keycode == AE_KEY_LCTRL || event.keycode == AE_KEY_RCTRL)
		io.KeyCtrl = (event.state == AE_BUTTON_PRESSED);
	if (event.keycode == AE_KEY_LALT || event.keycode == AE_KEY_RALT)
		io.KeyAlt = (event.state == AE_BUTTON_PRESSED);
	if (event.keycode == AE_KEY_LGUI || event.keycode == AE_KEY_RGUI)
		io.KeySuper = (event.state == AE_BUTTON_PRESSED);

}

void ImguiWrapper::TextInputHandler(Atlas::Events::TextInputEvent event) {

	ImGuiIO& io = ImGui::GetIO();

	io.AddInputCharactersUTF8(event.character.c_str());

}

void ImguiWrapper::UpdateMouseCursor() {

	ImGuiIO& io = ImGui::GetIO();
	if (io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange)
		return;

	ImGuiMouseCursor imgui_cursor = ImGui::GetMouseCursor();
	if (io.MouseDrawCursor || imgui_cursor == ImGuiMouseCursor_None) {
		// Hide OS mouse cursor if imgui is drawing it or if it wants no cursor
		SDL_ShowCursor(SDL_FALSE);
	}
	else
	{
		// Show OS mouse cursor
		SDL_SetCursor(mouseCursors[imgui_cursor] ? mouseCursors[imgui_cursor] : mouseCursors[ImGuiMouseCursor_Arrow]);
		SDL_ShowCursor(SDL_TRUE);
	}

}