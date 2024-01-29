#pragma once

#include "Window.h"
#include "resource/Resource.h"

namespace Atlas::Editor::UI {

    class ContentBrowserWindow : public Window {

    public:
        ContentBrowserWindow() : Window("Object browser") {}

        void Render();

    private:
        template<class T>
        void RenderResourceType(Resource<T>& resource) {



        }

    };

}