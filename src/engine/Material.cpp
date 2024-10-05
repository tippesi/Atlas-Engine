#include "Material.h"

namespace Atlas {

    Material::Material() {



    }

    bool Material::HasBaseColorMap() const {

        return baseColorMap.IsValid();

    }

    bool Material::HasOpacityMap() const {

        return opacityMap.IsValid();

    }

    bool Material::HasNormalMap() const {

        return normalMap.IsValid();

    }

    bool Material::HasRoughnessMap() const {

        return roughnessMap.IsValid();

    }

    bool Material::HasMetalnessMap() const {

        return metalnessMap.IsValid();

    }

    bool Material::HasAoMap() const {

        return aoMap.IsValid();

    }

    bool Material::HasDisplacementMap() const {

        return displacementMap.IsValid();

    }

    bool Material::HasEmissiveMap() const {

        return emissiveMap.IsValid();

    }

}