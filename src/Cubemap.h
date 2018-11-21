#ifndef CUBEMAP_H
#define CUBEMAP_H

#include "System.h"

class Cubemap {

public:
	Cubemap(string right, string left, string top,
		string bottom, string front, string back);

	void Bind(uint32_t unit);

private:
	uint32_t ID;

};

#endif