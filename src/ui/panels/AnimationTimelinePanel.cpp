#include "AnimationTimelinePanel.h"
#include <imgui.h>
#include <ImGuizmo.h>
#include <ImSequencer.h>
#include <ImCurveEdit.h>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include "animation/AnimationSerializer.h"
#include "scripting/DotNetHost.h"
#include "rendering/TextureLoader.h"
#include "ecs/EntityData.h"
#include "scripting/ManagedScriptComponent.h"
#include "animation/AnimationEvaluator.h"
#include <filesystem>
#include <editor/Project.h>

using namespace cm::animation;
void AnimationTimelinePanel::ScanAvailableTimelines()
{
    if (m_TimelineListScanned) return;
    m_TimelineListScanned = true;
    m_AvailableTimelinePaths.clear();
    std::error_code ec;
    for (auto it = std::filesystem::recursive_directory_iterator("assets", ec);
         it != std::filesystem::recursive_directory_iterator(); ++it) {
        if (it->is_regular_file(ec)) {
            auto p = it->path();
            if (p.extension() == ".animtl") m_AvailableTimelinePaths.push_back(p.string());
        }
    }
}
void AnimationTimelinePanel::ScanAvailableAnims()
{
    if (m_AnimListScanned) return;
    m_AnimListScanned = true;
    m_AvailableAnimPaths.clear();
    // Scan both assets/ and project directory (e.g., ClayProject/) for .anim files
    std::vector<std::filesystem::path> roots;
    roots.emplace_back("assets");
    const auto projDir = Project::GetProjectDirectory();
    if (!projDir.empty()) roots.emplace_back(projDir);
    std::error_code ec;
    for (const auto& root : roots) {
        if (!std::filesystem::exists(root, ec)) continue;
        for (auto it = std::filesystem::recursive_directory_iterator(root, ec);
             it != std::filesystem::recursive_directory_iterator(); ++it) {
            if (it->is_regular_file(ec)) {
                auto p = it->path();
                if (p.extension() == ".anim") m_AvailableAnimPaths.push_back(p.string());
            }
        }
    }
}

namespace {
    struct TrackSequence : ImSequencer::SequenceInterface
    {
        TimelineClip* Clip;
        float FPS;

        int GetFrameMin() const override { return 0; }
        int GetFrameMax() const override { return (int)std::ceil(Clip->Length * FPS); }
        int GetItemCount() const override { return (int)(Clip->Tracks.size() + Clip->ScriptTracks.size()); }

        void Get(int index, int** start, int** end, int* type, unsigned int* color) override {
            static int s, e; // transient ints backed by static storage (ImSequencer expects ptr lifetime)
            // Map index to property/script track
            size_t propCount = Clip->Tracks.size();
            if ((size_t)index < propCount) {
                const auto& t = Clip->Tracks[index];
                if (!t.Keys.empty()) {
                    s = (int)std::floor(t.Keys.front().Time * FPS);
                    e = (int)std::ceil(t.Keys.back().Time * FPS);
                } else { s = 0; e = 0; }
                if (start) *start = &s; if (end) *end = &e; if (type) *type = 0; if (color) *color = 0xFF4DA3FFu;
            } else {
                const auto& st = Clip->ScriptTracks[index - propCount];
                if (!st.Keys.empty()) {
                    s = (int)std::floor(st.Keys.front().Time * FPS);
                    e = (int)std::ceil(st.Keys.back().Time * FPS);
                } else { s = 0; e = 0; }
                if (start) *start = &s; if (end) *end = &e; if (type) *type = 1; if (color) *color = 0xFFFFA040u;
            }
        }

        const char* GetItemLabel(int index) const override {
            size_t propCount = Clip->Tracks.size();
            if ((size_t)index < propCount) return Clip->Tracks[index].PropertyPath.c_str();
            return Clip->ScriptTracks[index - propCount].Name.c_str();
        }
    };
}

bool AnimationTimelinePanel::Load(const std::string& path)
{
    m_Clip = LoadTimelineClip(path);
    m_OpenPath = path;
    m_TimeSec = 0.0f;
    return true;
}

bool AnimationTimelinePanel::Save(const std::string& path)
{
    if (SaveTimelineClip(m_Clip, path)) { m_OpenPath = path; return true; }
    return false;
}

void AnimationTimelinePanel::OnImGuiRender()
{
    ImGui::Begin("Animation Timeline");

    DrawToolbar();

    ImGui::Separator();
    DrawAssetHeader();

    // Frames bookkeeping
    int totalFrames = (int)std::ceil(std::max(0.001f, m_Clip.Length) * m_FPS);

    // Update playback BEFORE drawing the sequencer so the playhead moves this frame
    if (m_Playing && m_Clip.Length > 0.0f) {
        float dt = ImGui::GetIO().DeltaTime;
        m_TimeSec += dt;
        if (m_TimeSec > m_Clip.Length) m_TimeSec = std::fmod(m_TimeSec, m_Clip.Length);
    }

    m_CurrentFrame = (int)std::round(std::max(0.0f, m_TimeSec) * m_FPS);
    m_CurrentFrame = std::min(std::max(0, m_CurrentFrame), totalFrames);

    // Icons (lazy)
    if (!m_IconsLoaded) {
        m_IconMove   = TextureLoader::ToImGuiTextureID(TextureLoader::LoadIconTexture("assets/icons/move.svg"));
        m_IconRotate = TextureLoader::ToImGuiTextureID(TextureLoader::LoadIconTexture("assets/icons/rotate.svg"));
        m_IconScale  = TextureLoader::ToImGuiTextureID(TextureLoader::LoadIconTexture("assets/icons/scale.svg"));
        m_IconEvent  = TextureLoader::ToImGuiTextureID(TextureLoader::LoadIconTexture("assets/icons/keyframe_filled.svg"));
        m_IconKey    = TextureLoader::ToImGuiTextureID(TextureLoader::LoadIconTexture("assets/icons/key.svg"));
        m_IconsLoaded = true;
    }

    DrawTrackManagerUI();
    BuildRenderItems();
    DrawSequencer();

    // Scrub time from current frame (apply user drag on the sequencer playhead)
    m_TimeSec = m_CurrentFrame / std::max(1.0f, m_FPS);

    // Live application to selection and script events
    // (unchanged logic, just factored under cleaner UI)
    if (m_Scene && m_Selected && *m_Selected != (EntityID)-1) {
        EntityID id = *m_Selected;
        auto* data = m_Scene->GetEntityData(id);
        if (data) {
            for (const auto& t : m_Clip.Tracks) {
                if (t.PropertyPath == "Transform.Position.x" || t.PropertyPath == "Transform.Position.y" || t.PropertyPath == "Transform.Position.z") {
                    float value = data->Transform.Position.x;
                    if (t.PropertyPath.back() == 'y') value = data->Transform.Position.y; else if (t.PropertyPath.back() == 'z') value = data->Transform.Position.z;
                    for (const auto& k : t.Keys) if (k.Time <= m_TimeSec) value = k.Value; else break;
                    if (t.PropertyPath.back() == 'x') data->Transform.Position.x = value; else if (t.PropertyPath.back() == 'y') data->Transform.Position.y = value; else data->Transform.Position.z = value;
                    m_Scene->MarkTransformDirty(id);
                } else if (t.PropertyPath == "Transform.Rotation.x" || t.PropertyPath == "Transform.Rotation.y" || t.PropertyPath == "Transform.Rotation.z") {
                    float value = data->Transform.Rotation.x;
                    if (t.PropertyPath.back() == 'y') value = data->Transform.Rotation.y; else if (t.PropertyPath.back() == 'z') value = data->Transform.Rotation.z;
                    for (const auto& k : t.Keys) if (k.Time <= m_TimeSec) value = k.Value; else break;
                    if (t.PropertyPath.back() == 'x') data->Transform.Rotation.x = value; else if (t.PropertyPath.back() == 'y') data->Transform.Rotation.y = value; else data->Transform.Rotation.z = value;
                    m_Scene->MarkTransformDirty(id);
                } else if (t.PropertyPath == "Transform.Scale.x" || t.PropertyPath == "Transform.Scale.y" || t.PropertyPath == "Transform.Scale.z") {
                    float value = data->Transform.Scale.x;
                    if (t.PropertyPath.back() == 'y') value = data->Transform.Scale.y; else if (t.PropertyPath.back() == 'z') value = data->Transform.Scale.z;
                    for (const auto& k : t.Keys) if (k.Time <= m_TimeSec) value = k.Value; else break;
                    if (t.PropertyPath.back() == 'x') data->Transform.Scale.x = value; else if (t.PropertyPath.back() == 'y') data->Transform.Scale.y = value; else data->Transform.Scale.z = value;
                    m_Scene->MarkTransformDirty(id);
                } else if (t.PropertyPath == "Light.Intensity" && data->Light) {
                    float value = data->Light->Intensity;
                    for (const auto& k : t.Keys) if (k.Time <= m_TimeSec) value = k.Value; else break;
                    data->Light->Intensity = value;
                } else if (t.PropertyPath == "ParticleEmitter.ParticlesPerSecond" && data->Emitter) {
                    float value = data->Emitter->Uniforms.m_particlesPerSecond;
                    for (const auto& k : t.Keys) if (k.Time <= m_TimeSec) value = k.Value; else break;
                    data->Emitter->Uniforms.m_particlesPerSecond = value;
                }
            }

            if (g_Script_Invoke) {
                for (const auto& st : m_Clip.ScriptTracks) {
                    for (const auto& k : st.Keys) {
                        int kf = (int)std::round(k.Time * m_FPS);
                        if (kf == m_CurrentFrame) {
                            for (auto& si : data->Scripts) {
                                if (si.Instance && si.ClassName == k.ScriptClass) {
                                    if (auto managed = std::dynamic_pointer_cast<ManagedScriptComponent>(si.Instance)) {
                                        g_Script_Invoke(managed->GetHandle(), k.Method.c_str());
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    ImGui::Separator();
    DrawPerKeyInspector(totalFrames);

    ImGui::Separator();
    DrawPreviewBlock();

    // Optional: play skeletal .anim(s) referenced by this timeline onto the active entity (unified use)
    if (m_Scene && m_Selected && *m_Selected != (EntityID)-1 && !m_Clip.SkeletalClips.empty()) {
        EntityID id = *m_Selected; auto* data = m_Scene->GetEntityData(id);
        if (data && data->Skeleton && data->Skinning) {
            // For now, support single clip playback blended at weight 1; extend to blends later
            const auto& sc = m_Clip.SkeletalClips.front();
            static std::shared_ptr<AnimationClip> cached;
            static std::string cachedPath;
            if (!cached || cachedPath != sc.ClipPath) { cached = std::make_shared<AnimationClip>(LoadAnimationClip(sc.ClipPath)); cachedPath = sc.ClipPath; }
            if (cached && cached->Duration > 0.0f) {
                float t = m_TimeSec * std::max(0.0f, sc.Speed);
                if (sc.Loop && cached->Duration > 0.0f) t = std::fmod(t, cached->Duration);
                std::vector<glm::mat4> local;
                EvaluateAnimation(*cached, t, *data->Skeleton, local);
                std::vector<glm::mat4> global(local.size());
                for (size_t i = 0; i < local.size(); ++i) { int parent = (i < data->Skeleton->BoneParents.size()) ? data->Skeleton->BoneParents[i] : -1; global[i] = parent < 0 ? local[i] : global[parent] * local[i]; }
                data->Skinning->Palette.resize(global.size(), glm::mat4(1.0f));
                for (size_t i = 0; i < global.size(); ++i) { glm::mat4 invBind = (i < data->Skeleton->InverseBindPoses.size()) ? data->Skeleton->InverseBindPoses[i] : glm::mat4(1.0f); data->Skinning->Palette[i] = global[i] * invBind; }
            }
        }
    }

    ImGui::End();
}

// ----------------- UI Helpers -----------------
void AnimationTimelinePanel::DrawToolbar()
{
    ImGui::BeginChild("TimelineToolbar", ImVec2(0, 38), false, ImGuiWindowFlags_NoScrollbar);
    if (ImGui::Button(m_Playing ? "Pause" : "Play")) m_Playing = !m_Playing;
    ImGui::SameLine();
    if (ImGui::Button("Stop")) { m_Playing = false; m_TimeSec = 0.0f; }
    ImGui::SameLine();
    ImGui::DragFloat("FPS", &m_FPS, 0.1f, 1.0f, 240.0f);
    ImGui::SameLine();
    ImGui::TextDisabled("| Length");
    ImGui::SameLine();
    ImGui::DragFloat("##Length", &m_Clip.Length, 0.01f, 0.01f, 600.0f);
    ImGui::EndChild();
}

void AnimationTimelinePanel::DrawAssetHeader()
{
    ScanAvailableTimelines();
    if (ImGui::BeginCombo("Timeline", m_TimelinePath.empty() ? "<unsaved>" : m_TimelinePath.c_str())) {
        for (const auto& p : m_AvailableTimelinePaths) {
            bool sel = (p == m_TimelinePath);
            if (ImGui::Selectable(p.c_str(), sel)) {
                m_TimelinePath = p;
                m_Clip = LoadTimelineClip(p);
                EnsureDefaultTracks();
            }
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    if (ImGui::Button("New Timeline")) {
        m_Clip = TimelineClip{};
        m_Clip.Name = "NewTimeline";
        m_Clip.Length = 5.0f;
        EnsureDefaultTracks();
        m_TimelinePath.clear();
        m_TimeSec = 0.0f;
        m_CurrentFrame = 0;
    }
    ImGui::SameLine();
    if (ImGui::Button("Save Timeline")) {
        if (m_TimelinePath.empty()) {
            m_TimelinePath = std::string("assets/") + (m_Clip.Name.empty() ? std::string("Timeline") : m_Clip.Name) + ".animtl";
        }
        SaveTimelineClip(m_Clip, m_TimelinePath);
        m_TimelineListScanned = false; // rescan to include new file
    }
}

void AnimationTimelinePanel::DrawSequencer()
{
    TrackSequence seq; seq.Clip = &m_Clip; seq.FPS = m_FPS;
    ImSequencer::Sequencer(&seq, &m_CurrentFrame, &m_Expanded, &m_SelectedEntry, &m_FirstFrame, ImSequencer::SEQUENCER_EDIT_ALL);

    // After sequencer, derive geometry to support custom drawing/selection
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 seqMin = ImGui::GetItemRectMin();
    ImVec2 seqMax = ImGui::GetItemRectMax();
    // Estimate the left edge of the timeline area (label column width varies)
    float maxLabelWidth = 0.0f;
    for (size_t i = 0; i < (m_Clip.Tracks.size() + m_Clip.ScriptTracks.size()); ++i) {
        std::string lbl = (i < m_Clip.Tracks.size()) ? m_Clip.Tracks[i].PropertyPath : m_Clip.ScriptTracks[i - m_Clip.Tracks.size()].Name;
        maxLabelWidth = std::max(maxLabelWidth, ImGui::CalcTextSize(lbl.c_str()).x);
    }
    const float labelPaddingLeft = 28.0f;   // space for tree arrow/checkbox
    const float labelPaddingRight = 24.0f;  // spacing to timeline
    float timelineLeft = seqMin.x + labelPaddingLeft + maxLabelWidth + labelPaddingRight;
    float trackHeight = 18.0f;
    float rowTop = seqMin.y + 36.0f; // after header/ruler
    const float framesToPixels =  std::max(1.0f, (seqMax.x - timelineLeft - 8.0f)) / std::max(1.0f, (float)(int)std::ceil(m_Clip.Length * m_FPS));

    // Omit any custom segment overlay to avoid clashing visually with ImSequencer's blocks

    // No custom keyframe markers for now (testing phase)
    const float keyHalf = 4.0f;

    // Left-click near a keyframe to select it for inspector editing
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        ImVec2 mouse = ImGui::GetMousePos();
        int hoveredRow = (int)std::floor((mouse.y - rowTop) / trackHeight);
        if (hoveredRow >= 0 && hoveredRow < (int)m_Clip.Tracks.size() && mouse.x >= timelineLeft) {
            const auto& track = m_Clip.Tracks[hoveredRow];
            int bestIdx = -1; float bestDist = 1e9f;
            for (int i = 0; i < (int)track.Keys.size(); ++i) {
                int f = (int)std::round(track.Keys[i].Time * m_FPS);
                float cx = timelineLeft + f * framesToPixels;
                float cy = rowTop + hoveredRow * trackHeight + 9.0f;
                float dist = fabsf(mouse.x - cx) + fabsf(mouse.y - cy);
                if (dist < bestDist) { bestDist = dist; bestIdx = i; }
            }
            const float selectTol = keyHalf * 1.5f;
            if (bestIdx >= 0) {
                int f = (int)std::round(track.Keys[bestIdx].Time * m_FPS);
                float cx = timelineLeft + f * framesToPixels;
                float cy = rowTop + hoveredRow * trackHeight + 9.0f;
                if (fabsf(mouse.x - cx) + fabsf(mouse.y - cy) <= selectTol) {
                    m_SelectedEntry = hoveredRow;
                    m_SelectedKeyIndex = bestIdx;
                    // Snap playhead to selected key to drive inspector editing
                    m_CurrentFrame = (int)std::round(track.Keys[bestIdx].Time * m_FPS);
                    m_TimeSec = m_CurrentFrame / std::max(1.0f, m_FPS);

                    // Begin drag operation for this keyframe
                    m_IsDraggingKey = true;
                    m_KeyDragStartMouseX = mouse.x;
                    m_KeyDragOriginalTime = track.Keys[bestIdx].Time;
                }
            }
        }
    }

    // Handle dragging the selected keyframe horizontally to reposition it
    if (m_IsDraggingKey) {
        // If mouse is released or left the item, stop dragging
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) || !ImGui::IsItemHovered()) {
            m_IsDraggingKey = false;
        } else if (m_SelectedEntry >= 0 && m_SelectedEntry < (int)m_Clip.Tracks.size() && m_SelectedKeyIndex >= 0) {
            ImVec2 mouse = ImGui::GetMousePos();
            // Convert horizontal mouse delta to frame delta, then to time
            float dx = mouse.x - m_KeyDragStartMouseX;
            float dFrames = dx / std::max(1.0f, framesToPixels);
            float newFrame = std::max(0.0f, std::round((m_KeyDragOriginalTime * m_FPS) + dFrames));
            float newTime = newFrame / std::max(1.0f, m_FPS);
            // Apply to the key and keep keys sorted
            auto& track = m_Clip.Tracks[m_SelectedEntry];
            if (m_SelectedKeyIndex < (int)track.Keys.size()) {
                track.Keys[m_SelectedKeyIndex].Time = newTime;
                std::sort(track.Keys.begin(), track.Keys.end(), [](const KeyframeFloat& a, const KeyframeFloat& b){ return a.Time < b.Time; });
                // Re-find index of the dragged key (by matching value+time close)
                int newIndex = 0;
                float best = 1e9f;
                for (int i = 0; i < (int)track.Keys.size(); ++i) {
                    float diff = fabsf(track.Keys[i].Time - newTime);
                    if (diff < best) { best = diff; newIndex = i; }
                }
                m_SelectedKeyIndex = newIndex;
                // Update playhead to follow
                m_CurrentFrame = (int)newFrame;
                m_TimeSec = newTime;
            }
        }
    }

    // Right-click to add a keyframe to the hovered property track at current cursor time
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        ImVec2 mouse = ImGui::GetMousePos();
        if (mouse.x >= timelineLeft && mouse.y >= rowTop && mouse.y <= seqMax.y)
            ImGui::OpenPopup("TrackContext");
    }
    if (ImGui::BeginPopup("TrackContext")) {
        // Determine hovered track index
        ImVec2 mouse = ImGui::GetMousePos();
        int hoveredRow = (int)std::floor((mouse.y - rowTop) / trackHeight);
        if (hoveredRow >= 0 && hoveredRow < (int)m_Clip.Tracks.size() && mouse.x >= timelineLeft) {
            if (ImGui::MenuItem("Add Keyframe At Playhead")) {
                // Define keyframes independently of any entity: default to last key's value or 0.0
                m_SelectedEntry = hoveredRow; // select the track
                auto& track = m_Clip.Tracks[hoveredRow];
                float value = track.Keys.empty() ? 0.0f : track.Keys.back().Value;
                const float epsilon = 1e-4f;
                bool updated = false;
                for (auto& k : track.Keys) {
                    if (std::abs(k.Time - m_TimeSec) <= epsilon) { k.Value = value; updated = true; break; }
                }
                if (!updated) {
                    track.Keys.push_back({ m_TimeSec, value });
                    std::sort(track.Keys.begin(), track.Keys.end(), [](const KeyframeFloat& a, const KeyframeFloat& b){ return a.Time < b.Time; });
                }
            }
            if (ImGui::MenuItem("Delete Keyframe Near Playhead")) {
                auto& track = m_Clip.Tracks[hoveredRow];
                const float tol = 0.5f / std::max(1.0f, m_FPS);
                for (size_t i = 0; i < track.Keys.size(); ++i) {
                    if (std::abs(track.Keys[i].Time - m_TimeSec) <= tol) { track.Keys.erase(track.Keys.begin() + (long)i); break; }
                }
            }
        } else {
            ImGui::TextDisabled("(No property track under cursor)");
        }
        ImGui::EndPopup();
    }
}

void AnimationTimelinePanel::DrawPerKeyInspector(int totalFrames)
{
    if (m_SelectedEntry < 0) return;
    size_t propCount = m_Clip.Tracks.size();
    bool isProp = (size_t)m_SelectedEntry < propCount;
    if (isProp) {
        auto& track = m_Clip.Tracks[m_SelectedEntry];
        ImGui::Text("Track: %s", track.PropertyPath.c_str());

        // If a specific key is selected, expose direct editable fields bound to that key
        if (m_SelectedKeyIndex >= 0 && m_SelectedKeyIndex < (int)track.Keys.size()) {
            auto& k = track.Keys[m_SelectedKeyIndex];
            float frame = k.Time * m_FPS;
            if (ImGui::DragFloat("Frame", &frame, 1.0f, 0.0f, (float)totalFrames)) {
                k.Time = std::clamp(frame / m_FPS, 0.0f, (float)totalFrames / std::max(1.0f, m_FPS));
                std::sort(track.Keys.begin(), track.Keys.end(), [](const KeyframeFloat& a, const KeyframeFloat& b){ return a.Time < b.Time; });
            }
            ImGui::DragFloat("Value", &k.Value, 0.01f);
            if (ImGui::Button("Delete Key")) {
                track.Keys.erase(track.Keys.begin() + m_SelectedKeyIndex);
                m_SelectedKeyIndex = -1;
            }
        } else {
            // Fallback: add/edit nearest to playhead
            float tol = 0.5f / std::max(1.0f, m_FPS);
            // Value field bound to a temporary buffer
            static float s_ValueBuffer = 0.0f;
            ImGui::DragFloat("Value at Cursor", &s_ValueBuffer, 0.01f);
            ImGui::SameLine();
            if (ImGui::Button("Add/Update Near Cursor")) {
                const float epsilon = 1e-4f;
                bool updated = false;
                for (auto& existing : track.Keys) {
                    if (std::abs(existing.Time - m_TimeSec) <= epsilon) { existing.Value = s_ValueBuffer; updated = true; break; }
                }
                if (!updated) { track.Keys.push_back({ m_TimeSec, s_ValueBuffer }); }
                std::sort(track.Keys.begin(), track.Keys.end(), [](const KeyframeFloat& a, const KeyframeFloat& b){ return a.Time < b.Time; });
            }
            ImGui::SameLine();
            if (ImGui::Button("Delete Near Cursor")) {
                for (size_t i = 0; i < track.Keys.size(); ++i) {
                    if (std::abs(track.Keys[i].Time - m_TimeSec) <= tol) { track.Keys.erase(track.Keys.begin() + (long)i); break; }
                }
            }
            ImGui::TextDisabled("t=%.3fs", m_TimeSec);
        }

        ImGui::Separator();
        // Also list keys for quick selection
        for (int i = 0; i < (int)track.Keys.size(); ++i) {
            auto& k = track.Keys[i];
            ImGui::PushID(i);
            bool selected = (m_SelectedKeyIndex == i);
            if (ImGui::Selectable("Key", selected)) m_SelectedKeyIndex = i;
            ImGui::SameLine();
            ImGui::Text("t=%.3f v=%.3f", k.Time, k.Value);
            ImGui::PopID();
        }
        if (ImGui::Button("Add Key at Cursor")) {
            track.Keys.push_back({ m_TimeSec, 0.0f });
            std::sort(track.Keys.begin(), track.Keys.end(), [](auto&a,auto&b){return a.Time<b.Time;});
        }
    } else {
        auto& track = m_Clip.ScriptTracks[m_SelectedEntry - propCount];
        ImGui::Text("Script Track: %s", track.Name.c_str());
        for (int i = 0; i < (int)track.Keys.size(); ++i) {
            auto& k = track.Keys[i];
            ImGui::PushID(i);
            bool selected = (m_SelectedKeyIndex == i);
            if (ImGui::Selectable("", selected, 0, ImVec2(10, 10))) m_SelectedKeyIndex = i;
            ImGui::SameLine(); ImGui::Image(m_IconEvent, ImVec2(12, 12)); ImGui::SameLine();
            float frame = k.Time * m_FPS;
            if (ImGui::DragFloat("Frame", &frame, 1.0f, 0.0f, (float)totalFrames)) k.Time = frame / m_FPS;
            char bufC[128]; std::strncpy(bufC, k.ScriptClass.c_str(), sizeof(bufC)); bufC[sizeof(bufC)-1]=0; if (ImGui::InputText("Class", bufC, sizeof(bufC))) k.ScriptClass=bufC;
            char bufM[128]; std::strncpy(bufM, k.Method.c_str(), sizeof(bufM)); bufM[sizeof(bufM)-1]=0; if (ImGui::InputText("Method", bufM, sizeof(bufM))) k.Method=bufM;
            if (ImGui::Button("Delete")) { track.Keys.erase(track.Keys.begin()+i); if (m_SelectedKeyIndex == i) m_SelectedKeyIndex = -1; ImGui::PopID(); break; }
            ImGui::PopID();
        }
        if (ImGui::Button("Add Event at Cursor")) { track.Keys.push_back({ m_TimeSec, "", "" }); std::sort(track.Keys.begin(), track.Keys.end(), [](auto&a,auto&b){return a.Time<b.Time;}); }
    }
}

void AnimationTimelinePanel::DrawPreviewBlock()
{
    ImGui::Checkbox("Enable Skeletal .anim Preview", &m_EnableSkeletalPreview);
    if (!m_EnableSkeletalPreview) return;
    ScanAvailableAnims();
    if (ImGui::BeginCombo("Clip", m_PreviewClipPath.empty() ? "<none>" : m_PreviewClipPath.c_str())) {
        for (const auto& p : m_AvailableAnimPaths) {
            bool sel = (p == m_PreviewClipPath);
            if (ImGui::Selectable(p.c_str(), sel)) {
                m_PreviewClipPath = p;
                auto clip = LoadAnimationClip(m_PreviewClipPath);
                if (clip.Duration > 0.0f || !clip.BoneTracks.empty()) m_PreviewClip = std::make_shared<AnimationClip>(std::move(clip)); else m_PreviewClip.reset();
            }
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    if (ImGui::Button("New Animation")) { m_PreviewClip = std::make_shared<AnimationClip>(); m_PreviewClip->Name = "NewClip"; m_PreviewClip->Duration = 1.0f; m_PreviewClipPath.clear(); }
    ImGui::SameLine();
    if (ImGui::Button("Save Animation As")) { std::string out = m_PreviewClipPath.empty() ? std::string("assets/") + (m_PreviewClip ? m_PreviewClip->Name : "Clip") + ".anim" : m_PreviewClipPath; if (m_PreviewClip) SaveAnimationClip(*m_PreviewClip, out); }
    ImGui::Checkbox("Preview on Active", &m_PreviewOnActive);
    ImGui::SameLine();
    ImGui::TextDisabled("Any entity: applies pose to skeleton if present; otherwise shows tracks only");
    if (m_PreviewOnActive && m_PreviewClip && m_Scene && m_Selected && *m_Selected != (EntityID)-1) {
        EntityID id = *m_Selected; auto* data = m_Scene->GetEntityData(id);
        if (data && data->Skeleton && data->Skinning) {
            std::vector<glm::mat4> local; float t = std::fmod(m_TimeSec, std::max(0.001f, m_PreviewClip->Duration));
            EvaluateAnimation(*m_PreviewClip, t, *data->Skeleton, local);
            std::vector<glm::mat4> global(local.size());
            for (size_t i = 0; i < local.size(); ++i) { int parent = (i < data->Skeleton->BoneParents.size()) ? data->Skeleton->BoneParents[i] : -1; global[i] = parent < 0 ? local[i] : global[parent] * local[i]; }
            data->Skinning->Palette.resize(global.size(), glm::mat4(1.0f));
            for (size_t i = 0; i < global.size(); ++i) { glm::mat4 invBind = (i < data->Skeleton->InverseBindPoses.size()) ? data->Skeleton->InverseBindPoses[i] : glm::mat4(1.0f); data->Skinning->Palette[i] = global[i] * invBind; }
        }
    }

    // Show skeletal tracks (including humanoid bones) if the loaded .anim has them
    if (m_PreviewClip) {
        if (ImGui::CollapsingHeader("Skeletal Tracks")) {
            // Summary row
            ImGui::TextDisabled("Bones animated: %zu", m_PreviewClip->BoneTracks.size());
            const int maxRows = 64; int row = 0;
            for (const auto& [boneName, track] : m_PreviewClip->BoneTracks) {
                if (row++ >= maxRows) { ImGui::TextDisabled("... %zu more", m_PreviewClip->BoneTracks.size() - (size_t)maxRows); break; }
                ImGui::BulletText("%s  (P:%zu R:%zu S:%zu)", boneName.c_str(),
                                  track.PositionKeys.size(), track.RotationKeys.size(), track.ScaleKeys.size());
            }
            if (m_PreviewClip->IsHumanoid) {
                ImGui::TextDisabled("Humanoid: source rig: %s", m_PreviewClip->SourceAvatarRigName.c_str());
            }
        }
    }
}

void AnimationTimelinePanel::EnsureDefaultTracks()
{
    if (m_Clip.Tracks.empty() && m_Clip.ScriptTracks.empty()) {
        // Provide a few common transform tracks to make the sequencer useful immediately
        AddPropertyTrack("Transform.Position.x");
        AddPropertyTrack("Transform.Position.y");
        AddPropertyTrack("Transform.Position.z");
        AddScriptTrack();
    }
}

void AnimationTimelinePanel::DrawTrackManagerUI()
{
    ImGui::Separator();
    ImGui::TextDisabled("Tracks");
    ImGui::SameLine();
    if (ImGui::Button("+ Add Track")) ImGui::OpenPopup("AddTrackPopup");
    if (ImGui::BeginPopup("AddTrackPopup")) {
        if (ImGui::BeginMenu("Transform")) {
            if (ImGui::MenuItem("Position X")) AddPropertyTrack("Transform.Position.x");
            if (ImGui::MenuItem("Position Y")) AddPropertyTrack("Transform.Position.y");
            if (ImGui::MenuItem("Position Z")) AddPropertyTrack("Transform.Position.z");
            if (ImGui::MenuItem("Rotation X")) AddPropertyTrack("Transform.Rotation.x");
            if (ImGui::MenuItem("Rotation Y")) AddPropertyTrack("Transform.Rotation.y");
            if (ImGui::MenuItem("Rotation Z")) AddPropertyTrack("Transform.Rotation.z");
            if (ImGui::MenuItem("Scale X"))    AddPropertyTrack("Transform.Scale.x");
            if (ImGui::MenuItem("Scale Y"))    AddPropertyTrack("Transform.Scale.y");
            if (ImGui::MenuItem("Scale Z"))    AddPropertyTrack("Transform.Scale.z");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Rendering")) {
            if (ImGui::MenuItem("Light Intensity")) AddPropertyTrack("Light.Intensity");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Particles")) {
            if (ImGui::MenuItem("Particles Per Second")) AddPropertyTrack("ParticleEmitter.ParticlesPerSecond");
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Script Events")) AddScriptTrack();
        ImGui::EndPopup();
    }

    // Removal control for currently selected track
    if (m_SelectedEntry >= 0) {
        size_t propCount = m_Clip.Tracks.size();
        bool isProp = (size_t)m_SelectedEntry < propCount;
        ImGui::SameLine();
        if (ImGui::Button("Remove Selected")) {
            if (isProp) m_Clip.Tracks.erase(m_Clip.Tracks.begin() + m_SelectedEntry);
            else m_Clip.ScriptTracks.erase(m_Clip.ScriptTracks.begin() + (m_SelectedEntry - (int)propCount));
            m_SelectedEntry = -1; m_SelectedKeyIndex = -1;
        }
    }
}

void AnimationTimelinePanel::AddPropertyTrack(const std::string& propertyPath)
{
    cm::animation::PropertyTrack t; t.PropertyPath = propertyPath; m_Clip.Tracks.push_back(std::move(t));
}

void AnimationTimelinePanel::AddScriptTrack(const std::string& name)
{
    cm::animation::ScriptEventTrack t; t.Name = name; m_Clip.ScriptTracks.push_back(std::move(t));
}

// --------------- External integration for Inspector ---------------
bool AnimationTimelinePanel::AddOrUpdateKeyAtCursor(float value)
{
    if (!HasActivePropertySelection()) return false;
    auto& track = m_Clip.Tracks[m_SelectedEntry];
    // If key exists at (approximately) the same time, update it
    const float epsilon = 1e-4f;
    for (auto& k : track.Keys) {
        if (std::abs(k.Time - m_TimeSec) <= epsilon) { k.Value = value; return true; }
    }
    // Otherwise insert and keep sorted
    track.Keys.push_back({ m_TimeSec, value });
    std::sort(track.Keys.begin(), track.Keys.end(), [](const KeyframeFloat& a, const KeyframeFloat& b){ return a.Time < b.Time; });
    return true;
}

bool AnimationTimelinePanel::DeleteKeyNearCursor(float toleranceSec)
{
    if (!HasActivePropertySelection()) return false;
    auto& track = m_Clip.Tracks[m_SelectedEntry];
    for (size_t i = 0; i < track.Keys.size(); ++i) {
        if (std::abs(track.Keys[i].Time - m_TimeSec) <= toleranceSec) {
            track.Keys.erase(track.Keys.begin() + (long)i);
            return true;
        }
    }
    return false;
}

bool AnimationTimelinePanel::GetSelectedKey(KeyframeFloat& outKey) const
{
    int ti = GetSelectedTrackIndex();
    if (ti < 0) return false;
    if (m_SelectedKeyIndex < 0) return false;
    if ((size_t)ti >= m_Clip.Tracks.size()) return false;
    const auto& tr = m_Clip.Tracks[ti];
    if ((size_t)m_SelectedKeyIndex >= tr.Keys.size()) return false;
    outKey = tr.Keys[m_SelectedKeyIndex];
    return true;
}

bool AnimationTimelinePanel::SetSelectedKey(float newTimeSec, float newValue)
{
    int ti = GetSelectedTrackIndex();
    if (ti < 0) return false;
    if (m_SelectedKeyIndex < 0) return false;
    auto& tr = m_Clip.Tracks[ti];
    if ((size_t)m_SelectedKeyIndex >= tr.Keys.size()) return false;

    // Capture value to identify key after sort
    float oldVal = tr.Keys[m_SelectedKeyIndex].Value;
    tr.Keys[m_SelectedKeyIndex].Time = std::max(0.0f, newTimeSec);
    tr.Keys[m_SelectedKeyIndex].Value = newValue;
    std::sort(tr.Keys.begin(), tr.Keys.end(), [](const KeyframeFloat& a, const KeyframeFloat& b){ return a.Time < b.Time; });
    // Re-find by closest time match to newTimeSec
    int best = 0; float bestDiff = 1e9f;
    for (int i = 0; i < (int)tr.Keys.size(); ++i) {
        float d = std::fabs(tr.Keys[i].Time - newTimeSec) + std::fabs(tr.Keys[i].Value - newValue) * 1e-3f;
        if (d < bestDiff) { bestDiff = d; best = i; }
    }
    m_SelectedKeyIndex = best;
    return true;
}

bool AnimationTimelinePanel::RemoveSelectedKey()
{
    int ti = GetSelectedTrackIndex();
    if (ti < 0) return false;
    if (m_SelectedKeyIndex < 0) return false;
    auto& tr = m_Clip.Tracks[ti];
    if ((size_t)m_SelectedKeyIndex >= tr.Keys.size()) return false;
    tr.Keys.erase(tr.Keys.begin() + m_SelectedKeyIndex);
    m_SelectedKeyIndex = -1;
    return true;
}

// Build render items: contiguous segments where consecutive key values are equal
void AnimationTimelinePanel::BuildRenderItems()
{
    // Intentionally empty: no overlay segments for now
    m_RenderItems.clear();
}

int AnimationTimelinePanel::GetSelectedTrackIndex() const
{
    size_t propCount = m_Clip.Tracks.size();
    if (m_SelectedEntry < 0) return -1;
    if ((size_t)m_SelectedEntry < propCount) return m_SelectedEntry;
    return -1;
}

