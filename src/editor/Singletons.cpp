#include "Singletons.h"

namespace Atlas::Editor {

    Ref<ImguiExtension::ImguiWrapper> Singletons::imguiWrapper;
    Ref<Renderer::RenderTarget> Singletons::renderTarget;
    Ref<Renderer::PathTracerRenderTarget> Singletons::pathTraceRenderTarget;
    Ref<Renderer::MainRenderer> Singletons::mainRenderer;
    Ref<Icons> Singletons::icons;
    Ref<Config> Singletons::config;

    void Singletons::Destruct() {

        imguiWrapper.reset();
        renderTarget.reset();
        pathTraceRenderTarget.reset();
        mainRenderer.reset();
        icons.reset();
        config.reset();

    }

}