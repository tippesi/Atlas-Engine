#include "Font.h"

#include <fstream>
#include <sstream>

#define STB_TRUETYPE_IMPLEMENTATION
#include "../libraries/stb/stb_truetype.h"
#include "../libraries/stb/stb_image_write.h"

Font::Font(string filename, int32_t pixelSize, int32_t padding, uint8_t edgeValue) : edgeValue(edgeValue) {

	stbtt_fontinfo font;
	string fontString;
	ifstream fontFile;

	fontFile.open(filename, ios::in | ios::binary);

	if (!fontFile.is_open()) {
#ifdef ENGINE_SHOW_LOG
		EngineLog("Font %s not found", filename.c_str());
#endif
		throw EngineException("Couldn't open font file");
	}
	
	fontFile.seekg(0, fontFile.end);
	int64_t size = fontFile.tellg();
	fontFile.seekg(0);

	char* buffer = new char[size];
	fontFile.read(buffer, size);
	fontFile.close();

	glyphs = new Glyph[FONT_CHARACTER_COUNT];
	characterScales = new vec2[FONT_CHARACTER_COUNT];
	characterSizes = new vec2[FONT_CHARACTER_COUNT];
	characterOffsets = new vec2[FONT_CHARACTER_COUNT];

	int32_t resolution = pixelSize + 2 * padding;

	// Compute next power of two https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
	resolution--;
	resolution |= resolution >> 1;
	resolution |= resolution >> 2;
	resolution |= resolution >> 4;
	resolution |= resolution >> 8;
	resolution |= resolution >> 16;
	resolution++;

	glyphsTexture = new Texture(GL_UNSIGNED_BYTE, resolution, resolution, GL_R8,
		0.0f, GL_CLAMP_TO_EDGE, GL_LINEAR, false, false, FONT_CHARACTER_COUNT);

	if (!stbtt_InitFont(&font, (unsigned char*)buffer, 0)) {
		new EngineException("Failed loading font");
	}

	float scale = (float)stbtt_ScaleForPixelHeight(&font, pixelSize);

	stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);

	ascent = (int32_t)((float)ascent * scale);
	descent = (int32_t)((float)descent * scale);
	lineGap = (int32_t)((float)lineGap * scale);

	pixelDistanceScale = (float)edgeValue / (float)padding;

	for (int32_t i = 0; i < FONT_CHARACTER_COUNT; i++) {

		auto glyph = &glyphs[i];

		glyph->data = new uint8_t[resolution * resolution];

		for (int32_t i = 0; i < resolution * resolution; i++) {
			glyph->data[i] = 0;
		}

		int32_t xOffset, yOffset;
		uint8_t* data = stbtt_GetCodepointSDF(&font, scale, i, padding, edgeValue, pixelDistanceScale,
			&glyph->width, &glyph->height, &xOffset, &yOffset);

		glyph->textureScale.x = (float)glyph->width / (float)resolution;
		glyph->textureScale.y = (float)glyph->height / (float)resolution;

		glyph->offset.x = (float)xOffset;
		glyph->offset.y = (float)yOffset;

		characterScales[i] = glyph->textureScale;
		characterOffsets[i] = glyph->offset;
		characterSizes[i] = vec2((float)glyph->width, (float)glyph->height);

		for (int32_t x = 0; x < glyph->width; x++) {
			for (int32_t y = 0; y < glyph->height; y++) {
				glyph->data[y * resolution + x] = data[y * glyph->width + x];
			}
		}

		for (int32_t j = 0; j < FONT_CHARACTER_COUNT; j++) {
			glyph->kern[j] = (int32_t)((float)stbtt_GetCodepointKernAdvance(&font, i, j) * scale);
		}

		stbtt_GetCodepointHMetrics(&font, i, &glyph->advance, 0);

		glyph->advance *= scale;

		glyphsTexture->SetData(glyph->data, i);

		delete[] data;

	}

	delete[] buffer;

}

Glyph* Font::GetGlyph(char character) {

	uint8_t characterIndex = (uint8_t)character;

	if (characterIndex > FONT_CHARACTER_COUNT)
		return nullptr;

	return &glyphs[characterIndex];

}