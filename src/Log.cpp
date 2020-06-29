#include <Log.h>
#include <Clock.h>
#include <loader/AssetLoader.h>

namespace Atlas {

	std::vector<Log::Entry> Log::entries;
	std::mutex Log::mutex;

	void Log::Message(std::string message, int32_t severity) {
		AddEntry(message, severity, Type::TYPE_MESSAGE);
	}

	void Log::Warning(std::string message, int32_t severity) {
		AddEntry(message, severity, Type::TYPE_WARNING);
	}

	void Log::Error(std::string message, int32_t severity) {
		AddEntry(message, severity, Type::TYPE_ERROR);
	}

	std::vector<Log::Entry> Log::GetEntries() {
		std::lock_guard<std::mutex> lock(mutex);
		return entries;
	}

	void Log::AddEntry(std::string message, int32_t severity, int32_t type) {

		Entry entry;

		entry.message = message;

		entry.time = Clock::Get();
		entry.severity = severity;
		entry.type = type;

#ifdef AE_SHOW_LOG
		std::string consoleLog;
		consoleLog.append("[" + std::to_string(entry.time) + "] ");
		consoleLog.append(message);
#ifdef AE_OS_ANDROID
		size_t maxLogSize = 2000;
		for (size_t i = 0; i <= message.length() / maxLogSize; i++) {
		    auto start = i * maxLogSize;
		    auto end = (i + 1) * maxLogSize;
		    end = end > message.length() ? message.length() : end;
		    AtlasLog("%s", message.substr(start, end).c_str());
		}
#else
		AtlasLog("%s", consoleLog.c_str());
#endif
#endif

		std::lock_guard<std::mutex> lock(mutex);

		entries.push_back(entry);

	}

	void Log::Save(std::string filename) {

		auto stream = Loader::AssetLoader::WriteFile(filename, std::ios::out);

		if (!stream.is_open()) {
			Log::Warning("Couldn't save log to hard drive");
			return;
		}

		for (auto entry : entries) {
			std::string entryString;
			entryString.append("[" + std::to_string(entry.time) + "] ");
			entryString.append(entry.message);
			entryString.append("\n");
			stream << entryString;
		}

		stream.close();

	}

}