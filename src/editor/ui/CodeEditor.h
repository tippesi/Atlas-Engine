#pragma once

#include "System.h"

#include <imgui.h>

#include <string>
#include <vector>

namespace Atlas::Editor::UI {

    enum class CodeLanguage {
        GLSL = 0,
        Lua
    };

    // Text editor with syntax highlighting and line numbers. It keeps the cursor and undo history of the text
    // with the id of the last call, another id starts again
    class CodeEditor {

    public:
        // Returns whether the text changed. A size of zero fills the available space on that axis
        bool Render(const char* id, std::string& text, ImVec2 size = ImVec2(0.0f, 0.0f));

        // Always read only, e.g. for generated code
        void Render(const char* id, const std::string& text, ImVec2 size = ImVec2(0.0f, 0.0f));

        // True for one frame when the editor lost the focus after the text was changed
        bool IsDeactivatedAfterEdit() const;

        CodeLanguage language = CodeLanguage::GLSL;
        bool readOnly = false;

    private:
        // In the order of the code colors of the theme
        enum class TokenType {
            Keyword = 0,
            Type,
            Builtin,
            Number,
            String,
            Comment,
            Preprocessor
        };

        struct Token {
            int32_t start;
            int32_t end;
            TokenType type;
        };

        struct Snapshot {
            std::string text;
            int32_t cursor;
        };

        bool RenderEditor(const char* id, const std::string& text, std::string* editedText, ImVec2 size);

        void Tokenize();

        void AddToken(int32_t start, int32_t end, TokenType type);

        void HandleMouse(ImVec2 textOrigin, bool hovered, bool held, bool mouseInside);

        void HandleKeyboard(bool canEdit, float visibleHeight);

        void DrawLines(ImVec2 textOrigin, ImVec2 visibleMin, ImVec2 visibleMax, int32_t firstLine, int32_t lastLine, bool focused);

        void Replace(int32_t start, int32_t end, const std::string& value, bool mergeUndo);

        void Restore(std::vector<Snapshot>& from, std::vector<Snapshot>& to);

        void MoveCursor(int32_t offset, bool select);

        int32_t GetLine(int32_t offset) const;

        int32_t GetLineEnd(int32_t line) const;

        int32_t GetColumn(int32_t offset) const;

        int32_t GetOffset(int32_t line, int32_t column) const;

        int32_t GetOffsetAt(ImVec2 position, ImVec2 textOrigin) const;

        int32_t GetWordBoundary(int32_t offset, int32_t direction) const;

        bool IsWordCharacter(int32_t offset) const;

        std::string content;
        std::vector<int32_t> lineStarts = { 0 };
        std::vector<Token> tokens;
        std::vector<int32_t> lineTokens = { 0, 0 };
        int32_t maxColumns = 0;
        int32_t tokenizedLanguage = -1;
        ImGuiID lastId = 0;

        int32_t cursor = 0;
        int32_t anchor = 0;
        int32_t preferredColumn = -1;

        std::vector<Snapshot> undoStack;
        std::vector<Snapshot> redoStack;
        double lastEditTime = 0.0;

        float characterWidth = 0.0f;
        float lineHeight = 0.0f;
        float gutterWidth = 0.0f;
        double blinkStart = 0.0;

        bool changed = false;
        bool scrollToCursor = false;
        bool selecting = false;
        bool wasFocused = false;
        bool editedWhileFocused = false;
        bool deactivatedAfterEdit = false;

        const int32_t tabSize = 4;

    };

}
