#include "Constraint.h"
#include "PhysicsWorld.h"

#include <Jolt/Physics/Constraints/FixedConstraint.h>
#include <Jolt/Physics/Constraints/PointConstraint.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/SliderConstraint.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Constraints/ConeConstraint.h>

namespace Atlas::Physics {

    bool Constraint::IsValid() const {

        return world != nullptr && ref != nullptr && world->ContainsConstraint(*this);

    }

    void Constraint::SetEnabled(bool enabled) {

        if (!IsValid())
            return;

        ref->SetEnabled(enabled);
        world->system->GetBodyInterface().ActivateConstraint(ref);

    }

    bool Constraint::IsEnabled() const {

        if (!IsValid())
            return false;

        return ref->GetEnabled();

    }

    float Constraint::GetForce() const {

        if (!IsValid() || world->stepDeltaTime <= 0.0f)
            return 0.0f;

        float lambda = 0.0f;
        switch (ref->GetSubType()) {
            case JPH::EConstraintSubType::Fixed:
                lambda = static_cast<JPH::FixedConstraint*>(ref.GetPtr())->GetTotalLambdaPosition().Length(); break;
            case JPH::EConstraintSubType::Point:
                lambda = static_cast<JPH::PointConstraint*>(ref.GetPtr())->GetTotalLambdaPosition().Length(); break;
            case JPH::EConstraintSubType::Hinge:
                lambda = static_cast<JPH::HingeConstraint*>(ref.GetPtr())->GetTotalLambdaPosition().Length(); break;
            case JPH::EConstraintSubType::Slider: {
                auto sliderLambda = static_cast<JPH::SliderConstraint*>(ref.GetPtr())->GetTotalLambdaPosition();
                lambda = glm::length(vec2(sliderLambda[0], sliderLambda[1]));
                break;
            }
            case JPH::EConstraintSubType::Distance:
                lambda = glm::abs(static_cast<JPH::DistanceConstraint*>(ref.GetPtr())->GetTotalLambdaPosition()); break;
            case JPH::EConstraintSubType::Cone:
                lambda = static_cast<JPH::ConeConstraint*>(ref.GetPtr())->GetTotalLambdaPosition().Length(); break;
            default: break;
        }

        // Lambdas are impulses of the last collision step
        return lambda / world->stepDeltaTime;

    }

}
