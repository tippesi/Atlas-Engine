#ifndef AE_LOG_H
#define AE_LOG_H

#include "System.h"

#include <vector>
#include <mutex>

namespace Atlas {

	class Log {

	public:
		enum Severity {
			SEVERITY_LOW = 0,
			SEVERITY_MEDIUM,
			SEVERITY_HIGH
		};

		enum Type {
			TYPE_MESSAGE = 0,
			TYPE_WARNING,
			TYPE_ERROR
		};

		struct Entry {
			std::string message;

			float time;

			int32_t severity;
			int32_t type;
		};

		/**
		 * Creates a log entry of type message.
		 * @param message The message of the log entry.
		 * @param severity The severity of the log entry.
		 */
		static void Message(const std::string& message, int32_t severity = SEVERITY_LOW);

		/**
		 * Creates a log entry of type warning.
		 * @param message The message of the log entry.
		 * @param severity The severity of the log entry.
		 */
		static void Warning(const std::string& message, int32_t severity = SEVERITY_MEDIUM);

		/**
		 * Creates a log entry of type error.
		 * @param message The message of the log entry.
		 * @param severity The severity of the log entry.
		 */
		static void Error(const std::string& message, int32_t severity = SEVERITY_HIGH);

		/**
		 * Returns all log entries.
		 * @return All log entries.
		 */
		static std::vector<Entry> GetEntries();

		/**
		 * Save the log to the hard drive.
		 * @param filename The filename of the log.
		 */
		static void Save(const std::string& filename);

	private:
		static void AddEntry(const std::string& message, int32_t severity, int32_t type);

		static std::vector<Entry> entries;
		static std::mutex mutex;

	};

}

#endif