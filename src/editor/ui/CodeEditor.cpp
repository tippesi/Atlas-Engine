#include "CodeEditor.h"
#include "../Theme.h"

#include <imgui_internal.h>
#include <algorithm>
#include <cctype>
#include <string_view>

namespace Atlas::Editor::UI {

    bool CodeEditor::Render(const char* id, std::string& text, ImVec2 size) {

        return RenderEditor(id, text, readOnly ? nullptr : &text, size);

    }

    void CodeEditor::Render(const char* id, const std::string& text, ImVec2 size) {

        RenderEditor(id, text, nullptr, size);

    }

    bool CodeEditor::IsDeactivatedAfterEdit() const {

        return deactivatedAfterEdit;

    }

    bool CodeEditor::RenderEditor(const char* id, const std::string& text, std::string* editedText, ImVec2 size) {

        changed = false;
        deactivatedAfterEdit = false;

        // Another id is another text, e.g. of another node, the editor starts again
        auto editorId = ImGui::GetID(id);
        if (editorId != lastId) {
            lastId = editorId;
            content = text;
            cursor = 0;
            anchor = 0;
            undoStack.clear();
            redoStack.clear();
            wasFocused = false;
            editedWhileFocused = false;
            Tokenize();
        }
        // Text changed outside, e.g. by the undo of the window
        else if (text != content) {
            content = text;
            cursor = std::min(cursor, int32_t(content.size()));
            while (cursor > 0 && cursor < int32_t(content.size()) && (content[cursor] & 0xC0) == 0x80)
                cursor--;
            anchor = cursor;
            undoStack.clear();
            redoStack.clear();
            Tokenize();
        }
        else if (tokenizedLanguage != int32_t(language)) {
            Tokenize();
        }

        ImGui::PushFont(Theme::codeFont ? Theme::codeFont : ImGui::GetFont());
        characterWidth = ImGui::CalcTextSize("M").x;
        lineHeight = std::round(ImGui::GetFontSize() * 1.3f);

        auto lineCount = int32_t(lineStarts.size());
        gutterWidth = float(std::max(int32_t(std::to_string(lineCount).size()), 2) + 2) * characterWidth;

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        if (ImGui::BeginChild(id, size, ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar)) {
            auto window = ImGui::GetCurrentWindow();
            auto padding = ImGui::GetStyle().FramePadding.y;
            auto origin = ImGui::GetCursorScreenPos();
            auto textOrigin = ImVec2(origin.x + gutterWidth + characterWidth, origin.y + padding);
            const auto& inner = window->InnerRect;

            ImGui::Dummy(ImVec2(gutterWidth + float(maxColumns + 2) * characterWidth, float(lineCount) * lineHeight + 2.0f * padding));

            // One item over the visible area owns the mouse, otherwise dragging a selection moves the window
            auto itemId = window->GetID("##Text");
            bool hovered = false, held = false;
            ImGui::ItemAdd(inner, itemId);
            ImGui::ButtonBehavior(inner, itemId, &hovered, &held);
            HandleMouse(textOrigin, hovered, held, inner.Contains(ImGui::GetIO().MousePos));

            auto focused = ImGui::IsWindowFocused();
            if (focused) {
                auto& io = ImGui::GetIO();
                if (editedText) {
                    ImGui::GetCurrentContext()->WantTextInputNextFrame = 1;
                    io.WantTextInput = true;
                }
                ImGui::SetNextFrameWantCaptureKeyboard(true);
                io.WantCaptureKeyboard = true;
                HandleKeyboard(editedText != nullptr, inner.GetHeight());
            }
            if (wasFocused && !focused && editedWhileFocused) {
                deactivatedAfterEdit = true;
                editedWhileFocused = false;
            }
            wasFocused = focused;

            if (scrollToCursor) {
                auto y = padding + float(GetLine(cursor)) * lineHeight;
                if (y < ImGui::GetScrollY())
                    ImGui::SetScrollY(y - padding);
                else if (y + lineHeight > ImGui::GetScrollY() + inner.GetHeight())
                    ImGui::SetScrollY(y + lineHeight + padding - inner.GetHeight());
                auto x = float(GetColumn(cursor)) * characterWidth;
                auto textWidth = inner.GetWidth() - gutterWidth - 2.0f * characterWidth;
                if (x < ImGui::GetScrollX())
                    ImGui::SetScrollX(std::max(0.0f, x - 4.0f * characterWidth));
                else if (x > ImGui::GetScrollX() + textWidth)
                    ImGui::SetScrollX(x - textWidth + 4.0f * characterWidth);
                scrollToCursor = false;
            }

            auto firstLine = std::max(0, int32_t((ImGui::GetScrollY() - padding) / lineHeight));
            auto lastLine = std::min(lineCount - 1, int32_t((ImGui::GetScrollY() + inner.GetHeight()) / lineHeight) + 1);
            DrawLines(textOrigin, inner.Min, inner.Max, firstLine, lastLine, focused);
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        ImGui::PopFont();

        if (changed && editedText)
            *editedText = content;

        return changed;

    }

    void CodeEditor::HandleMouse(ImVec2 textOrigin, bool hovered, bool held, bool mouseInside) {

        const auto& io = ImGui::GetIO();

        if (hovered)
            ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            auto offset = GetOffsetAt(io.MousePos, textOrigin);
            if (io.MouseClickedCount[ImGuiMouseButton_Left] == 2 && IsWordCharacter(offset)) {
                for (anchor = offset; IsWordCharacter(anchor - 1); anchor--);
                for (cursor = offset; IsWordCharacter(cursor); cursor++);
            }
            else {
                MoveCursor(offset, io.KeyShift);
                selecting = true;
            }
            blinkStart = ImGui::GetTime();
        }
        if (selecting && !held)
            selecting = false;
        if (selecting && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            cursor = GetOffsetAt(io.MousePos, textOrigin);
            scrollToCursor = !mouseInside;
        }

    }

    void CodeEditor::DrawLines(ImVec2 textOrigin, ImVec2 visibleMin, ImVec2 visibleMax, int32_t firstLine, int32_t lastLine, bool focused) {

        auto drawList = ImGui::GetWindowDrawList();
        auto cursorLine = GetLine(cursor);
        auto selectionStart = std::min(cursor, anchor);
        auto selectionEnd = std::max(cursor, anchor);
        auto textOffset = (lineHeight - ImGui::GetFontSize()) * 0.5f;

        for (auto line = firstLine; line <= lastLine; line++) {
            auto lineStart = lineStarts[line];
            auto lineEnd = GetLineEnd(line);
            auto y = textOrigin.y + float(line) * lineHeight;

            if (focused && line == cursorLine && selectionStart == selectionEnd)
                drawList->AddRectFilled(ImVec2(visibleMin.x, y), ImVec2(visibleMax.x, y + lineHeight),
                    ImGui::GetColorU32(ImGuiCol_Text, 0.05f));

            if (selectionStart < selectionEnd && selectionStart <= lineEnd && selectionEnd >= lineStart) {
                auto x0 = float(GetColumn(std::max(selectionStart, lineStart))) * characterWidth;
                auto x1 = float(GetColumn(std::min(selectionEnd, lineEnd))) * characterWidth;
                if (selectionEnd > lineEnd)
                    x1 += characterWidth * 0.5f;
                drawList->AddRectFilled(ImVec2(textOrigin.x + x0, y), ImVec2(textOrigin.x + x1, y + lineHeight),
                    ImGui::GetColorU32(ImGuiCol_TextSelectedBg));
            }

            // Tabs are expanded, so the text is drawn in runs between them
            int32_t column = 0;
            auto drawText = [&](int32_t start, int32_t end, ImU32 color) {
                auto runStart = start;
                auto runColumn = column;
                for (auto i = start; i <= end; i++) {
                    if (i < end && content[i] != '\t') {
                        if ((content[i] & 0xC0) != 0x80)
                            column++;
                        continue;
                    }
                    if (i > runStart)
                        drawList->AddText(ImVec2(textOrigin.x + float(runColumn) * characterWidth, y + textOffset),
                            color, content.data() + runStart, content.data() + i);
                    if (i < end)
                        column = (column / tabSize + 1) * tabSize;
                    runStart = i + 1;
                    runColumn = column;
                }
                };

            auto defaultColor = ImGui::GetColorU32(ImGuiCol_Text);
            auto position = lineStart;
            for (auto i = lineTokens[line]; i < lineTokens[line + 1]; i++) {
                const auto& token = tokens[i];
                drawText(position, token.start, defaultColor);
                drawText(token.start, token.end, ImGui::GetColorU32(Theme::Code(int32_t(token.type))));
                position = token.end;
            }
            drawText(position, lineEnd, defaultColor);
        }

        auto blink = std::fmod(ImGui::GetTime() - blinkStart, 1.2) < 0.8;
        if (focused && blink) {
            auto x = textOrigin.x + float(GetColumn(cursor)) * characterWidth;
            auto y = textOrigin.y + float(cursorLine) * lineHeight;
            drawList->AddLine(ImVec2(x, y), ImVec2(x, y + lineHeight), ImGui::GetColorU32(ImGuiCol_Text), 1.5f);
        }

        // The gutter covers the text that is scrolled to the left
        drawList->AddRectFilled(visibleMin, ImVec2(visibleMin.x + gutterWidth, visibleMax.y), ImGui::GetColorU32(ImGuiCol_FrameBg));
        for (auto line = firstLine; line <= lastLine; line++) {
            auto number = std::to_string(line + 1);
            auto x = visibleMin.x + gutterWidth - float(number.size() + 1) * characterWidth;
            auto color = line == cursorLine ? ImGui::GetColorU32(ImGuiCol_Text) : ImGui::GetColorU32(Theme::TextMuted(0.7f));
            drawList->AddText(ImVec2(x, textOrigin.y + float(line) * lineHeight + textOffset), color, number.c_str());
        }

    }

    void CodeEditor::Tokenize() {

        const std::string_view glslKeywords[] = { "if", "else", "for", "while", "do", "return", "break", "continue",
            "discard", "switch", "case", "default", "struct", "const", "in", "out", "inout", "uniform", "buffer",
            "layout", "shared", "flat", "precision", "highp", "mediump", "lowp", "true", "false" };
        const std::string_view glslTypes[] = { "void", "bool", "int", "uint", "float", "double", "vec2", "vec3", "vec4",
            "ivec2", "ivec3", "ivec4", "uvec2", "uvec3", "uvec4", "bvec2", "bvec3", "bvec4", "mat2", "mat3", "mat4",
            "mat3x4", "mat4x3", "sampler", "sampler2D", "sampler3D", "samplerCube", "sampler2DArray", "texture2D",
            "texture2DArray", "image2D", "image3D" };
        const std::string_view glslBuiltins[] = { "abs", "sign", "floor", "ceil", "round", "trunc", "fract", "mod",
            "min", "max", "clamp", "mix", "step", "smoothstep", "sqrt", "inversesqrt", "pow", "exp", "exp2", "log",
            "log2", "sin", "cos", "tan", "asin", "acos", "atan", "radians", "degrees", "length", "distance", "dot",
            "cross", "normalize", "reflect", "refract", "transpose", "inverse", "determinant", "texture", "textureLod",
            "texelFetch", "textureSize", "imageLoad", "imageStore", "dFdx", "dFdy", "fwidth", "any", "all", "saturate" };
        const std::string_view luaKeywords[] = { "and", "break", "do", "else", "elseif", "end", "false", "for",
            "function", "goto", "if", "in", "local", "nil", "not", "or", "repeat", "return", "then", "true", "until", "while" };
        const std::string_view luaBuiltins[] = { "print", "pairs", "ipairs", "type", "tostring", "tonumber", "require",
            "select", "next", "error", "assert", "pcall", "setmetatable", "getmetatable", "math", "string", "table", "self" };

        auto isGLSL = language == CodeLanguage::GLSL;
        auto size = int32_t(content.size());

        lineStarts = { 0 };
        maxColumns = 0;
        for (int32_t i = 0; i < size; i++) {
            if (content[i] == '\n')
                lineStarts.push_back(i + 1);
        }
        for (int32_t line = 0; line < int32_t(lineStarts.size()); line++)
            maxColumns = std::max(maxColumns, GetColumn(GetLineEnd(line)));

        tokens.clear();
        auto findEnd = [&](int32_t from, std::string_view end) {
            auto position = content.find(end, from);
            return position == std::string::npos ? size : int32_t(position + end.size());
            };
        auto lineEndFrom = [&](int32_t from) {
            auto position = content.find('\n', from);
            return position == std::string::npos ? size : int32_t(position);
            };

        auto lineBegin = true;
        for (int32_t i = 0; i < size;) {
            auto c = content[i];
            auto next = i + 1 < size ? content[i + 1] : '\0';
            if (c == '\n' || std::isspace(uint8_t(c))) {
                lineBegin |= c == '\n';
                i++;
                continue;
            }

            auto start = i;
            if (isGLSL && lineBegin && c == '#') {
                i = lineEndFrom(i);
                AddToken(start, i, TokenType::Preprocessor);
            }
            else if ((isGLSL && c == '/' && next == '/') || (!isGLSL && c == '-' && next == '-')) {
                auto block = !isGLSL && content.compare(i + 2, 2, "[[") == 0;
                i = block ? findEnd(i + 4, "]]") : lineEndFrom(i);
                AddToken(start, i, TokenType::Comment);
            }
            else if (isGLSL && c == '/' && next == '*') {
                i = findEnd(i + 2, "*/");
                AddToken(start, i, TokenType::Comment);
            }
            else if (c == '"' || (!isGLSL && c == '\'')) {
                for (i++; i < size && content[i] != c && content[i] != '\n'; i++)
                    i += content[i] == '\\' ? 1 : 0;
                i = std::min(i + 1, size);
                AddToken(start, i, TokenType::String);
            }
            else if (!isGLSL && c == '[' && next == '[') {
                i = findEnd(i + 2, "]]");
                AddToken(start, i, TokenType::String);
            }
            else if (std::isdigit(uint8_t(c)) || (c == '.' && std::isdigit(uint8_t(next)))) {
                for (i++; i < size; i++) {
                    auto d = content[i];
                    auto exponentSign = (d == '+' || d == '-') && (content[i - 1] == 'e' || content[i - 1] == 'E');
                    if (!std::isalnum(uint8_t(d)) && d != '.' && !exponentSign)
                        break;
                }
                AddToken(start, i, TokenType::Number);
            }
            else if (std::isalpha(uint8_t(c)) || c == '_') {
                while (i < size && (std::isalnum(uint8_t(content[i])) || content[i] == '_'))
                    i++;
                auto word = std::string_view(content).substr(start, i - start);
                auto contains = [&](const auto& words) {
                    return std::find(std::begin(words), std::end(words), word) != std::end(words);
                    };
                if (isGLSL ? contains(glslKeywords) : contains(luaKeywords))
                    AddToken(start, i, TokenType::Keyword);
                else if (isGLSL && contains(glslTypes))
                    AddToken(start, i, TokenType::Type);
                else if (isGLSL ? (contains(glslBuiltins) || word.starts_with("gl_")) : contains(luaBuiltins))
                    AddToken(start, i, TokenType::Builtin);
            }
            else {
                i++;
            }
            lineBegin = false;
        }

        // Index of the first token of each line, the last entry ends the last line
        auto lineCount = int32_t(lineStarts.size());
        lineTokens.resize(lineCount + 1);
        int32_t token = 0;
        for (int32_t line = 0; line < lineCount; line++) {
            lineTokens[line] = token;
            while (token < int32_t(tokens.size()) && tokens[token].start <= GetLineEnd(line))
                token++;
        }
        lineTokens[lineCount] = token;

        tokenizedLanguage = int32_t(language);

    }

    void CodeEditor::AddToken(int32_t start, int32_t end, TokenType type) {

        // Tokens never span lines, such that each line can be drawn on its own
        while (start < end) {
            auto lineEnd = std::min(GetLineEnd(GetLine(start)), end);
            if (lineEnd > start)
                tokens.push_back({ start, lineEnd, type });
            start = lineEnd + 1;
        }

    }

    void CodeEditor::HandleKeyboard(bool canEdit, float visibleHeight) {

        auto& io = ImGui::GetIO();
        auto isOsx = io.ConfigMacOSXBehaviors;
        auto wordMove = isOsx ? io.KeyAlt : io.KeyCtrl;
        auto startEnd = isOsx && io.KeyCtrl && !io.KeyAlt;
        auto shortcut = io.KeyCtrl && !io.KeyAlt;
        auto shift = io.KeyShift;

        auto line = GetLine(cursor);
        auto lineStart = lineStarts[line];
        auto selectionStart = std::min(cursor, anchor);
        auto selectionEnd = std::max(cursor, anchor);
        auto hasSelection = selectionStart != selectionEnd;

        auto moveLines = [&](int32_t lines) {
            auto column = preferredColumn >= 0 ? preferredColumn : GetColumn(cursor);
            auto target = line + lines;
            auto offset = target < 0 ? 0 : target >= int32_t(lineStarts.size()) ? int32_t(content.size()) :
                GetOffset(target, column);
            MoveCursor(offset, shift);
            preferredColumn = column;
            };

        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
            if (hasSelection && !shift)
                MoveCursor(selectionStart, false);
            else
                MoveCursor(startEnd ? lineStart : wordMove ? GetWordBoundary(cursor, -1) : std::max(cursor - 1, 0), shift);
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
            if (hasSelection && !shift)
                MoveCursor(selectionEnd, false);
            else
                MoveCursor(startEnd ? GetLineEnd(line) : wordMove ? GetWordBoundary(cursor, 1) :
                    std::min(cursor + 1, int32_t(content.size())), shift);
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
            startEnd ? MoveCursor(0, shift) : moveLines(-1);
        else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
            startEnd ? MoveCursor(int32_t(content.size()), shift) : moveLines(1);
        else if (ImGui::IsKeyPressed(ImGuiKey_PageUp))
            moveLines(-int32_t(visibleHeight / lineHeight));
        else if (ImGui::IsKeyPressed(ImGuiKey_PageDown))
            moveLines(int32_t(visibleHeight / lineHeight));
        else if (ImGui::IsKeyPressed(ImGuiKey_Home))
            MoveCursor(io.KeyCtrl ? 0 : lineStart, shift);
        else if (ImGui::IsKeyPressed(ImGuiKey_End))
            MoveCursor(io.KeyCtrl ? int32_t(content.size()) : GetLineEnd(line), shift);
        else if (ImGui::IsKeyPressed(ImGuiKey_Escape))
            anchor = cursor;
        else if (shortcut && ImGui::IsKeyPressed(ImGuiKey_A)) {
            anchor = 0;
            cursor = int32_t(content.size());
        }
        else if (shortcut && hasSelection && (ImGui::IsKeyPressed(ImGuiKey_C) || ImGui::IsKeyPressed(ImGuiKey_X))) {
            ImGui::SetClipboardText(content.substr(selectionStart, selectionEnd - selectionStart).c_str());
            if (canEdit && ImGui::IsKeyPressed(ImGuiKey_X))
                Replace(selectionStart, selectionEnd, "", false);
        }

        if (!canEdit)
            return;

        if (shortcut && ImGui::IsKeyPressed(ImGuiKey_V)) {
            std::string value = ImGui::GetClipboardText() ? ImGui::GetClipboardText() : "";
            std::erase(value, '\r');
            Replace(selectionStart, selectionEnd, value, false);
        }
        else if (shortcut && ImGui::IsKeyPressed(ImGuiKey_Z))
            shift ? Restore(redoStack, undoStack) : Restore(undoStack, redoStack);
        else if (shortcut && ImGui::IsKeyPressed(ImGuiKey_Y))
            Restore(redoStack, undoStack);
        else if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) {
            // The new line keeps the indentation
            auto indentation = lineStart;
            while (indentation < selectionStart && (content[indentation] == ' ' || content[indentation] == '\t'))
                indentation++;
            Replace(selectionStart, selectionEnd, "\n" + content.substr(lineStart, indentation - lineStart), false);
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_Tab)) {
            auto spaces = tabSize - GetColumn(selectionStart) % tabSize;
            Replace(selectionStart, selectionEnd, std::string(spaces, ' '), false);
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
            auto start = hasSelection ? selectionStart : startEnd ? lineStart : wordMove ? GetWordBoundary(cursor, -1) : cursor - 1;
            while (!hasSelection && start > 0 && (content[start] & 0xC0) == 0x80)
                start--;
            if (start >= 0)
                Replace(start, selectionEnd, "", !hasSelection);
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
            auto end = hasSelection ? selectionEnd : wordMove ? GetWordBoundary(cursor, 1) : cursor + 1;
            while (end < int32_t(content.size()) && (content[end] & 0xC0) == 0x80)
                end++;
            if (end <= int32_t(content.size()))
                Replace(selectionStart, end, "", !hasSelection);
        }

        auto ignoreCharacters = (io.KeyCtrl && !io.KeyAlt) || (isOsx && io.KeyCtrl);
        for (auto character : io.InputQueueCharacters) {
            if (ignoreCharacters || character < 32 || character == 127)
                continue;
            char buffer[5] = {};
            ImTextCharToUtf8(buffer, character);
            Replace(std::min(cursor, anchor), std::max(cursor, anchor), buffer, true);
        }
        io.InputQueueCharacters.resize(0);

    }

    void CodeEditor::Replace(int32_t start, int32_t end, const std::string& value, bool mergeUndo) {

        if (start == end && value.empty())
            return;

        // Typing within a second becomes one undo step
        auto time = ImGui::GetTime();
        if (!mergeUndo || time - lastEditTime > 1.0 || undoStack.empty())
            undoStack.push_back({ content, cursor });
        if (undoStack.size() > 200)
            undoStack.erase(undoStack.begin());
        redoStack.clear();
        lastEditTime = mergeUndo ? time : 0.0;

        content.replace(start, end - start, value);
        Tokenize();
        MoveCursor(start + int32_t(value.size()), false);
        changed = true;
        editedWhileFocused = true;

    }

    void CodeEditor::Restore(std::vector<Snapshot>& from, std::vector<Snapshot>& to) {

        if (from.empty())
            return;

        to.push_back({ content, cursor });
        auto snapshot = from.back();
        from.pop_back();
        content = snapshot.text;
        Tokenize();
        MoveCursor(std::min(snapshot.cursor, int32_t(content.size())), false);
        lastEditTime = 0.0;
        changed = true;
        editedWhileFocused = true;

    }

    void CodeEditor::MoveCursor(int32_t offset, bool select) {

        // Never end up within a multi byte character
        while (offset > 0 && offset < int32_t(content.size()) && (content[offset] & 0xC0) == 0x80)
            offset += offset > cursor ? 1 : -1;

        cursor = offset;
        if (!select)
            anchor = offset;
        preferredColumn = -1;
        scrollToCursor = true;
        blinkStart = ImGui::GetTime();

    }

    int32_t CodeEditor::GetLine(int32_t offset) const {

        return int32_t(std::upper_bound(lineStarts.begin(), lineStarts.end(), offset) - lineStarts.begin()) - 1;

    }

    int32_t CodeEditor::GetLineEnd(int32_t line) const {

        return line + 1 < int32_t(lineStarts.size()) ? lineStarts[line + 1] - 1 : int32_t(content.size());

    }

    int32_t CodeEditor::GetColumn(int32_t offset) const {

        int32_t column = 0;
        for (auto i = lineStarts[GetLine(offset)]; i < offset; i++) {
            if (content[i] == '\t')
                column = (column / tabSize + 1) * tabSize;
            else if ((content[i] & 0xC0) != 0x80)
                column++;
        }
        return column;

    }

    int32_t CodeEditor::GetOffset(int32_t line, int32_t column) const {

        auto end = GetLineEnd(line);
        int32_t current = 0;
        for (auto i = lineStarts[line]; i < end; i++) {
            if ((content[i] & 0xC0) == 0x80)
                continue;
            auto next = content[i] == '\t' ? (current / tabSize + 1) * tabSize : current + 1;
            if (column < next)
                return 2 * (column - current) <= next - current ? i : i + 1;
            current = next;
        }
        return end;

    }

    int32_t CodeEditor::GetOffsetAt(ImVec2 position, ImVec2 textOrigin) const {

        auto line = std::clamp(int32_t(std::floor((position.y - textOrigin.y) / lineHeight)), 0, int32_t(lineStarts.size()) - 1);
        auto column = std::max(0, int32_t(std::round((position.x - textOrigin.x) / characterWidth)));
        return GetOffset(line, column);

    }

    int32_t CodeEditor::GetWordBoundary(int32_t offset, int32_t direction) const {

        // Skips the spaces and symbols up to the next word and then the word
        auto at = direction < 0 ? -1 : 0;
        while (offset + at >= 0 && offset + at < int32_t(content.size()) && !IsWordCharacter(offset + at))
            offset += direction;
        while (IsWordCharacter(offset + at))
            offset += direction;
        return offset;

    }

    bool CodeEditor::IsWordCharacter(int32_t offset) const {

        if (offset < 0 || offset >= int32_t(content.size()))
            return false;
        auto c = uint8_t(content[offset]);
        return std::isalnum(c) || c == '_' || c >= 0x80;

    }

}
