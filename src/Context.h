#ifndef AE_CONTEXT_H
#define AE_CONTEXT_H

#include "System.h"
#include "Window.h"

#include <SDL/include/SDL.h>

namespace Atlas {

	class Context {

	public:
		Context() {}

		Context(SDL_Window* window);

		~Context();

		void AttachTo(Window* window);

		void Bind();

		void Unbind();

		std::string name = "No name";

	private:
		void LocalAPISetup();

		static void DebugCallback(GLenum source, GLenum type, GLuint ID, GLenum severity,
			GLsizei length, const GLchar* message, const void* userParam);

		SDL_Window* window = nullptr;
		SDL_GLContext context = nullptr;

		bool isCreated = false;

	};

}

#endif