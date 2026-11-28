#pragma once

#include "Panel.h"

#include "scene/components/AnimationComponent.h"

#include <unordered_set>

namespace Atlas::Editor::UI {

    // Keys of the animation of a component per joint, with playback, scrubbing and key editing
    class AnimationTimelinePanel : public Panel {

    public:
        AnimationTimelinePanel() : Panel("Timeline") {}

        /**
         * Renders the panel into the current window
         * @param selectedJoint The joint selected in the skeleton, clicking a row selects its joint
         */
        void Render(AnimationComponent& animationComponent, const Mesh::Skeleton& skeleton, int32_t& selectedJoint);

        void SaveAnimation(ResourceHandle<Animation::Animation>& animation);

        bool HasSelectedKey() const { return selectedKey.IsValid(); }

        // Whether keys of the shown animation changed since it was loaded or saved
        bool modified = false;

        // Handled by the next render, the shown animation is only known there
        bool saveRequested = false;
        bool deleteKeyRequested = false;

    private:
        // The animation of the component or of one of its layers, with its playback state
        struct Track {
            ResourceHandle<Animation::Animation>* animation = nullptr;
            float* time = nullptr;
            float* speed = nullptr;
            bool* loop = nullptr;
            bool* playing = nullptr;
        };

        enum class KeyType {
            Position = 0,
            Rotation,
            Scale
        };

        struct KeyRef {
            int32_t channel = -1;
            KeyType type = KeyType::Position;
            int32_t key = -1;

            bool IsValid() const { return channel >= 0 && key >= 0; }
            bool operator==(const KeyRef& that) const = default;
        };

        // The animation of the component with -1, otherwise the one of the layer
        static Track GetTrack(AnimationComponent& animationComponent, int32_t layer);

        void RenderToolbar(AnimationComponent& animationComponent, Track& track);

        void RenderKeyEditor(Animation::Animation& animation, const Mesh::Skeleton& skeleton);

        static std::vector<float>& GetTimes(Animation::Channel& channel, KeyType type);

        // Inserts a key with the value the channel has at the time
        static int32_t InsertKey(Animation::Channel& channel, KeyType type, float time, const Mesh::Joint* joint);

        static void RemoveKey(Animation::Channel& channel, KeyType type, int32_t key);

        // Moves a key and keeps the keys sorted, returns the new index of the key
        static int32_t MoveKey(Animation::Channel& channel, KeyType type, int32_t key, float time);

        const Animation::Animation* shownAnimation = nullptr;
        std::unordered_set<const Animation::Animation*> modifiedAnimations;
        int32_t shownLayer = -1;

        KeyRef selectedKey;
        bool draggingKey = false;

        // Pixels per second
        float zoom = 200.0f;

    };

}
