#include "Font.h"

#include <fstream>
#include <sstream>

#define STB_TRUETYPE_IMPLEMENTATION
#include "../libraries/stb/stb_truetype.h"
#include "../libraries/stb/stb_image_write.h"

Font::Font(string filename, float pixelSize, int32_t padding, uint8_t edgeValue) : edgeValue(edgeValue) {

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

	if (!stbtt_InitFont(&font, (unsigned char*)buffer, 0)) {
		new EngineException("Failed loading font");
	}

	firstGlyphBuffer = new Buffer(UNIFORM_BUFFER, sizeof(GlyphInfo), BUFFER_DYNAMIC_STORAGE);
	firstGlyphBuffer->SetSize(GPU_GLYPH_COUNT / 2);

	secondGlyphBuffer = new Buffer(UNIFORM_BUFFER, sizeof(GlyphInfo), BUFFER_DYNAMIC_STORAGE);
	secondGlyphBuffer->SetSize(GPU_GLYPH_COUNT / 2);

	smoothing = 5.0f;

	float scale = (float)stbtt_ScaleForPixelHeight(&font, pixelSize);

	int32_t iAscent, iDescent, iLineGap;
	stbtt_GetFontVMetrics(&font, &iAscent, &iDescent, &iLineGap);

	ascent = (float)iAscent * scale;
	descent = (float)iDescent * scale;
	lineGap = (float)iLineGap * scale;

	lineHeight = ascent - descent + lineGap;

	pixelDistanceScale = (float)edgeValue / (float)padding;

	int32_t range = font.numGlyphs > FONT_GLYPH_COUNT ? FONT_GLYPH_COUNT : font.numGlyphs;

	uint8_t* data[FONT_GLYPH_COUNT];

	int32_t width = 0, height = 0, depth = 0;

	// Load the data and calculate the needed resolution for the texture
	for (int32_t i = font.fontstart; i < range; i++) {

		auto glyph = &glyphs[i];

		glyph->codepoint = i;

		int32_t xOffset, yOffset;
		data[i] = stbtt_GetCodepointSDF(&font, scale, i, padding, edgeValue, pixelDistanceScale,
			&glyph->width, &glyph->height, &xOffset, &yOffset);

        for (int32_t j = 0; j < FONT_GLYPH_COUNT; j++) {
            glyph->kern[j] = (int32_t)((float)stbtt_GetCodepointKernAdvance(&font, i, j) * scale);
        }

        stbtt_GetCodepointHMetrics(&font, i, &glyph->advance, 0);
        glyph->advance = (int32_t)((float)glyph->advance * scale);

        if (data[i] == nullptr) {
			glyph->height = 0;
			glyph->width = 0;
			glyph->offset = vec2(0.0f);
			glyph->textureScale = vec2(0.0f);
			glyph->texArrayIndex = GPU_GLYPH_COUNT;
			continue;
		}

		glyph->offset.x = (float)xOffset;
		glyph->offset.y = (float)yOffset;

		width = glyph->width > width ? glyph->width : width;
		height = glyph->height > height ? glyph->height : height;
		glyph->texArrayIndex = depth++;

	}

	depth = GPU_GLYPH_COUNT < depth ? GPU_GLYPH_COUNT : depth;

	// Create texture and process texture data
	glyphTexture = new Texture2DArray(GL_UNSIGNED_BYTE, width, height, depth, 
			GL_R8, GL_CLAMP_TO_EDGE, GL_LINEAR, false, false);

	for (int32_t i = font.fontstart; i < range; i++) {

        if (data[i] == nullptr)
            continue;

		auto glyph = &glyphs[i];

		glyph->data.resize(width * height);

		for (int32_t i = 0; i < width * height; i++) {
			glyph->data[i] = 0;
		}

		glyph->textureScale.x = (float)glyph->width / (float)width;
		glyph->textureScale.y = (float)glyph->height / (float)height;

		// Hard coded end. All other symbols aren't supported for now.
		// This still supports all the latin characters of UTF-8
		if (glyph->texArrayIndex < depth) {

			for (int32_t x = 0; x < glyph->width; x++) {
				for (int32_t y = 0; y < glyph->height; y++) {
					glyph->data[y * width + x] = data[i][y * glyph->width + x];
				}
			}

			glyphInfo[glyph->texArrayIndex].scale = glyph->textureScale;
			glyphInfo[glyph->texArrayIndex].size = vec2((float)glyph->width, (float)glyph->height);

			glyphTexture->SetData(glyph->data, glyph->texArrayIndex);

		}
		else {

			glyph->texArrayIndex = GPU_GLYPH_COUNT;

		}

		delete[] data[i];

	}

	firstGlyphBuffer->SetData(&glyphInfo[0], 0, GPU_GLYPH_COUNT / 2);
	secondGlyphBuffer->SetData(&glyphInfo[GPU_GLYPH_COUNT / 2], 0, GPU_GLYPH_COUNT / 2);

	delete[] buffer;

}

Font::~Font() {

	delete glyphTexture;
	delete firstGlyphBuffer;
	delete secondGlyphBuffer;

}

Glyph* Font::GetGlyph(char character) {

	uint8_t characterIndex = (uint8_t)character;

	if (characterIndex > FONT_GLYPH_COUNT)
		return nullptr;

	return &glyphs[characterIndex];

}

Glyph* Font::GetGlyphUTF8(const char*& character) {

	uint8_t byte = *character;
	uint32_t unicode = 0;

	if (!(byte & 0x80)) {
		unicode = (uint32_t)byte;
		character++;
	}
	else if (((byte & 0xc0) == 0xc0) && !(byte & 0x20)) {
		unicode = ((byte & 0x1f) << 6);
		character++;
		byte = *character;
		unicode |= (byte & 0x3f);
		character++;
	}
	else if (((byte & 0xe0) == 0xe0) && !(byte & 0x10)) {
		character += 3;
	}
	else {
		character += 4;
	}

	if (unicode > FONT_GLYPH_COUNT)
		return nullptr;

	return &glyphs[unicode];

}

void Font::ComputeDimensions(string text, float scale, float* width, float* height) {

	*width = 0;
	*height =  lineHeight * scale;

	auto ctext = text.c_str();

	auto nextGlyph = GetGlyphUTF8(ctext);

	while (nextGlyph->codepoint) {

		auto glyph = nextGlyph;
		nextGlyph = GetGlyphUTF8(ctext);

		*width += ((float)(glyph->advance + glyph->kern[nextGlyph->codepoint]) * scale);

	}

}