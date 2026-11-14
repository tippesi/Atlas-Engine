#pragma once

#include "System.h"
#include "Clock.h"

#include <imgui.h>

namespace Atlas::Editor {

	enum class NotificationType {
		Info = 0,
		Warning,
		Error
	};

	struct Notification {
		std::string message;
		NotificationType type = NotificationType::Info;

		float creationTime = Clock::Get();
		float displayTime = 5.0f;
		float fadeoutTime = 1.0f;
	};

	class Notifications {

	public:
		static void Push(const Notification& notification);

		static void Display();

		// The notifications of this session, newest first, e.g. for a popup
		static void DisplayHistory();

		static int32_t GetUnreadCount();

		static bool HasUnreadError();

		static void MarkAsRead();

	private:
		static ImVec4 GetColor(const Notification& notification, float alpha = 1.0f);

		static std::vector<Notification> notifications;
		static std::vector<Notification> history;
		static int32_t unreadCount;
		static std::mutex mutex;

	};

}