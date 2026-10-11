#pragma once

#include "Window.h"
#include "Log.h"

#include <deque>
#include <vector>

namespace Atlas::Editor::UI {

    class LogWindow : public Window {

    public:
        explicit LogWindow(bool show) : Window("Log", show) {}

        void Render();

    private:
        // One displayed line. Multi-line messages are split, such that every line has the same height
        struct Line {
            size_t entryIdx;    // Global index of the log entry
            uint32_t offset;    // Text range within the message
            uint32_t length;
            bool first;         // First line of the message, shows the time stamp
            ImGuiID id;         // Stable id for the selection
        };

        // Fetches new log entries and updates the lines incrementally. A full rebuild only happens
        // when filters change or the cache is trimmed
        void UpdateLines();

        void RebuildLines();

        void AppendLines(size_t entryIdx);

        const Atlas::Log::Entry& GetEntry(size_t entryIdx) const { return cachedEntries[entryIdx - cacheStartIdx]; }

        void CopyLines(bool onlySelected);

        bool RenderFilterToggle(const char* label, int32_t count, bool* enabled, ImVec4 color);

        // Amount of entries kept, trimming happens in batches to avoid rebuilding the lines for every new entry
        static constexpr size_t maxCachedEntries = 10000;
        static constexpr size_t trimBatchSize = 1000;

        std::deque<Atlas::Log::Entry> cachedEntries;
        size_t cacheStartIdx = 0;
        size_t fetchedEntryCount = 0;

        std::vector<Line> lines;
        bool linesDirty = true;

        // Counts of the entries after the last clear, for the filter buttons
        int32_t messageCount = 0;
        int32_t warningCount = 0;
        int32_t errorCount = 0;

        std::string logSearch;
        std::string lowerSearch;
        std::string lastFilterState;

        bool showMessages = true;
        bool showWarnings = true;
        bool showErrors = true;
        bool autoScroll = true;

        // Entries before this index are hidden after clearing the log
        size_t clearEntryIdx = 0;

        size_t lastLineCount = 0;

        // Selected lines, which can be copied
        ImGuiSelectionBasicStorage selection;

    };

}
