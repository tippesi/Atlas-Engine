#pragma once

#include "../System.h"

namespace Atlas::Scripting {

    class Script {

    public:
        explicit Script(const std::string& filename);

        void Reload();

        std::string code;

    private:
        std::string filename;

    };

}