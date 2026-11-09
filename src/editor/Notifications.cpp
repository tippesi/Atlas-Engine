#include "Notifications.h"
#include "common/MathHelper.h"
#include "Singletons.h"
#include "Theme.h"

#include "common/Hash.h"
#include "Log.h"

#include <imgui.h>

#include <algorithm>
#include <imgui_stdlib.h>

namespace Atlas::Editor {

	static ImVec4 Fade(ImVec4 color, float alpha) {

		color.w *= alpha;
		return color;

	}

	std::vector<Notification> Notifications::notifications;
	std::vector<Notification> Notifications::history;
	int32_t Notifications::unreadCount = 0;
	std::mutex Notifications::mutex;

	void Notifications::Push(const Notification& notification) {

		std::scoped_lock lock(mutex);

		notifications.emplace_back(notification);

		// Only the latest ones are kept, older ones are rarely of interest
		history.emplace_back(notification);
		if (history.size() > 100)
			history.erase(history.begin());
		unreadCount++;

	}

	void Notifications::DisplayHistory() {

		const float width = 380.0f;
		const float stripeWidth = 3.0f;

		std::scoped_lock lock(mutex);

		ImGui::TextUnformatted("Notifications");
		if (!history.empty()) {
			ImGui::SameLine(width - ImGui::CalcTextSize("Clear all").x - 2.0f * ImGui::GetStyle().FramePadding.x);
			if (ImGui::SmallButton("Clear all"))
				history.clear();
		}
		ImGui::Separator();

		if (history.empty()) {
			ImGui::Dummy(ImVec2(width, 0.0f));
			ImGui::TextDisabled("No notifications yet");
			return;
		}

		// The list grows with the notifications until it scrolls
		ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0.0f), ImVec2(width, 420.0f));
		ImGui::BeginChild("Notification history", ImVec2(width, 0.0f), ImGuiChildFlags_AutoResizeY);

		auto time = Clock::Get();
		for (auto iter = history.rbegin(); iter != history.rend(); iter++) {
			const auto& notification = *iter;

			auto isError = notification.type == NotificationType::Error;
			auto isWarning = notification.type == NotificationType::Warning;
			auto color = GetColor(notification);

			ImGui::PushID(&notification);
			auto start = ImGui::GetCursorScreenPos();
			ImGui::Indent(stripeWidth + 8.0f);

			ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width - stripeWidth - 16.0f);
			ImGui::TextUnformatted(notification.message.c_str());
			ImGui::PopTextWrapPos();

			// Older notifications show their age, the newest are just new
			auto age = int32_t(time - notification.creationTime);
			auto ageText = age < 10 ? std::string("Just now") : age < 60 ? std::to_string(age) + " s ago" :
				age < 3600 ? std::to_string(age / 60) + " min ago" : std::to_string(age / 3600) + " h ago";
			ImGui::TextDisabled("%s%s", isError ? "Error \xC2\xB7 " : isWarning ? "Warning \xC2\xB7 " : "", ageText.c_str());

			ImGui::Unindent(stripeWidth + 8.0f);
			auto end = ImGui::GetItemRectMax();
			ImGui::GetWindowDrawList()->AddRectFilled(start, ImVec2(start.x + stripeWidth, end.y),
				ImGui::GetColorU32(color), stripeWidth * 0.5f);
			ImGui::PopID();

			if (iter + 1 != history.rend())
				ImGui::Dummy(ImVec2(0.0f, 2.0f));
		}

		ImGui::EndChild();

	}

	int32_t Notifications::GetUnreadCount() {

		std::scoped_lock lock(mutex);

		return std::min(unreadCount, int32_t(history.size()));

	}

	bool Notifications::HasUnreadError() {

		std::scoped_lock lock(mutex);

		auto count = std::min(unreadCount, int32_t(history.size()));
		return std::any_of(history.end() - count, history.end(),
			[](const Notification& notification) { return notification.type == NotificationType::Error; });

	}

	void Notifications::MarkAsRead() {

		std::scoped_lock lock(mutex);

		unreadCount = 0;

	}

	void Notifications::Display() {

		const auto notificationWindowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking | 
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoFocusOnAppearing;

		auto viewport = ImGui::GetMainViewport();
		auto viewportSize = viewport->Size;

		std::scoped_lock lock(mutex);

		std::vector<uint32_t> toBeRemoved;

		auto textScale = 1.1f;
		auto notificationDist = 8.0f;
		auto padding = ImVec2(16.0f, 10.0f);
		auto stripeWidth = 4.0f;
		auto notificationHeight = ImGui::GetTextLineHeight() * textScale + 2.0f * padding.y;
		auto bottomY = viewport->Pos.y + viewportSize.y - 24.0f;

		auto notificationsReversed = notifications;
		std::reverse(notificationsReversed.begin(), notificationsReversed.end());

		auto time = Clock::Get();
		for (size_t i = 0; i < notificationsReversed.size(); i++) {

			const auto& notification = notificationsReversed[i];

			float timeRemaining = notification.creationTime + notification.displayTime - time;
			if (timeRemaining <= 0.0f) {
				toBeRemoved.push_back(notifications.size() - i - 1);
				continue;
			}

			auto alpha = Helper::Math::Saturate(timeRemaining - notification.fadeoutTime);

			Hash hash = 0;
			HashCombine(hash, notification.message);
			HashCombine(hash, notification.creationTime);

			auto stripeColor = GetColor(notification, alpha);

			ImGui::SetNextWindowBgAlpha(1.0f);
			ImGui::PushStyleColor(ImGuiCol_WindowBg, Fade(ImGui::GetStyleColorVec4(ImGuiCol_PopupBg), alpha));
			ImGui::PushStyleColor(ImGuiCol_Border, Fade(ImGui::GetStyleColorVec4(ImGuiCol_Border), alpha));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, Theme::overlayRounding);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

			auto textWidth = ImGui::CalcTextSize(notification.message.c_str()).x * textScale;
			auto notificationWidth = std::min(textWidth + 2.0f * padding.x + stripeWidth, viewportSize.x * 0.8f);

			auto windowName = "Notification##" + std::to_string(hash);
			ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + (viewportSize.x - notificationWidth) * 0.5f,
				bottomY - notificationHeight));
			ImGui::SetNextWindowSize(ImVec2(notificationWidth, notificationHeight));
			ImGui::Begin(windowName.c_str(), nullptr, notificationWindowFlags);

			auto windowPos = ImGui::GetWindowPos();
			ImGui::GetWindowDrawList()->AddRectFilled(windowPos, ImVec2(windowPos.x + stripeWidth, windowPos.y + notificationHeight),
				ImGui::GetColorU32(stripeColor), Theme::overlayRounding, ImDrawFlags_RoundCornersLeft);

			ImGui::SetWindowFontScale(textScale);
			ImGui::SetCursorPos(ImVec2(stripeWidth + padding.x, padding.y));

			ImGui::PushStyleColor(ImGuiCol_Text, Fade(ImGui::GetStyleColorVec4(ImGuiCol_Text), alpha));
			ImGui::TextUnformatted(notification.message.c_str());
			ImGui::PopStyleColor();

			ImGui::SetWindowFontScale(1.0f);

			bottomY -= notificationHeight + notificationDist;

			ImGui::End();

			ImGui::PopStyleVar(2);
			ImGui::PopStyleColor(2);

		}

		for (auto idx : toBeRemoved)
			notifications.erase(notifications.begin() + idx);

	}

	ImVec4 Notifications::GetColor(const Notification& notification, float alpha) {

		switch (notification.type) {
		case NotificationType::Error: return Theme::Error(alpha);
		case NotificationType::Warning: return Theme::Warning(alpha);
		default: return Theme::Accent(alpha);
		}

	}

}