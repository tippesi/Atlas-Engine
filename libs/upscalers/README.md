# Vendor upscalers

Optional upscalers selected with `PostProcessing::upscaler`. Each one lives in its own folder and is compiled
into the engine only when its SDK is found. Without the SDK, or on a GPU it doesn't support, the engine uses
FSR2 instead (`MainRenderer::IsUpscalerSupported`). The SDKs aren't part of the repository.

| Upscaler | SDK folder | Platforms | Runtime library |
|----------|------------|-----------|-----------------|
| DLSS | `dlss/sdk` or `ATLAS_DLSS_SDK` | Windows, Linux | `nvngx_dlss.dll` / `libnvidia-ngx-dlss.so.*` |
| FSR 4 | `fsr4/sdk` or `ATLAS_FSR4_SDK` | Windows | `amd_fidelityfx_upscaler_vk.dll` |

Set `ATLAS_DLSS=OFF` or `ATLAS_FSR4=OFF` to build without an SDK that is present.

## DLSS

From the [DLSS SDK](https://github.com/NVIDIA/DLSS) copy into `dlss/sdk`:

- `include/`
- `lib/Windows_x86_64/x64/` (`nvsdk_ngx_d.lib`, `nvsdk_ngx_d_dbg.lib`) and `lib/Windows_x86_64/rel/nvngx_dlss.dll`
- `lib/Linux_x86_64/libnvsdk_ngx.a` and `lib/Linux_x86_64/rel/libnvidia-ngx-dlss.so.*`
- `LICENSE.txt`

`lib/Windows_x86_64/dev` contains a DLSS library with debug overlays and hotkeys for checking jitter, motion
vectors and exposure. NGX needs an NVIDIA RTX GPU and loads the DLSS library from the executable folder or the SDK.

## FSR 4

From the [AMD FSR SDK](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK) (2.4 or newer) copy into `fsr4/sdk`:

- `Kits/FidelityFX/api/include/` to `api/include/`
- `Kits/FidelityFX/upscalers/include/` to `upscalers/include/`
- `Kits/FidelityFX/signedbin/amd_fidelityfx_upscaler_vk.dll` to `bin/`
- `Kits/FidelityFX/docs/license.md`

The signed library selects the upscaler version for the GPU: the machine learning FSR 4 on supported AMD GPUs with a
recent driver, FSR 3 otherwise. The log names the version in use.

## Packaging

Ship the runtime library next to the executable together with the SDK license.
