#pragma once

#include "../System.h"
#include "jobsystem/JobGroup.h"

#include "terrain/Terrain.h"

namespace Atlas::Terrain {

	class TerrainManager {

	public:
		static void Update();

		static void WaitForJobCompletion();

		static bool enable;

	private:
		static JobGroup loadTerrainCellGroup;

	};

}