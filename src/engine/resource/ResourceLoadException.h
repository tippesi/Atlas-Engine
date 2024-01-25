#pragma once

#include "../System.h"
#include "../Log.h"
#include <exception>

namespace Atlas {

    class ResourceLoadException : public std::exception {

    public:
        ResourceLoadException(const std::string& resourceName, const std::string& message) :
            resourceName(resourceName), message(message) {

            Log::Error("Error loading resource " + resourceName + ": " + message);

        }

        const char* what () const noexcept final {
            return message.c_str();
        }

    private:
        const std::string& resourceName;
        const std::string& message;

    };

}