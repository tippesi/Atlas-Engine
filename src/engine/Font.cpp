#include "Font.h"
#include "Log.h"

#include "loader/AssetLoader.h"
#include "jobsystem/JobSystem.h"

#include <cstring>

#include <fstream>
#include <sstream>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

namespace Atlas {

    Font::Font(const std::string& filename, float pixelSize, int32_t padding, uint8_t edgeValue) : edgeValue(edgeValue) {

        stbtt_fontinfo font;

        auto fontFile = Loader::AssetLoader::ReadFile(filename, std::ios::in | std::ios::binary);

        if (!fontFile.is_open()) {
            Log::Error("Font not found " + filename);
            return;
        }

        auto buffer = Loader::AssetLoader::GetFileContent(fontFile);

        fontFile.close();

        if (!stbtt_InitFont(&font, (unsigned char *) buffer.data(), 0)) {
            Log::Error("Failed loading font " + filename);
        }

        glyphs.resize(AE_FONT_GLYPH_COUNT);
        glyphInfo.resize(AE_FONT_GLYPH_COUNT);

        glyphBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit, sizeof(GlyphInfo), AE_GPU_GLYPH_COUNT);

        float scale = (float) stbtt_ScaleForPixelHeight(&font, pixelSize);

        int32_t iAscent, iDescent, iLineGap;
        stbtt_GetFontVMetrics(&font, &iAscent, &iDescent, &iLineGap);

        ascent = (float) iAscent * scale;
        descent = (float) iDescent * scale;
        lineGap = (float) iLineGap * scale;

        lineHeight = ascent - descent + lineGap;

        auto pixelDistanceScale = (float) edgeValue / (float) padding;

        int32_t codepointCount = 0;
        int32_t glyphCount = 0;
        // Check the number of codepoints for the total number of glyphs
        while (glyphCount < font.numGlyphs && codepointCount < AE_FONT_GLYPH_COUNT) {
            // Larger zero if there exists a corresponding glyph to the codepoint
            if (stbtt_FindGlyphIndex(&font, codepointCount))
                glyphCount++;
            codepointCount++;
        }

        codepointCount = codepointCount > AE_FONT_GLYPH_COUNT ? AE_FONT_GLYPH_COUNT : codepointCount;

        // The distance fields are independent of each other, stb_truetype only reads the font data
        std::vector<uint8_t*> data(AE_FONT_GLYPH_COUNT, nullptr);
        std::vector<ivec2> offsets(AE_FONT_GLYPH_COUNT, ivec2(0));
        std::vector<int32_t> glyphIndices(AE_FONT_GLYPH_COUNT, 0);

        JobGroup glyphGroup { "Font glyphs" };
        JobSystem::ParallelFor(glyphGroup, codepointCount, 64, [&](JobData&, int32_t i) {
            auto glyph = &glyphs[i];

            glyph->codepoint = i;
            glyphIndices[i] = stbtt_FindGlyphIndex(&font, i);

            data[i] = stbtt_GetGlyphSDF(&font, scale, glyphIndices[i], padding, edgeValue, pixelDistanceScale,
                &glyph->width, &glyph->height, &offsets[i].x, &offsets[i].y);

            int32_t advance;
            stbtt_GetGlyphHMetrics(&font, glyphIndices[i], &advance, 0);
            glyph->advance = (float) advance * scale;
            });
        JobSystem::Wait(glyphGroup);

        int32_t width = 0, height = 0, depth = 0;

        // Texture layers are assigned in codepoint order such that the result doesn't depend on the job order
        for (int32_t i = 0; i < codepointCount; i++) {

            auto glyph = &glyphs[i];

            if (!data[i]) {
                glyph->height = 0;
                glyph->width = 0;
                glyph->offset = vec2(0.0f);
                glyph->textureScale = vec2(0.0f);
                glyph->texArrayIndex = AE_GPU_GLYPH_COUNT;
                continue;
            }

            glyph->offset = vec2(offsets[i]);

            width = glyph->width > width ? glyph->width : width;
            height = glyph->height > height ? glyph->height : height;
            glyph->texArrayIndex = depth++;

        }

        // Only pairs with a kerning value are stored. The lookups use glyph indices and run in parallel,
        // one row of pairs per job.
        std::vector<int32_t> kernCodepoints;
        for (int32_t i = 0; i < codepointCount; i++)
            if (glyphIndices[i])
                kernCodepoints.push_back(i);

        std::vector<std::vector<std::pair<uint32_t, float>>> kernRows(kernCodepoints.size());
        JobGroup kernGroup { "Font kerning" };
        JobSystem::ParallelFor(kernGroup, int32_t(kernCodepoints.size()), 64, [&](JobData&, int32_t idx) {
            auto first = kernCodepoints[idx];
            for (auto second : kernCodepoints) {
                auto kern = stbtt_GetGlyphKernAdvance(&font, glyphIndices[first], glyphIndices[second]);
                if (kern)
                    kernRows[idx].emplace_back(KerningKey(first, second), (float) kern * scale);
            }
            });
        JobSystem::Wait(kernGroup);

        for (const auto& row : kernRows)
            for (const auto& [key, kern] : row)
                kerning[key] = kern;

        depth = AE_GPU_GLYPH_COUNT < depth ? AE_GPU_GLYPH_COUNT : depth;

        // Create texture and upload all layers at once
        glyphTexture = Texture::Texture2DArray(width, height, std::max(depth, 1), VK_FORMAT_R8_UNORM,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);

        std::vector<uint8_t> glyphData(size_t(width) * size_t(height) * size_t(std::max(depth, 1)), 0);

        for (int32_t i = 0; i < codepointCount; i++) {

            if (!data[i])
                continue;

            auto glyph = &glyphs[i];

            glyph->textureScale.x = (float) glyph->width / (float) width;
            glyph->textureScale.y = (float) glyph->height / (float) height;

            // Hard coded end. All other symbols aren't supported for now.
            // This still supports all the latin characters of UTF-8
            if (glyph->texArrayIndex < depth) {

                auto layer = glyphData.data() + size_t(glyph->texArrayIndex) * size_t(width) * size_t(height);
                for (int32_t y = 0; y < glyph->height; y++) {
                    std::memcpy(layer + size_t(y) * size_t(width), data[i] + size_t(y) * size_t(glyph->width),
                        size_t(glyph->width));
                }

                glyphInfo[glyph->texArrayIndex].scale = glyph->textureScale;
                glyphInfo[glyph->texArrayIndex].size = vec2((float) glyph->width, (float) glyph->height);

            } else {

                glyph->texArrayIndex = AE_GPU_GLYPH_COUNT;

            }

            stbtt_FreeSDF(data[i], nullptr);

        }

        if (depth > 0)
            glyphTexture.SetData(glyphData, 0, depth);

        glyphBuffer.SetData(&glyphInfo[0], 0, AE_GPU_GLYPH_COUNT);

    }

    Glyph *Font::GetGlyph(char character) {

        auto characterIndex = size_t(character);

        if (characterIndex > AE_FONT_GLYPH_COUNT)
            return nullptr;

        return &glyphs[characterIndex];

    }

    Glyph *Font::GetGlyphUTF8(const char *&character) {

        uint8_t byte = *character;
        uint32_t unicode = 0;

        if (!(byte & 0x80)) {
            unicode = (uint32_t) byte;
            character++;
        } else if (((byte & 0xc0) == 0xc0) && !(byte & 0x20)) {
            unicode = ((byte & 0x1f) << 6);
            character++;
            byte = *character;
            unicode |= (byte & 0x3f);
            character++;
        } else if (((byte & 0xe0) == 0xe0) && !(byte & 0x10)) {
            character += 3;
        } else {
            character += 4;
        }

        if (unicode > AE_FONT_GLYPH_COUNT)
            return nullptr;

        return &glyphs[unicode];

    }

    void Font::ComputeDimensions(std::string text, float scale, float *width, float *height) {

        *width = 0;
        *height = lineHeight * scale;

        auto ctext = text.c_str();

        auto nextGlyph = GetGlyphUTF8(ctext);

        while (nextGlyph->codepoint) {

            auto glyph = nextGlyph;
            nextGlyph = GetGlyphUTF8(ctext);

            *width += (glyph->advance + GetKerning(glyph->codepoint, nextGlyph->codepoint)) * scale;

        }

    }

    float Font::GetKerning(int32_t first, int32_t second) const {

        auto iter = kerning.find(KerningKey(first, second));
        return iter != kerning.end() ? iter->second : 0.0f;

    }

    uint32_t Font::KerningKey(int32_t first, int32_t second) {

        return (uint32_t(first) << 16) | uint32_t(second);

    }

}