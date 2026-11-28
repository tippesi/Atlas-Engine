#pragma once

namespace Atlas::Editor {

    class Workspace;

    // What the command palette lists for the windows of the workspace, with the shortcuts and where they work
    class EditorCommands {

    public:
        explicit EditorCommands(Workspace& workspace) : workspace(workspace) {}

        void Register();

        // The shortcuts of the commands, which can be changed, and what the mouse does
        void RenderShortcutsWindow(bool* show);

    private:
        Workspace& workspace;

    };

}
