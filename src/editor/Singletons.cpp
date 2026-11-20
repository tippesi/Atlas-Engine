#include "Singletons.h"
#include "ContentPreviews.h"

namespace Atlas::Editor {

    Ref<ImguiExtension::ImguiWrapper> Singletons::imguiWrapper;
    Ref<Renderer::MainRenderer> Singletons::mainRenderer;
    Ref<Icons> Singletons::icons;
    Ref<Config> Singletons::config;
    Ref<BlockingOperation> Singletons::blockingOperation;
    Ref<ContentPreviews> Singletons::contentPreviews;

    void Singletons::Destruct() {

        contentPreviews.reset();
        imguiWrapper.reset();
        mainRenderer.reset();
        icons.reset();
        config.reset();
        blockingOperation.reset();

    }

}