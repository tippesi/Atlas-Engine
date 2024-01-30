#include "Singletons.h"

namespace Atlas::Editor {

    Ref<ImguiWrapper> Singletons::ImguiWrapper;
    Ref<RenderTarget> Singletons::RenderTarget;
    Ref<Renderer::MainRenderer> Singletons::MainRenderer;

    void Singletons::Destruct() {

        ImguiWrapper.reset();
        RenderTarget.reset();
        MainRenderer.reset();

    }

}