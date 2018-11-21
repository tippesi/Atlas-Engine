#include "Font.h"

#include <fstream>
#include <sstream>

#define STB_TRUETYPE_IMPLEMENTATION
#include "../libraries/stb/stb_truetype.h"

Font::Font(string filename, int32_t pixelSize, int32_t padding, uint8_t edgeValue) {

	stbtt_fontinfo font;
	string fontString;
	ifstream fontFile;
	stringstream fontStream;

	fontFile.open(filename, ios::in | ios::binary);

	if (fontFile.is_open()) {
		fontStream << fontFile.rdbuf();
		fontFile.close();
		fontString = fontStream.str();
	}
	else {
#ifdef ENGINE_SHOW_LOG
		EngineLog("Font %s not found", filename.c_str());
#endif
		throw EngineException("Couldn't open font file");
	}

	glyphs = new Glyph[FONT_CHARACTER_COUNT];
	resolutionScales = new vec2[FONT_CHARACTER_COUNT];

	int32_t resolution = pixelSize + 2 * padding;

	glyphsLayered = new Texture(GL_UNSIGNED_BYTE, resolution, resolution, GL_R8,
		0.0f, GL_CLAMP_TO_EDGE, GL_LINEAR, false, false, FONT_CHARACTER_COUNT);

	stbtt_InitFont(&font, (unsigned char*)fontString.c_str(), 0);

	float scale = stbtt_ScaleForPixelHeight(&font, pixelSize);

	stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);

	ascent = (int32_t)((float)ascent * scale);
	descent = (int32_t)((float)descent * scale);;
	lineGap = (int32_t)((float)lineGap * scale);;

	float pixelDistanceScale = (float)edgeValue / (float)padding;

	for (int32_t i = 0; i < FONT_CHARACTER_COUNT; i++) {

		auto glyph = &glyphs[i];

		glyph->data = new uint8_t[resolution * resolution];

		uint8_t* data = stbtt_GetCodepointSDF(&font, scale, i, padding, edgeValue, pixelDistanceScale,
			&glyph->width, &glyph->height, &glyph->xOffset, &glyph->yOffset);

		glyph->resolutionScale.x = (float)resolution / (float)glyph->width;
		glyph->resolutionScale.y = (float)resolution / (float)glyph->height;

		resolutionScales[i] = glyph->resolutionScale;

		for (int32_t x = 0; x < glyph->width; x++) {
			for (int32_t y = 0; y < glyph->height; y++) {
				glyph->data[y * resolution + x] = data[y * glyph->width + x];
			}
		}

		for (int32_t j = 0; j < FONT_CHARACTER_COUNT; j++) {
			glyph->kern[j] = stbtt_GetCodepointKernAdvance(&font, i, j) * scale;
		}

		stbtt_GetCodepointHMetrics(&font, i, &glyph->advance, 0);

		glyph->advance *= scale;

		glyphsLayered->SetData(glyph->data, i);

	}

}