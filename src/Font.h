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

	vector<uint8_t> data;

	int32_t kern[FONT_CHARACTER_COUNT];

}Glyph;


class Font {

public:
	///
	/// \param filename The name of the font file.
	/// \param pixelSize
	/// \param padding
	/// \param edgeValue
	Font(string filename, float pixelSize, int32_t padding, uint8_t edgeValue);

	///
	/// \param character
	/// \return
	Glyph* GetGlyph(char character);

	///
	/// \param text The string to compute
	/// \param scale
	/// \param width
	/// \param height
	void ComputeDimensions(string text, float scale, float* width, float* height);

	~Font();

	float lineGap;
	float ascent;
	float descent;

	float lineHeight;

	float pixelDistanceScale;
	uint8_t edgeValue;

	vec2 characterScales[FONT_CHARACTER_COUNT];
	vec2 characterSizes[FONT_CHARACTER_COUNT];

	Texture* glyphsTexture;

private:
	Glyph glyphs[FONT_CHARACTER_COUNT];

};

#endif