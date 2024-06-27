#include "GraphicsBindings.h"

#include "texture/Texture2D.h"
#include "texture/Texture2DArray.h"
#include "texture/Texture3D.h"
#include "texture/Cubemap.h"

namespace Atlas::Scripting::Bindings {

    void GenerateGraphicBindings(sol::table* ns) {

        ns->new_usertype<Texture::Texture2D>("Texture2D",
            "width", &Texture::Texture2D::width,
            "height", &Texture::Texture2D::height,
            "channels", &Texture::Texture2D::channels
        );

        ns->new_usertype<Texture::Texture2DArray>("Texture2DArray",
            "width", &Texture::Texture2DArray::width,
            "height", &Texture::Texture2DArray::height,
            "depth", &Texture::Texture2DArray::depth,
            "channels", &Texture::Texture2DArray::channels
        );

        ns->new_usertype<Texture::Texture3D>("Texture3D",
            "width", &Texture::Texture3D::width,
            "height", &Texture::Texture3D::height,
            "depth", &Texture::Texture3D::depth,
            "channels", &Texture::Texture3D::channels
        );

        ns->new_usertype<Texture::Cubemap>("Cubemap",
            "width", &Texture::Cubemap::width,
            "height", &Texture::Cubemap::height,
            "channels", &Texture::Cubemap::channels
        );

    }

}