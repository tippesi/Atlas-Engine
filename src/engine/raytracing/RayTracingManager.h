#pragma once

#include "../System.h"
#include "jobsystem/JobGroup.h"

namespace Atlas::RayTracing {

	class RayTracingManager {
		
	public:
		static void Update();

	private:
		static JobGroup bvhUpdateGroup;

	};

}