#pragma once

#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace Atlas::Editor::UI {

    struct Command {
        std::string category;
        std::string name;
        std::string shortcut;

        std::function<void()> action;
        // Optional, commands without this function are always available
        std::function<bool()> isEnabled;
        // Optional, shows a check mark for commands that toggle something
        std::function<bool()> isChecked;
    };

    // Searchable list of all editor actions, opened with Ctrl+P
    class CommandPalette {

    public:
        void Register(const Command& command);

        void Open();

        void Render();

    private:
        struct Match {
            const Command* command;
            int32_t score;
        };

        // Fuzzy match: all characters of the query have to appear in order. Consecutive characters,
        // matches at word starts and prefix matches score higher. Returns -1 if there is no match.
        static int32_t Score(const std::string& query, const std::string& text);

        std::vector<Command> commands;

        std::string search;
        int32_t selectedIdx = 0;

        bool openRequested = false;
        bool wasJustOpened = false;

    };

}
