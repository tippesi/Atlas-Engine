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
		 *
		 * @param message
		 * @param severity
		 */
		static void Message(std::string message, int32_t severity = SEVERITY_LOW);

		/**
		 *
		 * @param message
		 * @param severity
		 */
		static void Warning(std::string message, int32_t severity = SEVERITY_MEDIUM);

		/**
		 *
		 * @param message
		 * @param severity
		 */
		static void Error(std::string message, int32_t severity = SEVERITY_HIGH);

		/**
		 *
		 * @return
		 */
		static std::vector<Entry> GetEntries();

	private:
		static void AddEntry(std::string message, int32_t severity, int32_t type);

		static std::vector<Entry> entries;
		static std::mutex mutex;

	};

}

#endif