#pragma once

#include "System.h"

namespace Atlas::Editor {

	class Guizmo {

	public:
		Guizmo() = default;

		void ApplyOffsetToTransform();

		void RemoveOffsetFromTransform();

		const float* GetSnappingPtr();

		vec3 offset;
		mat4 transform;

		bool snappingEnabled = false;
		vec3 translationSnap = vec3(0.1f);
		float rotationSnap = 1.0f;
		float scaleSnap = 0.01f;

		// Imguizmo translate mode
		int32_t mode = 7;
		bool objectSpace = false;

		bool needEnabled = false;

	};

}