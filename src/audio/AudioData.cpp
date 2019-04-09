#include "AudioData.h"
#include "AudioManager.h"

#include "../loader/AssetLoader.h"

#include <SDL_audio.h>

namespace Atlas {

    namespace Audio {

		AudioData::AudioData() {

			std::memset(&spec, 0, sizeof(SDL_AudioSpec));

		}

        AudioData::AudioData(std::string filename) {

            uint8_t* data;
            uint32_t length;

            auto stream = Loader::AssetLoader::ReadFile(filename, std::ios::in | std::ios::binary);

            if (!stream.is_open()) {
                throw AtlasException("Error loading audio file");
            }

            auto filedata = Loader::AssetLoader::GetFileContent(stream);

            auto rw = SDL_RWFromMem(filedata.data(), (int32_t)filedata.size());

            if (!rw) {
                throw AtlasException("Error getting RWOPS interface");
            }

            if (!SDL_LoadWAV_RW(rw, 1, &spec, &data, &length)) {
                throw AtlasException("Error loading audio data");
            }

            this->data.resize(length / 2);

            std::memcpy(this->data.data(), data, length);

            SDL_FreeWAV(data);

        }

		void AudioData::ApplyFormat(SDL_AudioSpec& formatSpec) {

			if (formatSpec.channels == spec.channels &&  formatSpec.format == spec.format &&
				formatSpec.freq == spec.freq) {
				return;
			}

			Convert(formatSpec.freq, formatSpec.channels, formatSpec.format);

		}

        bool AudioData::Convert(uint32_t frequency, uint8_t channels, uint32_t format) {

            SDL_AudioCVT cvt;

            if (SDL_BuildAudioCVT(&cvt, spec.format, spec.channels, spec.freq,
                    format, channels, frequency)) {

                std::vector<uint8_t> temporary(data.size() * 2 * cvt.len_mult);

                cvt.buf = temporary.data();
				cvt.len = (int32_t)data.size() * 2;

                std::memcpy(cvt.buf, data.data(), cvt.len);

                SDL_ConvertAudio(&cvt);

                spec.freq = frequency;
                spec.channels = channels;
                spec.format = format;

                data.resize(cvt.len_cvt / 2);
                std::memcpy(data.data(), cvt.buf, cvt.len_cvt);

                return true;

            }

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