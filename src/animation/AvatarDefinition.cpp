#include "animation/AvatarDefinition.h"
#include "animation/HumanoidAvatar.h"
#include "ecs/AnimationComponents.h"
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
        { HumanoidBone::Root, {"Root","Armature","ArmatureRoot","root"} },
        { HumanoidBone::Hips, {"Hips","Pelvis","hip","pelvis","root_pelvis"} },
        { HumanoidBone::Spine, {"Spine","Spine1","spine01","torso"} },
        { HumanoidBone::Chest, {"Chest","Spine2","upperchest","chest"} },
        { HumanoidBone::UpperChest, {"UpperChest","Spine3","upper_spine"} },
        { HumanoidBone::Neck, {"Neck","neck"} },
        { HumanoidBone::Head, {"Head","head"} },
        // Eyes
        { HumanoidBone::LeftEye, {"LeftEye","Eye_L","Eye.L","eye_l","lefteye"} },
        { HumanoidBone::RightEye, {"RightEye","Eye_R","Eye.R","eye_r","righteye"} },
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

        // Left fingers (Mixamo style: LeftHand{Thumb/Index/Middle/Ring/Pinky}{1,2,3})
        { HumanoidBone::LeftThumbProx, {"LeftHandThumb1","Thumb1_L","LThumb1","thumb_01_l"} },
        { HumanoidBone::LeftThumbInter, {"LeftHandThumb2","Thumb2_L","LThumb2","thumb_02_l"} },
        { HumanoidBone::LeftThumbDist, {"LeftHandThumb3","Thumb3_L","LThumb3","thumb_03_l"} },

        { HumanoidBone::LeftIndexProx, {"LeftHandIndex1","Index1_L","LIndex1","index_01_l"} },
        { HumanoidBone::LeftIndexInter, {"LeftHandIndex2","Index2_L","LIndex2","index_02_l"} },
        { HumanoidBone::LeftIndexDist, {"LeftHandIndex3","Index3_L","LIndex3","index_03_l"} },

        { HumanoidBone::LeftMiddleProx, {"LeftHandMiddle1","Middle1_L","LMiddle1","middle_01_l"} },
        { HumanoidBone::LeftMiddleInter, {"LeftHandMiddle2","Middle2_L","LMiddle2","middle_02_l"} },
        { HumanoidBone::LeftMiddleDist, {"LeftHandMiddle3","Middle3_L","LMiddle3","middle_03_l"} },

        { HumanoidBone::LeftRingProx, {"LeftHandRing1","Ring1_L","LRing1","ring_01_l"} },
        { HumanoidBone::LeftRingInter, {"LeftHandRing2","Ring2_L","LRing2","ring_02_l"} },
        { HumanoidBone::LeftRingDist, {"LeftHandRing3","Ring3_L","LRing3","ring_03_l"} },

        { HumanoidBone::LeftLittleProx, {"LeftHandPinky1","Pinky1_L","LLittle1","pinky_01_l","little_01_l"} },
        { HumanoidBone::LeftLittleInter, {"LeftHandPinky2","Pinky2_L","LLittle2","pinky_02_l","little_02_l"} },
        { HumanoidBone::LeftLittleDist, {"LeftHandPinky3","Pinky3_L","LLittle3","pinky_03_l","little_03_l"} },

        // Right fingers
        { HumanoidBone::RightThumbProx, {"RightHandThumb1","Thumb1_R","RThumb1","thumb_01_r"} },
        { HumanoidBone::RightThumbInter, {"RightHandThumb2","Thumb2_R","RThumb2","thumb_02_r"} },
        { HumanoidBone::RightThumbDist, {"RightHandThumb3","Thumb3_R","RThumb3","thumb_03_r"} },

        { HumanoidBone::RightIndexProx, {"RightHandIndex1","Index1_R","RIndex1","index_01_r"} },
        { HumanoidBone::RightIndexInter, {"RightHandIndex2","Index2_R","RIndex2","index_02_r"} },
        { HumanoidBone::RightIndexDist, {"RightHandIndex3","Index3_R","RIndex3","index_03_r"} },

        { HumanoidBone::RightMiddleProx, {"RightHandMiddle1","Middle1_R","RMiddle1","middle_01_r"} },
        { HumanoidBone::RightMiddleInter, {"RightHandMiddle2","Middle2_R","RMiddle2","middle_02_r"} },
        { HumanoidBone::RightMiddleDist, {"RightHandMiddle3","Middle3_R","RMiddle3","middle_03_r"} },

        { HumanoidBone::RightRingProx, {"RightHandRing1","Ring1_R","RRing1","ring_01_r"} },
        { HumanoidBone::RightRingInter, {"RightHandRing2","Ring2_R","RRing2","ring_02_r"} },
        { HumanoidBone::RightRingDist, {"RightHandRing3","Ring3_R","RRing3","ring_03_r"} },

        { HumanoidBone::RightLittleProx, {"RightHandPinky1","Pinky1_R","RLittle1","pinky_01_r","little_01_r"} },
        { HumanoidBone::RightLittleInter, {"RightHandPinky2","Pinky2_R","RLittle2","pinky_02_r","little_02_r"} },
        { HumanoidBone::RightLittleDist, {"RightHandPinky3","Pinky3_R","RLittle3","pinky_03_r","little_03_r"} },

        // Common twist naming seen across rigs (include Mixamo-style and Roll variants)
        { HumanoidBone::LeftUpperArmTwist, {"LeftUpperArmTwist","LeftArmTwist","UpperArmTwist_L","upperarm_twist_l","arm_twist_01_l","LeftArmRoll"} },
        { HumanoidBone::LeftLowerArmTwist, {"LeftLowerArmTwist","LeftForeArmTwist","ForeArmTwist_L","forearm_twist_l","arm_twist_02_l","LeftForeArmRoll"} },
        { HumanoidBone::RightUpperArmTwist, {"RightUpperArmTwist","RightArmTwist","UpperArmTwist_R","upperarm_twist_r","arm_twist_01_r","RightArmRoll"} },
        { HumanoidBone::RightLowerArmTwist, {"RightLowerArmTwist","RightForeArmTwist","ForeArmTwist_R","forearm_twist_r","arm_twist_02_r","RightForeArmRoll"} },
        { HumanoidBone::LeftUpperLegTwist, {"LeftUpperLegTwist","LeftUpLegTwist","ThighTwist_L","thigh_twist_01_l","LeftUpLegRoll"} },
        { HumanoidBone::LeftLowerLegTwist, {"LeftLowerLegTwist","LeftLegTwist","CalfTwist_L","calf_twist_01_l","LeftLegRoll"} },
        { HumanoidBone::RightUpperLegTwist, {"RightUpperLegTwist","RightUpLegTwist","ThighTwist_R","thigh_twist_01_r","RightUpLegRoll"} },
        { HumanoidBone::RightLowerLegTwist, {"RightLowerLegTwist","RightLegTwist","CalfTwist_R","calf_twist_01_r","RightLegRoll"} },
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

int HumanoidAvatar::HumanToSkeleton(int humanBoneId, const ::SkeletonComponent& skeleton) const {
    HumanBone hb = static_cast<HumanBone>(humanBoneId);
    auto it = BoneMapping.find(hb);
    if (it == BoneMapping.end()) return -1;
    return skeleton.GetBoneIndex(it->second);
}

} // namespace animation
} // namespace cm


