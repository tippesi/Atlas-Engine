#ifndef VIEWPORT_H
#define VIEWPORT_H

#include "system.h"


class Viewport {

public:
	///
	///
	///
	Viewport(int32_t x, int32_t y, int32_t width, int32_t height) : x(x), y(y), width(width), height(height) {}
	
	///
	///
	///
	void Set(int32_t x, int32_t y, int32_t width, int32_t height) { this->x = x; this->y = y; this->width = width; this->height = height; }

	int32_t x;
	int32_t y;

	int32_t width;
	int32_t height;

};

#endif