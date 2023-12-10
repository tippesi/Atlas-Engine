#pragma once

#include "../System.h"
#include "Texture2D.h"
#include "Texture2DArray.h"

#include "../graphics/CommandList.h"

#include <map>
#include <vector>

namespace Atlas {

    namespace Texture {

        class TextureAtlas {

        public:
            /**
             * Constructs a TextureAtlas object.
             * @note Texture atlases are only available as AE_RGBA8.
             */
            TextureAtlas() = default;

            /**
             * Constructs a TextureAtlas object.
             * @param that Another TextureAtlas object.
             * @note Texture atlases are only available as AE_RGBA8.
             */
            TextureAtlas(const TextureAtlas& that);

            /**
             * Constructs a TextureAtlas object.
             * @param that Another TextureAtlas object.
             * @note Texture atlases are only available as AE_RGBA8.
             */
            explicit TextureAtlas(const std::vector<Ref<Texture2D>>& textures,
                int32_t padding = 1, int32_t downscale = 1);

            /**
             * Copies the data from another texture atlas to the texture atlas object.
             * @param that Another texture atlas.
             * @return A reference to the texture atlas.
             * @note The graphics API objects will be changed.
             */
            TextureAtlas& operator=(const TextureAtlas& that);

            void Update(const std::vector<Ref<Texture2D>>& textures);

            void Clear();

            struct Slice {

                int32_t layer;
                ivec2 offset;
                ivec2 size;

            };

            Texture2DArray textureArray;
            std::vector<Ref<Texture2D>> textures;
            std::map<Texture2D*, std::vector<Slice>> slices;

        private:
            struct TextureStructure {
                int32_t width;
                int32_t height;
                int32_t channels;
                Texture2D* texture;
            };

            std::map<Texture2D*, TextureAtlas::Slice> CreateSlicesForAtlasLevel(std::vector<TextureStructure> textures, int32_t level);

            void FillAtlas(Graphics::CommandList* commandList, std::map<Texture2D*, TextureAtlas::Slice> levelSlices);

            int32_t padding = 1;
            int32_t downscale = 1;

            int32_t width = 0;
            int32_t height = 0;
            int32_t channels = 0;
            int32_t layers = 0;

            ivec2 offset;

        };

    }

}