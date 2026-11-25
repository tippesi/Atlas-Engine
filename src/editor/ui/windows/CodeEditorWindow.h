#pragma once

#include "Window.h"
#include "../CodeEditor.h"

#include <string>
#include <vector>

namespace Atlas::Editor::UI {

    // A Lua script or a shader file with syntax highlighting and completions, Ctrl+S saves it. Scripts complete the
    // bindings the engine and the programs record (Scripting::BindingRegistry), shaders the functions of the file
    // and its includes
    class CodeEditorWindow : public Window {

    public:
        explicit CodeEditorWindow(const std::string& path);

        void Render();

        bool Save();

        const std::string& GetPath() const { return path; }

        // The file was renamed
        void SetPath(const std::string& newPath);

        const std::string& GetText() const { return text; }

        // One undo step in the editor, the file is saved by the user
        void SetText(const std::string& newText);

        // Members of the bound types and tables for a context, e.g. after "sky." the fields of Atlas.Sky
        static std::vector<CodeCompletion> CompleteLua(const CodeCompletionContext& context);

        CodeEditor codeEditor;

    private:
        // Functions, structs and blocks of the text and the includes, found again when the text changed
        struct GlslCache;

        // Functions and code of the included files, collected when the file is opened or saved
        void LoadIncludes();

        // Functions, and after a . the fields of structs and blocks or the components of vectors
        std::vector<CodeCompletion> CompleteGlsl(const CodeCompletionContext& context) const;

        std::string path;
        std::string text;
        // The file uses Windows line endings, the editor works without the \r and saves them again
        bool windowsLineEndings = false;

        std::vector<CodeCompletion> includedFunctions;
        std::string includedCode;
        mutable std::shared_ptr<GlslCache> glslCache;

    };

}
