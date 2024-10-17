#pragma once

#include "jobsystem/JobSystem.h"

#include <vector>
#include <string>


namespace Atlas::Editor {

	class FileSystemHelper {

	public:
        static void Copy(const std::string& path, const std::string& destination);

        static void Copy(const std::vector<std::string>& paths, const std::string& destination);

        static void Delete(const std::string& path);

        static void Delete(const std::vector<std::string>& paths);

        static void Duplicate(const std::string& path);

    private:
        static JobGroup copyGroup;
        static JobGroup deleteGroup;
        static JobGroup duplicateGroup;

	};

}