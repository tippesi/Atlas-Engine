#pragma once

#include "LuaScriptManager.h"
#include "resource/ResourceManager.h"

namespace Atlas::Scripting {
    class LuaScriptBindings {
    public:
        LuaScriptBindings(Ref<sol::state> luaState, sol::table* atlasNs, sol::table* glmNs);

        void GenerateBindings();

    private:
        void GenerateSceneBindings(sol::table* ns);

        void GenerateEntityBindings(sol::table* ns);

        void GenerateComponentBindings(sol::table* ns);

        void GenerateUtilityBindings(sol::table* ns);

        void GenerateMaterialBindings(sol::table* ns);

        void GenerateAudioBindings(sol::table* ns);

        void GenerateMeshBindings(sol::table* ns);

        void GenerateMathBindings(sol::table* ns);

        void GenerateVolumeBindings(sol::table* ns);

        template<class T> 
        void GenerateResourceBinding(sol::table* ns, const std::string& name);

        template<class T, class S, typename... Args1, typename... Args2>
        sol::usertype<T> GenerateGlmTypeBinding(sol::table* ns, const std::string& name,
            const sol::constructor_list<Args1...> constructors, Args2&&... args);

        Ref<sol::state> luaState;
        sol::table* atlasNs;
        sol::table* glmNs;
    };

    template<class T> 
    void LuaScriptBindings::GenerateResourceBinding(sol::table* ns, const std::string& name) {

        auto type = ns->new_usertype<ResourceHandle<T>>(name,
            "IsValid", &ResourceHandle<T>::IsValid,
            "IsLoaded", &ResourceHandle<T>::IsLoaded,
            "WaitForLoad", &ResourceHandle<T>::WaitForLoad,
            "GetID", &ResourceHandle<T>::GetID
            );

        type.set_function("GetResource", sol::resolve<Ref<Resource<T>>&(void)>(&ResourceHandle<T>::GetResource) );
        type.set_function("Get", sol::resolve<Ref<T>&(void)>(&ResourceHandle<T>::Get) );

    }

    template<class T, class S, typename... Args1, typename... Args2>
    sol::usertype<T> LuaScriptBindings::GenerateGlmTypeBinding(sol::table* ns, const std::string& name,
        const sol::constructor_list<Args1...> constructors, Args2&&... args) {

        auto multiplication_overloads = sol::overload(
            [](const T& v0, const T& v1) { return v0 * v1; },
            [](const T& v0, S value) { return v0 * value; },
            [](const T& value, const T& v0) { return v0 * value; }
        );

        auto division_overloads = sol::overload(
            [](const T& v0, const T& v1) { return v0 / v1; },
            [](const T& v0, S value) { return v0 / value; },
            [](const T& value, const T& v0) { return value / v0; }
        );

        auto addition_overloads = sol::overload(
            [](const T& v0, const T& v1) { return v0 + v1; },
            [](const T& v0, S value) { return v0 + value; },
            [](const T& value, const T& v0) { return v0 + value; }
        );

        auto substraction_overloads = sol::overload(
            [](const T& v0, const T& v1) { return v0 - v1; },
            [](const T& v0, S value) { return v0 - value; },
            [](const T& value, const T& v0) { return v0 - value; }
        );

        return ns->new_usertype<T>(name,
            sol::call_constructor,
            constructors,
            sol::meta_function::multiplication, multiplication_overloads,
            sol::meta_function::division, division_overloads,
            sol::meta_function::addition, addition_overloads,
            sol::meta_function::subtraction, substraction_overloads,
            std::forward<Args2>(args)...);

    }

    template <typename T>
		inline decltype(auto) deref(T&& item) {
			using Tu = sol::meta::unqualified_t<T>;
			if constexpr (sol::meta::is_pointer_like_v<Tu>) {
				return *std::forward<T>(item);
			}
			else {
				return std::forward<T>(item);
			}
		}

}