#pragma once

#include <imgui.h>

#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace Atlas::Editor::UI {

    struct Command {
        std::string category;
        std::string name;
        ImGuiKeyChord shortcut = 0;
        ImGuiKeyChord alternative = 0;

        std::function<void()> action;
        // Optional, commands without this function are always available
        std::function<bool()> isEnabled;
        // Optional, shows a check mark for commands that toggle something
        std::function<bool()> isChecked;
        // Optional, whether the window is in front in which the shortcut works
        std::function<bool()> isFocused;
        bool repeat = false;
        bool whileTyping = false;
    };

    // Searchable list of all editor actions, opened with Ctrl+P
    class CommandPalette {

    public:
        void Register(const Command& command);

        // By its name or "Category: Name", nullptr if there is none
        const Command* Find(const std::string& name) const;

        // Runs the command of the pressed shortcut
        void HandleShortcuts();

        // The shortcuts of all commands, a click changes one
        void RenderShortcuts();

        // Empty without a shortcut
        std::string GetShortcutText(const std::string& name) const;

        // The text with the shortcut of a command in brackets
        std::string GetTooltip(const std::string& text, const std::string& name) const;

        void Open();

        void Render();

        // Zero if it isn't the name of a key with modifiers ("Ctrl+S")
        static ImGuiKeyChord ParseShortcut(const std::string& name);

        static std::string GetShortcutName(ImGuiKeyChord shortcut);

    private:
        struct Match {
            const Command* command;
            int32_t score;
        };

        // Fuzzy match: all characters of the query have to appear in order. Consecutive characters,
        // matches at word starts and prefix matches score higher. Returns -1 if there is no match.
        static int32_t Score(const std::string& query, const std::string& text);

        static std::string GetName(const Command& command) { return command.category + ": " + command.name; }

        // The one of the config, or the one of the command
        static ImGuiKeyChord GetShortcut(const Command& command);

        std::vector<Command> commands;

        // The command which gets the next pressed keys as its shortcut
        std::string recordedCommand;
        // The last frame which recorded, its keys don't run a command and the window might be closed since then
        int32_t recordedFrame = -1;
        int32_t handledFrame = -1;

        std::string search;
        int32_t selectedIdx = 0;

        bool openRequested = false;
        bool wasJustOpened = false;

    };

}
