#include "AudioData.h"
#include "AudioManager.h"

#include "../Log.h"
#include "../resource/ResourceLoadException.h"
#include "../loader/AssetLoader.h"

#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_iostream.h>

namespace Atlas {

    namespace Audio {

        AudioData::AudioData() {

            std::memset(&spec, 0, sizeof(SDL_AudioSpec));

        }

        AudioData::AudioData(const std::string& filename) : filename(filename) {

            uint8_t* data;
            uint32_t length;

            auto stream = Loader::AssetLoader::ReadFile(filename, std::ios::in | std::ios::binary);

            if (!stream.is_open()) {
                throw ResourceLoadException(filename, "Couldn't open file stream");
            }

            auto filedata = Loader::AssetLoader::GetFileContent(stream);

            auto io = SDL_IOFromConstMem(filedata.data(), filedata.size());

            if (!io) {
                throw ResourceLoadException(filename, "Couldn't get IO stream interface");
            }

            if (!SDL_LoadWAV_IO(io, true, &spec, &data, &length)) {
                throw ResourceLoadException(filename, "Couldn't load audio data");
            }

            this->data.resize(length / 2);

            std::memcpy(this->data.data(), data, length);

            SDL_free(data);

            // Automatically apply format on load
            isValid = ApplyFormat(AudioManager::audioSpec);

        }

        bool AudioData::ApplyFormat(const SDL_AudioSpec& formatSpec) {

            if (formatSpec.channels == spec.channels &&  formatSpec.format == spec.format &&
                formatSpec.freq == spec.freq) {
                return true;
            }

            return Convert(formatSpec.freq, formatSpec.channels, formatSpec.format);

        }

        bool AudioData::Convert(uint32_t frequency, uint8_t channels, uint32_t format) {

            SDL_AudioSpec dstSpec {
                .format = (SDL_AudioFormat)format,
                .channels = channels,
                .freq = (int32_t)frequency
            };

            uint8_t* dstData = nullptr;
            int32_t dstLength = 0;

            if (SDL_ConvertAudioSamples(&spec, (const uint8_t*)data.data(), (int32_t)data.size() * 2,
                    &dstSpec, &dstData, &dstLength)) {

                spec = dstSpec;

                data.resize(dstLength / 2);
                std::memcpy(data.data(), dstData, dstLength);

                SDL_free(dstData);

                return true;

            }

            Log::Warning("Couldn't convert audio data from " + filename + " internally");

            return false;

        }

        uint8_t AudioData::GetChannelCount() {

            return spec.channels;

        }

        int32_t AudioData::GetSampleSize() {

            return SDL_AUDIO_BITSIZE(spec.format) / 8 * spec.channels;

        }

        int32_t AudioData::GetFrequency() {

            return spec.freq;

        }

    }

}