#pragma once

#include "System.h"
#include "texture/Texture2D.h"
#include "texture/Texture.h"
#include "texture/Texture2DArray.h"
#include "loader/ImageLoader.h"
#include "resource/Resource.h"
#include "common/Ref.h"

#include <string>

namespace Atlas {

    class Material {

    public:
        Material();

        bool HasBaseColorMap() const;
        bool HasOpacityMap() const;
        bool HasNormalMap() const;
        bool HasRoughnessMap() const;
        bool HasMetalnessMap() const;
        bool HasAoMap() const;
        bool HasDisplacementMap() const;
        bool HasEmissiveMap() const;

        std::string name;

        ResourceHandle<Texture::Texture2D> baseColorMap;
        ResourceHandle<Texture::Texture2D> opacityMap;
        ResourceHandle<Texture::Texture2D> normalMap;
        ResourceHandle<Texture::Texture2D> roughnessMap;
        ResourceHandle<Texture::Texture2D> metalnessMap;
        ResourceHandle<Texture::Texture2D> aoMap;
        ResourceHandle<Texture::Texture2D> displacementMap;
        ResourceHandle<Texture::Texture2D> emissiveMap;

        vec3 baseColor = vec3(1.0f);
        vec3 transmissiveColor = vec3(0.0f);

        vec3 emissiveColor = vec3(0.0f);
        float emissiveIntensity = 1.0f;

        float opacity = 1.0f;

        float roughness = 1.0f;
        float metalness = 0.0f;
        float ao = 1.0f;

        float reflectance = 0.5f;

        float normalScale = 0.5f;
        float displacementScale = 0.01f;

        float tiling = 1.0f;

        bool twoSided = false;
        bool vertexColors = false;

        vec2 uvAnimation = vec2(0.0f);

        uint32_t uvChannel = 0;

    };


}