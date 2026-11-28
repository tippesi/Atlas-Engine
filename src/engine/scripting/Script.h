#pragma once

#include "../System.h"

namespace Atlas::Scripting {

    class Script {

    public:
        explicit Script(const std::string& filename);

        void Reload(const std::string& filename);

        std::string code;

    };

}