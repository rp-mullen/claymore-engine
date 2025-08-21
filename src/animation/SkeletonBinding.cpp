#include "animation/SkeletonBinding.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <unordered_map>
#include <algorithm>
#include <cstring>

// Basic FNV-1a 64-bit
uint64_t Hash64(const std::string& s) {
    const uint64_t FNV_OFFSET = 1469598103934665603ull;
    const uint64_t FNV_PRIME  = 1099511628211ull;
    uint64_t h = FNV_OFFSET;
    for (unsigned char c : s) { h ^= (uint64_t)c; h *= FNV_PRIME; }
    return h;
}

uint64_t Hash64_CombineGuidPath(const ClaymoreGUID& guid, const std::string& fullPath) {
    // Combine GUID bytes + path string under FNV-1a
    const uint64_t FNV_OFFSET = 1469598103934665603ull;
    const uint64_t FNV_PRIME  = 1099511628211ull;
    uint64_t h = FNV_OFFSET;
    auto mix = [&](const void* data, size_t size){
        const unsigned char* p = (const unsigned char*)data;
        for (size_t i = 0; i < size; ++i) { h ^= (uint64_t)p[i]; h *= FNV_PRIME; }
    };
    mix(&guid.high, sizeof(guid.high));
    mix(&guid.low, sizeof(guid.low));
    mix(fullPath.data(), fullPath.size());
    return h;
}

// Stubs; integrate with asset systems as needed
const SkeletonComponent* GetSkeleton(const ClaymoreGUID& guid) { (void)guid; return nullptr; }
const Mesh* GetMesh(const ClaymoreGUID& guid) { (void)guid; return nullptr; }
const AnimClip* GetClip(const ClaymoreGUID& guid) { (void)guid; return nullptr; }

static inline void BuildBoneNameIndex(const SkeletonComponent& skel,
                                      std::unordered_map<std::string, int>& out) {
    out.clear();
    if (!skel.BoneNameToIndex.empty()) { out = skel.BoneNameToIndex; return; }
    for (size_t i = 0; i < skel.BoneNames.size(); ++i) {
        if (!skel.BoneNames[i].empty()) out[skel.BoneNames[i]] = (int)i;
    }
}

bool BuildBoneRemap(const Mesh& mesh,
                    const SkeletonComponent& skel,
                    std::vector<uint16_t>& outRemap,
                    std::vector<uint16_t>& outUsedJointList) {
    outRemap.clear();
    outUsedJointList.clear();

    // If mesh has no skinning data, nothing to map
    if (!mesh.HasSkinning()) return false;

    // We rely on author-time name alignment between mesh bone indices and skeleton names via ModelLoader scene prepass
    // If unavailable, try best-effort identity up to skeleton size
    std::unordered_map<std::string, int> nameToIndex;
    BuildBoneNameIndex(skel, nameToIndex);

    // Build a set of actually used joints from mesh indices
    const size_t vertexCount = mesh.BoneIndices.size();
    if (vertexCount == 0) return false;
    std::vector<uint8_t> used(skel.InverseBindPoses.size(), 0);
    for (size_t v = 0; v < vertexCount; ++v) {
        glm::ivec4 bi = mesh.BoneIndices[v];
        for (int k = 0; k < 4; ++k) {
            int idx = bi[k];
            if (idx >= 0 && idx < (int)used.size()) used[(size_t)idx] = 1;
        }
    }

    for (size_t i = 0; i < used.size(); ++i) if (used[i]) outUsedJointList.push_back((uint16_t)i);

    // Remap is identity for now (mesh indices expected to match skeleton indices via import prepass)
    // If counts mismatch, clamp
    const size_t boneCount = skel.InverseBindPoses.size();
    outRemap.resize(boneCount);
    for (size_t i = 0; i < boneCount; ++i) outRemap[i] = (uint16_t)std::min<size_t>(i, UINT16_MAX);
    return true;
}

void BuildBonePaletteBuffer(const SkeletonComponent& skel,
                            const std::vector<uint16_t>& usedJointList,
                            std::vector<glm::mat4>& outPalette) {
    const size_t n = usedJointList.size();
    outPalette.resize(n);
    // For now, use bind pose globals if available; a caller can supply updated globals per frame
    for (size_t i = 0; i < n; ++i) {
        const uint16_t j = usedJointList[i];
        glm::mat4 global = (j < skel.BindPoseGlobals.size()) ? skel.BindPoseGlobals[j] : glm::mat4(1.0f);
        glm::mat4 invBind = (j < skel.InverseBindPoses.size()) ? skel.InverseBindPoses[j] : glm::mat4(1.0f);
        outPalette[i] = global * invBind;
    }
}

void Animator_AttachSkeleton(cm::animation::Animator& a, const SkeletonComponent& skel) {
    (void)a; (void)skel; /* Hook up if needed later */
}

void Animator_Tick(cm::animation::Animator& a, float dt) { (void)a; (void)dt; }

void Skeleton_ComputeGlobals(const SkeletonComponent& skel,
                             const std::vector<glm::mat4>& localTransforms,
                             std::vector<glm::mat4>& outGlobals) {
    const size_t count = localTransforms.size();
    outGlobals.resize(count);
    for (size_t i = 0; i < count; ++i) {
        int parent = (i < skel.BoneParents.size() ? skel.BoneParents[i] : -1);
        if (parent < 0) outGlobals[i] = localTransforms[i];
        else outGlobals[i] = outGlobals[(size_t)parent] * localTransforms[i];
    }
}

static void BuildNamePathsDFS(const SkeletonComponent& skel,
                              int idx,
                              const std::vector<int>& childrenStart,
                              const std::vector<int>& nextSibling,
                              const std::vector<int>& firstChild,
                              const std::vector<std::string>& names,
                              std::vector<std::string>& outPaths) {
    if (idx < 0) return;
    if ((size_t)idx >= names.size()) return;
    const std::string& name = names[(size_t)idx];
    int parent = (idx < (int)skel.BoneParents.size() ? skel.BoneParents[(size_t)idx] : -1);
    if (parent < 0) outPaths[(size_t)idx] = name;
    else outPaths[(size_t)idx] = outPaths[(size_t)parent] + "/" + name;

    // Recurse children via firstChild/nextSibling linked-list scheme
    for (int c = firstChild[(size_t)idx]; c >= 0; c = nextSibling[(size_t)c])
        BuildNamePathsDFS(skel, c, childrenStart, nextSibling, firstChild, names, outPaths);
}

void ComputeSkeletonJointGuids(SkeletonComponent& skel) {
    const size_t n = std::max(skel.InverseBindPoses.size(), skel.BoneParents.size());
    if (n == 0) { skel.JointGuids.clear(); return; }
    // Ensure BoneNames array exists
    if (skel.BoneNames.size() < n) skel.BoneNames.resize(n);
    // Build a naive children list to generate full paths
    std::vector<int> firstChild(n, -1), nextSibling(n, -1);
    for (size_t i = 0; i < n; ++i) {
        int p = (i < skel.BoneParents.size() ? skel.BoneParents[i] : -1);
        if (p >= 0) { nextSibling[i] = firstChild[(size_t)p]; firstChild[(size_t)p] = (int)i; }
    }
    std::vector<std::string> paths(n);
    for (size_t i = 0; i < n; ++i) if ((int)i >= 0 && (skel.BoneParents[i] < 0)) BuildNamePathsDFS(skel, (int)i, firstChild, nextSibling, firstChild, skel.BoneNames, paths);

    skel.JointGuids.resize(n);
    for (size_t i = 0; i < n; ++i) {
        const std::string& path = paths[i].empty() ? skel.BoneNames[i] : paths[i];
        skel.JointGuids[i] = Hash64_CombineGuidPath(skel.SkeletonGuid, path);
    }
}


