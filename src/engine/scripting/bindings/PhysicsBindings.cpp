#include "PhysicsBindings.h"

#include "physics/PhysicsWorld.h"

namespace Atlas::Scripting::Bindings {

    void GeneratePhysicsBindings(sol::table* ns) {

        ns->new_enum<Physics::Layers>("Layers", {
            { "Static", Physics::Layers::Static },
            { "Movable", Physics::Layers::Movable },
            { "NumLayers", Physics::Layers::NumLayers }
            });

        ns->new_enum<Physics::MotionQuality>("Layers", {
            { "Discrete", Physics::MotionQuality::Discrete },
            { "LinearCast", Physics::MotionQuality::LinearCast }
            });

        ns->new_enum<Physics::ShapeType>("ShapeType", {
            { "Mesh", Physics::ShapeType::Mesh },
            { "Sphere", Physics::ShapeType::Sphere },
            { "BoundingBox", Physics::ShapeType::BoundingBox },
            { "Capsule", Physics::ShapeType::Capsule },
            { "HeightField", Physics::ShapeType::HeightField }
            });

        ns->new_usertype<Physics::MeshShapeSettings>("MeshShapeSettings",
            "mesh", &Physics::MeshShapeSettings::mesh,
            "scale", &Physics::MeshShapeSettings::scale
            );

        ns->new_usertype<Physics::SphereShapeSettings>("SphereShapeSettings",
            "radius", &Physics::SphereShapeSettings::radius,
            "density", &Physics::SphereShapeSettings::density,
            "scale", &Physics::SphereShapeSettings::scale
            );

        ns->new_usertype<Physics::BoundingBoxShapeSettings>("BoundingBoxShapeSettings",
            "aabb", &Physics::BoundingBoxShapeSettings::aabb,
            "density", &Physics::BoundingBoxShapeSettings::density,
            "scale", &Physics::BoundingBoxShapeSettings::scale
            );

        ns->new_usertype<Physics::CapsuleShapeSettings>("CapsuleShapeSettings",
            "height", &Physics::CapsuleShapeSettings::height,
            "radius", &Physics::CapsuleShapeSettings::radius,
            "density", &Physics::CapsuleShapeSettings::density,
            "scale", &Physics::CapsuleShapeSettings::scale
            );

        ns->new_usertype<Physics::HeightFieldShapeSettings>("HeightFieldShapeSettings",
            "heightData", &Physics::HeightFieldShapeSettings::heightData,
            "translation", &Physics::HeightFieldShapeSettings::translation,
            "scale", &Physics::HeightFieldShapeSettings::scale
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
            "CreateHeightFieldShape", &Physics::ShapesManager::CreateShape<Physics::HeightFieldShapeSettings>
            );

        ns->new_usertype<Physics::BodyCreationSettings>("RigidBodyCreationSettings",
            "objectLayer", &Physics::BodyCreationSettings::objectLayer,
            "motionQuality", &Physics::BodyCreationSettings::motionQuality,
            "linearVelocity", &Physics::BodyCreationSettings::linearVelocity,
            "angularVelocity", &Physics::BodyCreationSettings::angularVelocity,
            "friction", &Physics::BodyCreationSettings::friction,
            "restitution", &Physics::BodyCreationSettings::restitution,
            "linearDampening", &Physics::BodyCreationSettings::linearDampening,
            "angularDampening", &Physics::BodyCreationSettings::angularDampening,
            "gravityFactor", &Physics::BodyCreationSettings::gravityFactor,  
            "shape", &Physics::BodyCreationSettings::shape
            );

    }

}