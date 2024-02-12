#pragma once

#include "System.h"
#include "Clock.h"

namespace Atlas::Editor {

	struct Notification {
		std::string message;
		vec3 color = vec3(1.0f);

		float creationTime = Clock::Get();
		float displayTime = 5.0f;
		float fadeoutTime = 1.0f;
	};

	class Notifications {

	public:
		static void Push(const Notification& notification);

		static void Display();

	private:
		static std::vector<Notification> notifications;
		static std::mutex mutex;

	};

}