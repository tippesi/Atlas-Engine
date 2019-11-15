#ifndef AE_FONT_H
#define AE_FONT_H

#include "System.h"
#include "texture/Texture2DArray.h"
#include "buffer/Buffer.h"

#include <vector>

#define AE_FONT_GLYPH_COUNT 4096
#define AE_GPU_GLYPH_COUNT 2048

namespace Atlas {

	/**
	 * Represents a character
	 */
	struct Glyph {

		int32_t codepoint;

		int32_t advance;

		int32_t width;
		int32_t height;

		int32_t texArrayIndex;

		vec2 textureScale;
		vec2 offset;

		std::vector<int32_t> kern;

	};

	/**
	 * Handles the font loading and management of characters.
	 */
	class Font {

	public:
		/**
		 * Constructs a font object.
		 */
		Font() {}

		/**
         * Loads a true type font from a file.
         * @param filename The file path to the true type font
         * @param pixelSize The height of the characters in pixels
         * @param padding Extra pixels around the characters which are filled with the distance
         * @param edgeValue The value in range 0-255 where the character is reconstructed (should normally be 127)
         * @note Only fully supports fonts with up to 2048 glyphs.
         * @remark Let's say you have a padding of 5 and an edgeValue of 100 with a pixelSize
         * of 10. A character texture now has a height of about 10 + 2 * 5 pixels where the padding
         * is filled with the distance values to the actual character which is about 10 pixels tall.
         * The rendered shape of the character begins at a distance of 3 from the actual character
         * because 5 - 5 * 100/255 is approximately 3. Everything outside this area can be used
         * as the outline.
         */
		Font(std::string filename, float pixelSize, int32_t padding, uint8_t edgeValue = 127);

		/**
         * Returns the glyph of a character
         * @param character The character which should be returned
         * @return A pointer to a {@link Glyph} object
         */
		Glyph *GetGlyph(char character);

		/**
         * Returns the glyph of a character
         * @param character The character which should be returned (in UTF-8)
         * @return A pointer to a {@link Glyph} object
         * @note The pointer to the character will be advanced to the next character
         * when calling this method. This is done because UTF-8 characters have variable length,
         * varying from 1 to 4 bytes.
         */
		Glyph *GetGlyphUTF8(const char *&character);

		/**
         * Computes the dimensions of a given string
         * @param text The string where the dimensions should be computed
         * @param scale The scale of the text which is later applied for rendering
         * @param width A pointer to a float where the width will be written into
         * @param height A pointer to a float where the height will be written into
         * @note The text is assumed to be UTF-8 encoded.
         */
		void ComputeDimensions(std::string text, float scale, float *width, float *height);

		float lineGap;
		float ascent;
		float descent;

		float lineHeight;

		float smoothness = 1.0f;

		uint8_t edgeValue;

		struct GlyphInfo {
			vec2 scale;
			vec2 size;
		};

		std::vector<GlyphInfo> glyphInfo;

		Texture::Texture2DArray glyphTexture;

		// We need two glyph buffers to store 2048 glyphs
		Buffer::Buffer firstGlyphBuffer;
		Buffer::Buffer secondGlyphBuffer;

	private:
		std::vector<Glyph> glyphs;

	};

}

#endif