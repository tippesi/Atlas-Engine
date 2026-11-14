#pragma once

#include "Engine.h"

#include <vector>
#include <thread>
#include <semaphore>

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
         * Renders the frame and submits it, on the render thread if renderThread is set. Is being called
         * from the outside.
         * @return False once the graphics device is lost.
         */
        bool RenderFrame(float deltaTime);

        /**
         * Waits until the render thread finished the last frame. Update(float) has to call it before it changes
         * anything rendering reads, at the latest before the scene update. Does nothing without a render thread.
         */
        void WaitForRender();

        /**
         * Returns renderThread as it was after LoadContent(), it can't be switched while a frame renders
         */
        bool UsesRenderThread();

        /**
         * Returns the exit code passed to Exit(), which main returns.
         */
        int32_t GetExitCode() const { return exitCode; }

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

        /**
         * Renders a frame on a separate thread while the next one is updated. Render(float) then only reads
         * the extracted render state of the scene and data the update doesn't change before WaitForRender().
         * @note Set it in LoadContent(), later changes have no effect.
         */
        bool renderThread = false;

    protected:
        /**
         * Returns the size of the screen.
         * @return An 2-component integer vector where x is the width and y is the height.
         */
        ivec2 GetScreenSize();

        /**
         * Closes the application.
         * @param exitCode The code the application returns, e.g. to report a failure to a calling process.
         */
        void Exit(int32_t exitCode = 0);

        Graphics::GraphicsDevice* graphicsDevice = nullptr;

        Graphics::Surface* surface = nullptr;

        Ref<Renderer::MainRenderer> mainRenderer = nullptr;

        std::vector<Display> displays;

    private:
        void RenderLoop();

        int32_t exitCode = 0;

        std::thread renderer;
        std::binary_semaphore renderStart{ 0 };
        std::binary_semaphore renderDone{ 0 };
        float renderDeltaTime = 0.0f;
        bool rendering = false;
        bool renderFrameBegun = false;
        bool usesRenderThread = false;
        bool renderThreadKnown = false;
        bool stopRendering = false;

    };

}