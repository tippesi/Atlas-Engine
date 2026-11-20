#pragma once

#include "animation/Animation.h"

namespace Atlas::Loader {

    class AnimationLoader {

    public:
        static Ref<Animation::Animation> LoadAnimation(const std::string& filename, bool binaryJson = true);

        static void SaveAnimation(const Ref<Animation::Animation>& animation, const std::string& filename, bool binaryJson = true);

    };

}
