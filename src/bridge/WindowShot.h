#pragma once

#include "Common.h"

#include "graphics/Buffer.h"

#include <string>

namespace Atlas::Bridge {

    // The window as the user sees it, the interface included: Copy() after the interface was rendered copies the swap
    // chain image into a buffer with the commands of the frame, Save() writes it once the frame is done. Unlike a
    // picture of the window taken from outside it works while the window is covered
    class WindowShot {

    public:
        // False if the swap chain can't be read
        bool Copy(const std::string& path);

        // False while no copy is waiting, call it in a later frame
        bool Save();

    private:
        Ref<Graphics::Buffer> buffer;
        VkExtent2D extent = {};
        bool bgra = false;
        std::string path;

    };

}
