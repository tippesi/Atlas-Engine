#pragma once

#include "../Entity.h"
#include "../../System.h"

#include "../../lighting/Light.h"

namespace Atlas {

	namespace Scene {

		namespace Components {

			class LightComponent {

			public:
				LightComponent() = default;
				LightComponent(const LightComponent& that) = default;

				Ref<Lighting::Light> light = nullptr;

			};

		}

	}

}