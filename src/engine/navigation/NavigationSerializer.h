#pragma once

#include "common/SerializationHelper.h"

#include "Navigation.h"

namespace Atlas::Navigation {

    void to_json(json& j, const NavigationSettings& p);

    void from_json(const json& j, NavigationSettings& p);

    void to_json(json& j, const Navigation& p);

    void from_json(const json& j, Navigation& p);

}
