#include "SdkWorld.h"

#include <Functions.h>
#include <Globals.h>
#include <Hooks.h>
#include <Glacier/ZOutfit.h>
#include <Glacier/ZScene.h>
#include <Glacier/SExternalReferences.h>
#include <Glacier/ZSpatialEntity.h>

void SdkWorld::OnLocalBundleCreated(const ZEntityRef& ref, const SMatrix& matrix, const std::string& outfit,
    int charset, int variation, bool spawnedByHitman, bool modifiers) {
    if (!m_InScene || m_ApplyingBundle || !spawnedByHitman || !ref) return;
    ProcessBundleDeletions(); // a new native bag can reuse a just-deleted address
    if (m_Bundles.size() >= 96) return;
    Bundle bundle;
    bundle.ref = ref;
    bundle.local = true;
    bundle.message.outfit = outfit;
    bundle.message.charset = charset;
    bundle.message.variation = variation;
    bundle.message.modifiers = modifiers;
    bundle.message.xf = ToTransform(matrix);
    TrackBundle(std::move(bundle));
    CoopLog::Info("bundle", "native discarded outfit bag captured: " + outfit);
}

void SdkWorld::TrackBundle(Bundle bundle) {
    ProcessBundleDeletions();
    bundle.generation = ++m_NextBundleGeneration;
    {
        std::lock_guard lock(m_BundleDeleteMutex);
        m_TrackedBundleGenerations[bundle.ref.m_pObj] = bundle.generation;
    }
    m_Bundles.insert_or_assign(bundle.ref.m_pObj, std::move(bundle));
}

void SdkWorld::SetBundleContext(const coop::SessionStatus& status) {
    const bool active = status.synced && status.epoch && m_InScene;
    if (active && (!m_BundleSynced || status.epoch != m_BundleEpoch))
        for (auto& [ptr, bundle] : m_Bundles)
            if (bundle.local) bundle.announcedEpoch = 0;
    m_BundleEpoch = status.epoch;
    const uint8_t side = status.role == coop::SessionRole::Host ? 1 : status.role == coop::SessionRole::Client ? 2 : 0;
    if (active && side && m_BundleSide && side != m_BundleSide)
        for (auto& [ptr, bundle] : m_Bundles)
            if (bundle.local) { m_BundleLedger.live.erase(bundle.message.id); bundle.message.id = 0; bundle.announcedEpoch = 0; }
    m_BundleSide = side;
    m_BundleSynced = active && m_BundleSide;
}

void SdkWorld::FlushBundles() {
    ProcessBundleDeletions();
    if (!m_InScene || !m_BundleSynced) return;
    for (auto& [ptr, bundle] : m_Bundles) {
        if (!bundle.local || bundle.announcedEpoch == m_BundleEpoch) continue;
        auto& message = bundle.message;
        if (!message.id || (message.id >> 56) != m_BundleSide) {
            m_BundleLedger.live.erase(message.id);
            message.id = (uint64_t(m_BundleSide) << 56) | ++m_BundleSequence;
        }
        message.epoch = m_BundleEpoch;
        message.scene = m_Scene.scene;
        if (const auto* spatial = bundle.ref.QueryInterface<ZSpatialEntity>())
            message.xf = ToTransform(spatial->GetObjectToWorldMatrix());
        coop::BundleMessage checked;
        if (!coop::BundleMessage::Decode(message.Encode(true), true, checked)) {
            CoopLog::Warn("bundle", "native bag metadata failed validation; not replicated");
            bundle.announcedEpoch = m_BundleEpoch;
            continue;
        }
        Tell("Coop_BundleSpawn", message.Encode(true));
        bundle.announcedEpoch = m_BundleEpoch;
        m_BundleLedger.Spawned(message.id);
        CoopLog::Info("bundle", "sent bag spawn id=" + std::to_string(message.id) + " outfit=" + message.outfit);
    }
}

void SdkWorld::OnBundleDeleting(const ZEntityRef& ref) {
    std::lock_guard lock(m_BundleDeleteMutex);
    const auto found = m_TrackedBundleGenerations.find(ref.m_pObj);
    if (found == m_TrackedBundleGenerations.end()) return;
    m_DeletedBundleRefs.emplace_back(ref.m_pObj, found->second);
    m_TrackedBundleGenerations.erase(found); // never dereference a dying entity
}

void SdkWorld::ProcessBundleDeletions() {
    std::vector<std::pair<void*, uint64_t>> deleted;
    {
        std::lock_guard lock(m_BundleDeleteMutex);
        deleted.swap(m_DeletedBundleRefs);
    }
    for (const auto& [ptr, generation] : deleted) {
        const auto found = m_Bundles.find(ptr);
        if (found == m_Bundles.end() || found->second.generation != generation) continue;
        const auto message = found->second.message;
        m_Bundles.erase(found);
        if (message.id) m_BundleLedger.Removed(message.id);
        if (m_InScene && m_BundleSynced && !m_ApplyingBundle && message.id) {
            auto take = message;
            take.epoch = m_BundleEpoch;
            take.scene = m_Scene.scene;
            Tell("Coop_BundleTake", take.Encode(false));
            CoopLog::Info("bundle", "native bag removed; told partner id=" + std::to_string(message.id));
        }
    }
}

void SdkWorld::ApplyPeerBundle(const std::string& text, bool spawn, uint32_t epoch, uint8_t peerSide) {
    coop::BundleMessage message;
    if (!m_InScene || !coop::BundleMessage::Decode(text, spawn, message) || !message.Matches(epoch, m_Scene.scene)) {
        CoopLog::Warn("bundle", "ignored malformed or stale bag event");
        return;
    }
    if (spawn) {
        if ((message.id >> 56) != peerSide || !m_BundleLedger.CanSpawn(message.id)) return;
        if (m_Bundles.size() >= 96 || m_BundleLedger.removed.size() >= 256) {
            CoopLog::Warn("bundle", "bag replication limit reached for this mission");
            return;
        }
        const auto* kit = FindOutfitKit(message.outfit);
        if (!kit || !kit->m_pInterfaceRef || size_t(message.charset) >= kit->m_pInterfaceRef->m_aCharSets.size() ||
            !Hooks::ZClothBundleEntity_CreateClothBundle) {
            CoopLog::Warn("bundle", "partner outfit not loaded; bag not spawned: " + message.outfit);
            return;
        }
        TEntityRef<ZClothBundleEntity> result;
        // Bypass our detour so a replicated native bag cannot announce itself back.
        Hooks::ZClothBundleEntity_CreateClothBundle->CallOriginal(result, ToMatrix(message.xf),
            ZRepositoryID(ZString(message.outfit)), message.variation, message.charset, true, message.modifiers);
        if (!result.m_entityRef) {
            CoopLog::Warn("bundle", "native bag creation returned no entity");
            return;
        }
        Bundle bundle;
        bundle.ref = result.m_entityRef;
        bundle.message = message;
        TrackBundle(std::move(bundle));
        m_BundleLedger.Spawned(message.id);
        CoopLog::Info("bundle", "partner bag spawned here id=" + std::to_string(message.id) + " outfit=" + message.outfit);
        return;
    }

    auto found = std::find_if(m_Bundles.begin(), m_Bundles.end(), [&](const auto& row) { return row.second.message.id == message.id; });
    // A remove before a spawn must also leave a tombstone, but do not allow a peer
    // to consume arbitrary future IDs in our own namespace.
    if (found == m_Bundles.end() && (message.id >> 56) != peerSide) return;
    if (m_BundleLedger.removed.size() >= 256) return;
    m_BundleLedger.Removed(message.id);
    if (found == m_Bundles.end()) return;
    const ZEntityRef ref = found->second.ref;
    m_Bundles.erase(found);
    if (ref && Globals::EntityManager && Functions::ZEntityManager_DeleteEntity->Exists())
        Functions::ZEntityManager_DeleteEntity->Call(Globals::EntityManager, ref, {});
    CoopLog::Info("bundle", "partner consumed bag; removed here id=" + std::to_string(message.id));
}

void SdkWorld::ResetBundles(bool sceneCleared) {
    if (!sceneCleared) ProcessBundleDeletions();
    m_BundleSynced = false;
    if (sceneCleared) {
        // The scene frees its own entities. Never dereference refs during teardown.
        m_Bundles.clear();
        { std::lock_guard lock(m_BundleDeleteMutex); m_DeletedBundleRefs.clear(); m_TrackedBundleGenerations.clear(); }
        m_BundleLedger = {};
        m_BundleEpoch = 0;
        return;
    }
    std::vector<ZEntityRef> copies;
    for (auto it = m_Bundles.begin(); it != m_Bundles.end();) {
        if (it->second.local) { it->second.announcedEpoch = 0; ++it; }
        else { copies.push_back(it->second.ref); m_BundleLedger.live.erase(it->second.message.id); it = m_Bundles.erase(it); }
    }
    if (m_InScene && Globals::EntityManager && Functions::ZEntityManager_DeleteEntity->Exists())
        for (const auto& ref : copies)
            if (ref) Functions::ZEntityManager_DeleteEntity->Call(Globals::EntityManager, ref, {});
}
