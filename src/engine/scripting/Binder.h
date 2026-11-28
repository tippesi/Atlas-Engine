#pragma once

#include "BindingRegistry.h"

#include <sol/sol.hpp>

#include <functional>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

namespace Atlas::Scripting {

    // The Lua type of a C++ type, resolved at compile time. Bound types are placeholders the registry resolves later
    template<class T>
    struct LuaType {
        static std::string Get() {
            using D = std::remove_cvref_t<std::remove_pointer_t<std::remove_cvref_t<T>>>;
            if constexpr (std::is_void_v<D>)
                return "nil";
            else if constexpr (std::is_same_v<D, bool>)
                return "boolean";
            else if constexpr (std::is_arithmetic_v<D>)
                return "number";
            else if constexpr (std::is_same_v<D, std::string> || std::is_same_v<D, std::string_view> || std::is_same_v<D, char>)
                return "string";
            else if constexpr (std::is_same_v<D, sol::table>)
                return "table";
            else if constexpr (std::is_same_v<D, sol::function> || std::is_same_v<D, sol::protected_function>)
                return "function";
            else if constexpr (std::is_same_v<D, sol::variadic_args>)
                return "...";
            else if constexpr (std::is_base_of_v<sol::reference, D> || std::is_same_v<D, sol::object>)
                return "any";
            else
                return BindingRegistry::Placeholder(typeid(D));
        }
    };

    template<class T> struct LuaType<std::vector<T>> { static std::string Get() { return LuaType<T>::Get() + "[]"; } };
    template<class T> struct LuaType<std::optional<T>> { static std::string Get() { return LuaType<T>::Get() + "?"; } };
    template<class T> struct LuaType<sol::optional<T>> { static std::string Get() { return LuaType<T>::Get() + "?"; } };
    template<class T> struct LuaType<std::shared_ptr<T>> { static std::string Get() { return LuaType<T>::Get(); } };
    template<class T> struct LuaType<sol::as_table_t<T>> { static std::string Get() { return LuaType<T>::Get(); } };
    template<class R, class... A> struct LuaType<std::function<R(A...)>> { static std::string Get() { return "function"; } };
    template<class... T> struct LuaType<std::tuple<T...>> {
        static std::string Get() {
            std::string result;
            ((result += (result.empty() ? "" : ", ") + LuaType<T>::Get()), ...);
            return result;
        }
    };
    template<class A, class B> struct LuaType<std::pair<A, B>> { static std::string Get() { return LuaType<A>::Get() + ", " + LuaType<B>::Get(); } };
    template<class T> struct LuaType<const T> : LuaType<T> {};
    template<class T> struct LuaType<T&> : LuaType<T> {};
    template<class T> struct LuaType<const T&> : LuaType<T> {};
    template<class T> struct LuaType<T*> : LuaType<T> {};

    // Signatures of free functions, member functions and lambdas
    template<class F> struct Callable : Callable<decltype(&F::operator())> {
        static constexpr bool isMember = false;
    };
    template<class R, class... A> struct Callable<R(*)(A...)> {
        using Return = R; using Args = std::tuple<A...>; static constexpr bool isMember = false;
    };
    template<class R, class... A> struct Callable<R(*)(A...) noexcept> : Callable<R(*)(A...)> {};
    template<class R, class... A> struct Callable<R(A...)> : Callable<R(*)(A...)> {};
    template<class R, class... A> struct Callable<R(A...) noexcept> : Callable<R(*)(A...)> {};
    template<class R, class C, class... A> struct Callable<R(C::*)(A...)> {
        using Return = R; using Args = std::tuple<A...>; using Class = C; static constexpr bool isMember = true;
    };
    template<class R, class C, class... A> struct Callable<R(C::*)(A...) const> : Callable<R(C::*)(A...)> {};
    template<class R, class C, class... A> struct Callable<R(C::*)(A...) noexcept> : Callable<R(C::*)(A...)> {};
    template<class R, class C, class... A> struct Callable<R(C::*)(A...) const noexcept> : Callable<R(C::*)(A...)> {};

    template<class F>
    concept HasCallSignature = std::is_function_v<std::remove_pointer_t<F>> || std::is_member_function_pointer_v<F> ||
        requires { &F::operator(); };

    // Lua doesn't see the state, environment and variadic arguments of sol2 as parameters
    template<class T>
    constexpr bool IsHiddenParameter = std::is_same_v<std::remove_cvref_t<T>, sol::this_state> ||
        std::is_same_v<std::remove_cvref_t<T>, sol::this_environment> || std::is_same_v<std::remove_cvref_t<T>, sol::this_main_state>;

    template<class Tuple, size_t First = 0>
    std::string ParameterList() {
        std::string result;
        [&]<size_t... I>(std::index_sequence<I...>) {
            ((IsHiddenParameter<std::tuple_element_t<I + First, Tuple>> ? void() :
                void(result += (result.empty() ? "" : ", ") + LuaType<std::tuple_element_t<I + First, Tuple>>::Get())), ...);
        }(std::make_index_sequence<std::tuple_size_v<Tuple> - First>());
        return result;
    }

    // "fun(number, Vec3): Entity", self is left out of methods
    template<class F>
    std::string FunctionType(bool dropFirst = false) {
        if constexpr (HasCallSignature<F>) {
            using Traits = Callable<F>;
            using Args = typename Traits::Args;
            std::string parameters;
            if constexpr (std::tuple_size_v<Args> > 0)
                parameters = dropFirst ? ParameterList<Args, 1>() : ParameterList<Args>();
            auto returns = std::is_void_v<typename Traits::Return> ? std::string() : ": " + LuaType<typename Traits::Return>::Get();
            return "fun(" + parameters + ")" + returns;
        }
        else {
            return "function";
        }
    }

    // Whether a function bound to type T takes its object first, Lua calls it with :
    template<class T, class F>
    constexpr bool IsMethodOf() {
        if constexpr (std::is_member_function_pointer_v<F>)
            return true;
        else if constexpr (HasCallSignature<F>) {
            using Args = typename Callable<F>::Args;
            if constexpr (std::tuple_size_v<Args> == 0)
                return false;
            else
                return std::is_same_v<std::remove_cvref_t<std::remove_pointer_t<std::remove_cvref_t<std::tuple_element_t<0, Args>>>>, T>;
        }
        return false;
    }

    // The sol2 wrappers bindings use, recognized by their type
    template<class> struct ReadonlyOf { static constexpr bool value = false; };
    template<class X> struct ReadonlyOf<sol::readonly_wrapper<X>> { static constexpr bool value = true; using Inner = X; };
    template<class> struct PropertyOf { static constexpr bool value = false; };
    template<class R, class W> struct PropertyOf<sol::property_wrapper<R, W>> { static constexpr bool value = true; using Getter = R; using Setter = W; };
    template<class> struct FunctionsOf { static constexpr bool value = false; };
    template<class... F> struct FunctionsOf<sol::overload_set<F...>> { static constexpr bool value = true; using Functions = std::tuple<F...>; };
    template<class... F> struct FunctionsOf<sol::factory_wrapper<F...>> { static constexpr bool value = true; using Functions = std::tuple<F...>; };

    template<class T, class V>
    BindingRegistry::Member DescribeValue(const std::string& name) {
        using D = std::remove_cvref_t<V>;
        if constexpr (std::is_member_object_pointer_v<D>) {
            using M = std::remove_reference_t<decltype(std::declval<T&>().*std::declval<D>())>;
            return { name, "field", LuaType<M>::Get() };
        }
        else if constexpr (ReadonlyOf<D>::value) {
            auto member = DescribeValue<T, typename ReadonlyOf<D>::Inner>(name);
            member.kind = "readonly";
            return member;
        }
        else if constexpr (PropertyOf<D>::value) {
            using R = typename PropertyOf<D>::Getter;
            using W = typename PropertyOf<D>::Setter;
            // Getters which return any object, e.g. DependentMember, have the type in the value of their setter
            if constexpr (HasCallSignature<R> && HasCallSignature<W> &&
                std::is_same_v<std::remove_cvref_t<typename Callable<R>::Return>, sol::object>) {
                using Args = typename Callable<W>::Args;
                if constexpr (std::tuple_size_v<Args> >= 2)
                    return { name, "property", LuaType<std::tuple_element_t<1, Args>>::Get() };
                else
                    return { name, "property", "any" };
            }
            else if constexpr (HasCallSignature<R>)
                return { name, "property", LuaType<typename Callable<R>::Return>::Get() };
            else
                return { name, "property", "any" };
        }
        else if constexpr (FunctionsOf<D>::value) {
            using Functions = typename FunctionsOf<D>::Functions;
            std::string type;
            bool method = false;
            [&]<size_t... I>(std::index_sequence<I...>) {
                ((method |= IsMethodOf<T, std::tuple_element_t<I, Functions>>(),
                    type += (type.empty() ? "" : " | ") + FunctionType<std::tuple_element_t<I, Functions>>(
                        IsMethodOf<T, std::tuple_element_t<I, Functions>>() && !std::is_member_function_pointer_v<std::tuple_element_t<I, Functions>>)), ...);
            }(std::make_index_sequence<std::tuple_size_v<Functions>>());
            return { name, name == "new" ? "constructor" : method ? "method" : "function", type };
        }
        else if constexpr (HasCallSignature<D>) {
            auto method = IsMethodOf<T, D>();
            return { name, method ? "method" : "function", FunctionType<D>(method && !std::is_member_function_pointer_v<D>) };
        }
        else {
            return { name, "value", LuaType<D>::Get() };
        }
    }

    // Constructors of sol::constructors<T(A...), ...>
    template<class T, class... Signatures>
    std::string ConstructorTypes(const sol::constructor_list<Signatures...>&) {
        std::string type;
        ((type += (type.empty() ? "" : " | ") + FunctionType<std::add_pointer_t<Signatures>>()), ...);
        return type;
    }

    template<class T>
    class UsertypeBinder;

    // Wraps a table of Lua with the calls the bindings use and records what they bind in the BindingRegistry, the
    // bindings themselves are those of sol2. The path names the table, e.g. Atlas or Host
    class Binder {

    public:
        Binder(sol::table table, const std::string& path) : table(table), path(path) {}

        template<class T, class... Args>
        UsertypeBinder<T> new_usertype(const std::string& name, Args&&... args);

        template<class E>
        void new_enum(const std::string& name, std::initializer_list<std::pair<std::string_view, E>> items) {
            auto enumPath = Join(name);
            BindingRegistry::AddType(typeid(E), enumPath);
            if (BindingRegistry::BeginOwner(enumPath, "enum")) {
                for (const auto& [key, _] : items)
                    BindingRegistry::AddMember(enumPath, { std::string(key), "value", enumPath });
            }
            table.new_enum(name, items);
        }

        // Key value pairs, the values may be of different types like those of sol2
        template<class... Args> requires (sizeof...(Args) >= 2 && sizeof...(Args) % 2 == 0)
        void new_enum(const std::string& name, Args&&... args) {
            auto enumPath = Join(name);
            if (BindingRegistry::BeginOwner(enumPath, "enum"))
                RecordEnumMembers(enumPath, args...);
            table.new_enum(name, std::forward<Args>(args)...);
        }

        template<class F>
        void set_function(const std::string& name, F&& function) {
            if (!BindingRegistry::IsRecorded(path, name)) {
                auto member = DescribeValue<void, F>(name);
                member.kind = "function";
                BindingRegistry::AddMember(path, member);
            }
            table.set_function(name, std::forward<F>(function));
        }

        template<class V>
        void set(const std::string& name, V&& value) {
            if (!BindingRegistry::IsRecorded(path, name))
                BindingRegistry::AddMember(path, DescribeValue<void, V>(name));
            table.set(name, std::forward<V>(value));
        }

        Binder create_named(const std::string& name) {
            return Binder(table.create_named(name), Join(name));
        }

        sol::state_view lua_state() const { return sol::state_view(table.lua_state()); }

        sol::table& GetTable() { return table; }

        const std::string& GetPath() const { return path; }

    private:
        std::string Join(const std::string& name) const { return path.empty() ? name : path + "." + name; }

        template<class Key, class Value, class... Rest>
        static void RecordEnumMembers(const std::string& enumPath, const Key& key, const Value& value, const Rest&... rest) {
            using V = std::remove_cvref_t<Value>;
            if constexpr (std::is_enum_v<V>) {
                BindingRegistry::AddType(typeid(V), enumPath);
                BindingRegistry::AddMember(enumPath, { std::string(key), "value", enumPath });
            }
            else {
                BindingRegistry::AddMember(enumPath, DescribeValue<void, Value>(std::string(key)));
            }
            if constexpr (sizeof...(Rest) >= 2)
                RecordEnumMembers(enumPath, rest...);
        }

        sol::table table;
        std::string path;

    };

    template<class T>
    class UsertypeBinder {

    public:
        UsertypeBinder(sol::usertype<T> usertype, const std::string& path) : usertype(usertype), path(path) {}

        template<class F>
        void set_function(const std::string& name, F&& function) {
            if (!BindingRegistry::IsRecorded(path, name))
                BindingRegistry::AddMember(path, DescribeValue<T, F>(name));
            usertype.set_function(name, std::forward<F>(function));
        }

        template<class V>
        void set(const std::string& name, V&& value) {
            if (!BindingRegistry::IsRecorded(path, name))
                BindingRegistry::AddMember(path, DescribeValue<T, V>(name));
            usertype.set(name, std::forward<V>(value));
        }

        template<class K>
        auto operator[](K&& key) { return usertype[std::forward<K>(key)]; }

        sol::usertype<T>& GetUsertype() { return usertype; }

    private:
        sol::usertype<T> usertype;
        std::string path;

    };

    template<class T, class Key, class Value, class... Rest>
    void RecordUsertypeMembers(const std::string& path, const Key& key, const Value& value, const Rest&... rest) {
        using K = std::remove_cvref_t<Key>;
        if constexpr (std::is_convertible_v<const Key&, std::string_view> && !std::is_same_v<K, sol::meta_function>) {
            BindingRegistry::AddMember(path, DescribeValue<T, Value>(std::string(std::string_view(key))));
        }
        else if constexpr (std::is_same_v<K, sol::call_construction>) {
            if constexpr (requires { ConstructorTypes<T>(value); })
                BindingRegistry::AddMember(path, { "(call)", "constructor", ConstructorTypes<T>(value) });
            else
                BindingRegistry::AddMember(path, DescribeValue<T, Value>("(call)"));
        }
        if constexpr (sizeof...(Rest) >= 2)
            RecordUsertypeMembers<T>(path, rest...);
    }

    template<class T, class First, class... Rest>
    void RecordUsertype(const std::string& path, const First& first, const Rest&... rest) {
        // A leading constructor list or no_constructor isn't a key value pair
        if constexpr (requires { ConstructorTypes<T>(first); }) {
            BindingRegistry::AddMember(path, { "new", "constructor", ConstructorTypes<T>(first) });
            if constexpr (sizeof...(Rest) >= 2)
                RecordUsertypeMembers<T>(path, rest...);
        }
        else if constexpr (std::is_same_v<std::remove_cvref_t<First>, sol::no_construction>) {
            if constexpr (sizeof...(Rest) >= 2)
                RecordUsertypeMembers<T>(path, rest...);
        }
        else if constexpr (sizeof...(Rest) >= 1) {
            RecordUsertypeMembers<T>(path, first, rest...);
        }
    }

    template<class T, class... Args>
    UsertypeBinder<T> Binder::new_usertype(const std::string& name, Args&&... args) {
        auto typePath = Join(name);
        BindingRegistry::AddType(typeid(T), typePath);
        if constexpr (sizeof...(Args) > 0) {
            if (BindingRegistry::BeginOwner(typePath, "type"))
                RecordUsertype<T>(typePath, args...);
        }
        else {
            BindingRegistry::BeginOwner(typePath, "type");
        }
        return UsertypeBinder<T>(table.new_usertype<T>(name, std::forward<Args>(args)...), typePath);
    }

}
