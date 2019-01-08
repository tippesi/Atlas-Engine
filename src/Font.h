#ifndef FONT_H
#define FONT_H

#include "System.h"
#include "texture/Texture2DArray.h"
#include "buffer/Buffer.h"

#include <vector>

#define FONT_CHARACTER_COUNT 2048
#define GPU_CHARACTER_COUNT 1024

/**
 * Represents a character
 */
typedef struct Glyph {

	int32_t codepoint;

	int32_t advance;

	int32_t width;
	int32_t height;

	int32_t texArrayIndex;

	vec2 textureScale;
	vec2 offset;

	vector<uint8_t> data;

	int32_t kern[FONT_CHARACTER_COUNT];

}Glyph;

/**
 * Handles the font loading and management of characters.
 */
class Font {

public:

	/**
	 * Loads a true type font from a file.
	 * @param filename The file path to the true type font
	 * @param pixelSize The height of the characters in pixels
	 * @param padding Extra pixels around the characters which are filled with the distance
	 * @param edgeValue The value in range 0-255 where the character is reconstructed
	 * @remark Let's say you have a padding of 5 and an edgeValue of 100 with a pixelSize
	 * of 10. A character texture now has a height of about 10 + 2 * 5 pixels where the padding
	 * is filled with the distance values to the actual character which is about 10 pixels tall.
	 * The rendered shape of the character begins at a distance of 3 from the actual character
	 * because 5 - 5 * 100/255 is approximately 3. Everything outside this area can be used
	 * as the outline.
	 */
	Font(string filename, float pixelSize, int32_t padding, uint8_t edgeValue);

	/**
	 * Returns a specific character
	 * @param character The character which should be returned
	 * @return A pointer to a {@link Glyph} object
	 */
	Glyph* GetGlyph(char character);

	Glyph* GetGlyphUTF8(const char*& character);

	/**
	 * Computes the dimensions of a given string
	 * @param text The string where the dimensions should be computed
	 * @param scale The scale of the text which is later applied for rendering
	 * @param width A pointer to a float where the width will be written into
	 * @param height A pointer to a float where the height will be written into
	 */
	void ComputeDimensions(string text, float scale, float* width, float* height);

	~Font();

	float lineGap;
	float ascent;
	float descent;

	float lineHeight;

	float smoothing;

	float pixelDistanceScale;
	uint8_t edgeValue;

	/**
	 * Will be uploaded into the uniform buffer
	 */
	struct GlyphInfo {
		vec2 scale;
		vec2 size;
	}glyphInfo[GPU_CHARACTER_COUNT];

	Texture2DArray* glyphTexture;
	Buffer* glyphBuffer;

private:
	Glyph glyphs[FONT_CHARACTER_COUNT];

};

#endif