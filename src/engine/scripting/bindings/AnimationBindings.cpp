#include "AnimationBindings.h"

#include "animation/Animation.h"

namespace Atlas::Scripting::Bindings {

    void GenerateAnimationBindings(sol::table* ns) {

        ns->new_usertype<Animation::Channel>("AnimationChannel",
            "SamplePosition", &Animation::Channel::SamplePosition,
            "SampleRotation", &Animation::Channel::SampleRotation,
            "SampleScale", &Animation::Channel::SampleScale,
            "jointName", &Animation::Channel::jointName,
            "positionTimes", &Animation::Channel::positionTimes,
            "positions", &Animation::Channel::positions,
            "rotationTimes", &Animation::Channel::rotationTimes,
            "rotations", &Animation::Channel::rotations,
            "scaleTimes", &Animation::Channel::scaleTimes,
            "scales", &Animation::Channel::scales
            );

        ns->new_usertype<Animation::Animation>("Animation",
            "name", &Animation::Animation::name,
            "duration", &Animation::Animation::duration,
            "channels", &Animation::Animation::channels
            );

    }

}
