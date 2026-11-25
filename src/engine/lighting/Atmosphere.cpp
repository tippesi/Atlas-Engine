#include "Atmosphere.h"

namespace Atlas {

    namespace Lighting {

        Atmosphere::Atmosphere(float height, int32_t probeResolution) : height(height) {

            probe = CreateRef<EnvironmentProbe>(probeResolution);

        }

        void Atmosphere::SetProbeResolution(int32_t resolution) {

            if (resolution != GetProbeResolution())
                probe = CreateRef<EnvironmentProbe>(resolution, probe->GetPosition());

        }

        int32_t Atmosphere::GetProbeResolution() const {

            return probe->GetCubemap().width;

        }

    }

}