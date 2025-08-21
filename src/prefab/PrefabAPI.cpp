#include "prefab/PrefabAPI.h"
#include "prefab/PrefabSerializer.h"
#include "prefab/PrefabCache.h"
#include "serialization/Serializer.h"
#include "animation/SkeletonBinding.h"
#include "prefab/PathResolver.h"
#include "pipeline/AssetLibrary.h"
#include "rendering/ModelBuild.h"
#include "rendering/RendererFactory.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <unordered_map>
#include <filesystem>
#include <editor/Project.h>
#include <glm/gtx/matrix_decompose.hpp>

using json = nlohmann::json;

static std::string AuthoringPrefabPathFromGuid(const ClaymoreGUID& guid) {
    return std::string("assets/prefabs/") + guid.ToString() + ".prefab.json";
}

static void GenerateBoneEntitiesForSkeleton(Scene& scene, EntityID skeletonEntity)
{
    auto* skelData = scene.GetEntityData(skeletonEntity);
    if (!skelData || !skelData->Skeleton) return;
    SkeletonComponent& sk = *skelData->Skeleton;
    const size_t n = sk.InverseBindPoses.size();
    if (n == 0) return;

    // First try to bind existing descendants by name to avoid duplicating authored bones
    if (sk.BoneEntities.size() != n) sk.BoneEntities.assign(n, (EntityID)-1);
    // Build descendant name->entity map under skeletonEntity
    std::unordered_map<std::string, EntityID> nameToEntity;
    {
        std::function<void(EntityID)> dfs = [&](EntityID id){
            if (auto* d = scene.GetEntityData(id)) {
                nameToEntity[d->Name] = id;
                for (EntityID c : d->Children) dfs(c);
            }
        };
        dfs(skeletonEntity);
    }
    size_t bound = 0;
    if (sk.BoneNames.size() < n) sk.BoneNames.resize(n);
    for (size_t i = 0; i < n; ++i) {
        if (sk.BoneEntities[i] != (EntityID)-1) { ++bound; continue; }
        const std::string& nm = sk.BoneNames[i];
        if (!nm.empty()) {
            auto it = nameToEntity.find(nm);
            if (it != nameToEntity.end()) { sk.BoneEntities[i] = it->second; ++bound; }
        }
    }
    if (bound == n) return; // all mapped to existing authored bones

    // Create any missing bones and parent them using BoneParents; set local from inverse bind
    for (size_t b = 0; b < n; ++b) {
        if (sk.BoneEntities[b] != (EntityID)-1) continue;
        const std::string name = sk.BoneNames[b].empty() ? (std::string("Bone_") + std::to_string(b)) : sk.BoneNames[b];
        Entity boneEnt = scene.CreateEntity(name);
        sk.BoneEntities[b] = boneEnt.GetID();
    }
    for (size_t b = 0; b < n; ++b) {
        EntityID boneID = sk.BoneEntities[b];
        int pIdx = (b < sk.BoneParents.size() ? sk.BoneParents[b] : -1);
        EntityID parentEntity = (pIdx >= 0 && (size_t)pIdx < n) ? sk.BoneEntities[(size_t)pIdx] : skeletonEntity;
        scene.SetParent(boneID, parentEntity);

        glm::mat4 thisGlobal = glm::inverse(sk.InverseBindPoses[b]);
        glm::mat4 parentGlobal = (pIdx >= 0 && (size_t)pIdx < n) ? glm::inverse(sk.InverseBindPoses[(size_t)pIdx]) : glm::mat4(1.0f);
        glm::mat4 localBind = glm::inverse(parentGlobal) * thisGlobal;

        glm::vec3 t, scale, skew; glm::vec4 persp; glm::quat rq;
        glm::decompose(localBind, scale, rq, t, skew, persp);
        if (auto* bd = scene.GetEntityData(boneID)) {
            bd->Transform.Position = t;
            bd->Transform.Scale    = scale;
            bd->Transform.RotationQ = glm::normalize(rq);
            bd->Transform.UseQuatRotation = true;
            bd->Transform.Rotation = glm::degrees(glm::eulerAngles(rq));
            bd->Transform.TransformDirty = true;
        }
    }
}

EntityID InstantiatePrefab(const ClaymoreGUID& prefabGuid, Scene& dst, const PrefabOverrides* instanceOverridesOpt) {
    // For now prefer authoring always; compiled cache parenting is not yet implemented
    PrefabAsset author;
    if (!PrefabIO::LoadAuthoringPrefabJSON(AuthoringPrefabPathFromGuid(prefabGuid), author)) {
        std::cerr << "[Prefab] Failed to load authoring prefab for " << prefabGuid.ToString() << std::endl;
        return (EntityID)-1;
    }

    // Pass 1: create entities and record GUID→ID map (handle compact model-asset nodes like scene deserialization)
    std::unordered_map<uint64_t, EntityID> guidToId; guidToId.reserve(author.Entities.size()*2);
    auto pack = [](const ClaymoreGUID& g)->uint64_t { return (g.high ^ (g.low << 1)); };
    for (const auto& e : author.Entities) {
        bool handledAsAsset = false;
        if (e.Components.contains("asset") && e.Components["asset"].is_object()) {
            const auto& a = e.Components["asset"];
            std::string type = a.value("type", "");
            if (type == "model") {
                std::string p = a.value("path", "");
                std::string resolved = p;
                if (!resolved.empty() && !std::filesystem::exists(resolved)) resolved = (Project::GetProjectDirectory() / p).string();
                for (char& c : resolved) if (c=='\\') c = '/';
                try {
                    std::string gstr = a.value("guid", "");
                    if (!gstr.empty()) {
                        ClaymoreGUID g = ClaymoreGUID::FromString(gstr);
                        if (!(g.high == 0 && g.low == 0)) {
                            std::string v = p; for (char& ch : v) if (ch=='\\') ch = '/';
                            AssetLibrary::Instance().RegisterAsset(AssetReference(g, 0, (int)AssetType::Mesh), AssetType::Mesh, v, v);
                            if (!resolved.empty()) AssetLibrary::Instance().RegisterPathAlias(g, resolved);
                        }
                    }
                } catch(...) {}

                // Prefer .meta fast path
                std::string metaTry = resolved;
                std::string ext = std::filesystem::path(resolved).extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext != ".meta") {
                    std::filesystem::path rp(resolved);
                    std::filesystem::path mp = rp.parent_path() / (rp.stem().string() + ".meta");
                    if (std::filesystem::exists(mp)) metaTry = mp.string();
                }
                EntityID nid = (EntityID)-1;
                if (!metaTry.empty() && std::filesystem::path(metaTry).extension() == ".meta") {
                    nid = dst.InstantiateModelFast(metaTry, glm::vec3(0.0f));
                    if (nid == (EntityID)0 || nid == (EntityID)-1) nid = dst.InstantiateModel(resolved, glm::vec3(0.0f));
                } else {
                    nid = dst.InstantiateModel(resolved, glm::vec3(0.0f));
                }
                if (nid != (EntityID)-1 && nid != (EntityID)0) {
                    auto* nd = dst.GetEntityData(nid);
                    if (nd) {
                        nd->Name = e.Name;
                        if (e.Components.contains("transform")) { Serializer::DeserializeTransform(e.Components["transform"], nd->Transform); nd->Transform.TransformDirty = true; }
                        if (e.Components.contains("scripts")) { Serializer::DeserializeScripts(e.Components["scripts"], nd->Scripts); }
                        if (e.Components.contains("animator")) { if (!nd->AnimationPlayer) nd->AnimationPlayer = std::make_unique<cm::animation::AnimationPlayerComponent>(); Serializer::DeserializeAnimator(e.Components["animator"], *nd->AnimationPlayer); }
                        // Attach GUID to the model root for stable mapping
                        nd->EntityGuid = e.Guid;
                    }
                    guidToId[pack(e.Guid)] = nid;
                    handledAsAsset = true;
                }
            }
        }
        if (!handledAsAsset) {
            Entity ent = dst.CreateEntityExact(e.Name);
            auto* d = dst.GetEntityData(ent.GetID()); if (!d) continue;
            d->EntityGuid = e.Guid;
            guidToId[pack(e.Guid)] = ent.GetID();
        }
    }

    // Determine root entity by RootGuid
    EntityID root = (EntityID)-1;
    auto itRoot = guidToId.find(pack(author.RootGuid));
    if (itRoot != guidToId.end()) root = itRoot->second; else if (!author.Entities.empty()) root = guidToId.begin()->second;

    // Pass 2: parent and deserialize component shells (no asset resolution yet)
    for (const auto& e : author.Entities) {
        auto itId = guidToId.find(pack(e.Guid)); if (itId == guidToId.end()) continue;
        EntityID id = itId->second;
        auto* d = dst.GetEntityData(id); if (!d) continue;
        if (e.ParentGuid.high != 0 || e.ParentGuid.low != 0) {
            auto itP = guidToId.find(pack(e.ParentGuid)); if (itP != guidToId.end()) dst.SetParent(id, itP->second);
        }
        // Components (shells) — skip for compact model asset nodes (they were handled on creation)
        if (e.Components.contains("asset") && e.Components["asset"].is_object()) continue;
        if (e.Components.contains("transform")) { Serializer::DeserializeTransform(e.Components["transform"], d->Transform); d->Transform.TransformDirty = true; d->Transform.CalculateLocalMatrix(); }
        if (e.Components.contains("mesh")) { d->Mesh = std::make_unique<MeshComponent>(); /* defer actual mesh load to build step */ }
        if (e.Components.contains("skeleton")) { d->Skeleton = std::make_unique<SkeletonComponent>(); Serializer::DeserializeSkeleton(e.Components["skeleton"], *d->Skeleton); }
        if (e.Components.contains("skinning")) { d->Skinning = std::make_unique<SkinningComponent>(); /* SkeletonRoot resolved later */ }
        if (e.Components.contains("animator")) { if (!d->AnimationPlayer) d->AnimationPlayer = std::make_unique<cm::animation::AnimationPlayerComponent>(); Serializer::DeserializeAnimator(e.Components["animator"], *d->AnimationPlayer); }
        if (e.Components.contains("scripts")) { Serializer::DeserializeScripts(e.Components["scripts"], d->Scripts); }
        if (e.Components.contains("camera")) { d->Camera = std::make_unique<CameraComponent>(); Serializer::DeserializeCamera(e.Components["camera"], *d->Camera); }
        if (e.Components.contains("light")) { d->Light = std::make_unique<LightComponent>(); Serializer::DeserializeLight(e.Components["light"], *d->Light); }
        if (e.Components.contains("collider")) { d->Collider = std::make_unique<ColliderComponent>(); Serializer::DeserializeCollider(e.Components["collider"], *d->Collider); }
        if (e.Components.contains("rigidbody")) { d->RigidBody = std::make_unique<RigidBodyComponent>(); Serializer::DeserializeRigidBody(e.Components["rigidbody"], *d->RigidBody); }
        if (e.Components.contains("staticbody")) { d->StaticBody = std::make_unique<StaticBodyComponent>(); Serializer::DeserializeStaticBody(e.Components["staticbody"], *d->StaticBody); }
        if (e.Components.contains("terrain")) { d->Terrain = std::make_unique<TerrainComponent>(); Serializer::DeserializeTerrain(e.Components["terrain"], *d->Terrain); }
        if (e.Components.contains("emitter")) { d->Emitter = std::make_unique<ParticleEmitterComponent>(); Serializer::DeserializeParticleEmitter(e.Components["emitter"], *d->Emitter); }
        if (e.Components.contains("canvas")) { d->Canvas = std::make_unique<CanvasComponent>(); Serializer::DeserializeCanvas(e.Components["canvas"], *d->Canvas); }
        if (e.Components.contains("panel")) { d->Panel = std::make_unique<PanelComponent>(); Serializer::DeserializePanel(e.Components["panel"], *d->Panel); }
        if (e.Components.contains("button")) { d->Button = std::make_unique<ButtonComponent>(); Serializer::DeserializeButton(e.Components["button"], *d->Button); }
    }

    // Pass 3: ensure skeleton bone entities exist (generate from bind pose if missing)
    for (const auto& e : author.Entities) {
        auto itId = guidToId.find(pack(e.Guid)); if (itId == guidToId.end()) continue;
        auto* d = dst.GetEntityData(itId->second); if (!d || !d->Skeleton) continue;
        GenerateBoneEntitiesForSkeleton(dst, itId->second);
    }

    // Pass 4a: Apply per-node overrides under compact model roots (if any entity had an asset block)
    for (const auto& e : author.Entities) {
        if (!(e.Components.contains("asset") && e.Components["asset"].is_object())) continue;
        const auto& a = e.Components["asset"]; if (a.value("type", "") != std::string("model")) continue;
        auto itId = guidToId.find(pack(e.Guid)); if (itId == guidToId.end()) continue;
        EntityID rootNew = itId->second; if (rootNew == (EntityID)-1) continue;
        if (!(e.Components.contains("children") && e.Components["children"].is_array())) continue;

        // Helpers copied from scene deserialization
        auto resolveByPath = [&](EntityID rootEntity, const std::string& path) -> EntityID {
            EntityID target = rootEntity; if (path.empty()) return target;
            std::stringstream ss(path); std::string part;
            auto normalize = [](const std::string& name) -> std::string {
                size_t us = name.find_last_of('_');
                if (us == std::string::npos) return name;
                bool digits = true; for (size_t i = us + 1; i < name.size(); ++i) { if (!std::isdigit(static_cast<unsigned char>(name[i]))) { digits = false; break; } }
                return digits ? name.substr(0, us) : name;
            };
            while (std::getline(ss, part, '/')) {
                auto* d = dst.GetEntityData(target); if (!d) return (EntityID)-1;
                const std::string partNorm = normalize(part);
                EntityID next = (EntityID)-1;
                for (EntityID c : d->Children) { auto* cd = dst.GetEntityData(c); if (!cd) continue; const std::string childName = cd->Name; if (childName == part || normalize(childName) == partNorm) { next = c; break; } }
                if (next == (EntityID)-1) return (EntityID)-1; target = next;
            }
            return target;
        };
        auto findByMeshFileId = [&](EntityID rootEntity, int fileID) -> EntityID {
            std::function<EntityID(EntityID)> dfs = [&](EntityID id)->EntityID{
                auto* d = dst.GetEntityData(id); if (!d) return (EntityID)-1;
                if (d->Mesh && d->Mesh->meshReference.fileID == fileID) return id;
                for (EntityID c : d->Children) { EntityID r = dfs(c); if (r != (EntityID)-1) return r; }
                return (EntityID)-1;
            };
            return dfs(rootEntity);
        };

        // Sort overrides by depth so parents first
        struct OverrideItem { std::string relPath; const nlohmann::json* j; int depth; };
        std::vector<OverrideItem> items;
        for (const auto& childOverride : e.Components["children"]) {
            if (!childOverride.contains("_modelNodePath")) continue;
            std::string relPath = childOverride["_modelNodePath"].get<std::string>();
            int depth = 0; for (char c : relPath) if (c=='/') ++depth;
            items.push_back({std::move(relPath), &childOverride, depth});
        }
        std::sort(items.begin(), items.end(), [](const OverrideItem& a, const OverrideItem& b){ return a.depth < b.depth; });

        for (const auto& it : items) {
            const auto& childOverride = *it.j;
            const std::string& relPath = it.relPath;
            EntityID target = resolveByPath(rootNew, relPath);
            if (target == (EntityID)-1 && childOverride.contains("mesh") && childOverride["mesh"].contains("fileID")) {
                int fid = childOverride["mesh"]["fileID"].get<int>();
                target = findByMeshFileId(rootNew, fid);
            }
            if (target == (EntityID)-1) continue;
            auto* td = dst.GetEntityData(target); if (!td) continue;
            if (childOverride.contains("transform")) { Serializer::DeserializeTransform(childOverride["transform"], td->Transform); td->Transform.TransformDirty = true; }
            if (childOverride.contains("mesh")) {
                if (!td->Mesh) td->Mesh = std::make_unique<MeshComponent>();
                ClaymoreGUID meshGuid{}; int fileId = 0; ClaymoreGUID skelGuid{};
                try { if (childOverride["mesh"].contains("meshReference")) { AssetReference tmp; childOverride["mesh"]["meshReference"].get_to(tmp); meshGuid = tmp.guid; fileId = tmp.fileID; } } catch(...) {}
                try { if (childOverride.contains("skeleton") && childOverride["skeleton"].contains("skeletonGuid")) childOverride["skeleton"]["skeletonGuid"].get_to(skelGuid); } catch(...) {}
                BuildModelParams bp{ meshGuid, fileId, skelGuid, nullptr, target, &dst };
                BuildResult br = BuildRendererFromAssets(bp);
                if (!br.ok) std::cerr << "[Prefab] ERROR: Override build failed under model root." << std::endl;
            }
            if (childOverride.contains("light")) { if (!td->Light) td->Light = std::make_unique<LightComponent>(); Serializer::DeserializeLight(childOverride["light"], *td->Light); }
            if (childOverride.contains("collider")) { if (!td->Collider) td->Collider = std::make_unique<ColliderComponent>(); Serializer::DeserializeCollider(childOverride["collider"], *td->Collider); }
            if (childOverride.contains("rigidbody")) { if (!td->RigidBody) td->RigidBody = std::make_unique<RigidBodyComponent>(); Serializer::DeserializeRigidBody(childOverride["rigidbody"], *td->RigidBody); }
            if (childOverride.contains("staticbody")) { if (!td->StaticBody) td->StaticBody = std::make_unique<StaticBodyComponent>(); Serializer::DeserializeStaticBody(childOverride["staticbody"], *td->StaticBody); }
            if (childOverride.contains("camera")) { if (!td->Camera) td->Camera = std::make_unique<CameraComponent>(); Serializer::DeserializeCamera(childOverride["camera"], *td->Camera); }
            if (childOverride.contains("terrain")) { if (!td->Terrain) td->Terrain = std::make_unique<TerrainComponent>(); Serializer::DeserializeTerrain(childOverride["terrain"], *td->Terrain); }
            if (childOverride.contains("emitter")) { if (!td->Emitter) td->Emitter = std::make_unique<ParticleEmitterComponent>(); Serializer::DeserializeParticleEmitter(childOverride["emitter"], *td->Emitter); }
            if (childOverride.contains("canvas")) { if (!td->Canvas) td->Canvas = std::make_unique<CanvasComponent>(); Serializer::DeserializeCanvas(childOverride["canvas"], *td->Canvas); }
            if (childOverride.contains("panel")) { if (!td->Panel) td->Panel = std::make_unique<PanelComponent>(); Serializer::DeserializePanel(childOverride["panel"], *td->Panel); }
            if (childOverride.contains("button")) { if (!td->Button) td->Button = std::make_unique<ButtonComponent>(); Serializer::DeserializeButton(childOverride["button"], *td->Button); }
            if (childOverride.contains("scripts")) { Serializer::DeserializeScripts(childOverride["scripts"], td->Scripts); }
            if (childOverride.contains("animator")) { if (!td->AnimationPlayer) td->AnimationPlayer = std::make_unique<cm::animation::AnimationPlayerComponent>(); Serializer::DeserializeAnimator(childOverride["animator"], *td->AnimationPlayer); }
            if (childOverride.contains("name")) { td->Name = childOverride["name"].get<std::string>(); }
        }
    }

    // Pass 4b: asset resolution and renderer construction via shared builder for non-asset nodes
    for (const auto& e : author.Entities) {
        auto itId = guidToId.find(pack(e.Guid)); if (itId == guidToId.end()) continue;
        EntityID id = itId->second; auto* d = dst.GetEntityData(id); if (!d) continue;
        if (e.Components.contains("asset") && e.Components["asset"].is_object()) continue; // already handled
        if (d->Mesh) {
            // Read mesh reference from authoring JSON if present
            ClaymoreGUID meshGuid{}; int fileId = 0; ClaymoreGUID skelGuid{};
            if (e.Components.contains("mesh") && e.Components["mesh"].contains("meshReference")) {
                AssetReference tmp; e.Components["mesh"]["meshReference"].get_to(tmp);
                meshGuid = tmp.guid; fileId = tmp.fileID;
            }
            if (e.Components.contains("skeleton") && e.Components["skeleton"].contains("skeletonGuid")) {
                e.Components["skeleton"]["skeletonGuid"].get_to(skelGuid);
            }
            BuildModelParams bp{ meshGuid, fileId, skelGuid, {}, id, &dst };
            BuildResult br = BuildRendererFromAssets(bp);
            if (!br.ok) {
                std::cerr << "[Prefab] ERROR: Failed to build renderer for entity '" << d->Name << "' (meshGuid=" << meshGuid.ToString() << ")" << std::endl;
            }
        }
    }

    // Pass 5: for each skeleton, populate BoneEntities mapping by matching descendant entity names to BoneNames (if still missing)
    for (const auto& e : author.Entities) {
        auto itId = guidToId.find(pack(e.Guid)); if (itId == guidToId.end()) continue;
        EntityID id = itId->second; auto* d = dst.GetEntityData(id); if (!d || !d->Skeleton) continue;
        auto& sk = *d->Skeleton;
        if (sk.BoneEntities.size() != sk.BoneNames.size()) sk.BoneEntities.assign(sk.BoneNames.size(), (EntityID)-1);
        // Build descendant name->id under this skeleton root
        std::unordered_map<std::string, EntityID> nameToEntity;
        std::vector<EntityID> stack; stack.push_back(id);
        while (!stack.empty()) {
            EntityID cur = stack.back(); stack.pop_back();
            auto* cd = dst.GetEntityData(cur); if (!cd) continue;
            nameToEntity[cd->Name] = cur;
            for (EntityID c : cd->Children) stack.push_back(c);
        }
        for (size_t i = 0; i < sk.BoneNames.size(); ++i) {
            auto itE = nameToEntity.find(sk.BoneNames[i]); if (itE != nameToEntity.end()) sk.BoneEntities[i] = itE->second;
        }
    }

    if (root != (EntityID)-1) dst.MarkTransformDirty(root), dst.UpdateTransforms();
    if (root != (EntityID)-1 && instanceOverridesOpt) {
        for (const auto& op : instanceOverridesOpt->Ops) if (op.Op == "set") { ResolvedTarget tgt; if (ResolvePath(op.Path, root, dst, tgt)) ApplySet(dst, tgt, op.Value); }
        dst.UpdateTransforms();
    }
    return root;
}

EntityID InstantiatePrefabFromAuthoringPath(const std::string& authoringPath, Scene& dst, const PrefabOverrides* instanceOverridesOpt) {
    PrefabAsset author;
    if (!PrefabIO::LoadAuthoringPrefabJSON(authoringPath, author)) return (EntityID)-1;
    std::unordered_map<uint64_t, EntityID> guidToId; guidToId.reserve(author.Entities.size()*2);
    auto pack = [](const ClaymoreGUID& g)->uint64_t { return (g.high ^ (g.low << 1)); };
    for (const auto& e : author.Entities) {
        Entity ent = dst.CreateEntityExact(e.Name);
        auto* d = dst.GetEntityData(ent.GetID()); if (!d) continue;
        d->EntityGuid = e.Guid; guidToId[pack(e.Guid)] = ent.GetID();
    }
    EntityID root = (EntityID)-1; auto itRoot = guidToId.find(pack(author.RootGuid)); if (itRoot != guidToId.end()) root = itRoot->second; else if (!author.Entities.empty()) root = guidToId.begin()->second;
    for (const auto& e : author.Entities) {
        auto itId = guidToId.find(pack(e.Guid)); if (itId == guidToId.end()) continue;
        EntityID id = itId->second; auto* d = dst.GetEntityData(id); if (!d) continue;
        if (e.ParentGuid.high != 0 || e.ParentGuid.low != 0) { auto itP = guidToId.find(pack(e.ParentGuid)); if (itP != guidToId.end()) dst.SetParent(id, itP->second); }
        if (e.Components.contains("transform")) { Serializer::DeserializeTransform(e.Components["transform"], d->Transform); d->Transform.TransformDirty = true; d->Transform.CalculateLocalMatrix(); }
        if (e.Components.contains("mesh")) { d->Mesh = std::make_unique<MeshComponent>(); Serializer::DeserializeMesh(e.Components["mesh"], *d->Mesh); }
        if (e.Components.contains("skeleton")) { d->Skeleton = std::make_unique<SkeletonComponent>(); Serializer::DeserializeSkeleton(e.Components["skeleton"], *d->Skeleton); }
        if (e.Components.contains("skinning")) { d->Skinning = std::make_unique<SkinningComponent>(); Serializer::DeserializeSkinning(e.Components["skinning"], *d->Skinning); }
        if (e.Components.contains("animator")) { if (!d->AnimationPlayer) d->AnimationPlayer = std::make_unique<cm::animation::AnimationPlayerComponent>(); Serializer::DeserializeAnimator(e.Components["animator"], *d->AnimationPlayer); }
        if (e.Components.contains("scripts")) { Serializer::DeserializeScripts(e.Components["scripts"], d->Scripts); }
    }
    // Skinning fixup for authoring path
    for (const auto& e : author.Entities) {
        auto itId = guidToId.find(pack(e.Guid)); if (itId == guidToId.end()) continue;
        EntityID id = itId->second; auto* d = dst.GetEntityData(id); if (!d || !d->Skinning) continue;
        EntityID p = d->Parent; EntityID found = (EntityID)-1;
        while (p != (EntityID)-1 && p != 0) { auto* pd = dst.GetEntityData(p); if (!pd) break; if (pd->Skeleton) { found = p; break; } p = pd->Parent; }
        d->Skinning->SkeletonRoot = found;
        if (d->Mesh && !std::dynamic_pointer_cast<SkinnedPBRMaterial>(d->Mesh->material)) { d->Mesh->material = MaterialManager::Instance().CreateSkinnedPBRMaterial(); }
    }
    if (root != (EntityID)-1) dst.MarkTransformDirty(root), dst.UpdateTransforms();
    if (root != (EntityID)-1 && instanceOverridesOpt) { for (const auto& op : instanceOverridesOpt->Ops) if (op.Op == "set") { ResolvedTarget tgt; if (ResolvePath(op.Path, root, dst, tgt)) ApplySet(dst, tgt, op.Value); } dst.UpdateTransforms(); }
    return root;
}

bool SavePrefab(const ClaymoreGUID& prefabGuid, const PrefabAsset& src) {
    return PrefabIO::SaveAuthoringPrefabJSON(AuthoringPrefabPathFromGuid(prefabGuid), src);
}

PrefabOverrides ComputeOverrides(const PrefabAsset& base, const Scene& editedScene, EntityID editedRoot) {
    (void)base; (void)editedScene; (void)editedRoot; PrefabOverrides ov; return ov; // stub
}

bool ApplyOverrides(EntityID root, const PrefabOverrides& ov, Scene& scene) {
    // Helpers
    auto deepInstantiate = [&](auto&& self, const nlohmann::json& node, EntityID parent, std::unordered_map<uint64_t, EntityID>& guidToId)->EntityID {
        // Expect fields: guid, name, components{}, children[] (nodes)
        ClaymoreGUID guid{}; std::string name = node.value("name", std::string("Entity"));
        try { if (node.contains("guid")) node.at("guid").get_to(guid); } catch(...) {}
        Entity e = scene.CreateEntityExact(name);
        EntityID id = e.GetID();
        auto* d = scene.GetEntityData(id); if (!d) return (EntityID)-1;
        d->EntityGuid = guid;
        if (parent != (EntityID)-1) scene.SetParent(id, parent);

        auto pack = [](const ClaymoreGUID& g)->uint64_t { return (g.high ^ (g.low << 1)); };
        if (!(guid.high == 0 && guid.low == 0)) guidToId[pack(guid)] = id;

        // Component shells
        if (node.contains("components") && node["components"].is_object()) {
            const auto& c = node["components"];
            if (c.contains("transform")) { Serializer::DeserializeTransform(c["transform"], d->Transform); d->Transform.TransformDirty = true; d->Transform.CalculateLocalMatrix(); }
            if (c.contains("mesh")) { if (!d->Mesh) d->Mesh = std::make_unique<MeshComponent>(); }
            if (c.contains("skeleton")) { if (!d->Skeleton) d->Skeleton = std::make_unique<SkeletonComponent>(); Serializer::DeserializeSkeleton(c["skeleton"], *d->Skeleton); }
            if (c.contains("skinning")) { if (!d->Skinning) d->Skinning = std::make_unique<SkinningComponent>(); }
            if (c.contains("animator")) { if (!d->AnimationPlayer) d->AnimationPlayer = std::make_unique<cm::animation::AnimationPlayerComponent>(); Serializer::DeserializeAnimator(c["animator"], *d->AnimationPlayer); }
            if (c.contains("scripts")) { Serializer::DeserializeScripts(c["scripts"], d->Scripts); }
            if (c.contains("camera")) { if (!d->Camera) d->Camera = std::make_unique<CameraComponent>(); Serializer::DeserializeCamera(c["camera"], *d->Camera); }
            if (c.contains("light")) { if (!d->Light) d->Light = std::make_unique<LightComponent>(); Serializer::DeserializeLight(c["light"], *d->Light); }
            if (c.contains("collider")) { if (!d->Collider) d->Collider = std::make_unique<ColliderComponent>(); Serializer::DeserializeCollider(c["collider"], *d->Collider); }
            if (c.contains("rigidbody")) { if (!d->RigidBody) d->RigidBody = std::make_unique<RigidBodyComponent>(); Serializer::DeserializeRigidBody(c["rigidbody"], *d->RigidBody); }
            if (c.contains("staticbody")) { if (!d->StaticBody) d->StaticBody = std::make_unique<StaticBodyComponent>(); Serializer::DeserializeStaticBody(c["staticbody"], *d->StaticBody); }
            if (c.contains("terrain")) { if (!d->Terrain) d->Terrain = std::make_unique<TerrainComponent>(); Serializer::DeserializeTerrain(c["terrain"], *d->Terrain); }
            if (c.contains("emitter")) { if (!d->Emitter) d->Emitter = std::make_unique<ParticleEmitterComponent>(); Serializer::DeserializeParticleEmitter(c["emitter"], *d->Emitter); }
            if (c.contains("canvas")) { if (!d->Canvas) d->Canvas = std::make_unique<CanvasComponent>(); Serializer::DeserializeCanvas(c["canvas"], *d->Canvas); }
            if (c.contains("panel")) { if (!d->Panel) d->Panel = std::make_unique<PanelComponent>(); Serializer::DeserializePanel(c["panel"], *d->Panel); }
            if (c.contains("button")) { if (!d->Button) d->Button = std::make_unique<ButtonComponent>(); Serializer::DeserializeButton(c["button"], *d->Button); }
        }

        // Recurse children
        if (node.contains("children") && node["children"].is_array()) {
            for (const auto& ch : node["children"]) self(self, ch, id, guidToId);
        }

        // Resolve assets and build renderer after hierarchy is attached
        if (node.contains("components") && node["components"].is_object()) {
            const auto& c = node["components"];
            if (c.contains("mesh")) {
                ClaymoreGUID meshGuid{}; int fileId = 0; ClaymoreGUID skelGuid{};
                try {
                    if (c["mesh"].contains("meshReference")) { AssetReference tmp; c["mesh"]["meshReference"].get_to(tmp); meshGuid = tmp.guid; fileId = tmp.fileID; }
                } catch(...) {}
                try { if (c.contains("skeleton") && c["skeleton"].contains("skeletonGuid")) c["skeleton"]["skeletonGuid"].get_to(skelGuid); } catch(...) {}
                BuildModelParams bp{ meshGuid, fileId, skelGuid, nullptr, id, &scene };
                BuildResult br = BuildRendererFromAssets(bp);
                if (!br.ok) {
                    std::cerr << "[Prefab] ERROR: addEntity build failed for '" << name << "'" << std::endl;
                }
            }
        }
        return id;
    };

    bool ok = true;
    for (const auto& op : ov.Ops) {
        if (op.Op == "set") {
            ResolvedTarget tgt; if (ResolvePath(op.Path, root, scene, tgt)) { ok &= ApplySet(scene, tgt, op.Value); }
            else { std::cerr << "[Prefab] ERROR: override path could not resolve: " << op.Path << std::endl; ok = false; }
        } else if (op.Op == "addEntity") {
            ResolvedTarget tgt; if (!ResolvePath(op.Path, root, scene, tgt)) { std::cerr << "[Prefab] ERROR: addEntity parent path not found: " << op.Path << std::endl; ok = false; continue; }
            EntityID parent = tgt.Entity;
            if (parent == (EntityID)-1) { std::cerr << "[Prefab] ERROR: addEntity parent invalid for path: " << op.Path << std::endl; ok = false; continue; }
            if (!op.Value.is_object()) { std::cerr << "[Prefab] ERROR: addEntity value must be an object node" << std::endl; ok = false; continue; }
            std::unordered_map<uint64_t, EntityID> g2i; (void)g2i;
            (void)deepInstantiate(deepInstantiate, op.Value, parent, g2i);
        }
        // TODO: removeEntity, add/removeComponent, reparent
    }
    // Update transforms once after overrides
    if (root != (EntityID)-1) { scene.MarkTransformDirty(root); scene.UpdateTransforms(); }
    return ok;
}

Diagnostics ValidatePrefab(const ClaymoreGUID& prefabGuid) {
    Diagnostics d; (void)prefabGuid; return d; // stub
}

bool BuildPrefabAssetFromScene(Scene& scene, EntityID root, PrefabAsset& out) {
    auto* rd = scene.GetEntityData(root); if (!rd) return false;
    out.Guid = rd->EntityGuid; // use root entity guid as prefab guid by default; editor can override
    out.Name = rd->Name;
    out.RootGuid = rd->EntityGuid;
    out.Entities.clear();
    std::function<void(EntityID, ClaymoreGUID)> dfs = [&](EntityID id, ClaymoreGUID parent) {
        auto* d = scene.GetEntityData(id); if (!d) return;
        PrefabAssetEntityNode n; n.Guid = d->EntityGuid; n.ParentGuid = parent; n.Name = d->Name;
        // Use existing serializer for stable component JSON
        nlohmann::json e = Serializer::SerializeEntity(id, scene);
        if (e.contains("transform")) n.Components["transform"] = e["transform"];
        if (e.contains("mesh")) n.Components["mesh"] = e["mesh"];
        if (e.contains("skeleton")) n.Components["skeleton"] = e["skeleton"];
        if (e.contains("skinning")) n.Components["skinning"] = e["skinning"];
        if (e.contains("animator")) n.Components["animator"] = e["animator"];
        if (e.contains("scripts")) n.Components["scripts"] = e["scripts"];
        for (EntityID c : d->Children) { auto* cd = scene.GetEntityData(c); if (cd) n.Children.push_back(cd->EntityGuid); }
        out.Entities.push_back(std::move(n));
        for (EntityID c : d->Children) dfs(c, d->EntityGuid);
    };
    dfs(root, ClaymoreGUID{});
    return true;
}


