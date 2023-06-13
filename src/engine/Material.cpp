#include "Material.h"

namespace Atlas {

    Material::Material() {



    }

    bool Material::HasBaseColorMap() const {

        return baseColorMap ? true : false;

    }

    bool Material::HasOpacityMap() const {

        return opacityMap ? true : false;

    }

    bool Material::HasNormalMap() const {

        return normalMap ? true : false;

    }

    bool Material::HasRoughnessMap() const {

        return roughnessMap ? true : false;

    }

    bool Material::HasMetalnessMap() const {

        return metalnessMap ? true : false;

    }

    bool Material::HasAoMap() const {

        return aoMap ? true : false;

    }

    bool Material::HasDisplacementMap() const {

        return displacementMap ? true : false;

    }

}