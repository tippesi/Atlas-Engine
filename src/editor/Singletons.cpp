#include "Singletons.h"

namespace Atlas::Editor {

    Ref<ImguiExtension::ImguiWrapper> Singletons::imguiWrapper;
    Ref<Renderer::RenderTarget> Singletons::renderTarget;
    Ref<Renderer::MainRenderer> Singletons::mainRenderer;
    Ref<Icons> Singletons::icons;
    Ref<Config> Singletons::config;
    Ref<BlockingOperation> Singletons::blockingOperation;

    void Singletons::Destruct() {

        imguiWrapper.reset();
        renderTarget.reset();
        mainRenderer.reset();
        icons.reset();
        config.reset();
        blockingOperation.reset();

    }

}