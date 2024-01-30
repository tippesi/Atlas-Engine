#pragma once

#include "Engine.h"

#include <vector>

namespace Atlas {

    class EngineInstance {

        friend class Engine;

    public:
        /**
         * Constructs an EngineInstance object.
         * @param windowTitle The title of the window
         * @param windowWidth The width of the window
         * @param windowHeight The height of the window
         * @param flags Additional window flags. See {@link Window.h} for more.
         * @param createMainRenderer Specifies wether the EngineInstance should create
         *                           a default main renderer.
         * @note The constructors of derived classes shouldn't do anything,
         * except calling this. For everything else use LoadContent().
         */
        EngineInstance(const std::string& windowTitle, int32_t windowWidth,
            int32_t windowHeight, int32_t flags = AE_WINDOW_RESIZABLE,
                         bool createMainRenderer = true);

        EngineInstance(const EngineInstance& that) = delete;

        virtual ~EngineInstance();

        EngineInstance& operator=(const EngineInstance& that) = delete;

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
        const static EngineConfig engineConfig;

        /**
         * The main window
         */
        Window window;

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
         * Closes the application.
         */
        void Exit();

        Graphics::GraphicsDevice* graphicsDevice = nullptr;

        Graphics::Surface* surface = nullptr;

        Ref<Renderer::MainRenderer> mainRenderer = nullptr;

        std::vector<Display> displays;

    };

}