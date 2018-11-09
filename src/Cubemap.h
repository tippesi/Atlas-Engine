#ifndef CUBEMAP_H
#define CUBEMAP_H

#include "System.h"

class Cubemap {

public:
	Cubemap(const char* right, const char* left, const char* top,
		const char* bottom, const char* front, const char* back);

	void Bind(uint32_t unit);

private:
	uint32_t ID;

};

#endif