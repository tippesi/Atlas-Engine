#ifndef AE_CONTEXT_H
#define AE_CONTEXT_H

#include "System.h"

#include <SDL/include/SDL.h>

namespace Atlas {

	class Context {

	public:
		Context();

	private:
		SDL_GLContext context;

	};

}

#endif