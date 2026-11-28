#include "FileSystemHelper.h"
#include "Notifications.h"
#include "Log.h"
#include "common/Path.h"
#include "ContentDiscovery.h"

#include <filesystem>
#include <stdexcept>

#ifdef AE_OS_WINDOWS
#define NOMINMAX
#include <Windows.h>
#include <shellapi.h>
#endif

namespace Atlas::Editor {

    std::vector<FileSystemHelper::Conflict> FileSystemHelper::conflicts;

    std::set<std::filesystem::path> FileSystemHelper::copiedTargets;
    std::mutex FileSystemHelper::copiedTargetsMutex;

    JobGroup FileSystemHelper::copyGroup { "File copy" };
    JobGroup FileSystemHelper::deleteGroup { "File delete" };
    JobGroup FileSystemHelper::duplicateGroup { "File duplicate" };

    void FileSystemHelper::Copy(const std::string& path, const std::string& destination) {

        std::vector<std::string> paths = { path };
        Copy(paths, destination);

    }

    void FileSystemHelper::Copy(const std::vector<std::string>& paths, const std::string& destination) {

        std::vector<CopyOperation> operations;
        for (const auto& path : paths) {
            std::filesystem::path source(path);
            auto target = std::filesystem::path(destination) / source.filename();

            // A copy of the folder would contain itself
            std::error_code error;
            auto relative = std::filesystem::relative(destination, source, error);
            if (std::filesystem::is_directory(source, error) && !relative.empty() && *relative.begin() != "..") {
                Notifications::Push({ "Can't copy the folder " + path + " into itself", NotificationType::Error });
                continue;
            }

            // Pasted next to the original
            if (std::filesystem::equivalent(source, target, error))
                target = GetFreePath(target);

            if (IsFree(target)) {
                operations.emplace_back(source, target);
                std::lock_guard lock(copiedTargetsMutex);
                copiedTargets.insert(target);
            }
            else {
                WriteOrAsk(target, [source](const std::filesystem::path& target) { ExecuteCopies({ { source, target } }); });
            }
        }

        if (!operations.empty())
            ExecuteCopies(operations);

    }

    void FileSystemHelper::ExecuteCopies(const std::vector<CopyOperation>& operations) {

        {
            std::lock_guard lock(copiedTargetsMutex);
            for (const auto& [source, target] : operations)
                copiedTargets.insert(target);
        }

        JobSystem::Execute(copyGroup, [operations](JobData&) {
            bool success = true;
            for (const auto& [source, target] : operations) {
                std::error_code error;
                std::filesystem::copy(source, target, std::filesystem::copy_options::overwrite_existing |
                    std::filesystem::copy_options::recursive, error);
                if (error) {
                    success = false;
                    auto message = "Error copying " + source.string() + " to " + target.string() + ": " + error.message();
                    Notifications::Push({ message, NotificationType::Error });
                    Log::Error(message);
                }
                std::lock_guard lock(copiedTargetsMutex);
                copiedTargets.erase(target);
            }
            if (success)
                Notifications::Push({ operations.size() == 1 ? "Copied " + operations.front().second.filename().string() :
                    "Copied " + std::to_string(operations.size()) + " files" });
            ContentDiscovery::Execute();
            });

    }

    void FileSystemHelper::WriteOrAsk(const std::filesystem::path& target,
        const std::function<void(const std::filesystem::path& target)>& write) {

        if (IsFree(target))
            write(target);
        else
            conflicts.push_back({ target, write });

    }

    std::filesystem::path FileSystemHelper::GetFreePath(const std::filesystem::path& path) {

        std::error_code error;
        auto isDirectory = std::filesystem::is_directory(path, error);
        auto stem = isDirectory ? path.filename().string() : path.stem().string();
        auto extension = isDirectory ? std::string() : path.extension().string();

        auto freePath = path;
        for (int32_t counter = 1; !IsFree(freePath); counter++)
            freePath = path.parent_path() / (stem + "(" + std::to_string(counter) + ")" + extension);

        return freePath;

    }

    bool FileSystemHelper::IsFree(const std::filesystem::path& path) {

        std::error_code error;
        std::lock_guard lock(copiedTargetsMutex);
        return !copiedTargets.contains(path) && !std::filesystem::exists(path, error);

    }

    void FileSystemHelper::Delete(const std::string& path) {

        std::vector<std::string> paths = { path };
        Delete(paths);

    }

    void FileSystemHelper::Delete(const std::vector<std::string>& paths) {

        JobSystem::Execute(deleteGroup, [paths](JobData&) {
            bool success = true;
            for (const auto& path : paths) {
                try {
                    if (hasRecycleBin)
                        MoveToRecycleBin(path);
                    else
                        std::filesystem::remove_all(path);
                }
                catch (std::exception& e) {
                    success = false;
                    auto message = "Error deleting " + path + ": " + e.what();
                    Notifications::Push({ message, NotificationType::Error });
                    Log::Error(message);
                }
            }
            if (success) {
                Notifications::Push({ hasRecycleBin ? "Moved to the recycle bin" : "Successfully deleted files(s)" });
                ContentDiscovery::Execute();
            }
            });

    }

    void FileSystemHelper::MoveToRecycleBin(const std::string& path) {

#ifdef AE_OS_WINDOWS
        // Two terminations, it's a list of paths
        auto source = std::filesystem::absolute(path).lexically_normal().make_preferred().wstring() + std::wstring(2, L'\0');

        SHFILEOPSTRUCTW operation = {};
        operation.wFunc = FO_DELETE;
        operation.pFrom = source.c_str();
        // Windows asks itself if a file doesn't fit into the recycle bin
        operation.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT | FOF_WANTNUKEWARNING;

        auto result = SHFileOperationW(&operation);
        if (result != 0)
            throw std::runtime_error("the system returned " + std::to_string(result));
        if (operation.fAnyOperationsAborted)
            throw std::runtime_error("it was cancelled");
#endif

    }

    void FileSystemHelper::Duplicate(const std::string& path) {

        JobSystem::Execute(duplicateGroup, [path](JobData&) {

            try {
                std::filesystem::copy(path, GetFreePath(path), std::filesystem::copy_options::recursive);
                Notifications::Push({ "Successfully duplicated " + path });
                ContentDiscovery::Execute();
            }
            catch (std::exception& e) {
                auto message = "Error duplicating " + path + ": " + e.what();
                Notifications::Push({ message, NotificationType::Error });
                Log::Error(message);
            }
            });


    }

}