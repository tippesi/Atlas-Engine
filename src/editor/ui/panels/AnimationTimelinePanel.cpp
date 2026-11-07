#include "AnimationTimelinePanel.h"

#include "../../Singletons.h"
#include "../../Notifications.h"
#include "../../Theme.h"
#include "../Widgets.h"

#include "loader/AnimationLoader.h"

#include <imgui_internal.h>

namespace Atlas::Editor::UI {

    void AnimationTimelinePanel::Render(AnimationComponent& animationComponent, const Mesh::Skeleton& skeleton,
        int32_t& selectedJoint) {

        // Without an animation on the component the first layer with one is shown
        if (shownLayer >= animationComponent.GetLayerCount() || !GetTrack(animationComponent, shownLayer).animation->IsLoaded()) {
            shownLayer = -1;
            for (int32_t layer = -1; layer < animationComponent.GetLayerCount(); layer++) {
                if (GetTrack(animationComponent, layer).animation->IsLoaded()) {
                    shownLayer = layer;
                    break;
                }
            }
        }

        auto track = GetTrack(animationComponent, shownLayer);
        if (!track.animation->IsLoaded()) {
            ImGui::TextDisabled("Play an animation to see its keys");
            saveRequested = deleteKeyRequested = false;
            return;
        }

        auto& animation = *track.animation->Get();
        if (shownAnimation != &animation) {
            shownAnimation = &animation;
            selectedKey = KeyRef();
            draggingKey = false;
        }
        modified = modifiedAnimations.contains(&animation);

        RenderToolbar(animationComponent, track);

        const auto& style = ImGui::GetStyle();
        auto lineHeight = ImGui::GetTextLineHeight();
        auto laneHeight = std::floor(0.5f * lineHeight) + 2.0f;
        auto rowHeight = 3.0f * laneHeight + 4.0f;
        auto rulerHeight = lineHeight + 8.0f;
        auto keyRadius = 0.5f * laneHeight;
        const float labelWidth = 12.0f * lineHeight;

        auto editorWidth = selectedKey.IsValid() ? 18.0f * lineHeight : 0.0f;
        auto region = ImGui::GetContentRegionAvail();
        auto timelineWidth = std::max(animation.duration * zoom, region.x - editorWidth - labelWidth - 3.0f * style.ItemSpacing.x)
            + 2.0f * keyRadius;

        auto mouse = ImGui::GetIO().MousePos;
        float timelineOriginX = 0.0f;

        auto timeToX = [&](float time) { return timelineOriginX + time * zoom; };
        auto xToTime = [&](float x) { return glm::clamp((x - timelineOriginX) / zoom, 0.0f, animation.duration); };

        auto tableFlags = ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV |
            ImGuiTableFlags_RowBg | ImGuiTableFlags_NoPadOuterX;
        if (ImGui::BeginTable("Keys", 2, tableFlags, ImVec2(region.x - (editorWidth > 0.0f ? editorWidth + style.ItemSpacing.x : 0.0f), 0.0f))) {
            // Scrolling tables have their own window to draw into
            auto drawList = ImGui::GetWindowDrawList();
            ImGui::TableSetupScrollFreeze(1, 1);
            ImGui::TableSetupColumn("Joint", ImGuiTableColumnFlags_WidthFixed, labelWidth);
            ImGui::TableSetupColumn("Keys", ImGuiTableColumnFlags_WidthFixed, timelineWidth);

            // Ruler, dragging on it scrubs through the animation
            ImGui::TableNextRow(ImGuiTableRowFlags_Headers, rulerHeight);
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled("%d animated joints", int32_t(animation.channels.size()));
            ImGui::TableNextColumn();

            auto rulerMin = ImGui::GetCursorScreenPos();
            timelineOriginX = rulerMin.x + keyRadius;
            ImGui::InvisibleButton("Ruler", ImVec2(timelineWidth, rulerHeight));
            if (ImGui::IsItemActive()) {
                *track.time = xToTime(mouse.x);
                *track.playing = false;
            }

            // Labels at least 60 pixels apart
            float steps[] = { 0.01f, 0.02f, 0.05f, 0.1f, 0.2f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f, 30.0f, 60.0f };
            float step = steps[IM_ARRAYSIZE(steps) - 1];
            for (auto candidate : steps) {
                if (candidate * zoom >= 60.0f) {
                    step = candidate;
                    break;
                }
            }

            auto tickColor = ImGui::GetColorU32(ImGuiCol_TextDisabled);
            for (int32_t i = 0; float(i) * step <= animation.duration + 0.0001f; i++) {
                auto time = float(i) * step;
                auto x = timeToX(time);
                drawList->AddLine(ImVec2(x, rulerMin.y + rulerHeight - 6.0f), ImVec2(x, rulerMin.y + rulerHeight), tickColor);
                char label[16];
                std::snprintf(label, sizeof(label), "%g", time);
                drawList->AddText(ImVec2(x + 3.0f, rulerMin.y + 2.0f), tickColor, label);
            }

            auto playheadColor = ImGui::GetColorU32(Theme::Accent());
            auto playheadX = timeToX(*track.time);
            drawList->AddTriangleFilled(ImVec2(playheadX - 5.0f, rulerMin.y + rulerHeight - 8.0f),
                ImVec2(playheadX + 5.0f, rulerMin.y + rulerHeight - 8.0f), ImVec2(playheadX, rulerMin.y + rulerHeight), playheadColor);

            ImU32 keyColors[] = {
                ImGui::GetColorU32(Theme::Axis(0)),
                ImGui::GetColorU32(Theme::Axis(1)),
                ImGui::GetColorU32(Theme::Axis(2))
            };
            const char* laneNames[] = { "Position", "Rotation", "Scale" };

            ImGuiListClipper clipper;
            clipper.Begin(int32_t(animation.channels.size()), rowHeight);
            while (clipper.Step()) {
                for (int32_t channelIdx = clipper.DisplayStart; channelIdx < clipper.DisplayEnd; channelIdx++) {
                    auto& channel = animation.channels[channelIdx];
                    auto jointIdx = skeleton.GetJointIndex(channel.jointName);

                    ImGui::PushID(channelIdx);
                    ImGui::TableNextRow(0, rowHeight);
                    if (jointIdx >= 0 && jointIdx == selectedJoint)
                        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(Theme::Accent(0.18f)));

                    ImGui::TableNextColumn();
                    if (ImGui::Selectable(channel.jointName.c_str(), false, 0, ImVec2(0.0f, rowHeight - style.CellPadding.y * 2.0f)))
                        selectedJoint = jointIdx;
                    if (jointIdx < 0)
                        ImGui::SetItemTooltip("The skeleton has no joint with this name");

                    ImGui::TableNextColumn();
                    auto rowMin = ImGui::GetCursorScreenPos();
                    ImGui::InvisibleButton("Lanes", ImVec2(timelineWidth, rowHeight - style.CellPadding.y * 2.0f));
                    auto rowHovered = ImGui::IsItemHovered();
                    auto hoveredLane = glm::clamp(int32_t((mouse.y - rowMin.y) / laneHeight), 0, 2);

                    drawList->AddLine(ImVec2(playheadX, rowMin.y), ImVec2(playheadX, rowMin.y + rowHeight), playheadColor);

                    KeyRef hoveredKey;
                    for (int32_t lane = 0; lane < 3; lane++) {
                        auto type = static_cast<KeyType>(lane);
                        const auto& times = GetTimes(channel, type);
                        auto y = rowMin.y + (float(lane) + 0.5f) * laneHeight;

                        for (int32_t key = 0; key < int32_t(times.size()); key++) {
                            auto x = timeToX(times[key]);
                            auto ref = KeyRef { channelIdx, type, key };
                            auto selected = ref == selectedKey;
                            auto hovered = rowHovered && lane == hoveredLane && std::abs(mouse.x - x) <= keyRadius + 1.0f;
                            if (hovered)
                                hoveredKey = ref;

                            auto radius = selected || hovered ? keyRadius + 1.5f : keyRadius;
                            ImVec2 diamond[] = { ImVec2(x, y - radius), ImVec2(x + radius, y), ImVec2(x, y + radius), ImVec2(x - radius, y) };
                            drawList->AddConvexPolyFilled(diamond, 4, keyColors[lane]);
                            if (selected)
                                drawList->AddPolyline(diamond, 4, ImGui::GetColorU32(ImGuiCol_Text), ImDrawFlags_Closed, 1.5f);
                        }
                    }

                    if (rowHovered) {
                        if (hoveredKey.IsValid())
                            ImGui::SetTooltip("%s %s at %.3f s", channel.jointName.c_str(), laneNames[int32_t(hoveredKey.type)],
                                GetTimes(channel, hoveredKey.type)[hoveredKey.key]);

                        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !hoveredKey.IsValid()) {
                            auto type = static_cast<KeyType>(hoveredLane);
                            auto joint = jointIdx >= 0 ? &skeleton.joints[jointIdx] : nullptr;
                            selectedKey = { channelIdx, type, InsertKey(channel, type, xToTime(mouse.x), joint) };
                            modified = true;
                        }
                        else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                            selectedKey = hoveredKey;
                            draggingKey = hoveredKey.IsValid();
                            if (jointIdx >= 0)
                                selectedJoint = jointIdx;
                        }
                    }

                    ImGui::PopID();
                }
            }

            // Dragging the selected key moves it in time
            if (draggingKey && selectedKey.IsValid()) {
                if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                    auto& channel = animation.channels[selectedKey.channel];
                    auto time = xToTime(mouse.x);
                    if (GetTimes(channel, selectedKey.type)[selectedKey.key] != time) {
                        selectedKey.key = MoveKey(channel, selectedKey.type, selectedKey.key, time);
                        modified = true;
                    }
                }
                else {
                    draggingKey = false;
                }
            }

            // Ctrl + mouse wheel zooms the timeline
            const auto& io = ImGui::GetIO();
            if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && io.KeyCtrl && io.MouseWheel != 0.0f)
                zoom = std::clamp(zoom * std::pow(1.25f, io.MouseWheel), 10.0f, 5000.0f);

            ImGui::EndTable();
        }

        auto deleteKeyPressed = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) &&
            !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete, false);
        if (selectedKey.IsValid() && (deleteKeyRequested || deleteKeyPressed)) {
            RemoveKey(animation.channels[selectedKey.channel], selectedKey.type, selectedKey.key);
            selectedKey = KeyRef();
            modified = true;
        }
        deleteKeyRequested = false;

        if (selectedKey.IsValid()) {
            ImGui::SameLine();
            ImGui::BeginChild("Key editor", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
            RenderKeyEditor(animation, skeleton);
            ImGui::EndChild();
        }

        if (modified)
            modifiedAnimations.insert(&animation);
        else
            modifiedAnimations.erase(&animation);

    }

    void AnimationTimelinePanel::SaveAnimation(ResourceHandle<Animation::Animation>& animation) {

        if (!animation.IsLoaded())
            return;

        Loader::AnimationLoader::SaveAnimation(animation.Get(), animation.GetResource()->path, true);
        Notifications::Push({ .message = "Saved animation " + animation.GetResource()->GetFileName() });
        modified = false;

    }

    AnimationTimelinePanel::Track AnimationTimelinePanel::GetTrack(AnimationComponent& animationComponent, int32_t layer) {

        Track track;
        auto setTrack = [&](auto& playback) {
            track.animation = std::addressof(playback.animation);
            track.time = &playback.time;
            track.speed = &playback.speed;
            track.loop = &playback.loop;
            track.playing = &playback.playing;
            };

        if (layer < 0)
            setTrack(animationComponent);
        else
            setTrack(animationComponent.GetLayer(layer));

        return track;

    }

    void AnimationTimelinePanel::RenderToolbar(AnimationComponent& animationComponent, Track& track) {

        auto& animation = *track.animation;
        auto iconSize = ImGui::GetTextLineHeight();

        auto& playIcon = Singletons::icons->Get(*track.playing ? IconType::Pause : IconType::Play);
        ImGui::AlignTextToFramePadding();
        if (Widgets::IconButton("Play", playIcon, iconSize, *track.playing ? "Pause" : "Play"))
            *track.playing = !*track.playing;

        // The base animation and the layers each have their own keys and playback
        auto trackName = [&](int32_t layer) {
            auto layerTrack = GetTrack(animationComponent, layer);
            auto name = layerTrack.animation->IsLoaded() ? layerTrack.animation->Get()->name : std::string("No animation");
            return (layer < 0 ? std::string("Base: ") : "Layer " + std::to_string(layer) + ": ") + name;
            };

        ImGui::SameLine();
        ImGui::SetNextItemWidth(14.0f * iconSize);
        if (ImGui::BeginCombo("##Track", trackName(shownLayer).c_str())) {
            for (int32_t layer = -1; layer < animationComponent.GetLayerCount(); layer++) {
                ImGui::BeginDisabled(!GetTrack(animationComponent, layer).animation->IsLoaded());
                if (ImGui::Selectable(trackName(layer).c_str(), layer == shownLayer))
                    shownLayer = layer;
                ImGui::EndDisabled();
            }
            ImGui::EndCombo();
        }
        ImGui::SetItemTooltip("Animation of the component or of one of its layers");
        if (modified) {
            ImGui::SameLine(0.0f, 2.0f);
            ImGui::TextColored(Theme::Warning(), "*");
            ImGui::SetItemTooltip("The keys changed since the animation was saved");
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(6.0f * iconSize);
        ImGui::DragFloat("##Time", track.time, 0.01f, 0.0f, animation->duration, "%.2f s");
        ImGui::SetItemTooltip("Time");
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("/");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(6.0f * iconSize);
        if (ImGui::DragFloat("##Duration", &animation->duration, 0.01f, 0.01f, 600.0f, "%.2f s"))
            modified = true;
        ImGui::SetItemTooltip("Duration");

        ImGui::SameLine();
        ImGui::Checkbox("Loop", track.loop);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(5.0f * iconSize);
        ImGui::DragFloat("##Speed", track.speed, 0.01f, -10.0f, 10.0f, "%.2fx");
        ImGui::SetItemTooltip("Playback speed");

        // Save on the right
        auto saveLabel = "Save animation";
        auto saveWidth = ImGui::CalcTextSize(saveLabel).x + 2.0f * ImGui::GetStyle().FramePadding.x;
        ImGui::SameLine(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowContentRegionMax().x - saveWidth));
        if (modified)
            Theme::PushPrimaryButton();
        if (ImGui::Button(saveLabel) || saveRequested)
            SaveAnimation(animation);
        saveRequested = false;
        if (modified)
            Theme::PopPrimaryButton();
        ImGui::SetItemTooltip("Writes the keys to %s", animation.GetResource()->path.c_str());

        ImGui::TextDisabled("Click a key to select it, drag to move it, Delete removes it, double click a lane adds one. "
            "Ctrl + mouse wheel zooms");

    }

    void AnimationTimelinePanel::RenderKeyEditor(Animation::Animation& animation, const Mesh::Skeleton& skeleton) {

        auto& channel = animation.channels[selectedKey.channel];
        auto& times = GetTimes(channel, selectedKey.type);
        if (selectedKey.key >= int32_t(times.size())) {
            selectedKey = KeyRef();
            return;
        }

        const char* typeNames[] = { "Position", "Rotation", "Scale" };
        ImGui::SeparatorText((channel.jointName + " " + typeNames[int32_t(selectedKey.type)]).c_str());

        auto time = times[selectedKey.key];
        if (ImGui::DragFloat("Time", &time, 0.005f, 0.0f, animation.duration, "%.3f s")) {
            selectedKey.key = MoveKey(channel, selectedKey.type, selectedKey.key, time);
            modified = true;
        }

        switch (selectedKey.type) {
        case KeyType::Position:
            modified |= Widgets::DragVec3("Position", channel.positions[selectedKey.key], 0.001f);
            break;
        case KeyType::Rotation: {
            auto& rotation = channel.rotations[selectedKey.key];
            auto euler = glm::degrees(glm::eulerAngles(rotation));
            if (Widgets::DragVec3("Rotation", euler, 0.1f, 0.0f, 0.0f, 0.0f, "%.1f")) {
                rotation = glm::quat(glm::radians(euler));
                modified = true;
            }
            break;
        }
        case KeyType::Scale:
            modified |= Widgets::DragVec3("Scale", channel.scales[selectedKey.key], 0.001f, 1.0f);
            break;
        }

        if (ImGui::Button("Delete key", ImVec2(-FLT_MIN, 0.0f))) {
            RemoveKey(channel, selectedKey.type, selectedKey.key);
            selectedKey = KeyRef();
            modified = true;
        }

    }

    std::vector<float>& AnimationTimelinePanel::GetTimes(Animation::Channel& channel, KeyType type) {

        switch (type) {
        case KeyType::Rotation: return channel.rotationTimes;
        case KeyType::Scale: return channel.scaleTimes;
        default: return channel.positionTimes;
        }

    }

    int32_t AnimationTimelinePanel::InsertKey(Animation::Channel& channel, KeyType type, float time, const Mesh::Joint* joint) {

        auto& times = GetTimes(channel, type);
        auto it = std::lower_bound(times.begin(), times.end(), time);
        auto key = int32_t(it - times.begin());

        switch (type) {
        case KeyType::Position: {
            auto value = channel.SamplePosition(time, joint ? joint->position : vec3(0.0f));
            channel.positions.insert(channel.positions.begin() + key, value);
            break;
        }
        case KeyType::Rotation: {
            auto value = channel.SampleRotation(time, joint ? joint->rotation : quat(1.0f, 0.0f, 0.0f, 0.0f));
            channel.rotations.insert(channel.rotations.begin() + key, value);
            break;
        }
        case KeyType::Scale: {
            auto value = channel.SampleScale(time, joint ? joint->scale : vec3(1.0f));
            channel.scales.insert(channel.scales.begin() + key, value);
            break;
        }
        }

        times.insert(times.begin() + key, time);
        return key;

    }

    void AnimationTimelinePanel::RemoveKey(Animation::Channel& channel, KeyType type, int32_t key) {

        auto& times = GetTimes(channel, type);
        times.erase(times.begin() + key);

        switch (type) {
        case KeyType::Position: channel.positions.erase(channel.positions.begin() + key); break;
        case KeyType::Rotation: channel.rotations.erase(channel.rotations.begin() + key); break;
        case KeyType::Scale: channel.scales.erase(channel.scales.begin() + key); break;
        }

    }

    int32_t AnimationTimelinePanel::MoveKey(Animation::Channel& channel, KeyType type, int32_t key, float time) {

        auto& times = GetTimes(channel, type);

        // Swap with the neighbours until the order is restored, the values move along
        auto swapKeys = [&](int32_t a, int32_t b) {
            std::swap(times[a], times[b]);
            switch (type) {
            case KeyType::Position: std::swap(channel.positions[a], channel.positions[b]); break;
            case KeyType::Rotation: std::swap(channel.rotations[a], channel.rotations[b]); break;
            case KeyType::Scale: std::swap(channel.scales[a], channel.scales[b]); break;
            }
            };

        times[key] = time;
        while (key > 0 && times[key - 1] > times[key]) {
            swapKeys(key - 1, key);
            key--;
        }
        while (key + 1 < int32_t(times.size()) && times[key + 1] < times[key]) {
            swapKeys(key, key + 1);
            key++;
        }

        return key;

    }

}
