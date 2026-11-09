#include "UndoHistory.h"

#include <imgui.h>
#include <ImGuizmo.h>

namespace Atlas::Editor {

    void UndoHistory::Push(Step step) {

        steps.resize(position);
        steps.push_back(std::move(step));

        if (steps.size() > maxSteps)
            steps.erase(steps.begin());

        position = steps.size();

    }

    bool UndoHistory::Undo() {

        if (!CanUndo())
            return false;

        position--;
        steps[position].undo();

        return true;

    }

    bool UndoHistory::Redo() {

        if (!CanRedo())
            return false;

        steps[position].redo();
        position++;

        return true;

    }

    bool UndoHistory::CanUndo() const {

        return position > 0;

    }

    bool UndoHistory::CanRedo() const {

        return position < steps.size();

    }

    std::string UndoHistory::GetUndoName() const {

        return CanUndo() ? steps[position - 1].name : std::string();

    }

    std::string UndoHistory::GetRedoName() const {

        return CanRedo() ? steps[position].name : std::string();

    }

    void UndoHistory::Clear() {

        steps.clear();
        position = 0;

    }

    bool UndoHistory::IsInteractionFinished() {

        auto interacting = ImGui::IsAnyItemActive() || ImGui::IsAnyMouseDown() || ImGuizmo::IsUsing();

        // Shortcuts like dropping the selection to the ground edit without an active item
        auto keyReleased = false;
        for (auto key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_NamedKey_END; key = ImGuiKey(key + 1))
            keyReleased |= ImGui::IsKeyReleased(key);

        auto finished = !interacting && (wasInteracting || keyReleased);
        wasInteracting = interacting;

        return finished;

    }

    bool UndoHistory::HandleShortcuts() {

        auto direction = GetShortcutDirection();
        if (direction < 0)
            return Undo();
        if (direction > 0)
            return Redo();

        return false;

    }

    int32_t UndoHistory::GetShortcutDirection() const {

        // Text fields have their own undo
        if (ImGui::GetIO().WantTextInput)
            return 0;

        bool controlDown;
#ifdef AE_OS_MACOS
        controlDown = ImGui::IsKeyDown(ImGuiKey_LeftSuper) || ImGui::IsKeyDown(ImGuiKey_RightSuper);
#else
        controlDown = ImGui::GetIO().KeyCtrl;
#endif
        if (!controlDown)
            return 0;

        if (ImGui::IsKeyPressed(ImGuiKey_Z) && !ImGui::GetIO().KeyShift)
            return -1;
        if ((ImGui::IsKeyPressed(ImGuiKey_Z) && ImGui::GetIO().KeyShift) || ImGui::IsKeyPressed(ImGuiKey_Y))
            return 1;

        return 0;

    }

}
