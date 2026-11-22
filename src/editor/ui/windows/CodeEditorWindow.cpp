#include "CodeEditorWindow.h"
#include "../../Notifications.h"
#include "../../Theme.h"
#include "../../Singletons.h"
#include "../Widgets.h"

#include "common/Path.h"
#include "loader/AssetLoader.h"
#include "scripting/BindingRegistry.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <regex>
#include <unordered_map>

namespace Atlas::Editor::UI {

    CodeEditorWindow::CodeEditorWindow(const std::string& path)
        : Window(Common::Path::GetFileName(path), true), path(path) {

        // The file identifies the window, two files can have the same name
        SetTitle(name, path);

        auto stream = Loader::AssetLoader::ReadFile(path, std::ios::in | std::ios::binary);
        if (stream.is_open())
            text.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
        else
            Notifications::Push({ .message = "Couldn't read " + path, .type = NotificationType::Error });
        windowsLineEndings = text.find("\r\n") != std::string::npos;
        std::erase(text, '\r');
        codeEditor.Reset(text);

        if (Common::Path::GetFileType(path) == "lua") {
            codeEditor.language = CodeLanguage::Lua;
            codeEditor.completionProvider = &CodeEditorWindow::CompleteLua;
            return;
        }

        codeEditor.language = CodeLanguage::GLSL;
        LoadIncludes();
        codeEditor.completionProvider = [this](const CodeCompletionContext& context) { return CompleteGlsl(context); };

    }

    void CodeEditorWindow::Render() {

        SetTitle(name, path);
        if (!Begin(ImGuiWindowFlags_NoScrollbar))
            return;

        inFocus = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        auto controlDown = ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeySuper;
        if (inFocus && controlDown && ImGui::IsKeyPressed(ImGuiKey_S, false))
            Save();

        // A small toolbar, the shortcuts stand in the tooltips
        const auto& icons = Singletons::icons;
        ImGui::BeginDisabled(!hasUnsavedChanges);
        if (Widgets::ToolbarIconButton("Save", icons->Get(IconType::Save), "Save (Ctrl+S)"))
            Save();
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!codeEditor.CanUndo());
        if (Widgets::ToolbarIconButton("Undo", icons->Get(IconType::Undo), "Undo (Ctrl+Z)"))
            codeEditor.Undo();
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!codeEditor.CanRedo());
        if (Widgets::ToolbarIconButton("Redo", icons->Get(IconType::Redo), "Redo (Ctrl+Y)"))
            codeEditor.Redo();
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (Widgets::ToolbarIconButton("Find", icons->Get(IconType::Search), "Find (Ctrl+F), replace (Ctrl+H)"))
            codeEditor.OpenSearch(false);
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(Theme::TextMuted(), "%s", path.c_str());

        if (codeEditor.Render("##Code", text))
            hasUnsavedChanges = true;

        End();

    }

    void CodeEditorWindow::SetText(const std::string& newText) {

        text = newText;
        std::erase(text, '\r');
        codeEditor.SetText(text);
        hasUnsavedChanges = true;

    }

    bool CodeEditorWindow::Save() {

        auto stream = Loader::AssetLoader::WriteFile(path, std::ios::out | std::ios::binary);
        if (!stream.is_open()) {
            Notifications::Push({ .message = "Couldn't write " + path, .type = NotificationType::Error });
            return false;
        }
        std::string file;
        if (windowsLineEndings) {
            file.reserve(text.size() + text.size() / 32);
            for (auto c : text)
                file += c == '\n' ? std::string("\r\n") : std::string(1, c);
        }
        const auto& content = windowsLineEndings ? file : text;
        stream.write(content.data(), std::streamsize(content.size()));
        hasUnsavedChanges = false;

        if (codeEditor.language == CodeLanguage::GLSL)
            LoadIncludes();
        return true;

    }

    void CodeEditorWindow::LoadIncludes() {

        includedFunctions.clear();
        includedCode.clear();
        glslCache.reset();

        // Includes are relative to the including file and read once, like the shader loader does
        std::vector<std::string> visited = { Common::Path::Normalize(path) };
        auto collect = [&](auto&& self, const std::string& file, const std::string& code) -> void {
            auto directory = Common::Path::GetDirectory(file) + "/";
            size_t position = 0;
            while ((position = code.find("#include ", position)) != std::string::npos) {
                auto start = code.find_first_of("\"<", position) + 1;
                auto end = code.find_first_of("\">", start);
                position = end;
                if (start == 0 || end == std::string::npos)
                    break;

                auto includePath = Common::Path::Normalize(directory + code.substr(start, end - start));
                if (std::find(visited.begin(), visited.end(), includePath) != visited.end())
                    continue;
                visited.push_back(includePath);

                auto stream = Loader::AssetLoader::ReadFile(includePath, std::ios::in | std::ios::binary);
                if (!stream.is_open())
                    continue;
                std::string includeCode((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
                includedCode += includeCode + "\n";
                auto functions = CodeEditor::FindGlslFunctions(includeCode);
                includedFunctions.insert(includedFunctions.end(), functions.begin(), functions.end());
                self(self, includePath, includeCode);
            }
            };
        collect(collect, path, text);

    }

    using Owners = std::unordered_map<std::string, const Scripting::BindingRegistry::Owner*>;

    // A link of a Lua or GLSL expression like children[i + 1]:GetCameraComponent(), with its calls and indices
    struct ExpressionSegment {
        std::string name;
        bool call = false;
        int32_t indices = 0;
    };

    static std::vector<ExpressionSegment> SplitExpression(const std::string& expression) {

        std::vector<ExpressionSegment> segments;
        size_t i = 0;
        while (i < expression.size()) {
            auto c = expression[i];
            if (std::isalnum((unsigned char)c) || c == '_') {
                auto begin = i;
                while (i < expression.size() && (std::isalnum((unsigned char)expression[i]) || expression[i] == '_'))
                    i++;
                segments.push_back({ expression.substr(begin, i - begin) });
                continue;
            }
            if ((c == '(' || c == '[') && !segments.empty()) {
                auto close = c == '(' ? ')' : ']';
                auto depth = 0;
                for (; i < expression.size(); i++) {
                    depth += expression[i] == c ? 1 : expression[i] == close ? -1 : 0;
                    if (depth == 0)
                        break;
                }
                if (c == '(')
                    segments.back().call = true;
                else
                    segments.back().indices++;
            }
            else if (c != '.' && c != ':') {
                return {};
            }
            i++;
        }
        return segments;

    }

    // The expression last assigned to a variable in the code, e.g. "scene:GetMainCamera()" for "local camera = ..."
    static std::pair<std::string, size_t> FindLuaAssignment(std::string_view text, const std::string& name) {

        auto isWord = [](char c) { return std::isalnum((unsigned char)c) || c == '_'; };
        auto position = text.size();
        while ((position = text.rfind(name, position == 0 ? 0 : position - 1)) != std::string_view::npos) {
            auto end = position + name.size();
            if ((position == 0 || !isWord(text[position - 1])) && (end >= text.size() || !isWord(text[end]))) {
                auto equals = end;
                while (equals < text.size() && text[equals] == ' ')
                    equals++;
                if (equals < text.size() && text[equals] == '=' && (equals + 1 >= text.size() || text[equals + 1] != '=')) {
                    auto begin = equals + 1;
                    while (begin < text.size() && text[begin] == ' ')
                        begin++;
                    // The expression ends where something else than names, calls and indices follows
                    auto i = begin;
                    while (i < text.size()) {
                        auto c = text[i];
                        if (c == '(' || c == '[') {
                            auto close = c == '(' ? ')' : ']';
                            auto depth = 0;
                            for (; i < text.size(); i++) {
                                depth += text[i] == c ? 1 : text[i] == close ? -1 : 0;
                                if (depth == 0)
                                    break;
                            }
                            i++;
                            continue;
                        }
                        if (!isWord(c) && c != '.' && c != ':')
                            break;
                        i++;
                    }
                    return { std::string(text.substr(begin, i - begin)), position };
                }
            }
            if (position == 0)
                break;
        }
        return { "", 0 };

    }

    // "fun(number): Glm.Vec3, boolean | fun(): Glm.Vec3" -> "Glm.Vec3", the first value of the first overload
    static std::string GetLuaReturnType(const std::string& type) {

        if (!type.starts_with("fun("))
            return "";
        auto depth = 0;
        size_t i = 3;
        for (; i < type.size(); i++) {
            depth += type[i] == '(' ? 1 : type[i] == ')' ? -1 : 0;
            if (depth == 0)
                break;
        }
        if (type.compare(i + 1, 2, ": ") != 0)
            return "";
        auto result = type.substr(i + 3);
        return result.substr(0, std::min(result.find(", "), result.find(" | ")));

    }

    // Optional values and the component references of entities have the members of their type
    static std::string NormalizeLuaType(std::string type, const Owners& owners) {

        if (type.ends_with("?"))
            type.pop_back();
        if (type.ends_with("Ref")) {
            auto base = type.substr(0, type.size() - 3);
            auto reference = owners.find(type);
            if (owners.contains(base) && (reference == owners.end() || reference->second->members.empty()))
                type = base;
        }
        return type;

    }

    // The type of an expression: locals are followed to their assignment, calls give the return type, indices the
    // element type. Empty if it isn't known
    static std::string ResolveLuaType(const std::string& expression, std::string_view text, const Owners& owners, int32_t depth) {

        auto segments = SplitExpression(expression);
        if (segments.empty() || depth > 8)
            return "";

        auto apply = [&](std::string type, const ExpressionSegment& segment) {
            if (segment.call)
                type = GetLuaReturnType(type);
            type = NormalizeLuaType(type, owners);
            for (int32_t i = 0; i < segment.indices && type.ends_with("[]"); i++)
                type = type.substr(0, type.size() - 2);
            return NormalizeLuaType(type, owners);
            };

        std::string type;
        const auto& first = segments.front();
        auto [assigned, position] = FindLuaAssignment(text, first.name);
        auto globals = owners.find("_G");
        if (!assigned.empty()) {
            type = ResolveLuaType(assigned, text.substr(0, position), owners, depth + 1);
        }
        else if (globals != owners.end()) {
            for (const auto& member : globals->second->members)
                if (member.name == first.name)
                    type = member.type;
        }
        if (type.empty() && owners.contains(first.name))
            type = first.name;
        type = apply(type, first);

        for (size_t i = 1; i < segments.size() && !type.empty(); i++) {
            auto owner = owners.find(type);
            std::string next;
            if (owner != owners.end()) {
                for (const auto& member : owner->second->members)
                    if (member.name == segments[i].name)
                        next = member.type;
            }
            // Tables contain types and tables, e.g. Atlas.Sky
            if (next.empty() && owners.contains(type + "." + segments[i].name))
                next = type + "." + segments[i].name;
            type = next.empty() ? "" : apply(next, segments[i]);
        }
        return type;

    }

    std::vector<CodeCompletion> CodeEditorWindow::CompleteLua(const CodeCompletionContext& context) {

        auto lower = [](std::string text) {
            std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return char(std::tolower(c)); });
            return text;
            };
        auto lastSegment = [](const std::string& path) { return path.substr(path.rfind('.') + 1); };

        // The bindings only change when new ones are recorded
        static std::vector<Scripting::BindingRegistry::Owner> owners;
        static Owners byPath;
        static uint64_t ownersRevision = std::numeric_limits<uint64_t>::max();
        if (ownersRevision != Scripting::BindingRegistry::GetRevision()) {
            ownersRevision = Scripting::BindingRegistry::GetRevision();
            owners = Scripting::BindingRegistry::GetOwners();
            byPath.clear();
            for (const auto& owner : owners)
                byPath[owner.path] = &owner;
        }

        std::vector<CodeCompletion> completions;
        auto addMembers = [&](const Scripting::BindingRegistry::Owner& owner) {
            for (const auto& member : owner.members) {
                // Methods are called with :, fields and functions of tables with .
                auto isMethod = member.kind == "method";
                if (context.separator && (context.separator == ':') != isMethod)
                    continue;
                completions.push_back({ member.name, member.kind, member.type, member.description });
            }
            };

        if (!context.separator) {
            // Globals: the functions of the script environment and the tables like Atlas, Glm or Host
            std::vector<std::string> roots;
            for (const auto& owner : owners) {
                if (owner.path == "_G") {
                    addMembers(owner);
                    continue;
                }
                auto root = owner.path.substr(0, owner.path.find('.'));
                if (std::find(roots.begin(), roots.end(), root) == roots.end())
                    roots.push_back(root);
            }
            for (const auto& root : roots)
                completions.push_back({ root, "table", "", "" });
            return completions;
        }

        std::vector<std::string> types;
        auto resolved = ResolveLuaType(context.qualifier, context.text, byPath, 0);
        if (!resolved.empty()) {
            types.push_back(resolved);
        }
        else {
            // Without a known type, the qualifier is matched to a bound type or table by its name (scene, sky, Host)
            // and by the fields of that name whose type is bound (scene.postProcessing)
            auto qualifier = lower(context.qualifier);
            for (const auto& owner : owners) {
                if (lower(owner.path) == qualifier || lower(lastSegment(owner.path)) == qualifier)
                    types.push_back(owner.path);
                for (const auto& member : owner.members) {
                    if (lower(member.name) == qualifier && member.kind != "method" && member.kind != "function")
                        types.push_back(NormalizeLuaType(member.type.substr(0, member.type.find('[')), byPath));
                }
            }
        }

        // Children of a table, e.g. Atlas. offers Sky, Scene and the other types
        for (const auto& owner : owners) {
            auto parent = owner.path.find('.') == std::string::npos ? "" : owner.path.substr(0, owner.path.rfind('.'));
            if (std::find(types.begin(), types.end(), parent) != types.end())
                completions.push_back({ lastSegment(owner.path), owner.kind, "", owner.description });
        }

        for (const auto& owner : owners) {
            if (std::find(types.begin(), types.end(), owner.path) != types.end())
                addMembers(owner);
        }

        // Several types can match a name, each member is offered once
        std::sort(completions.begin(), completions.end(), [](const auto& a, const auto& b) { return a.text < b.text; });
        completions.erase(std::unique(completions.begin(), completions.end(),
            [](const auto& a, const auto& b) { return a.text == b.text; }), completions.end());
        return completions;

    }


    // A declared GLSL type, e.g. "vec3", a struct or a block, and whether the declaration is an array
    struct GlslType {
        std::string name;
        bool array = false;
    };

    struct GlslField {
        GlslType type;
        std::string name;
    };

    // Structs and blocks with their fields, and the instance names of blocks ("} uniforms;")
    struct GlslScope {
        std::unordered_map<std::string, std::vector<GlslField>> structs;
        std::unordered_map<std::string, GlslType> instances;
    };

    static const std::vector<std::string_view> glslQualifiers = { "const", "in", "out", "inout", "uniform", "buffer",
        "shared", "flat", "highp", "mediump", "lowp", "readonly", "writeonly", "coherent", "restrict", "volatile", "precise" };

    static GlslScope FindGlslScope(const std::string& code) {

        GlslScope scope;
        static const std::regex blockPattern(R"(\b(struct|uniform|buffer)\s+(\w+)\s*\{([^}]*)\}\s*(\w+)?(\s*\[)?)");
        static const std::regex namePattern(R"((\w+)\s*(\[[^\]]*\])?)");
        static const std::regex layoutPattern(R"(layout\s*\([^)]*\))");
        for (auto block = std::sregex_iterator(code.begin(), code.end(), blockPattern); block != std::sregex_iterator(); block++) {
            auto& fields = scope.structs[(*block)[2]];
            auto body = std::regex_replace((*block)[3].str(), layoutPattern, "");
            size_t start = 0;
            while (start < body.size()) {
                auto end = std::min(body.find(';', start), body.size());
                auto statement = body.substr(start, end - start);
                start = end + 1;

                // The first word which isn't a qualifier is the type, the others are the names
                std::string type;
                for (auto word = std::sregex_iterator(statement.begin(), statement.end(), namePattern); word != std::sregex_iterator(); word++) {
                    auto name = (*word)[1].str();
                    if (std::isdigit((unsigned char)name[0]))
                        continue;
                    if (type.empty()) {
                        if (std::find(glslQualifiers.begin(), glslQualifiers.end(), name) == glslQualifiers.end())
                            type = name;
                        continue;
                    }
                    fields.push_back({ { type, (*word)[2].matched }, name });
                }
            }
            if ((*block)[1] != "struct" && (*block)[4].matched)
                scope.instances[(*block)[4]] = { (*block)[2], (*block)[5].matched };
        }
        return scope;

    }

    // The type a variable is declared with last in the code: locals, parameters and globals
    static GlslType FindGlslDeclaration(const std::string& code, const std::string& name) {

        static const std::vector<std::string_view> notTypes = { "return", "else", "case", "discard", "break", "continue" };
        std::regex pattern(R"(\b([A-Za-z_]\w*)\s+)" + name + R"(\s*(\[[^\]]*\])?\s*[;=,)\[])");
        GlslType type;
        for (auto match = std::sregex_iterator(code.begin(), code.end(), pattern); match != std::sregex_iterator(); match++) {
            auto typeName = (*match)[1].str();
            if (std::find(notTypes.begin(), notTypes.end(), typeName) == notTypes.end())
                type = { typeName, (*match)[2].matched };
        }
        return type;

    }

    // "vec3" -> 3 components of "float", matrices index to their columns
    static int32_t GetGlslVectorSize(const std::string& type, std::string& scalar) {

        static const std::regex vectorPattern(R"(^(vec|ivec|uvec|bvec|dvec)([234])$)");
        std::smatch match;
        if (!std::regex_match(type, match, vectorPattern))
            return 0;
        static const std::unordered_map<std::string, std::string> scalars = { { "vec", "float" }, { "ivec", "int" },
            { "uvec", "uint" }, { "bvec", "bool" }, { "dvec", "double" } };
        scalar = scalars.at(match[1]);
        return match[2].str()[0] - '0';

    }

    static GlslType IndexGlslType(GlslType type) {

        if (type.array)
            return { type.name, false };

        static const std::regex matrixPattern(R"(^mat([234])(?:x([234]))?$)");
        std::smatch match;
        if (std::regex_match(type.name, match, matrixPattern))
            return { "vec" + (match[2].matched ? match[2].str() : match[1].str()) };

        std::string scalar;
        if (GetGlslVectorSize(type.name, scalar))
            return { scalar };
        return type;

    }

    struct CodeEditorWindow::GlslCache {
        std::string text;
        std::vector<CodeCompletion> functions;
        GlslScope scope;
    };

    std::vector<CodeCompletion> CodeEditorWindow::CompleteGlsl(const CodeCompletionContext& context) const {

        // Structs and blocks can be anywhere in the file
        if (!glslCache || glslCache->text != text) {
            glslCache = std::make_shared<GlslCache>();
            glslCache->text = text;
            glslCache->functions = CodeEditor::FindGlslFunctions(text);
            glslCache->functions.insert(glslCache->functions.end(), includedFunctions.begin(), includedFunctions.end());
            glslCache->scope = FindGlslScope(includedCode + "\n" + text);
        }
        const auto& functions = glslCache->functions;
        const auto& scope = glslCache->scope;
        if (!context.separator)
            return functions;
        if (context.separator != '.')
            return {};

        auto segments = SplitExpression(context.qualifier);
        if (segments.empty())
            return {};

        // Declarations of the includes come before the code
        auto code = includedCode + "\n" + std::string(context.text);

        GlslType type;
        const auto& first = segments.front();
        if (first.call) {
            // The return type is in front of the name, e.g. "vec3 Rotate(vec3 v)"
            for (const auto& function : functions) {
                if (function.text != first.name)
                    continue;
                auto declaration = function.signature.substr(0, function.signature.find(' ' + first.name + '('));
                type.name = declaration.substr(declaration.rfind(' ') + 1);
            }
        }
        else if (auto instance = scope.instances.find(first.name); instance != scope.instances.end()) {
            type = instance->second;
        }
        else {
            type = FindGlslDeclaration(code, first.name);
        }

        auto applyIndices = [&](const ExpressionSegment& segment) {
            for (int32_t i = 0; i < segment.indices; i++)
                type = IndexGlslType(type);
            };
        applyIndices(first);

        for (size_t i = 1; i < segments.size() && !type.name.empty(); i++) {
            std::string scalar;
            if (GetGlslVectorSize(type.name, scalar)) {
                // A swizzle of one component is a scalar, more are a vector of the same kind
                auto length = segments[i].name.size();
                type = { length == 1 ? scalar : type.name.substr(0, type.name.size() - 1) + std::to_string(length) };
            }
            else {
                auto fields = scope.structs.find(type.name);
                GlslType next;
                if (fields != scope.structs.end()) {
                    for (const auto& field : fields->second)
                        if (field.name == segments[i].name)
                            next = field.type;
                }
                type = next;
            }
            applyIndices(segments[i]);
        }

        std::vector<CodeCompletion> completions;
        if (type.array || type.name.empty())
            return completions;
        std::string scalar;
        if (auto size = GetGlslVectorSize(type.name, scalar)) {
            for (const auto* components : { "xyzw", "rgba" })
                for (int32_t i = 0; i < size; i++)
                    completions.push_back({ std::string(1, components[i]), "Component", scalar });
        }
        else if (auto fields = scope.structs.find(type.name); fields != scope.structs.end()) {
            for (const auto& field : fields->second)
                completions.push_back({ field.name, "Field", field.type.name + (field.type.array ? "[]" : "") + " " + field.name });
        }
        return completions;

    }

}
