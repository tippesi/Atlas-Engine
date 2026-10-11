#pragma once

#include "ImguiExtension/ImguiWrapper.h"
#include "renderer/target/RenderTarget.h"
#include "renderer/MainRenderer.h"
#include "Icons.h"
#include "Config.h"
#include "BlockingOperation.h"

namespace Atlas::Editor {

    class Singletons {

    public:
        static void Destruct();

        static Ref<ImguiExtension::ImguiWrapper> imguiWrapper;
        static Ref<Renderer::RenderTarget> renderTarget;
        static Ref<Renderer::MainRenderer> mainRenderer;
        static Ref<Icons> icons;
        static Ref<Config> config;
        static Ref<BlockingOperation> blockingOperation;

    };

}