#include "UriHelper.h"

#include <cctype>
#include <charconv>
#include <cstdint>
#include <format>
#include <string_view>

namespace Atlas::Editor {

    std::string UriHelper::Decode(const std::string& uri) {

        std::string path;
        for (size_t i = 0; i < uri.size(); i++) {
            uint8_t value = 0;
            auto digits = std::string_view(uri).substr(i + 1, 2);
            if (uri[i] == '%' && digits.size() == 2 &&
                std::from_chars(digits.data(), digits.data() + 2, value, 16).ptr == digits.data() + 2) {
                path += char(value);
                i += 2;
            }
            else {
                path += uri[i];
            }
        }
        return path;

    }

    std::string UriHelper::Encode(const std::string& path) {

        std::string uri;
        for (auto c : path) {
            if (std::isalnum(uint8_t(c)) || std::string_view("-._~/()!*',;:@&=+$").contains(c))
                uri += c;
            else
                uri += std::format("%{:02X}", uint8_t(c));
        }
        return uri;

    }

}
