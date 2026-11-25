#pragma once

#include "scripting/Binder.h"

namespace Atlas::Editor {

    class Workspace;

    // The table Editor of the Lua of the MCP server: windows, selection, commands, opening files and the text of code windows
    class EditorFunctions {

    public:
        explicit EditorFunctions(Workspace& workspace) : workspace(workspace) {}

        void Add(Scripting::Binder& editor);

    private:
        Workspace& workspace;

    };

}
