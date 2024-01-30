#pragma once

#include "ImguiExtension/ImguiWrapper.h"
#include "RenderTarget.h"
#include "renderer/MainRenderer.h"

namespace Atlas::Editor {

    class Singletons {

    public:
        static void Destruct();

        static Ref<ImguiWrapper> ImguiWrapper;
        static Ref<RenderTarget> RenderTarget;
        static Ref<Renderer::MainRenderer> MainRenderer;

    };

}