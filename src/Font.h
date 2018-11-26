#ifndef FONT_H
#define FONT_H

#include "System.h"
#include "Texture.h"

#define FONT_CHARACTER_COUNT 128

typedef struct Glyph {

	int32_t advance;

	int32_t width;
	int32_t height;

	vec2 textureScale;
	vec2 offset;

	uint8_t* data;

	int32_t kern[FONT_CHARACTER_COUNT];

}Glyph;


class Font {

public:
	///
	/// \param filename
	/// \param pixelSize
	/// \param padding
	/// \param edgeValue
	Font(string filename, int32_t pixelSize, int32_t padding, uint8_t edgeValue);

	///
	/// \param character
	/// \return
	Glyph* GetGlyph(char character);

	///
	/// \param text
	/// \param scale
	/// \param width
	/// \param height
	void ComputeDimensions(string text, float scale, int32_t* width, int32_t* height);

	~Font();

	int32_t lineGap;
	int32_t ascent;
	int32_t descent;

	float pixelDistanceScale;
	int32_t edgeValue;

	vec2* characterScales;
	vec2* characterSizes;

	Texture* glyphsTexture;

private:
	Glyph* glyphs;

};

#endif