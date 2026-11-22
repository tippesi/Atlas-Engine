#pragma once

#include "System.h"

#include <imgui.h>

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace Atlas::Editor::UI {

    enum class CodeLanguage {
        GLSL = 0,
        Lua
    };

    struct CodeCompletion {
        std::string text;
        std::string kind;
        std::string signature;
        std::string description;
    };

    // The word in front of the cursor and, after a . or :, what it belongs to, e.g. "entity" and ':' for "entity:Get".
    // The qualifier keeps calls and indices ("scene:GetMainCamera()"), text is the code in front of it
    struct CodeCompletionContext {
        std::string prefix;
        std::string qualifier;
        char separator = 0;
        std::string_view text;
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

        // Actions of a toolbar, they happen in the next Render
        void Undo() { requestedAction = Action::Undo; }

        void Redo() { requestedAction = Action::Redo; }

        void OpenSearch(bool replace) { requestedAction = replace ? Action::Replace : Action::Search; }

        // Starts with a text before the editor is rendered, e.g. the file of a window, without undo steps
        void Reset(const std::string& text);

        // Replaces the whole text as one undo step, the caller changes its own copy of the text too
        void SetText(const std::string& text) { Replace(0, int32_t(content.size()), text, false); }

        bool CanUndo() const { return !undoStack.empty(); }

        bool CanRedo() const { return !redoStack.empty(); }

        CodeLanguage language = CodeLanguage::GLSL;
        bool readOnly = false;

        // Suggestions for a context, the editor filters them by the prefix and adds the keywords and builtins of the language
        std::function<std::vector<CodeCompletion>(const CodeCompletionContext& context)> completionProvider;

        // Functions defined in GLSL code with their signatures, the // comment lines above become the description.
        // Overloads are one completion with a signature per line
        static std::vector<CodeCompletion> FindGlslFunctions(const std::string& code);

    private:
        enum class Action {
            None = 0,
            Undo,
            Redo,
            Search,
            Replace
        };

        void RenderSearch(bool canEdit);

        // Replaces the selected match and selects the next one, or replaces all matches as one undo step
        void ReplaceMatch(bool all);

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

        bool HandleCompletionKeys(bool canEdit);

        void UpdateCompletion();

        // Below the line of the cursor, or above it if there is no room below
        void RenderCompletion(ImVec2 position, float lineTop);

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

        // Selects the next match of the search after the selection, or the one before it
        void FindNext(bool forward);

        // Offsets of the matches of the search, ignoring case
        std::vector<int32_t> FindMatches() const;

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

        std::vector<CodeCompletion> completions;
        int32_t completionIndex = 0;
        int32_t completionStart = 0;
        bool completionOpen = false;
        bool completionChosen = false;
        float completionHeight = 0.0f;
        // The font around the editor, descriptions are text and not code
        ImFont* interfaceFont = nullptr;

        bool changed = false;
        bool scrollToCursor = false;
        bool selecting = false;
        bool wasFocused = false;
        bool editedWhileFocused = false;
        bool deactivatedAfterEdit = false;

        // Ctrl+F opens the search above the text, Enter and F3 go to the next match, with Shift to the one before
        bool searchOpen = false;
        bool searchFocusRequested = false;
        bool replaceOpen = false;
        std::string searchText;
        std::string replaceText;
        // Offsets of the matches, found once per frame
        std::vector<int32_t> searchMatches;
        Action requestedAction = Action::None;

        const int32_t tabSize = 4;

        const std::vector<std::string_view> glslKeywords = { "if", "else", "for", "while", "do", "return", "break", "continue",
            "discard", "switch", "case", "default", "struct", "const", "in", "out", "inout", "uniform", "buffer",
            "layout", "shared", "flat", "precision", "highp", "mediump", "lowp", "true", "false" };
        const std::vector<std::string_view> glslTypes = { "void", "bool", "int", "uint", "float", "double", "vec2", "vec3", "vec4",
            "ivec2", "ivec3", "ivec4", "uvec2", "uvec3", "uvec4", "bvec2", "bvec3", "bvec4", "mat2", "mat3", "mat4",
            "mat3x4", "mat4x3", "sampler", "sampler2D", "sampler3D", "samplerCube", "sampler2DArray", "texture2D",
            "texture2DArray", "image2D", "image3D" };
        const std::vector<std::string_view> glslBuiltins = { "abs", "sign", "floor", "ceil", "round", "trunc", "fract", "mod",
            "min", "max", "clamp", "mix", "step", "smoothstep", "sqrt", "inversesqrt", "pow", "exp", "exp2", "log",
            "log2", "sin", "cos", "tan", "asin", "acos", "atan", "radians", "degrees", "length", "distance", "dot",
            "cross", "normalize", "reflect", "refract", "transpose", "inverse", "determinant", "texture", "textureLod",
            "texelFetch", "textureSize", "imageLoad", "imageStore", "dFdx", "dFdy", "fwidth", "any", "all", "saturate" };
        const std::vector<std::string_view> luaKeywords = { "and", "break", "do", "else", "elseif", "end", "false", "for",
            "function", "goto", "if", "in", "local", "nil", "not", "or", "repeat", "return", "then", "true", "until", "while" };
        const std::vector<std::string_view> luaBuiltins = { "print", "pairs", "ipairs", "type", "tostring", "tonumber", "require",
            "select", "next", "error", "assert", "pcall", "setmetatable", "getmetatable", "math", "string", "table", "self" };

    };

}
