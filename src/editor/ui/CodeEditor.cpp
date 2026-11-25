#include "CodeEditor.h"
#include "../Theme.h"
#include "../Singletons.h"
#include "Widgets.h"

#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <algorithm>
#include <cctype>
#include <string_view>
#include <unordered_set>

namespace Atlas::Editor::UI {

    CodeEditor* CodeEditor::focusedEditor = nullptr;
    int32_t CodeEditor::focusedFrame = 0;

    CodeEditor::~CodeEditor() {

        if (focusedEditor == this)
            focusedEditor = nullptr;

    }

    CodeEditor* CodeEditor::GetFocused() {

        return focusedFrame == ImGui::GetFrameCount() ? focusedEditor : nullptr;

    }

    bool CodeEditor::Render(const char* id, std::string& text, ImVec2 size) {

        return RenderEditor(id, text, readOnly ? nullptr : &text, size);

    }

    void CodeEditor::Render(const char* id, const std::string& text, ImVec2 size) {

        RenderEditor(id, text, nullptr, size);

    }

    void CodeEditor::Reset(const std::string& text) {

        content = text;
        cursor = 0;
        anchor = 0;
        undoStack.clear();
        redoStack.clear();
        Tokenize();

    }

    void CodeEditor::GoToLine(int32_t line) {

        line = std::clamp(line - 1, 0, int32_t(lineStarts.size()) - 1);
        anchor = GetLineEnd(line);
        cursor = lineStarts[line];
        scrollToCursor = true;

    }

    bool CodeEditor::IsDeactivatedAfterEdit() const {

        return deactivatedAfterEdit;

    }

    std::vector<CodeCompletion> CodeEditor::FindGlslFunctions(const std::string& code) {

        std::vector<CodeCompletion> functions;

        // Definitions start at the beginning of a line, e.g. "float MaterialGraphRemap(float value, ...) {"
        std::string comment;
        size_t lineStart = 0;
        while (lineStart < code.size()) {
            auto lineEnd = code.find('\n', lineStart);
            if (lineEnd == std::string::npos)
                lineEnd = code.size();
            auto line = code.substr(lineStart, lineEnd - lineStart);
            lineStart = lineEnd + 1;
            if (!line.empty() && line.back() == '\r')
                line.pop_back();

            if (line.starts_with("// ")) {
                comment += (comment.empty() ? "" : " ") + line.substr(3);
                continue;
            }

            auto argumentsStart = line.find('(');
            auto nameStart = line.rfind(' ', argumentsStart);
            if (!line.empty() && std::isalpha((unsigned char)line[0]) && argumentsStart != std::string::npos &&
                nameStart != std::string::npos && nameStart + 1 < argumentsStart && line.ends_with(") {")) {
                auto name = line.substr(nameStart + 1, argumentsStart - nameStart - 1);
                auto signature = line.substr(0, line.size() - 2);
                auto iter = std::find_if(functions.begin(), functions.end(),
                    [&](const CodeCompletion& completion) { return completion.text == name; });
                if (iter == functions.end())
                    functions.push_back({ name, "Function", signature, comment });
                else if (iter->signature.find(signature) == std::string::npos)
                    iter->signature += "\n" + signature;
            }
            comment.clear();
        }
        return functions;

    }

    bool CodeEditor::RenderEditor(const char* id, const std::string& text, std::string* editedText, ImVec2 size) {

        changed = false;
        deactivatedAfterEdit = false;

        // Another id is another text, e.g. of another node, the editor starts again. A text set before the first render
        // keeps its undo steps
        auto editorId = ImGui::GetID(id);
        if (editorId != lastId) {
            auto prepared = lastId == 0 && content == text;
            lastId = editorId;
            content = text;
            if (!prepared) {
                cursor = 0;
                anchor = 0;
                undoStack.clear();
                redoStack.clear();
            }
            wasFocused = false;
            editedWhileFocused = false;
            completionOpen = false;
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
            completionOpen = false;
            Tokenize();
        }
        else if (tokenizedLanguage != int32_t(language)) {
            Tokenize();
        }

        interfaceFont = ImGui::GetFont();
        ImGui::PushFont(Theme::codeFont ? Theme::codeFont : interfaceFont);
        // ImGui::CalcTextSize rounds up to whole pixels, the advance of the glyphs isn't rounded
        characterWidth = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, 0.0f, "M").x;
        lineHeight = std::round(ImGui::GetFontSize() * 1.3f);

        auto action = std::exchange(requestedAction, Action::None);
        if (editedText && action == Action::Undo)
            Restore(undoStack, redoStack);
        if (editedText && action == Action::Redo)
            Restore(redoStack, undoStack);
        if ((action == Action::FindNext || action == Action::FindPrevious) && !searchText.empty())
            FindNext(action == Action::FindNext);
        if (action == Action::Search || action == Action::Replace) {
            // A selection within a line is searched for
            auto selectionStart = std::min(cursor, anchor);
            auto selectionEnd = std::max(cursor, anchor);
            if (selectionStart != selectionEnd && content.find('\n', selectionStart) >= size_t(selectionEnd))
                searchText = content.substr(selectionStart, selectionEnd - selectionStart);
            searchOpen = true;
            searchFocusRequested = true;
            replaceOpen = action == Action::Replace && editedText;
        }

        searchMatches = searchOpen ? FindMatches() : std::vector<int32_t>();
        if (searchOpen)
            RenderSearch(editedText != nullptr);

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
                focusedEditor = this;
                focusedFrame = ImGui::GetFrameCount();
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

            if (!focused)
                completionOpen = false;
            auto cursorPosition = ImVec2(textOrigin.x + float(GetColumn(cursor)) * characterWidth,
                textOrigin.y + float(GetLine(cursor)) * lineHeight);
            if (completionOpen && inner.Contains(cursorPosition))
                RenderCompletion(ImVec2(cursorPosition.x, cursorPosition.y + lineHeight), cursorPosition.y);
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
                completionOpen = false;
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

            if (searchOpen && !searchText.empty()) {
                auto first = std::lower_bound(searchMatches.begin(), searchMatches.end(), lineStart);
                for (auto iter = first; iter != searchMatches.end() && *iter <= lineEnd; iter++) {
                    auto match = *iter;
                    auto x0 = float(GetColumn(match)) * characterWidth;
                    auto x1 = float(GetColumn(std::min(match + int32_t(searchText.size()), lineEnd))) * characterWidth;
                    drawList->AddRectFilled(ImVec2(textOrigin.x + x0, y), ImVec2(textOrigin.x + x1, y + lineHeight),
                        ImGui::GetColorU32(Theme::Accent(0.3f)));
                }
            }

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

        if (completionOpen && (hasSelection || completionStart > cursor))
            completionOpen = false;
        if (completionOpen && HandleCompletionKeys(canEdit))
            return;
        // Edits keep the suggestions open, e.g. a deleted character only changes the filter
        auto completionWasOpen = completionOpen;

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
        else if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            anchor = cursor;
            searchOpen = false;
        }
        else if (shortcut && ImGui::IsKeyPressed(ImGuiKey_A)) {
            anchor = 0;
            cursor = int32_t(content.size());
            completionOpen = false;
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
        auto completionRequested = shortcut && ImGui::IsKeyPressed(ImGuiKey_Space);
        for (auto character : io.InputQueueCharacters) {
            if (ignoreCharacters || character < 32 || character == 127)
                continue;
            char buffer[5] = {};
            ImTextCharToUtf8(buffer, character);
            Replace(std::min(cursor, anchor), std::max(cursor, anchor), buffer, true);
            // Words and members are completed while they are typed
            completionRequested = character >= 128 || std::isalnum(int(character)) || character == '_' || character == '.' || character == ':';
        }
        io.InputQueueCharacters.resize(0);

        if (completionRequested || (completionWasOpen && changed))
            UpdateCompletion();

    }

    bool CodeEditor::HandleCompletionKeys(bool canEdit) {

        auto& io = ImGui::GetIO();
        if (!io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
            completionIndex = (completionIndex + 1) % int32_t(completions.size());
            completionChosen = true;
            return true;
        }
        if (!io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
            completionIndex = (completionIndex + int32_t(completions.size()) - 1) % int32_t(completions.size());
            completionChosen = true;
            return true;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            completionOpen = false;
            return true;
        }
        // Enter at the end of a word is a new line, unless a suggestion was chosen with the arrows
        auto enter = ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter);
        if (canEdit && ((enter && completionChosen) || ImGui::IsKeyPressed(ImGuiKey_Tab))) {
            Replace(completionStart, cursor, completions[completionIndex].text, false);
            completionOpen = false;
            return true;
        }
        return false;

    }

    void CodeEditor::UpdateCompletion() {

        // The word in front of the cursor and, after a . or :, the expression it belongs to
        CodeCompletionContext context;
        completionStart = cursor;
        while (IsWordCharacter(completionStart - 1))
            completionStart--;
        context.prefix = content.substr(completionStart, cursor - completionStart);
        if (completionStart > 0 && (content[completionStart - 1] == '.' || content[completionStart - 1] == ':')) {
            context.separator = content[completionStart - 1];
            // The whole expression with calls and indices, e.g. scene:GetMainCamera() or children[i + 1]
            auto qualifierStart = completionStart - 1;
            while (qualifierStart > 0) {
                auto c = content[qualifierStart - 1];
                if (c == ')' || c == ']') {
                    auto open = c == ')' ? '(' : '[';
                    auto depth = 0;
                    auto i = qualifierStart - 1;
                    for (; i >= 0; i--) {
                        depth += content[i] == c ? 1 : content[i] == open ? -1 : 0;
                        if (depth == 0)
                            break;
                    }
                    if (i < 0)
                        break;
                    qualifierStart = i;
                    continue;
                }
                if (!IsWordCharacter(qualifierStart - 1) && c != '.' && c != ':')
                    break;
                qualifierStart--;
            }
            context.qualifier = content.substr(qualifierStart, completionStart - 1 - qualifierStart);
            context.text = std::string_view(content).substr(0, qualifierStart);
        }

        completions.clear();
        completionOpen = false;
        if (context.prefix.empty() && !context.separator)
            return;

        auto candidates = completionProvider ? completionProvider(context) : std::vector<CodeCompletion>();
        if (!context.separator) {
            auto addWords = [&](const auto& words, const char* kind) {
                for (const auto& word : words)
                    candidates.push_back({ std::string(word), kind });
                };
            if (language == CodeLanguage::GLSL) {
                addWords(glslKeywords, "Keyword");
                addWords(glslTypes, "Type");
                addWords(glslBuiltins, "Function");
            }
            else {
                addWords(luaKeywords, "Keyword");
                addWords(luaBuiltins, "Builtin");
            }

            // Names in the text, e.g. local variables, except the word being typed
            std::unordered_set<std::string_view> words;
            for (int32_t i = 0; i < int32_t(content.size());) {
                if (!IsWordCharacter(i) || std::isdigit((unsigned char)content[i])) {
                    i++;
                    continue;
                }
                auto start = i;
                while (IsWordCharacter(i))
                    i++;
                if (start != completionStart && i - start > 1)
                    words.insert(std::string_view(content).substr(start, i - start));
            }
            for (auto word : words)
                candidates.push_back({ std::string(word), "Word" });
        }

        // Case doesn't matter while typing, an exactly typed word needs no suggestion
        auto startsWith = [&](const std::string& text) {
            return text.size() >= context.prefix.size() && std::equal(context.prefix.begin(), context.prefix.end(), text.begin(),
                [](char a, char b) { return std::tolower((unsigned char)a) == std::tolower((unsigned char)b); });
            };
        for (auto& candidate : candidates) {
            if (candidate.text != context.prefix && startsWith(candidate.text))
                completions.push_back(std::move(candidate));
        }
        std::stable_sort(completions.begin(), completions.end(),
            [](const CodeCompletion& a, const CodeCompletion& b) { return a.text < b.text; });
        completions.erase(std::unique(completions.begin(), completions.end(),
            [](const CodeCompletion& a, const CodeCompletion& b) { return a.text == b.text; }), completions.end());

        completionIndex = 0;
        completionChosen = false;
        completionOpen = !completions.empty();

    }

    void CodeEditor::RenderCompletion(ImVec2 position, float lineTop) {

        const int32_t visibleCount = 8;
        const float padding = 6.0f;
        const float rowPadding = 3.0f;

        auto first = std::clamp(completionIndex - visibleCount / 2, 0, std::max(int32_t(completions.size()) - visibleCount, 0));
        auto last = std::min(first + visibleCount, int32_t(completions.size()));

        // As wide as the visible suggestions with their kinds, but not wider than a comfortable reading width
        auto kindWidth = [&](const std::string& kind) {
            ImGui::PushFont(interfaceFont);
            auto width = ImGui::CalcTextSize(kind.c_str()).x;
            ImGui::PopFont();
            return width;
            };
        auto contentWidth = ImGui::GetFontSize() * 16.0f;
        for (auto i = first; i < last; i++)
            contentWidth = std::max(contentWidth, ImGui::CalcTextSize(completions[i].text.c_str()).x +
                kindWidth(completions[i].kind) + 4.0f * padding);
        contentWidth = std::min(contentWidth, ImGui::GetFontSize() * 28.0f);

        // Inside the window, above the line if there is no room below it
        auto viewport = ImGui::GetMainViewport();
        auto width = contentWidth + 2.0f * padding;
        position.x = std::max(viewport->WorkPos.x, std::min(position.x, viewport->WorkPos.x + viewport->WorkSize.x - width));
        auto rowHeight = ImGui::GetTextLineHeight() + 2.0f * rowPadding;
        auto height = std::max(completionHeight, float(last - first) * rowHeight + 2.0f * padding);
        if (position.y + height > viewport->WorkPos.y + viewport->WorkSize.y)
            position.y = lineTop - height;

        // Looks like a tooltip but isn't one, other tooltips would be added to it or hide it
        const auto flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoDocking;
        const auto& style = ImGui::GetStyle();
        ImGui::SetNextWindowPos(position);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, style.Colors[ImGuiCol_PopupBg]);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, style.PopupRounding);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, style.PopupBorderSize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padding, padding));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
        auto open = ImGui::Begin("##CodeCompletion", nullptr, flags);
        ImGui::PopStyleVar(4);
        ImGui::PopStyleColor();
        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
        if (!open) {
            ImGui::End();
            return;
        }

        auto drawList = ImGui::GetWindowDrawList();
        for (auto i = first; i < last; i++) {
            const auto& completion = completions[i];
            auto min = ImGui::GetCursorScreenPos();
            auto max = ImVec2(min.x + contentWidth, min.y + rowHeight);
            if (i == completionIndex)
                drawList->AddRectFilled(min, max, ImGui::GetColorU32(Theme::Accent(0.35f)), 4.0f);

            auto kindX = max.x - padding - kindWidth(completion.kind);
            drawList->PushClipRect(min, ImVec2(kindX - padding, max.y), true);
            drawList->AddText(ImVec2(min.x + padding, min.y + rowPadding), ImGui::GetColorU32(ImGuiCol_Text), completion.text.c_str());
            drawList->PopClipRect();

            ImGui::PushFont(interfaceFont);
            auto kindY = min.y + (rowHeight - ImGui::GetFontSize()) * 0.5f;
            drawList->AddText(ImVec2(kindX, kindY), ImGui::GetColorU32(Theme::TextMuted()), completion.kind.c_str());
            ImGui::PopFont();

            ImGui::Dummy(ImVec2(contentWidth, rowHeight));
        }

        const auto& selected = completions[completionIndex];
        if (!selected.signature.empty() || !selected.description.empty()) {
            ImGui::Dummy(ImVec2(0.0f, padding));
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0.0f, padding));
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + contentWidth);
            if (!selected.signature.empty())
                ImGui::TextUnformatted(selected.signature.c_str());
            if (!selected.description.empty()) {
                ImGui::PushFont(interfaceFont);
                ImGui::TextColored(Theme::TextMuted(), "%s", selected.description.c_str());
                ImGui::PopFont();
            }
            ImGui::PopTextWrapPos();
        }

        completionHeight = ImGui::GetWindowHeight();
        ImGui::End();

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
        completionOpen = false;
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

    void CodeEditor::RenderSearch(bool canEdit) {

        auto& io = ImGui::GetIO();
        const auto& style = ImGui::GetStyle();
        auto matches = int32_t(searchMatches.size());
        auto countText = std::to_string(matches) + (matches == 1 ? " match" : " matches");

        ImGui::PushFont(interfaceFont);
        auto buttonWidth = [&](const char* label) { return ImGui::CalcTextSize(label).x + 2.0f * style.FramePadding.x; };
        auto iconWidth = ImGui::GetTextLineHeight() + 2.0f * style.FramePadding.x;
        auto rightWidth = ImGui::CalcTextSize(countText.c_str()).x + 3.0f * iconWidth + (canEdit ? buttonWidth("Replace") : 0.0f) +
            (canEdit ? 5.0f : 4.0f) * style.ItemSpacing.x;

        ImGui::SetNextItemWidth(-rightWidth);
        if (searchFocusRequested) {
            ImGui::SetKeyboardFocusHere();
            searchFocusRequested = false;
        }
        if (ImGui::InputTextWithHint("##Search", "Search...", &searchText, ImGuiInputTextFlags_EnterReturnsTrue)) {
            FindNext(!io.KeyShift);
            // Enter takes the focus from the field, the next match follows with the next Enter
            ImGui::SetKeyboardFocusHere(-1);
        }
        if (ImGui::IsItemActive() && ImGui::IsKeyPressed(ImGuiKey_Escape))
            searchOpen = false;

        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(Theme::TextMuted(), "%s", countText.c_str());
        ImGui::SameLine();
        if (Widgets::ToolbarIconButton("Previous match", Singletons::icons->Get(IconType::ArrowUp),
            Singletons::commandPalette->GetTooltip("Previous match", "Code: Find previous").c_str()))
            FindNext(false);
        ImGui::SameLine();
        if (Widgets::ToolbarIconButton("Next match", Singletons::icons->Get(IconType::ArrowDown),
            Singletons::commandPalette->GetTooltip("Next match", "Code: Find next").c_str()))
            FindNext(true);
        if (canEdit) {
            ImGui::SameLine();
            if (replaceOpen)
                Theme::PushPrimaryButton();
            auto toggled = ImGui::Button("Replace");
            if (replaceOpen)
                Theme::PopPrimaryButton();
            ImGui::SetItemTooltip("%s", Singletons::commandPalette->GetTooltip("Replace", "Code: Replace").c_str());
            if (toggled)
                replaceOpen = !replaceOpen;
        }
        ImGui::SameLine();
        if (Widgets::ToolbarIconButton("Close search", Singletons::icons->Get(IconType::Close), "Close (Esc)"))
            searchOpen = false;

        if (canEdit && replaceOpen) {
            ImGui::SetNextItemWidth(-(buttonWidth("Replace") + buttonWidth("Replace all") + 2.0f * style.ItemSpacing.x));
            ImGui::InputTextWithHint("##Replace", "Replace with...", &replaceText);
            ImGui::SameLine();
            ImGui::BeginDisabled(matches == 0);
            if (ImGui::Button("Replace"))
                ReplaceMatch(false);
            ImGui::SameLine();
            if (ImGui::Button("Replace all"))
                ReplaceMatch(true);
            ImGui::EndDisabled();
        }
        ImGui::PopFont();

    }

    void CodeEditor::ReplaceMatch(bool all) {

        auto matches = FindMatches();
        if (matches.empty())
            return;

        auto length = int32_t(searchText.size());
        if (all) {
            // One replacement of the whole text is one undo step
            std::string replaced;
            int32_t position = 0;
            for (auto match : matches) {
                replaced += content.substr(position, match - position) + replaceText;
                position = match + length;
            }
            replaced += content.substr(position);
            Replace(0, int32_t(content.size()), replaced, false);
            MoveCursor(std::min(matches.front(), int32_t(content.size())), false);
            return;
        }

        // The selected match is replaced, otherwise the next one is selected first
        auto selectionStart = std::min(cursor, anchor);
        auto selected = std::max(cursor, anchor) - selectionStart == length &&
            std::find(matches.begin(), matches.end(), selectionStart) != matches.end();
        if (selected)
            Replace(selectionStart, selectionStart + length, replaceText, false);
        FindNext(true);

    }

    std::vector<int32_t> CodeEditor::FindMatches() const {

        std::vector<int32_t> matches;
        if (searchText.empty())
            return matches;
        auto equal = [](char a, char b) { return std::tolower((unsigned char)a) == std::tolower((unsigned char)b); };
        auto iter = content.begin();
        while ((iter = std::search(iter, content.end(), searchText.begin(), searchText.end(), equal)) != content.end()) {
            matches.push_back(int32_t(iter - content.begin()));
            iter += searchText.size();
        }
        return matches;

    }

    void CodeEditor::FindNext(bool forward) {

        auto matches = FindMatches();
        if (matches.empty())
            return;

        auto selectionStart = std::min(cursor, anchor);
        auto selectionEnd = std::max(cursor, anchor);
        auto match = forward ? matches.front() : matches.back();
        if (forward) {
            auto next = std::lower_bound(matches.begin(), matches.end(), selectionEnd);
            if (next != matches.end())
                match = *next;
        }
        else {
            auto previous = std::lower_bound(matches.begin(), matches.end(), selectionStart);
            if (previous != matches.begin())
                match = *(previous - 1);
        }

        anchor = match;
        cursor = match + int32_t(searchText.size());
        scrollToCursor = true;
        completionOpen = false;

    }

    bool CodeEditor::IsWordCharacter(int32_t offset) const {

        if (offset < 0 || offset >= int32_t(content.size()))
            return false;
        auto c = uint8_t(content[offset]);
        return std::isalnum(c) || c == '_' || c >= 0x80;

    }

}
