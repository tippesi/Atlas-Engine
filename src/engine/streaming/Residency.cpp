#include "Residency.h"
#include "../graphics/Image.h"
#include "../jobsystem/JobSystem.h"
#include "../resource/ResourceLoadException.h"

#include <algorithm>
#include <tuple>

namespace Atlas::Streaming {

    bool ResidencyEntry::TryLoad(int32_t level) {

        try {
            return Load(level);
        }
        catch (const ResourceLoadException&) {
            // Logged by the exception
            return false;
        }

    }

    void ResidencyEntry::ResetPlan(int32_t updateCount) {

        requestedLevel = residentLevel;
        targetLevel = residentLevel;
        requestedUpdate = updateCount;
        lastDesiredLevel = residentLevel;

    }

    void ResidencyEntry::ApplyLoaded() {

        Apply();
        residentLevel = loadedLevel;
        loaded = false;

    }

    size_t MipResidencyEntry::GetSize(int32_t level) const {

        return levelSizes[level];

    }

    int32_t MipResidencyEntry::GetResolution(int32_t level) const {

        return std::max(std::max(width, height) >> level, 1);

    }

    int32_t MipResidencyEntry::GetMinLevel(int32_t minResolution) const {

        auto level = 0;
        while (level + 1 < mipCount && GetResolution(level) > minResolution)
            level++;
        return level;

    }

    void MipResidencyEntry::AddLevelSizes(const Graphics::Image& image) {

        levelSizes.resize(size_t(mipCount + 1));
        for (int32_t level = 0; level < mipCount; level++) {
            auto mipLevels = std::max(int32_t(image.mipLevels) + residentLevel - level, 1);
            levelSizes[level] += image.GetMemorySize(uint32_t(std::max(width >> level, 1)),
                uint32_t(std::max(height >> level, 1)), uint32_t(mipLevels));
        }

    }

    void MipResidencyEntry::PrepareLevels(const StreamingSettings::Levels& settings) {

        budgetLevel = GetMinLevel(settings.minResolution);
        desiredLevel = budgetLevel;
        auto resolution = request * settings.resolutionScale;
        while (desiredLevel > 0 && float(GetResolution(desiredLevel)) < resolution)
            desiredLevel--;
        priority = float(GetResolution(requestedLevel));
        request = 0.0f;

    }

    Residency::Residency(const char* name, int32_t planOffset) : loadJobs(name, JobPriority::Low), planOffset(planOffset) {



    }

    void Residency::Insert(const void* key, const Ref<ResidencyEntry>& entry) {

        // Reductions of new entries wait for the reduction delay as well
        entry->ResetPlan(updateCount);

        auto& slot = entries[key];
        if (slot) {
            slot->removed = true;
            changes.clear();
        }
        slot = entry;

    }

    ResidencyEntry* Residency::Find(const void* key) const {

        auto it = entries.find(key);
        return it != entries.end() ? it->second.get() : nullptr;

    }

    void Residency::Update(const StreamingSettings::Limits& settings) {

        // Jobs run inline without worker threads, so they start once the lock is released
        std::vector<std::pair<Ref<ResidencyEntry>, int32_t>> loads;

        {
            std::scoped_lock lock(mutex);

            updateCount++;

            for (const auto& entry : loadedEntries) {
                if (!entry->removed && entry->loaded)
                    entry->ApplyLoaded();
            }
            loadedEntries.clear();

            // All requests of a rotation are in by now, the residencies plan in different updates
            if (updateCount % requestRotation == planOffset) {
                std::vector<ResidencyEntry*> aliveEntries;
                aliveEntries.reserve(entries.size());
                for (auto it = entries.begin(); it != entries.end();) {
                    auto& entry = it->second;
                    if (!entry->Prepare()) {
                        entry->removed = true;
                        it = entries.erase(it);
                        continue;
                    }

                    aliveEntries.push_back(entry.get());
                    ++it;
                }

                changes = Plan(aliveEntries, settings);
            }

            // The changes are worked through until the next plan, a teleport can make thousands of entries due at once
            auto releases = 0;
            for (auto entry : changes) {
                if (entry->loading || entry->loaded || entry->targetLevel == entry->residentLevel ||
                    updateCount < entry->retryUpdate)
                    continue;

                if (entry->targetLevel > entry->residentLevel) {
                    if (releases >= maxReleasesPerUpdate)
                        continue;

                    auto release = entry->ReleaseTo(entry->targetLevel);
                    if (release == ResidencyEntry::Release::Done) {
                        entry->residentLevel = entry->targetLevel;
                        releases++;
                    }
                    if (release != ResidencyEntry::Release::NeedsLoad)
                        continue;
                }

                if (stats.pendingLoads >= settings.maxPendingLoads)
                    continue;

                entry->loading = true;
                stats.pendingLoads++;
                stats.loadCount++;
                loads.push_back({ entry->shared_from_this(), entry->targetLevel });
            }
        }

        for (const auto& [entry, level] : loads)
            LoadAsync(entry, level);

    }

    void Residency::LoadNow(const void* key, int32_t level) {

        // A load that is still running finishes first
        JobSystem::Wait(loadJobs);

        std::scoped_lock lock(mutex);

        auto entry = Find(key);
        if (!entry)
            return;

        if (entry->loaded)
            entry->ApplyLoaded();

        if (entry->residentLevel != level && entry->TryLoad(level)) {
            entry->Apply();
            entry->residentLevel = level;
        }

        // Kept until the requests stay lower for the reduction delay
        entry->ResetPlan(updateCount);

    }

    ResidencyStats Residency::GetStats() {

        std::scoped_lock lock(mutex);
        return stats;

    }

    void Residency::Shutdown() {

        JobSystem::Wait(loadJobs);

        std::scoped_lock lock(mutex);
        for (auto& [_, entry] : entries)
            entry->removed = true;
        entries.clear();
        changes.clear();
        loadedEntries.clear();
        stats = {};

    }

    std::vector<ResidencyEntry*> Residency::Plan(const std::vector<ResidencyEntry*>& entries,
        const StreamingSettings::Limits& settings) {

        stats = { .count = int32_t(entries.size()), .pendingLoads = stats.pendingLoads, .loadCount = stats.loadCount };

        for (auto entry : entries) {
            // Reductions wait until a lower request persisted
            auto level = settings.enable ? std::min(entry->desiredLevel, entry->lastDesiredLevel) : 0;
            entry->lastDesiredLevel = entry->desiredLevel;
            if (level <= entry->requestedLevel || updateCount - entry->requestedUpdate >= settings.reductionDelay) {
                entry->requestedLevel = level;
                entry->requestedUpdate = updateCount;
            }

            auto residentSize = entry->GetSize(entry->residentLevel);
            stats.residentCount += residentSize > 0 ? 1 : 0;
            stats.residentBytes += residentSize;
            stats.requestedBytes += entry->GetSize(entry->requestedLevel);
        }

        auto getBiasedLevel = [](const ResidencyEntry* entry, int32_t bias) {
            return std::min(entry->requestedLevel + bias, std::max(entry->budgetLevel, entry->requestedLevel));
            };
        auto getBiasedSize = [&](int32_t bias) {
            size_t size = 0;
            for (auto entry : entries)
                size += entry->GetSize(getBiasedLevel(entry, bias));
            return size;
            };

        // Above the budget all entries are lowered by the same number of levels, the entries with the highest
        // priority get one level back from what is left
        const int32_t maxBias = 16;
        auto budget = size_t(std::max(settings.budget, 0)) << 20;
        auto bias = 0;
        auto size = stats.requestedBytes;
        while (settings.enable && bias < maxBias && size > budget) {
            // All entries reached their budget level, the budget can't be met
            auto biasedSize = getBiasedSize(bias + 1);
            if (biasedSize >= size)
                break;
            size = biasedSize;
            bias++;
        }

        // The bias only goes down again once the requests fit well into the budget, otherwise requests close to the
        // budget reduce and load all entries again and again
        const float budgetHysteresis = 0.9f;
        if (settings.enable && bias < budgetBias && size > size_t(float(budget) * budgetHysteresis)) {
            bias = budgetBias;
            size = getBiasedSize(bias);
        }
        budgetBias = bias;
        stats.budgetBias = bias;

        std::vector<ResidencyEntry*> biasedEntries;
        for (auto entry : entries) {
            entry->targetLevel = getBiasedLevel(entry, bias);
            if (entry->targetLevel > entry->requestedLevel)
                biasedEntries.push_back(entry);
        }

        // Entries keep a level they have before others get one, so entries of similar priority don't swap their
        // levels with every move of the camera
        std::sort(biasedEntries.begin(), biasedEntries.end(), [](const ResidencyEntry* a, const ResidencyEntry* b) {
            auto aKeeps = a->residentLevel < a->targetLevel;
            auto bKeeps = b->residentLevel < b->targetLevel;
            return aKeeps != bKeeps ? aKeeps : a->priority > b->priority;
            });
        for (auto entry : biasedEntries) {
            auto increase = entry->GetSize(entry->targetLevel - 1) - entry->GetSize(entry->targetLevel);
            if (size + increase > budget)
                continue;
            entry->targetLevel--;
            size += increase;
        }

        std::vector<ResidencyEntry*> changes;
        for (auto entry : entries) {
            if (!entry->loading && !entry->loaded && updateCount >= entry->retryUpdate &&
                entry->targetLevel != entry->residentLevel)
                changes.push_back(entry);
        }

        // Reductions free memory, they go first while the budget is exceeded. Larger changes go first
        auto overBudget = settings.enable && stats.residentBytes > budget;
        auto rank = [&](const ResidencyEntry* entry) {
            auto change = entry->residentLevel - entry->targetLevel;
            auto reduction = change < 0;
            return std::make_tuple(reduction != overBudget, -std::abs(change), -entry->priority);
            };
        std::sort(changes.begin(), changes.end(), [&](const ResidencyEntry* a, const ResidencyEntry* b) {
            return rank(a) < rank(b);
            });

        return changes;

    }

    void Residency::LoadAsync(const Ref<ResidencyEntry>& entry, int32_t level) {

        JobSystem::Execute(loadJobs, [this, entry, level](JobData&) {
            auto loaded = entry->TryLoad(level);

            std::scoped_lock lock(mutex);
            stats.pendingLoads--;
            entry->loading = false;

            // The resource might be gone in the meantime
            if (entry->removed)
                return;

            if (loaded) {
                entry->loaded = true;
                entry->loadedLevel = level;
                loadedEntries.push_back(entry);
            }
            else {
                entry->retryUpdate = updateCount + failedLoadRetryDelay;
            }
            });

    }

}
