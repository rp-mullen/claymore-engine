#include "editor/animation/AnimationTimelinePanel.h"
#include <algorithm>
#include <cmath>

using cm::animation::PreviewContext;

bool AnimTimelinePanel::OpenAsset(const std::string& path) {
    return m_Doc.Load(path);
}

void AnimTimelinePanel::DrawToolbar()
{
    ImGui::BeginChild("AnimToolbar", ImVec2(0, 40), false, ImGuiWindowFlags_NoScrollbar);
    if (ImGui::Button("New")) { m_Doc.New(); m_Playing = false; }
    ImGui::SameLine();
    if (ImGui::Button("Open")) {
        auto res = DrawAssetPicker({"*.anim", "Open Animation", true});
        if (res.chosen) OpenAsset(res.path);
    }
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        if (m_Doc.path.empty()) {
            auto res = DrawAssetPicker({"*.anim", "Save Animation As", true});
            if (res.chosen) m_Doc.Save(res.path);
        } else {
            m_Doc.Save(m_Doc.path);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Save As")) {
        auto res = DrawAssetPicker({"*.anim", "Save Animation As", true});
        if (res.chosen) m_Doc.Save(res.path);
    }

    ImGui::SameLine(); ImGui::TextDisabled("|"); ImGui::SameLine();
    if (ImGui::Button(m_Playing ? "Pause" : "Play")) m_Playing = !m_Playing;
    ImGui::SameLine(); if (ImGui::Button("Stop")) { m_Playing = false; m_Doc.time = 0.0f; }
    ImGui::SameLine(); ImGui::Checkbox("Loop", &m_Doc.loop);
    ImGui::SameLine(); ImGui::Checkbox("Snap Frame", &m_Doc.snapToFrame);
    ImGui::SameLine(); ImGui::Checkbox("Snap 0.1s", &m_Doc.snapTo01);
    ImGui::SameLine(); ImGui::SetNextItemWidth(80); ImGui::DragFloat("FPS", &m_Doc.fps, 0.1f, 1.0f, 240.0f, "%.1f");
    ImGui::SameLine(); ImGui::Text("t: %.3fs", m_Doc.time);
    ImGui::EndChild();
}

void AnimTimelinePanel::DrawTrackTreeAndLanes()
{
    ImGui::BeginChild("TrackTree", ImVec2(240, 0), true);
    ImGui::TextDisabled("Tracks");
    // Add Track menu
    if (ImGui::Button("+ Add Track")) ImGui::OpenPopup("AddTrackPopup");
    if (ImGui::BeginPopup("AddTrackPopup")) {
        if (ImGui::MenuItem("Bone Track")) {
            auto t = std::make_unique<cm::animation::AssetBoneTrack>();
            t->id = (cm::animation::TrackID)(m_Doc.asset.tracks.size() + 1);
            t->name = "Bone";
            m_Doc.asset.tracks.push_back(std::move(t)); m_Doc.MarkDirty();
        }
        if (ImGui::MenuItem("Avatar Track")) {
            auto t = std::make_unique<cm::animation::AssetAvatarTrack>();
            t->id = (cm::animation::TrackID)(m_Doc.asset.tracks.size() + 1);
            t->name = "Humanoid";
            m_Doc.asset.tracks.push_back(std::move(t)); m_Doc.MarkDirty();
        }
        if (ImGui::MenuItem("Property Track (Float)")) {
            auto t = std::make_unique<cm::animation::AssetPropertyTrack>();
            t->id = (cm::animation::TrackID)(m_Doc.asset.tracks.size() + 1);
            t->name = "Property"; t->binding.type = cm::animation::PropertyType::Float; t->curve = cm::animation::CurveFloat{};
            m_Doc.asset.tracks.push_back(std::move(t)); m_Doc.MarkDirty();
        }
        if (ImGui::MenuItem("Script Event Track")) {
            auto t = std::make_unique<cm::animation::AssetScriptEventTrack>();
            t->id = (cm::animation::TrackID)(m_Doc.asset.tracks.size() + 1);
            t->name = "Script Events";
            m_Doc.asset.tracks.push_back(std::move(t)); m_Doc.MarkDirty();
        }
        ImGui::EndPopup();
    }

    for (size_t i = 0; i < m_Doc.asset.tracks.size(); ++i) {
        auto* t = m_Doc.asset.tracks[i].get();
        ImGui::PushID((int)i);
        bool sel = std::find(m_Doc.selectedTracks.begin(), m_Doc.selectedTracks.end(), t->id) != m_Doc.selectedTracks.end();
        if (ImGui::Selectable((t->name + "##trk").c_str(), sel)) {
            m_Doc.selectedTracks.clear();
            m_Doc.selectedTracks.push_back(t->id);
        }
        if (ImGui::BeginPopupContextItem("TrackCtx")) {
            if (ImGui::MenuItem("Rename")) { /* TODO: inline rename */ }
            if (ImGui::MenuItem(t->muted ? "Unmute" : "Mute")) { t->muted = !t->muted; m_Doc.MarkDirty(); }
            if (ImGui::MenuItem("Duplicate")) {
                // Shallow duplicate of metadata; curves copied by value
                if (auto* b = dynamic_cast<cm::animation::AssetBoneTrack*>(t)) {
                    auto c = std::make_unique<cm::animation::AssetBoneTrack>(*b); c->id = (cm::animation::TrackID)(m_Doc.asset.tracks.size() + 1); m_Doc.asset.tracks.push_back(std::move(c));
                } else if (auto* a = dynamic_cast<cm::animation::AssetAvatarTrack*>(t)) {
                    auto c = std::make_unique<cm::animation::AssetAvatarTrack>(*a); c->id = (cm::animation::TrackID)(m_Doc.asset.tracks.size() + 1); m_Doc.asset.tracks.push_back(std::move(c));
                } else if (auto* p = dynamic_cast<cm::animation::AssetPropertyTrack*>(t)) {
                    auto c = std::make_unique<cm::animation::AssetPropertyTrack>(*p); c->id = (cm::animation::TrackID)(m_Doc.asset.tracks.size() + 1); m_Doc.asset.tracks.push_back(std::move(c));
                } else if (auto* s = dynamic_cast<cm::animation::AssetScriptEventTrack*>(t)) {
                    auto c = std::make_unique<cm::animation::AssetScriptEventTrack>(*s); c->id = (cm::animation::TrackID)(m_Doc.asset.tracks.size() + 1); m_Doc.asset.tracks.push_back(std::move(c));
                }
                m_Doc.MarkDirty();
            }
            if (ImGui::MenuItem("Delete")) {
                m_Doc.asset.tracks.erase(m_Doc.asset.tracks.begin() + (long)i); m_Doc.MarkDirty(); ImGui::EndPopup(); ImGui::PopID(); break;
            }
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("Lanes", ImVec2(0, 0), true);
    float dur = std::max(0.001f, m_Doc.Duration());
    ImGui::Text("Duration: %.3fs", dur);
    // Ruler
    ImGui::PushID("Ruler");
    ImGui::SliderFloat("##Time", &m_Doc.time, 0.0f, dur, "Time: %.3fs");
    if (m_Doc.snapToFrame && m_Doc.fps > 0.0f) {
        float frame = std::round(m_Doc.time * m_Doc.fps);
        m_Doc.time = std::clamp(frame / m_Doc.fps, 0.0f, dur);
    } else if (m_Doc.snapTo01) {
        float step = 0.1f; m_Doc.time = std::round(m_Doc.time / step) * step;
    }
    ImGui::PopID();
    // TODO: lanes + key editing UI
    ImGui::EndChild();
}

void AnimTimelinePanel::DrawInspector()
{
    ImGui::BeginChild("Inspector", ImVec2(280, 0), true);
    ImGui::TextDisabled("Inspector");
    if (!m_Doc.selectedTracks.empty()) {
        ImGui::Text("Track selected: %zu", m_Doc.selectedTracks.size());
    } else {
        ImGui::TextDisabled("No selection");
    }
    ImGui::EndChild();
}

void AnimTimelinePanel::DrawPreviewViewport()
{
    ImGui::TextDisabled("Preview");
    ImGui::Checkbox("Preview on Active", &m_PreviewOnActive);
    ImGui::SameLine(); ImGui::SetNextItemWidth(120); ImGui::DragFloat("Speed", &m_PlaySpeed, 0.01f, 0.1f, 3.0f, "%.2fx");

    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImVec2 size  = ImVec2(std::max(160.0f, avail.x), std::max(140.0f, avail.y * 0.35f));
    ImGui::BeginChild("PreviewViewport", size, true, ImGuiWindowFlags_NoScrollbar);

    ImGuiIO& io = ImGui::GetIO();
    bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);
    bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
    io.WantCaptureMouse |= hovered;
    io.WantCaptureKeyboard |= focused;

    ImVec2 vp = ImGui::GetContentRegionAvail();
    int w = (int)vp.x, h = (int)vp.y;
    if (w > 0 && h > 0) {
        if (!m_PreviewCtx) m_PreviewCtx = std::make_unique<PreviewContext>();
        if (!bgfx::isValid(m_PreviewCtx->fb)) m_PreviewCtx->Initialize(w, h);
        else m_PreviewCtx->Resize(w, h);
    }

    if (m_PreviewCtx && bgfx::isValid(m_PreviewCtx->fb)) {
        // Sample active document into preview pose
        cm::animation::EvalInputs in{ &m_Doc.asset, m_Doc.time, m_Doc.loop };
        cm::animation::EvalContext ctx{}; // no ECS touch
        cm::animation::EvalTargets tgt{ &m_PreviewCtx->pose };
        cm::animation::SampleAsset(in, ctx, tgt, nullptr, nullptr);

        cm::animation::Begin(*m_PreviewCtx, ImGui::GetItemRectMin(), vp);
        // TODO: Draw skeleton/mesh if bound
        cm::animation::End(*m_PreviewCtx);
        if (bgfx::isValid(m_PreviewCtx->color)) {
            ImGui::Image((ImTextureID)(uintptr_t)m_PreviewCtx->color.idx, vp);
        }
    }

    ImGui::EndChild();
}

void AnimTimelinePanel::OnImGuiRender()
{
    ImGui::Begin("Animation Timeline");

    // Toolbar
    DrawToolbar();

    // Update time when playing
    if (m_Playing) {
        float len = std::max(0.0f, m_Doc.Duration());
        m_Doc.time += ImGui::GetIO().DeltaTime * m_PlaySpeed;
        if (m_Doc.loop && len > 0.0f) {
            m_Doc.time = std::fmod(std::fmod(m_Doc.time, len) + len, len);
        } else {
            if (m_Doc.time < 0.0f) m_Doc.time = 0.0f;
            if (m_Doc.time > len) { m_Doc.time = len; m_Playing = false; }
        }
    }

    // Layout: Left tree, center lanes, right inspector; bottom preview
    const float leftW = 260.0f;
    const float rightW = 300.0f;
    const float splitter = 6.0f;

    float fullH = std::max(1.0f, ImGui::GetContentRegionAvail().y - 200.0f);
    ImGui::BeginChild("TopRegion", ImVec2(0, fullH), false);
    // Left
    ImGui::BeginChild("LeftPane", ImVec2(leftW, 0), true);
    DrawTrackTreeAndLanes();
    ImGui::EndChild();
    // Splitter (needs non-zero height for ImGui::InvisibleButton)
    ImGui::SameLine();
    float topAvailH = ImGui::GetContentRegionAvail().y;
    if (topAvailH < 1.0f) topAvailH = 1.0f;
    ImGui::InvisibleButton("split1", ImVec2(splitter, topAvailH));
    // Center
    ImGui::SameLine(); ImGui::BeginChild("CenterPane", ImVec2(-rightW - splitter, 0), true);
    // Simple lanes placeholder rendered in DrawTrackTreeAndLanes
    ImGui::EndChild();
    // Right
    ImGui::SameLine(); ImGui::BeginChild("RightPane", ImVec2(rightW, 0), true);
    DrawInspector();
    ImGui::EndChild();
    ImGui::EndChild();

    // Bottom preview
    DrawPreviewViewport();

    ImGui::End();
}


