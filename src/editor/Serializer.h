#pragma once

#include <string>

namespace Atlas::Editor {

    class Serializer {

    public:
        static void SerializeConfig();

        static void DeserializeConfig();

    private:
        static const std::string configFilename;

    };

}