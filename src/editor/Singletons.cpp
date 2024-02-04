#include "Singletons.h"

namespace Atlas::Editor {

    Ref<ImguiWrapper> Singletons::imguiWrapper;
    Ref<RenderTarget> Singletons::renderTarget;
    Ref<Renderer::MainRenderer> Singletons::mainRenderer;
    Ref<Icons> Singletons::icons;

    void Singletons::Destruct() {

        imguiWrapper.reset();
        renderTarget.reset();
        mainRenderer.reset();
        icons.reset();

    }

}