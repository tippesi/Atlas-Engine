#pragma once

#include "ImguiExtension/ImguiWrapper.h"
#include "RenderTarget.h"
#include "renderer/MainRenderer.h"
#include "Icons.h"

namespace Atlas::Editor {

    class Singletons {

    public:
        static void Destruct();

        static Ref<ImguiWrapper> imguiWrapper;
        static Ref<RenderTarget> renderTarget;
        static Ref<Renderer::MainRenderer> mainRenderer;
        static Ref<Icons> icons;

    };

}