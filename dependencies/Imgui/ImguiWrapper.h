#ifndef AE_IMGUIWRAPPER_H
#define AE_IMGUIWRAPPER_H

// Imgui includes
#include <Imgui/imgui/imgui.h>

// Atlas engine includes
#include <Window.h>
#include <Context.h>

class ImguiWrapper {

public:
	ImguiWrapper() {}

	ImguiWrapper(Atlas::Window* window, Atlas::Context* context);

	~ImguiWrapper();

	void Update(Atlas::Window* window, float deltaTime);

	void Render();

private:
	void MouseMotionHandler(Atlas::Events::MouseMotionEvent event);
	void MouseButtonHandler(Atlas::Events::MouseButtonEvent event);
	void MouseWheelHandler(Atlas::Events::MouseWheelEvent event);
	void KeyboardHandler(Atlas::Events::KeyboardEvent event);
	void TextInputHandler(Atlas::Events::TextInputEvent event);
	void WindowHandler(Atlas::Events::WindowEvent event);

	void UpdateMouseCursor();

	SDL_Cursor* mouseCursors[ImGuiMouseCursor_COUNT];

	int32_t windowID = 0;
	int32_t mouseMotionID = 0;
	int32_t mouseButtonID = 0;
	int32_t mouseWheelID = 0;
	int32_t keyboardID = 0;
	int32_t textInputID = 0;

	Atlas::Window* window;

};

#endif