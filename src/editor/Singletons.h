#pragma once

#include "ImguiExtension/ImguiWrapper.h"

namespace Atlas::Editor {

    class Singletons {

    public:
        static void Destruct();

        static ImguiWrapper ImguiWrapper;

    };

}