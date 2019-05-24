#ifndef AE_ENGINEINSTANCE_H
#define AE_ENGINEINSTANCE_H

#include "Engine.h"
#include "Context.h"

#include <vector>

namespace Atlas {

	class EngineInstance {

	public:
	    /**
	     * Constructs an EngineInstance object.
	     * @param windowTitle The title of the window
	     * @param windowWidth The width of the window
	     * @param windowHeight The height of the window
	     * @param flags Additional window flags. See {@link Window.h} for more.
	     * @note The constructors of derived classes shouldn't do anything,
	     * except calling this. For everything else use LoadContent().
	     */
		EngineInstance(std::string windowTitle, int32_t windowWidth,
			int32_t windowHeight, int32_t flags = AE_WINDOW_RESIZABLE);

		/**
		 * Derived classes should load their content and data.
		 */
		virtual void LoadContent() = 0;

        /**
         * Derived classes should delete their content and data.
         */
		virtual void UnloadContent() = 0;

        /**
         * Derived classes should update their content and data
         * @note This method is called per frame.
         */
		virtual void Update(float deltaTime) = 0;

        /**
         * Derived classes should render their content and data.
         * @note This method is called per frame.
         */
		virtual void Render(float deltaTime) = 0;

		/**
		 * Updates stuff in the class internally. Is being called from
		 * the outside.
		 */
		void Update();

		/**
		 * Needs to be implemented by the user.
		 */
		static std::string assetDirectory;

		/**
		 * Needs to be implemented by the user.
		 * @note This path must be relative to the asset directory
		 */
		static std::string shaderDirectory;

		/**
		 * The main window to which the context is attached to
		 */
		Window window;

		/**
		 * A default context which needs to be used for rendering.
		 */
		Context context;

        /**
         * Command line arguments.
         * @note This member will be first available when LoadContent() is called.
         */
		std::vector<std::string> args;

	protected:
		/**
		 * Returns the size of the screen.
		 * @return An 2-component integer vector where x is the width and y is the height.
		 */
		ivec2 GetScreenSize();

		/**
		 * Locks the frame rate to the next available target frame rate of the monitor
		 * like min(availableTargetFramerate, possibleFramerate).
		 */
		void LockFramerate();

		/**
		 * Unlocks the framerate.
		 */
		void UnlockFramerate();

		/**
		 * Closes the application.
		 */
		void Exit();

		Renderer::MasterRenderer masterRenderer;

	};

}


#endif