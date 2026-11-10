#include "Player.h"
#include "SceneEditor.h"

#include "resource/ResourceManager.h"
#include "scripting/Script.h"
#include "scene/components/LuaScriptComponent.h"

namespace Atlas::Capture {

    using namespace Scene::Components;

    void Player::Reset(const Job& job) {

        Stop();
        timeline.Reset(job.timeline);

    }

    void Player::Start(const Job& job, Scene::Scene& playedScene, std::vector<EventSpec> events, Report& jobReport,
        sol::state* luaState, uint32_t windowId) {

        scene = &playedScene;
        report = &jobReport;
        timeline.windowId = windowId;

        if (auto dropped = timeline.Start(std::move(events), job.duration))
            report->Warn(std::to_string(dropped) + " timeline events after the duration of " + FormatFloat(job.duration, 2) + "s are ignored");

        for (const auto& spec : job.scripts) {
            auto script = ResourceManager<Scripting::Script>::GetOrLoadResource(spec.path);
            if (!script.IsLoaded())
                throw std::runtime_error("Couldn't load script " + spec.path);

            std::vector<Scene::Entity> entities;
            if (spec.entity.empty()) {
                auto entity = scene->CreateEntity();
                entity.AddComponent<NameComponent>("Capture script " + fs::path(spec.path).stem().string());
                entity.AddComponent<TransformComponent>(mat4(1.0f), false);
                entities.push_back(entity);
            }
            else {
                entities = FindTopmostEntities(*scene, spec.entity);
                if (entities.empty())
                    throw std::runtime_error("Script " + spec.path + ": no entity matches '" + spec.entity + "'");
            }

            for (auto entity : entities) {
                if (entity.HasComponent<LuaScriptComponent>())
                    throw std::runtime_error(GetEntityLabel(entity) + " already has a script, attach to a new entity instead");
                entity.AddComponent<LuaScriptComponent>(script);
                report->Print("Attached script " + spec.path + " to " + GetEntityLabel(entity));
            }
        }

        lua.Start(luaState ? *luaState : scene->GetLuaState(), scene, *report, [this]() { return timeline.time; });

    }

    void Player::Stop() {

        lua.Stop();
        scene = nullptr;

    }

    bool Player::Update(const OwnerEvent& ownerEvent) {

        timeline.Update([&](const json& condition) {

            bool passed = false;
            Expect(condition, passed);
            return passed;

        }, [&](const std::string& message) { report->Print(message); });

        while (auto due = timeline.GetDueEvent()) {
            const auto& event = *due;

            auto result = EventResult::Done;
            std::visit([&](const auto& action) {

                using Action = std::decay_t<decltype(action)>;
                if constexpr (std::is_same_v<Action, CaptureEvent> || std::is_same_v<Action, ResetScriptsEvent>)
                    result = ownerEvent(event);
                else
                    Run(action);

            }, event.action);

            if (result == EventResult::Defer)
                break;
            timeline.PopEvent();
            if (result == EventResult::Suspend)
                return false;
        }

        return true;

    }

    void Player::ReportMissedEvents(CheckRunner& checks) {

        for (const auto& event : timeline.GetMissedEvents()) {
            auto message = "Conditional event never happened: " + event.data.dump();
            if (event.optional)
                report->Warn(message);
            else
                checks.AddFailure({ "timeline", event.type, timeline.time, message });
        }

    }

    json Player::Expect(const json& expectation, bool& passed) {

        if (expectation.contains("lua"))
            return lua.Expect(expectation, passed);
        return EvaluateExpectation(*scene, expectation, passed);

    }

    void Player::Run(const InputEvent& input) {

        timeline.Fire(input);
        auto time = FormatTime(timeline.time);
        if (input.device == InputEvent::Device::Axis)
            report->Print(time + " controller " + input.name + " " + FormatFloat(input.value, 2));
        else if (input.device == InputEvent::Device::Mouse)
            report->Print(time + " mouse " + input.action + (input.action == "move" || input.action == "wheel" ? "" : " " + input.name) +
                (input.position ? " at " + FormatFloat(input.position->x, 0) + ", " + FormatFloat(input.position->y, 0) : "") +
                (input.action == "wheel" ? " " + FormatFloat(input.value, 1) : ""));
        else
            report->Print(time + (input.device == InputEvent::Device::Key ? " key " : " controller ") + input.name + " " + input.action);

    }

    void Player::Run(const LookEvent& look) {

        auto entities = FindTopmostEntities(*scene, look.entity);
        if (entities.empty())
            report->Warn(FormatTime(timeline.time) + " look: no entity matches '" + look.entity + "'");
        for (auto entity : entities)
            SetLookDirection(entity, look.yaw, look.pitch);

    }

    void Player::Run(const TeleportEvent& teleport) {

        auto entities = FindTopmostEntities(*scene, teleport.entity);
        if (entities.empty())
            report->Warn(FormatTime(timeline.time) + " teleport: no entity matches '" + teleport.entity + "'");

        for (auto entity : entities) {
            if (auto player = entity.TryGetComponent<PlayerComponent>(); player && player->IsValid())
                player->SetPosition(teleport.position);
            SetWorldPosition(entity, teleport.position);
            if (teleport.yaw || teleport.pitch)
                SetLookDirection(entity, teleport.yaw, teleport.pitch);
            report->Print(FormatTime(timeline.time) + " teleported " + GetEntityLabel(entity) + " to " + FormatVec3(teleport.position));
        }

    }

    void Player::Run(const AnimationEvent& spec) {

        auto entities = FindTopmostEntities(*scene, spec.entity);
        if (entities.empty())
            report->Warn(FormatTime(timeline.time) + " animation: no entity matches '" + spec.entity + "'");

        ResourceHandle<Animation::Animation> animation;
        try {
            animation = LoadAnimationForScene(spec.animation, spec.model);
        }
        catch (const std::exception& e) {
            report->Warn(FormatTime(timeline.time) + " animation: " + e.what());
            return;
        }

        for (auto entity : entities) {
            auto animationComponent = entity.TryGetComponent<AnimationComponent>();
            if (!animationComponent) {
                report->Warn(FormatTime(timeline.time) + " animation: " + GetEntityLabel(entity) + " has no animation component");
                continue;
            }
            animationComponent->Play(animation, spec.blendTime);
            report->Print(FormatTime(timeline.time) + " playing " + animation.GetResource()->path + " on " + GetEntityLabel(entity));
        }

    }

    void Player::Run(const LogEvent& log) {

        report->Print(FormatTime(timeline.time) + " " + log.text);

    }

    void Player::Run(const ExpectEvent& expect) {

        bool passed = false;
        auto result = Expect(expect.expectation, passed);
        result["t"] = timeline.time;
        result["passed"] = passed;
        report->expectations.push_back(result);

        report->Print(FormatTime(timeline.time) + " expectation " + (passed ? "passed: " : "FAILED: ") + result.dump());

    }

    void Player::Run(const PauseEvent& pause) {

        // The job time continues, e.g. to check that nothing moves and scripts keep their state
        scene->paused = pause.pause;
        report->Print(FormatTime(timeline.time) + (scene->paused ? " paused the scene" : " resumed the scene"));

    }

    void Player::Run(const EndEvent&) {

        // The remaining timed events are skipped, conditional events that didn't happen yet still fail
        report->Print(FormatTime(timeline.time) + " ended the job");
        timeline.End();

    }

    void Player::Run(const LuaEvent& event) {

        auto name = (event.file.empty() ? "event" : fs::path(event.file).filename().string()) + " at " + FormatTime(timeline.time);
        if (!event.result.empty()) {
            report->results[event.result] = lua.Evaluate(event.code, name);
            report->Print(FormatTime(timeline.time) + " result " + event.result + " = " + report->results[event.result].dump());
            return;
        }

        auto result = lua.Run(event.code, name);
        if (!result.is_null())
            report->Print(FormatTime(timeline.time) + " lua returned " + result.dump());

    }

    void Player::SetLookDirection(Scene::Entity entity, std::optional<float> yaw, std::optional<float> pitch) {

        auto camera = entity.TryGetComponent<CameraComponent>();
        if (!camera) {
            // Entities without a camera (e.g. driven by a script) are turned around the world Y axis instead
            auto transform = entity.TryGetComponent<TransformComponent>();
            if (!transform || !yaw) {
                report->Warn(GetEntityLabel(entity) + " has no camera, only the yaw of its transform can be set");
                return;
            }

            auto global = glm::translate(vec3(transform->globalMatrix[3])) * glm::rotate(glm::radians(*yaw), vec3(0.0f, 1.0f, 0.0f));
            transform->SetGlobal(global, scene->GetParentEntity(entity));
            report->Print(FormatTime(timeline.time) + " " + GetEntityLabel(entity) + " turned to yaw " + FormatFloat(*yaw, 1));
            return;
        }

        // Yaw 0 looks towards +Z, 90 towards +X
        if (yaw)
            camera->rotation.x = glm::radians(*yaw);
        if (pitch)
            camera->rotation.y = glm::radians(*pitch);

        report->Print(FormatTime(timeline.time) + " " + GetEntityLabel(entity) + " looks at yaw " +
            FormatFloat(glm::degrees(camera->rotation.x), 1) + ", pitch " +
            FormatFloat(glm::degrees(camera->rotation.y), 1));

    }

}
