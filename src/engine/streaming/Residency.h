#pragma once

#include "../System.h"
#include "StreamingSettings.h"
#include "../jobsystem/JobGroup.h"

#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace Atlas::Graphics {

    class Image;

}

namespace Atlas::Streaming {

    struct ResidencyStats {
        int32_t count = 0;
        int32_t residentCount = 0;
        int32_t pendingLoads = 0;
        // A quickly growing count means the entries keep changing their levels
        int32_t loadCount = 0;
        size_t residentBytes = 0;
        // Without the budget
        size_t requestedBytes = 0;
        // Levels all entries are lowered by to fit into the budget
        int32_t budgetBias = 0;
    };

    /**
     * The GPU side of a streamed resource, its data stays in RAM. A level is how far the resource is reduced from its
     * full detail (e.g. the mip levels left out of a texture), the last level of resources which can leave the GPU
     * completely holds nothing.
     */
    class ResidencyEntry : public std::enable_shared_from_this<ResidencyEntry> {

    public:
        enum class Release {
            Done,
            NeedsLoad,
            Wait
        };

        virtual ~ResidencyEntry() = default;

        // Turns the requests since the last update into desiredLevel and priority, false once the resource is gone
        virtual bool Prepare() = 0;

        virtual size_t GetSize(int32_t level) const = 0;

        // Runs in a job
        virtual bool Load(int32_t level) = 0;

        // Makes the loaded level resident, runs during the update
        virtual void Apply() = 0;

        // Reduces the entry to the level right away if it doesn't need a load for it
        virtual Release ReleaseTo(int32_t level) { return Release::NeedsLoad; }

        // The budget reduces the entry at most to this level
        int32_t budgetLevel = 0;
        // The budget and the reduction delay decide when this level becomes resident
        int32_t desiredLevel = 0;
        // Entries with a higher priority get the memory left within the budget first
        float priority = 0.0f;

        int32_t residentLevel = 0;
        int32_t requestedLevel = 0;

    private:
        bool TryLoad(int32_t level);

        void ResetPlan(int32_t updateCount);

        void ApplyLoaded();

        int32_t requestedUpdate = 0;
        int32_t targetLevel = 0;

        // Of the last plan
        int32_t lastDesiredLevel = 0;

        bool loading = false;
        bool loaded = false;
        bool removed = false;
        int32_t loadedLevel = 0;
        int32_t retryUpdate = 0;

        friend class Residency;

    };

    /**
     * A resource streamed by its mip levels, a level is the number of mip levels left out of the largest one. The level
     * after the last mip level holds nothing.
     */
    class MipResidencyEntry : public ResidencyEntry {

    public:
        size_t GetSize(int32_t level) const override;

        // Texels on the longer side
        int32_t GetResolution(int32_t level) const;

        int32_t GetMinLevel(int32_t minResolution) const;

        // Sizes include the driver's alignment
        void AddLevelSizes(const Graphics::Image& image);

        // Of the largest mip level
        int32_t width = 0;
        int32_t height = 0;
        int32_t mipCount = 0;

        // Texels on the longer side, the largest request until the next update counts
        float request = 0.0f;

    protected:
        void PrepareLevels(const StreamingSettings::Levels& settings);

    private:
        std::vector<size_t> levelSizes;

    };

    /**
     * Plans and loads the GPU residency of the streamed resources of one type within a budget. Entries are found by
     * the address of their resource, the mutex guards them.
     */
    class Residency {

    public:
        // Each resource is requested once within this number of updates, so plans are this number of updates apart
        static constexpr int32_t requestRotation = 4;

        // The plan offset is the update of a rotation this residency plans in
        Residency(const char* name, int32_t planOffset);

        // Both have to be called with the mutex locked
        void Insert(const void* key, const Ref<ResidencyEntry>& entry);

        ResidencyEntry* Find(const void* key) const;

        // Applies finished loads, plans the levels and starts the loads, has to run while nothing renders
        void Update(const StreamingSettings::Limits& settings);

        // Loads the level of the entry right away and waits for it, has to run while nothing renders
        void LoadNow(const void* key, int32_t level);

        ResidencyStats GetStats();

        void Shutdown();

        std::mutex mutex;

    private:
        std::vector<ResidencyEntry*> Plan(const std::vector<ResidencyEntry*>& entries, const StreamingSettings::Limits& settings);

        void LoadAsync(const Ref<ResidencyEntry>& entry, int32_t level);

        static constexpr int32_t failedLoadRetryDelay = 600;
        static constexpr int32_t maxReleasesPerUpdate = 64;

        std::unordered_map<const void*, Ref<ResidencyEntry>> entries;
        JobGroup loadJobs;

        // Entries are only removed right before the next plan, or this list is cleared
        std::vector<ResidencyEntry*> changes;
        std::vector<Ref<ResidencyEntry>> loadedEntries;

        int32_t planOffset = 0;
        int32_t updateCount = 0;
        int32_t budgetBias = 0;
        ResidencyStats stats;

    };

}
