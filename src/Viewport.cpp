#include "Viewport.h"

namespace Atlas {

	Viewport::Viewport(int32_t x, int32_t y, int32_t width, int32_t height) :
			x(x), y(y), width(width), height(height) {


	}

	void Viewport::Set(int32_t x, int32_t y, int32_t width, int32_t height) {

		this->x = x;
		this->y = y;
		this->width = width;
		this->height = height;

	}

	vec3 Viewport::Unproject(vec3 point, Camera *camera) {

		float fWidth = (float) width + (float) x;
		float fHeight = (float) height + (float) y;
		float fX = (float) x;
		float fY = (float) y;

		if (point.x > fWidth || point.x < fX ||
			point.y > fHeight || point.y < fY) {

			point.x = fX + (fWidth - fX) / 2.0f;
			point.y = fY + (fHeight - fY) / 2.0f;

		}

		point.z = glm::clamp(2.0f * point.z - 1.0f, -1.0f, 1.0f);
		point.x = 2.0f * (point.x - fX) / (fWidth - fX) - 1.0f;
		point.y = 2.0f * (point.y - fY) / (fHeight - fY) - 1.0f;

		point.y *= -1.0f;

		vec4 transformed = camera->inverseViewMatrix * camera->inverseProjectionMatrix * vec4(point, 1.0f);

		transformed.w = 1.0f / transformed.w;
		transformed.x *= transformed.w;
		transformed.y *= transformed.w;
		transformed.z *= transformed.w;

		return vec3(transformed);

	}

}