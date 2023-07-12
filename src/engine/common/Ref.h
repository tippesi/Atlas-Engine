#ifndef AE_REF_H
#define AE_REF_H

#include <memory>

namespace Atlas {

    template<typename T>
    using Ref = std::shared_ptr<T>;

    template<typename T>
    using Scope = std::unique_ptr<T>;

    template<typename T>
    using Weak = std::weak_ptr<T>;

    template<typename T>
    Ref<T> CreateRef(const T& t) {
        return std::make_shared<T>(t);
    }

    template<typename T, typename ...Args>
    Ref<T> CreateRef(Args&&... args) {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }

}

#endif
