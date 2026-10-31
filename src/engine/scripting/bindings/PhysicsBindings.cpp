#include "PhysicsBindings.h"
#include "MemberBindings.h"

#include "physics/PhysicsWorld.h"

namespace Atlas::Scripting::Bindings {

    void GeneratePhysicsBindings(sol::table* ns) {

        ns->new_enum<Physics::Layers>("Layers", {
            { "Static", Physics::Layers::Static },
            { "Movable", Physics::Layers::Movable },
            { "Kinematic", Physics::Layers::Kinematic },
            { "NumLayers", Physics::Layers::NumLayers }
            });

        ns->new_enum<Physics::MotionQuality>("MotionQuality", {
            { "Discrete", Physics::MotionQuality::Discrete },
            { "LinearCast", Physics::MotionQuality::LinearCast }
            });

        ns->new_enum<Physics::ShapeType>("ShapeType", {
            { "Mesh", Physics::ShapeType::Mesh },
            { "Sphere", Physics::ShapeType::Sphere },
            { "BoundingBox", Physics::ShapeType::BoundingBox },
            { "Capsule", Physics::ShapeType::Capsule },
            { "HeightField", Physics::ShapeType::HeightField },
            { "ConvexHull", Physics::ShapeType::ConvexHull },
            { "Cylinder", Physics::ShapeType::Cylinder },
            { "Compound", Physics::ShapeType::Compound }
            });

        ns->new_enum<Physics::ContactEventType>("ContactEventType", {
            { "Begin", Physics::ContactEventType::Begin },
            { "End", Physics::ContactEventType::End }
            });

        ns->new_enum<Physics::ConstraintType>("ConstraintType", {
            { "Fixed", Physics::ConstraintType::Fixed },
            { "Point", Physics::ConstraintType::Point },
            { "Hinge", Physics::ConstraintType::Hinge },
            { "Slider", Physics::ConstraintType::Slider },
            { "Distance", Physics::ConstraintType::Distance },
            { "Cone", Physics::ConstraintType::Cone }
            });

        ns->new_usertype<Physics::MeshShapeSettings>("MeshShapeSettings",
            sol::call_constructor,
            sol::constructors<Physics::MeshShapeSettings>(),
            "mesh", &Physics::MeshShapeSettings::mesh,
            "scale", &Physics::MeshShapeSettings::scale
            );

        ns->new_usertype<Physics::SphereShapeSettings>("SphereShapeSettings",
            sol::call_constructor,
            sol::constructors<Physics::SphereShapeSettings>(),
            "radius", &Physics::SphereShapeSettings::radius,
            "density", &Physics::SphereShapeSettings::density,
            "scale", &Physics::SphereShapeSettings::scale
            );

        ns->new_usertype<Physics::BoundingBoxShapeSettings>("BoundingBoxShapeSettings",
            sol::call_constructor,
            sol::constructors<Physics::BoundingBoxShapeSettings>(),
            "aabb", &Physics::BoundingBoxShapeSettings::aabb,
            "density", &Physics::BoundingBoxShapeSettings::density,
            "scale", &Physics::BoundingBoxShapeSettings::scale
            );

        ns->new_usertype<Physics::CapsuleShapeSettings>("CapsuleShapeSettings",
            sol::call_constructor,
            sol::constructors<Physics::CapsuleShapeSettings>(),
            "height", &Physics::CapsuleShapeSettings::height,
            "radius", &Physics::CapsuleShapeSettings::radius,
            "density", &Physics::CapsuleShapeSettings::density,
            "scale", &Physics::CapsuleShapeSettings::scale
            );

        ns->new_usertype<Physics::HeightFieldShapeSettings>("HeightFieldShapeSettings",
            sol::call_constructor,
            sol::constructors<Physics::HeightFieldShapeSettings>(),
            "heightData", &Physics::HeightFieldShapeSettings::heightData,
            "translation", &Physics::HeightFieldShapeSettings::translation,
            "scale", &Physics::HeightFieldShapeSettings::scale
            );

        ns->new_usertype<Physics::ConvexHullShapeSettings>("ConvexHullShapeSettings",
            sol::call_constructor,
            sol::constructors<Physics::ConvexHullShapeSettings>(),
            "mesh", &Physics::ConvexHullShapeSettings::mesh,
            "density", &Physics::ConvexHullShapeSettings::density,
            "scale", &Physics::ConvexHullShapeSettings::scale
            );

        ns->new_usertype<Physics::CylinderShapeSettings>("CylinderShapeSettings",
            sol::call_constructor,
            sol::constructors<Physics::CylinderShapeSettings>(),
            "height", &Physics::CylinderShapeSettings::height,
            "radius", &Physics::CylinderShapeSettings::radius,
            "density", &Physics::CylinderShapeSettings::density,
            "scale", &Physics::CylinderShapeSettings::scale
            );

        ns->new_usertype<Physics::CompoundShapeChild>("CompoundShapeChild",
            sol::call_constructor,
            sol::constructors<Physics::CompoundShapeChild>(),
            "shape", &Physics::CompoundShapeChild::shape,
            "translation", &Physics::CompoundShapeChild::translation,
            "rotation", &Physics::CompoundShapeChild::rotation
            );

        ns->new_usertype<Physics::CompoundShapeSettings>("CompoundShapeSettings",
            sol::call_constructor,
            sol::constructors<Physics::CompoundShapeSettings>(),
            "children", &Physics::CompoundShapeSettings::children,
            "scale", &Physics::CompoundShapeSettings::scale
            );

        ns->new_usertype<Physics::Shape>("Shape",
            "IsValid", &Physics::Shape::IsValid,
            "TryCreate", &Physics::Shape::TryCreate,
            "Scale", &Physics::Shape::Scale,
            "type", &Physics::Shape::type,
            "settings", &Physics::Shape::settings
            );

        ns->new_usertype<Physics::ShapesManager>("ShapesManager",
            "CreateMeshShape", &Physics::ShapesManager::CreateShape<Physics::MeshShapeSettings>,
            "CreateSphereShape", &Physics::ShapesManager::CreateShape<Physics::SphereShapeSettings>,
            "CreateBoundingBoxShape", &Physics::ShapesManager::CreateShape<Physics::BoundingBoxShapeSettings>,
            "CreateCapsuleShape", &Physics::ShapesManager::CreateShape<Physics::CapsuleShapeSettings>,
            "CreateHeightFieldShape", &Physics::ShapesManager::CreateShape<Physics::HeightFieldShapeSettings>,
            "CreateConvexHullShape", &Physics::ShapesManager::CreateShape<Physics::ConvexHullShapeSettings>,
            "CreateCylinderShape", &Physics::ShapesManager::CreateShape<Physics::CylinderShapeSettings>,
            "CreateCompoundShape", &Physics::ShapesManager::CreateShape<Physics::CompoundShapeSettings>
            );

        ns->new_usertype<Physics::BodyCreationSettings>("RigidBodyCreationSettings",
            sol::call_constructor,
            sol::constructors<Physics::BodyCreationSettings>(),
            "objectLayer", &Physics::BodyCreationSettings::objectLayer,
            "motionQuality", &Physics::BodyCreationSettings::motionQuality,
            "linearVelocity", DependentMember<&Physics::BodyCreationSettings::linearVelocity>(),
            "angularVelocity", DependentMember<&Physics::BodyCreationSettings::angularVelocity>(),
            "friction", &Physics::BodyCreationSettings::friction,
            "restitution", &Physics::BodyCreationSettings::restitution,
            "linearDampening", &Physics::BodyCreationSettings::linearDampening,
            "angularDampening", &Physics::BodyCreationSettings::angularDampening,
            "gravityFactor", &Physics::BodyCreationSettings::gravityFactor,  
            "isTrigger", &Physics::BodyCreationSettings::isTrigger,
            "shape", &Physics::BodyCreationSettings::shape
            );

        ns->new_usertype<Physics::ConstraintCreationSettings>("ConstraintCreationSettings",
            sol::call_constructor,
            sol::constructors<Physics::ConstraintCreationSettings>(),
            "type", &Physics::ConstraintCreationSettings::type,
            "point", &Physics::ConstraintCreationSettings::point,
            "otherPoint", &Physics::ConstraintCreationSettings::otherPoint,
            "axis", &Physics::ConstraintCreationSettings::axis,
            "enableLimits", &Physics::ConstraintCreationSettings::enableLimits,
            "limitsMin", &Physics::ConstraintCreationSettings::limitsMin,
            "limitsMax", &Physics::ConstraintCreationSettings::limitsMax,
            "halfConeAngle", &Physics::ConstraintCreationSettings::halfConeAngle,
            "breakForce", &Physics::ConstraintCreationSettings::breakForce
            );

    }

}