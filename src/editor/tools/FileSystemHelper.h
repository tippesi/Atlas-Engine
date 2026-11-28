#pragma once

#include "jobsystem/JobSystem.h"

#include <filesystem>
#include <functional>
#include <mutex>
#include <set>
#include <vector>
#include <string>


namespace Atlas::Editor {

	class FileSystemHelper {

	public:
        // A write to a file which exists, the user decides about it
        struct Conflict {
            std::filesystem::path target;
            std::function<void(const std::filesystem::path& target)> write;
        };

        // Writes right away if the target is free, otherwise the user is asked first
        static void WriteOrAsk(const std::filesystem::path& target,
            const std::function<void(const std::filesystem::path& target)>& write);

        // The path itself or the first numbered one which is free
        static std::filesystem::path GetFreePath(const std::filesystem::path& path);

        static void Copy(const std::string& path, const std::string& destination);

        static void Copy(const std::vector<std::string>& paths, const std::string& destination);

        // Into the recycle bin where the system has one, otherwise for good
        static void Delete(const std::string& path);

        static void Delete(const std::vector<std::string>& paths);

        static void Duplicate(const std::string& path);

        static std::vector<Conflict> conflicts;

#ifdef AE_OS_WINDOWS
        static constexpr bool hasRecycleBin = true;
#else
        static constexpr bool hasRecycleBin = false;
#endif

    private:
        using CopyOperation = std::pair<std::filesystem::path, std::filesystem::path>;

        // There is no such file, also not one which is about to be copied
        static bool IsFree(const std::filesystem::path& path);

        static void MoveToRecycleBin(const std::string& path);

        static void ExecuteCopies(const std::vector<CopyOperation>& operations);

        // Targets of copies which haven't finished
        static std::set<std::filesystem::path> copiedTargets;
        static std::mutex copiedTargetsMutex;

        static JobGroup copyGroup;
        static JobGroup deleteGroup;
        static JobGroup duplicateGroup;

	};

}