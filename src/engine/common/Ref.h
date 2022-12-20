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

}

#endif
