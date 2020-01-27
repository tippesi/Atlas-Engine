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

	vec3 Viewport::Project(vec3 point, Camera* camera) {

		vec4 transformed = camera->unjitterdProjection * camera->viewMatrix * vec4(point, 1.0f);

		transformed.w = 1.0f / transformed.w;
		transformed.x *= transformed.w;
		transformed.y *= transformed.w;
		transformed.z *= transformed.w;

		transformed = 0.5f * transformed + 0.5f;

		transformed.y = 1.0f - transformed.y;

		float fX = (float)x;
		float fY = (float)y;
		float fWidth = (float)width;
		float fHeight = (float)height;

		transformed = glm::clamp(transformed, 0.0f, 1.0f);

		transformed.x *= fWidth;
		transformed.y *= fHeight;

		transformed.x += fX;
		transformed.y += fY;

		return vec3(transformed);

	}

	vec3 Viewport::Unproject(vec3 point, Camera *camera) {

		float fWidth = (float) width + (float) x;
		float fHeight = (float) height + (float) y;
		float fX = (float) x;
		float fY = (float) y;

		point.x = glm::clamp(point.x, fX, fWidth);
		point.y = glm::clamp(point.y, fY, fHeight);

		point.z = glm::clamp(2.0f * point.z - 1.0f, -1.0f, 1.0f);
		point.x = 2.0f * (point.x - fX) / (fWidth - fX) - 1.0f;
		point.y = 2.0f * (point.y - fY) / (fHeight - fY) - 1.0f;

		point.y *= -1.0f;

		vec4 transformed = camera->invViewMatrix * camera->invUnjitteredProjection * vec4(point, 1.0f);

		transformed.w = 1.0f / transformed.w;
		transformed.x *= transformed.w;
		transformed.y *= transformed.w;
		transformed.z *= transformed.w;

		return vec3(transformed);

	}

}