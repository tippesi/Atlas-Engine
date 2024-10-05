#include "Notifications.h"
#include "Singletons.h"

#include "common/Hash.h"
#include "Log.h"

#include <imgui.h>
#include <imgui_stdlib.h>

namespace Atlas::Editor {

	std::vector<Notification> Notifications::notifications;
	std::mutex Notifications::mutex;

	void Notifications::Push(const Notification& notification) {

		std::scoped_lock lock(mutex);

		notifications.emplace_back(notification);

	}

	void Notifications::Display() {

		const auto notificationWindowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking | 
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoFocusOnAppearing;

		auto viewport = ImGui::GetMainViewport();
		auto viewportSize = viewport->Size;

		auto config = Singletons::config;

		std::scoped_lock lock(mutex);

		std::vector<uint32_t> toBeRemoved;

		auto textScale = 1.15f;
		auto notificationDist = 8.0f;
		auto notificationWidth = viewportSize.x * 0.5f;
		auto notificationHeight = ImGui::GetTextLineHeightWithSpacing() * textScale + 16.0f;
		ImVec2 notificationOffset = ImVec2((viewportSize.x - notificationWidth) / 2.0f,
			viewportSize.y - notificationDist - notificationHeight);

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

			auto alpha = std::min(timeRemaining - notification.fadeoutTime, 1.0f);

			Hash hash = 0;
			HashCombine(hash, notification.message);
			HashCombine(hash, notification.creationTime);

			if (config->darkMode) {
				ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.5f, 0.5f, 0.5f, alpha * 0.9f));
				ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1f, 0.1f, 0.1f, alpha * 0.9f));
			}
			else {
				ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.1f, 0.1f, 0.1f, alpha * 0.9f));
				ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.7f, 0.7f, 0.7f, alpha * 0.9f));
			}

			auto windowName = "Notification##" + std::to_string(hash);
			ImGui::Begin(windowName.c_str(), nullptr, notificationWindowFlags);

			ImGui::SetWindowPos(notificationOffset);
			ImGui::SetWindowSize(ImVec2(notificationWidth, notificationHeight));

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(notification.color.r,
				notification.color.g, notification.color.b, alpha));

			ImGui::SetWindowFontScale(textScale);

			auto textWidth = ImGui::CalcTextSize(notification.message.c_str()).x;

			ImGui::SetCursorPosX((notificationWidth - textWidth) * 0.5f);

			ImGui::Text("%s", notification.message.c_str());

			ImGui::SetWindowFontScale(1.0f);

			ImGui::PopStyleColor();

			notificationOffset.y = notificationOffset.y - ImGui::GetWindowHeight() - notificationDist;

			ImGui::End();

			ImGui::PopStyleColor();
			ImGui::PopStyleColor();

		}

		for (auto idx : toBeRemoved)
			notifications.erase(notifications.begin() + idx);

	}

}