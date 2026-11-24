#include "ProjectBindings.h"
#include "MemberBindings.h"

#include "project/Project.h"
#include "scene/Scene.h"

namespace Atlas::Scripting::Bindings {

    void GenerateProjectBindings(Binder* ns) {

        ns->new_usertype<ProjectSettings>("ProjectSettings",
            "streaming", DependentMember<&ProjectSettings::streaming>(),
            "occlusionCulling", &ProjectSettings::occlusionCulling,
            "upscaler", &ProjectSettings::upscaler,
            "meshLodErrorScale", &ProjectSettings::meshLodErrorScale,
            "binarySceneColumns", &ProjectSettings::binarySceneColumns
            );

        // Changes take effect with ApplySettings and ApplyTo(scene) and stay in the file with Save
        ns->new_usertype<Project>("Project", sol::no_constructor,
            "name", &Project::name,
            "assetDirectory", &Project::assetDirectory,
            "mainScript", &Project::mainScript,
            "startScene", &Project::startScene,
            "settings", DependentMember<&Project::settings>(),
            "path", sol::readonly(&Project::path),
            "engineVersion", sol::readonly(&Project::engineVersion),
            "Save", &Project::Save,
            "ApplySettings", &Project::ApplySettings,
            "ApplyTo", &Project::ApplyTo,
            "GetDirectory", &Project::GetDirectory,
            "GetAssetDirectory", &Project::GetAssetDirectory
            );

    }

}
