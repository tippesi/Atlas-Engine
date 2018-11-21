#ifndef FONT_H
#define FONT_H

#include "../System.h"
#include "../Texture.h"

#define FONT_CHARACTER_COUNT 128

typedef struct Glyph {

	int32_t advance;

	int32_t xOffset;
	int32_t yOffset;
	int32_t width;
	int32_t height;

	vec2 resolutionScale;

	uint8_t* data;

	int32_t kern[FONT_CHARACTER_COUNT];

}Glyph;

class Font {

public:
	Font(const char* filename, int32_t scale, int32_t padding, uint8_t edgeValue);

	int32_t lineGap;
	int32_t ascent;
	int32_t descent;

	vec2* resolutionScales;

private:
	Texture* glyphsLayered;

	Glyph* glyphs;

};

#endif