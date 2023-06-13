#include "TerrainStorageCell.h"
#include "TerrainStorage.h"

namespace Atlas {

    namespace Terrain {

        TerrainStorageCell::TerrainStorageCell(TerrainStorage* storage) : storage(storage) {

            

        }

        bool TerrainStorageCell::IsLoaded() {

            if (!heightField.IsValid())
                return false;

            if (!normalMap.IsValid())
                return false;

            return true;

        }

    }

}