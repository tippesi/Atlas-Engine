#pragma once

#include <string>

namespace Atlas::Editor {

    // Characters which have another meaning in URIs, e.g. in the texture paths of glTF files, are encoded as %20
    class UriHelper {

    public:
        static std::string Decode(const std::string& uri);

        static std::string Encode(const std::string& path);

    };

}
