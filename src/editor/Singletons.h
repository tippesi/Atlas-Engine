#pragma once

#include "ImguiExtension/ImguiWrapper.h"
#include "renderer/target/RenderTarget.h"
#include "renderer/MainRenderer.h"
#include "Icons.h"
#include "Config.h"
#include "BlockingOperation.h"
#include "ui/popups/CommandPalette.h"

#include "project/Project.h"

namespace Atlas::Editor {

    class ContentPreviews;

    class Singletons {

    public:
        static void Destruct();

        static Ref<ImguiExtension::ImguiWrapper> imguiWrapper;
        static Ref<Renderer::MainRenderer> mainRenderer;
        static Ref<Icons> icons;
        static Ref<Config> config;
        static Ref<BlockingOperation> blockingOperation;
        static Ref<UI::CommandPalette> commandPalette;
        // Shared by everything which shows content, e.g. the content browser and resource selections
        static Ref<ContentPreviews> contentPreviews;
        // Without a project the editor shows the project hub
        static Ref<Project> project;

    };

}