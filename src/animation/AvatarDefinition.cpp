#include "animation/AvatarDefinition.h"
#include "ecs/AnimationComponents.h"
#include <algorithm>
#include <cctype>

namespace cm {
namespace animation {

AvatarDefinition::AvatarDefinition()
{
    Map.resize(HumanoidBoneCount);
    BindModel.assign(HumanoidBoneCount, glm::mat4(1.0f));
    BindLocal.assign(HumanoidBoneCount, glm::mat4(1.0f));
    RetargetModel.assign(HumanoidBoneCount, glm::mat4(1.0f));
    Present.assign(HumanoidBoneCount, false);
    RestOffsetRot.assign(HumanoidBoneCount, glm::quat(1,0,0,0));
    // Initialize HumanoidMapEntry bones
    for (uint16_t i = 0; i < HumanoidBoneCount; ++i) {
        Map[i].Bone = static_cast<HumanoidBone>(i);
    }
}

bool AvatarDefinition::IsBonePresent(HumanoidBone b) const
{
    return Present[static_cast<uint16_t>(b)];
}

int32_t AvatarDefinition::GetMappedBoneIndex(HumanoidBone b) const
{
    return Map[static_cast<uint16_t>(b)].BoneIndex;
}

const std::string& AvatarDefinition::GetMappedBoneName(HumanoidBone b) const
{
    return Map[static_cast<uint16_t>(b)].BoneName;
}

static std::string to_canonical(const std::string& in)
{
    std::string s;
    s.reserve(in.size());
    for (char c : in) {
        if (std::isalnum(static_cast<unsigned char>(c))) s.push_back((char)std::tolower(c));
    }
    return s;
}

static bool name_matches(const std::string& name, const std::vector<std::string>& candidates)
{
    const std::string canon = to_canonical(name);
    for (const auto& c : candidates) {
        if (canon.find(to_canonical(c)) != std::string::npos) return true;
    }
    return false;
}

const std::unordered_map<HumanoidBone, std::vector<std::string>>& avatar_builders::DefaultNameSeeds()
{
    static std::unordered_map<HumanoidBone, std::vector<std::string>> map = {
        { HumanoidBone::Hips, {"Hips","Pelvis","hip","pelvis","root_pelvis"} },
        { HumanoidBone::Spine, {"Spine","Spine1","spine01","torso"} },
        { HumanoidBone::Chest, {"Chest","Spine2","upperchest","chest"} },
        { HumanoidBone::UpperChest, {"UpperChest","Spine3","upper_spine"} },
        { HumanoidBone::Neck, {"Neck","neck"} },
        { HumanoidBone::Head, {"Head","head"} },
        { HumanoidBone::LeftShoulder, {"LeftShoulder","L_Shoulder","clavicle_l","shoulder_l"} },
        { HumanoidBone::LeftUpperArm, {"LeftArm","LeftUpperArm","upperarm_l","arm_l"} },
        { HumanoidBone::LeftLowerArm, {"LeftForeArm","LeftLowerArm","lowerarm_l","forearm_l"} },
        { HumanoidBone::LeftHand, {"LeftHand","hand_l"} },
        { HumanoidBone::RightShoulder, {"RightShoulder","R_Shoulder","clavicle_r","shoulder_r"} },
        { HumanoidBone::RightUpperArm, {"RightArm","RightUpperArm","upperarm_r","arm_r"} },
        { HumanoidBone::RightLowerArm, {"RightForeArm","RightLowerArm","lowerarm_r","forearm_r"} },
        { HumanoidBone::RightHand, {"RightHand","hand_r"} },
        { HumanoidBone::LeftUpperLeg, {"LeftUpLeg","LeftThigh","thigh_l","upleg_l"} },
        { HumanoidBone::LeftLowerLeg, {"LeftLeg","LeftCalf","calf_l","leg_l"} },
        { HumanoidBone::LeftFoot, {"LeftFoot","foot_l"} },
        { HumanoidBone::LeftToes, {"LeftToeBase","toe_l","toes_l"} },
        { HumanoidBone::RightUpperLeg, {"RightUpLeg","RightThigh","thigh_r","upleg_r"} },
        { HumanoidBone::RightLowerLeg, {"RightLeg","RightCalf","calf_r","leg_r"} },
        { HumanoidBone::RightFoot, {"RightFoot","foot_r"} },
        { HumanoidBone::RightToes, {"RightToeBase","toe_r","toes_r"} },
    };
    return map;
}

void avatar_builders::BuildFromSkeleton(const SkeletonComponent& skeleton,
                                        AvatarDefinition& outAvatar,
                                        bool autoMap,
                                        const std::unordered_map<HumanoidBone, std::vector<std::string>>& nameMap)
{
    const auto& seeds = nameMap.empty() ? DefaultNameSeeds() : nameMap;

    // Build reverse name list for quick testing
    std::vector<std::string> names;
    names.reserve(skeleton.BoneNameToIndex.size());
    for (const auto& [name, idx] : skeleton.BoneNameToIndex) {
        if (idx >= 0) {
            if ((size_t)idx >= names.size()) names.resize(idx + 1);
            names[idx] = name;
        }
    }

    // Reset mapping
    for (uint16_t i = 0; i < HumanoidBoneCount; ++i) {
        outAvatar.Map[i].BoneIndex = -1;
        outAvatar.Map[i].BoneName.clear();
        outAvatar.Present[i] = false;
        outAvatar.BindModel[i] = glm::mat4(1.0f);
        outAvatar.BindLocal[i] = glm::mat4(1.0f);
    }

    if (autoMap) {
        for (uint16_t i = 0; i < HumanoidBoneCount; ++i) {
            HumanoidBone hb = static_cast<HumanoidBone>(i);
            auto it = seeds.find(hb);
            if (it == seeds.end()) continue;

            // Linear scan bone names for candidates
            for (size_t bi = 0; bi < names.size(); ++bi) {
                const std::string& n = names[bi];
                if (n.empty()) continue;
                if (name_matches(n, it->second)) {
                    outAvatar.Map[i].BoneIndex = (int32_t)bi;
                    outAvatar.Map[i].BoneName = n;
                    outAvatar.Present[i] = true;
                    break;
                }
            }
        }
    }

    // Compute bind transforms
    // Model bind from inverse bind
    std::vector<glm::mat4> modelBind(names.size(), glm::mat4(1.0f));
    for (size_t bi = 0; bi < names.size(); ++bi) {
        if (bi < skeleton.InverseBindPoses.size()) {
            modelBind[bi] = glm::inverse(skeleton.InverseBindPoses[bi]);
        }
    }

    // For each mapped bone, fill bind model and local via parent chain
    for (uint16_t i = 0; i < HumanoidBoneCount; ++i) {
        if (!outAvatar.Present[i]) continue;
        const int32_t bi = outAvatar.Map[i].BoneIndex;
        if (bi < 0 || (size_t)bi >= modelBind.size()) continue;
        outAvatar.BindModel[i] = modelBind[bi];
        int parentIdx = (bi < skeleton.BoneParents.size()) ? skeleton.BoneParents[bi] : -1;
        glm::mat4 parentModel = parentIdx >= 0 ? modelBind[parentIdx] : glm::mat4(1.0f);
        outAvatar.BindLocal[i] = glm::inverse(parentModel) * outAvatar.BindModel[i];
    }
}

} // namespace animation
} // namespace cm


