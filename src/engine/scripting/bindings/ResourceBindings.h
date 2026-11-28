#pragma once

#include "../LuaScriptManager.h"
#include "../Binder.h"

#include "resource/ResourceManager.h"

namespace Atlas::Scripting::Bindings {

    void GenerateResourceManagerBindings(Binder* ns);

    void GenerateAssetLoaderBindings(Binder* ns);

    void GenerateModelImporterBindings(Binder* ns);

    template<class T>
    void GenerateResourceBinding(Binder* ns, const std::string& name) {

        auto type = ns->new_usertype<ResourceHandle<T>>(name,
            "IsValid", &ResourceHandle<T>::IsValid,
            "IsLoaded", &ResourceHandle<T>::IsLoaded,
            "WaitForLoad", &ResourceHandle<T>::WaitForLoad,
            "GetID", &ResourceHandle<T>::GetID,
            "Reset", &ResourceHandle<T>::Reset
        );

        type.set_function("GetResource", sol::resolve<Ref<Resource<T>>&(void)>(&ResourceHandle<T>::GetResource));
        type.set_function("Get", sol::resolve<Ref<T>&(void)>(&ResourceHandle<T>::Get));

    }

}