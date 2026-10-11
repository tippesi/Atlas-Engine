#include "Notifications.h"
#include "common/MathHelper.h"
#include "Singletons.h"
#include "Theme.h"

#include "common/Hash.h"
#include "Log.h"

#include <imgui.h>
#include <imgui_stdlib.h>

namespace Atlas::Editor {

	static ImVec4 Fade(ImVec4 color, float alpha) {

		color.w *= alpha;
		return color;

	}

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

			// White is the default color, which means "informational": use the accent then
			bool isDefaultColor = notification.color == vec3(1.0f);
			auto stripeColor = isDefaultColor ? Theme::Accent(alpha) :
				ImVec4(notification.color.r, notification.color.g, notification.color.b, alpha);

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

}