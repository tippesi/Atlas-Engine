#pragma once

#include "System.h"

namespace Atlas::ImguiExtension {

    std::string NumberToString(auto number) {

        auto str = std::to_string(number);
        auto pos = str.find(".");
        if (pos != std::string::npos)
            return str.substr(0, pos + 4);
        return str;

    }

    std::string VecToString(auto vector) {

        return NumberToString(vector.x) + ", "
               + NumberToString(vector.y) + ", "
               + NumberToString(vector.z);

    }

}