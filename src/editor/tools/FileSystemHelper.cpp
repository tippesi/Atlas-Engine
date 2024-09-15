#include "FileSystemHelper.h"
#include "Notifications.h"
#include "Log.h"
#include "common/Path.h"
#include "ContentDiscovery.h"

#include <filesystem>

namespace Atlas::Editor {

    JobGroup FileSystemHelper::copyGroup;
    JobGroup FileSystemHelper::deleteGroup;
    JobGroup FileSystemHelper::duplicateGroup;

    void FileSystemHelper::Copy(const std::string& path, const std::string& destination) {

        std::vector<std::string> paths = { path };
        Copy(paths, destination);

    }

    void FileSystemHelper::Copy(const std::vector<std::string>& paths, const std::string& destination) {

        JobSystem::Execute(copyGroup, [paths, destination](JobData&) {
            bool success = true;
            for (const auto& path : paths) {
                try {
                    std::filesystem::copy(path, destination,
                        std::filesystem::copy_options::overwrite_existing |
                        std::filesystem::copy_options::recursive);
                }
                catch (std::exception& e) {
                    success = false;
                    auto message = "Error copying " + path + " to asset directory: " + e.what();
                    Notifications::Push({ message, vec3(1.0f, 0.0f, 0.0f) });
                    Log::Error(message);
                }
            }
            if (success) {
                Notifications::Push({ "Finished copying file(s) to " + Common::Path::GetAbsolute(destination) });
                ContentDiscovery::Execute();
            }
            });

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
                    std::filesystem::remove_all(path);
                }
                catch (std::exception& e) {
                    success = false;
                    auto message = "Error deleting " + path + ": " + e.what();
                    Notifications::Push({ message, vec3(1.0f, 0.0f, 0.0f) });
                    Log::Error(message);
                }
            }
            if (success) {
                Notifications::Push({ "Successfully deleted files(s)" });
                ContentDiscovery::Execute();
            }
            });

    }

    void FileSystemHelper::Duplicate(const std::string& path) {

        JobSystem::Execute(duplicateGroup, [path](JobData&) {
            int32_t counter = 0;
            auto filePath = std::filesystem::path(path);
            auto dupFilePath = std::filesystem::path(path);

            try {
                do {
                    dupFilePath = path;
                    dupFilePath.replace_extension("");
                    auto replacement = dupFilePath.filename().string() + "(" + std::to_string(++counter) + ")";
                    dupFilePath.replace_filename(replacement);
                    dupFilePath.replace_extension(filePath.extension());
                } while (std::filesystem::exists(dupFilePath) && counter < 20);
                std::filesystem::copy(filePath, dupFilePath);
                Notifications::Push({ "Successfully duplicated " + path });
                ContentDiscovery::Execute();
            }
            catch (std::exception& e) {
                auto message = "Error duplicating " + path + ": " + e.what();
                Notifications::Push({ message, vec3(1.0f, 0.0f, 0.0f) });
                Log::Error(message);
            }
            });


    }

}