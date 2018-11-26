#ifndef CUBEMAP_H
#define CUBEMAP_H

#include "System.h"

class Cubemap {

public:
	///
	/// \param right
	/// \param left
	/// \param top
	/// \param bottom
	/// \param front
	/// \param back
	Cubemap(string right, string left, string top,
		string bottom, string front, string back);

	///
	/// \param unit
	void Bind(uint32_t unit);

private:
	uint32_t ID;

};

#endif