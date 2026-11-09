#pragma once

#include <functional>
#include <string>
#include <vector>

namespace Atlas::Editor {

    // Undo steps of one document, e.g. a scene or an asset editor
    class UndoHistory {

    public:
        struct Step {
            std::string name;
            std::function<void()> undo;
            std::function<void()> redo;
        };

        // Removes the steps which were undone before
        void Push(Step step);

        bool Undo();

        bool Redo();

        bool CanUndo() const;

        bool CanRedo() const;

        std::string GetUndoName() const;

        std::string GetRedoName() const;

        void Clear();

        // Edits are compared once the user stops interacting, such that e.g. a drag is a single step
        bool IsInteractionFinished();

        // Handles the undo and redo shortcuts, returns true if a step was undone or redone
        bool HandleShortcuts();

        // -1 when the undo shortcut was pressed, 1 for the redo shortcut and 0 otherwise
        int32_t GetShortcutDirection() const;

        size_t maxSteps = 256;

    private:
        std::vector<Step> steps;
        size_t position = 0;

        bool wasInteracting = false;

    };

}
