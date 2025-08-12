#include "editor/animation/AnimationTimelinePanel.h"
#include <algorithm>
#include <cmath>
#include <cstring>

bool AnimTimelinePanel::OpenAsset(const std::string& path) {
    return m_Doc.Load(path);
}

void AnimTimelinePanel::DrawToolbar()
{
    ImGui::BeginChild("AnimToolbar", ImVec2(0, 40), false, ImGuiWindowFlags_NoScrollbar);
    if (ImGui::Button("New")) { m_Doc.New(); m_Playing = false; }
    ImGui::SameLine();
    if (ImGui::Button("Open")) {
        // Prefer native dialog for clarity; fallback to asset picker if canceled
        std::string path = ShowOpenFileDialogExt(L"Animation (*.anim)", L"anim");
        if (!path.empty()) OpenAsset(path);
        else {
        auto res = DrawAssetPicker({"*.anim", "Open Animation", true});
        if (res.chosen) OpenAsset(res.path);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        if (m_Doc.path.empty()) {
            std::string path = ShowSaveFileDialogExt(L"NewAnimation.anim", L"Animation (*.anim)", L"anim");
            if (!path.empty()) m_Doc.Save(path);
            else {
            auto res = DrawAssetPicker({"*.anim", "Save Animation As", true});
            if (res.chosen) m_Doc.Save(res.path);
            }
        } else {
            m_Doc.Save(m_Doc.path);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Save As")) {
        std::string path = ShowSaveFileDialogExt(L"Animation.anim", L"Animation (*.anim)", L"anim");
        if (!path.empty()) m_Doc.Save(path);
        else {
        auto res = DrawAssetPicker({"*.anim", "Save Animation As", true});
        if (res.chosen) m_Doc.Save(res.path);
        }
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
    // Top ruler
    ImGui::Text("Duration: %.3fs", dur);
    ImGui::PushID("Ruler");
    ImGui::SliderFloat("##Time", &m_Doc.time, 0.0f, dur, "Time: %.3fs");
    m_Doc.time = std::clamp(ApplySnap(m_Doc.time), 0.0f, dur);
    ImGui::PopID();

    // Draw per-track lanes with keyframes and interactions
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 laneOrigin = ImGui::GetCursorScreenPos();
    float laneW = ImGui::GetContentRegionAvail().x - 12.0f;
    if (laneW < 50.0f) laneW = 50.0f;
    float laneH = 30.0f;
    const float keyHalf = 3.0f;
    m_HoverKey = 0; // reset each frame
    m_ContextLaneTrackIndex = -1;

    for (int i = 0; i < (int)m_Doc.asset.tracks.size(); ++i) {
        auto* tr = m_Doc.asset.tracks[i].get();
        if (!tr) continue;
        ImVec2 a = ImVec2(laneOrigin.x, laneOrigin.y + i * (laneH + 6.0f));
        ImVec2 b = ImVec2(laneOrigin.x + laneW, a.y + laneH);
        // Background for lane
        dl->AddRectFilled(a, b, IM_COL32(32,32,32,255));
        dl->AddRect(a, b, IM_COL32(64,64,64,255));

        // Keys per track type
        auto drawKeyRow = [&](float kt, cm::animation::KeyID id, int row, ImU32 baseCol){
            float x = TimeToX(kt, dur, a.x, laneW);
            float subH = laneH / 3.0f; float pad = 2.0f;
            float y0 = a.y + row * subH + pad;
            float y1 = y0 + subH - pad * 2.0f;
            ImVec2 p0(x - keyHalf, y0);
            ImVec2 p1(x + keyHalf, y1);
            ImU32 col = (std::find(m_Doc.selectedKeys.begin(), m_Doc.selectedKeys.end(), id) != m_Doc.selectedKeys.end()) ? IM_COL32(180,220,100,255) : baseCol;
            dl->AddRectFilled(p0, p1, col);
            dl->AddRect(p0, p1, IM_COL32(40,40,40,255));

            // Hover/drag detection
            if (ImGui::IsMouseHoveringRect(p0, p1)) {
                m_HoverKey = id;
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    SelectSingleKey(id);
                    m_DragKey = id;
                    m_DragStartMouseX = ImGui::GetIO().MousePos.x;
                    m_DragStartTime = kt;
                }
            }
        };

        switch (tr->type) {
            case cm::animation::TrackType::Bone: {
                auto* t = static_cast<cm::animation::AssetBoneTrack*>(tr);
                for (auto const& k : t->t.keys) drawKeyRow(k.t, k.id, 0, IM_COL32(160,200,255,255));
                for (auto const& k : t->r.keys) drawKeyRow(k.t, k.id, 1, IM_COL32(255,180,120,255));
                for (auto const& k : t->s.keys) drawKeyRow(k.t, k.id, 2, IM_COL32(180,255,160,255));
            } break;
            case cm::animation::TrackType::Avatar: {
                auto* t = static_cast<cm::animation::AssetAvatarTrack*>(tr);
                for (auto const& k : t->t.keys) drawKeyRow(k.t, k.id, 0, IM_COL32(160,200,255,255));
                for (auto const& k : t->r.keys) drawKeyRow(k.t, k.id, 1, IM_COL32(255,180,120,255));
                for (auto const& k : t->s.keys) drawKeyRow(k.t, k.id, 2, IM_COL32(180,255,160,255));
            } break;
            case cm::animation::TrackType::Property: {
                auto* t = static_cast<cm::animation::AssetPropertyTrack*>(tr);
                auto visit = [&](auto const& curve){
                    for (auto const& k : curve.keys) {
                        float x = TimeToX(k.t, dur, a.x, laneW);
                        ImVec2 p0(x - keyHalf, a.y);
                        ImVec2 p1(x + keyHalf, b.y);
                        ImU32 col = (std::find(m_Doc.selectedKeys.begin(), m_Doc.selectedKeys.end(), k.id) != m_Doc.selectedKeys.end()) ? IM_COL32(180,220,100,255) : IM_COL32(200,200,200,255);
                        dl->AddRectFilled(p0, p1, col);
                        dl->AddRect(p0, p1, IM_COL32(40,40,40,255));
                        if (ImGui::IsMouseHoveringRect(p0, p1)) {
                            m_HoverKey = k.id;
                            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) { SelectSingleKey(k.id); m_DragKey = k.id; m_DragStartMouseX = ImGui::GetIO().MousePos.x; m_DragStartTime = k.t; }
                        }
                    }
                };
                switch (t->binding.type) {
                    case cm::animation::PropertyType::Float: visit(std::get<cm::animation::CurveFloat>(t->curve)); break;
                    case cm::animation::PropertyType::Vec2:  visit(std::get<cm::animation::CurveVec2>(t->curve)); break;
                    case cm::animation::PropertyType::Vec3:  visit(std::get<cm::animation::CurveVec3>(t->curve)); break;
                    case cm::animation::PropertyType::Quat:  visit(std::get<cm::animation::CurveQuat>(t->curve)); break;
                    case cm::animation::PropertyType::Color: visit(std::get<cm::animation::CurveColor>(t->curve)); break;
                }
            } break;
            case cm::animation::TrackType::ScriptEvent: {
                auto* t = static_cast<cm::animation::AssetScriptEventTrack*>(tr);
                for (auto const& e : t->events) drawKeyRow(e.time, e.id, 1, IM_COL32(220,220,220,255));
            } break;
        }

        // Context menu for adding keyframe at mouse position; prevent hover bleed
        // Lane interaction surface (ensures we can capture right click and avoid overlapping hover from other items)
        ImGui::PushID(i);
        ImGui::SetCursorScreenPos(a);
        ImGui::InvisibleButton("LaneBtn", ImVec2(laneW, laneH));
        if (ImGui::BeginPopupContextItem("LaneCtx", ImGuiPopupFlags_MouseButtonRight)) {
            m_ContextLaneTrackIndex = i;
            float mouseX = ImGui::GetIO().MousePos.x;
            float tAtMouse = XToTime(mouseX, a.x, laneW, dur);
            tAtMouse = ApplySnap(tAtMouse);
            if (ImGui::MenuItem("Add Keyframe")) {
                auto* trw = m_Doc.asset.tracks[m_ContextLaneTrackIndex].get();
                using namespace cm::animation;
                switch (trw->type) {
                    case TrackType::Bone: {
                        auto* t = static_cast<AssetBoneTrack*>(trw);
                        KeyID id = m_Doc.GenerateKeyID();
                        t->t.keys.push_back({id, tAtMouse, glm::vec3(0)});
                        t->r.keys.push_back({m_Doc.GenerateKeyID(), tAtMouse, glm::quat(1,0,0,0)});
                        t->s.keys.push_back({m_Doc.GenerateKeyID(), tAtMouse, glm::vec3(1)});
                        SelectSingleKey(id);
                    } break;
                    case TrackType::Avatar: {
                        auto* t = static_cast<AssetAvatarTrack*>(trw);
                        KeyID id = m_Doc.GenerateKeyID();
                        t->t.keys.push_back({id, tAtMouse, glm::vec3(0)});
                        t->r.keys.push_back({m_Doc.GenerateKeyID(), tAtMouse, glm::quat(1,0,0,0)});
                        t->s.keys.push_back({m_Doc.GenerateKeyID(), tAtMouse, glm::vec3(1)});
                        SelectSingleKey(id);
                    } break;
                    case TrackType::Property: {
                        auto* t = static_cast<AssetPropertyTrack*>(trw);
                        auto push = [&](auto& curve, auto v){ cm::animation::KeyID id = m_Doc.GenerateKeyID(); curve.keys.push_back({id, tAtMouse, v}); SelectSingleKey(id); };
                        switch (t->binding.type) {
                            case cm::animation::PropertyType::Float: push(std::get<cm::animation::CurveFloat>(t->curve), 0.0f); break;
                            case cm::animation::PropertyType::Vec2:  push(std::get<cm::animation::CurveVec2>(t->curve), glm::vec2(0)); break;
                            case cm::animation::PropertyType::Vec3:  push(std::get<cm::animation::CurveVec3>(t->curve), glm::vec3(0)); break;
                            case cm::animation::PropertyType::Quat:  push(std::get<cm::animation::CurveQuat>(t->curve), glm::quat(1,0,0,0)); break;
                            case cm::animation::PropertyType::Color: push(std::get<cm::animation::CurveColor>(t->curve), glm::vec4(1)); break;
                        }
                    } break;
                    case TrackType::ScriptEvent: {
                        auto* t = static_cast<AssetScriptEventTrack*>(trw);
                        cm::animation::KeyID id = m_Doc.GenerateKeyID();
                        t->events.push_back({id, tAtMouse, "", "", {}});
                        SelectSingleKey(id);
                    } break;
                }
                m_Doc.MarkDirty();
            }
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }

    // Drag move for a selected key
    if (m_DragKey != 0) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            float dx = ImGui::GetIO().MousePos.x - m_DragStartMouseX;
            float laneWAll = laneW; // approximate per-lane scaling
            float newT = m_DragStartTime + (dx / laneWAll) * dur;
            newT = std::clamp(ApplySnap(newT), 0.0f, dur);
            // Update the key's time wherever it lives
            for (auto& up : m_Doc.asset.tracks) {
                if (!up) continue;
                using namespace cm::animation;
                if (up->type == TrackType::Bone) {
                    auto* t = static_cast<AssetBoneTrack*>(up.get());
                    for (auto& k : t->t.keys) if (k.id == m_DragKey) k.t = newT;
                    for (auto& k : t->r.keys) if (k.id == m_DragKey) k.t = newT;
                    for (auto& k : t->s.keys) if (k.id == m_DragKey) k.t = newT;
                } else if (up->type == TrackType::Avatar) {
                    auto* t = static_cast<AssetAvatarTrack*>(up.get());
                    for (auto& k : t->t.keys) if (k.id == m_DragKey) k.t = newT;
                    for (auto& k : t->r.keys) if (k.id == m_DragKey) k.t = newT;
                    for (auto& k : t->s.keys) if (k.id == m_DragKey) k.t = newT;
                } else if (up->type == TrackType::Property) {
                    auto* t = static_cast<AssetPropertyTrack*>(up.get());
                    auto upd = [&](auto& curve){ for (auto& k : curve.keys) if (k.id == m_DragKey) k.t = newT; };
                    switch (t->binding.type) {
                        case cm::animation::PropertyType::Float: upd(std::get<cm::animation::CurveFloat>(t->curve)); break;
                        case cm::animation::PropertyType::Vec2:  upd(std::get<cm::animation::CurveVec2>(t->curve)); break;
                        case cm::animation::PropertyType::Vec3:  upd(std::get<cm::animation::CurveVec3>(t->curve)); break;
                        case cm::animation::PropertyType::Quat:  upd(std::get<cm::animation::CurveQuat>(t->curve)); break;
                        case cm::animation::PropertyType::Color: upd(std::get<cm::animation::CurveColor>(t->curve)); break;
                    }
                } else if (up->type == TrackType::ScriptEvent) {
                    auto* t = static_cast<AssetScriptEventTrack*>(up.get());
                    for (auto& e : t->events) if (e.id == m_DragKey) e.time = newT;
                }
            }
            m_Doc.MarkDirty();
        } else {
            // Release
            m_DragKey = 0;
        }
    }

    ImGui::EndChild();
}

void AnimTimelinePanel::DrawInspector()
{
    ImGui::BeginChild("Inspector", ImVec2(280, 0), true);
    ImGui::TextDisabled("Inspector");
    // Keyframe inspector when one key is selected
    if (!m_Doc.selectedKeys.empty()) {
        cm::animation::KeyID sel = m_Doc.selectedKeys.front();
        float* pFloat = nullptr; glm::vec2* pV2 = nullptr; glm::vec3* pV3 = nullptr; glm::quat* pQ = nullptr; glm::vec4* pC = nullptr; float* pT = nullptr;
        std::string* pScriptClass = nullptr; std::string* pScriptMethod = nullptr;
        // Locate the selected key and present its value editor
        for (auto& up : m_Doc.asset.tracks) {
            if (!up) continue;
            using namespace cm::animation;
            if (up->type == TrackType::Bone) {
                auto* t = static_cast<AssetBoneTrack*>(up.get());
                for (auto& k : t->t.keys) if (k.id == sel) { pV3 = &k.v; pT = &k.t; }
                for (auto& k : t->r.keys) if (k.id == sel) { pQ = &k.v; pT = &k.t; }
                for (auto& k : t->s.keys) if (k.id == sel) { pV3 = &k.v; pT = &k.t; }
            } else if (up->type == TrackType::Avatar) {
                auto* t = static_cast<AssetAvatarTrack*>(up.get());
                for (auto& k : t->t.keys) if (k.id == sel) { pV3 = &k.v; pT = &k.t; }
                for (auto& k : t->r.keys) if (k.id == sel) { pQ = &k.v; pT = &k.t; }
                for (auto& k : t->s.keys) if (k.id == sel) { pV3 = &k.v; pT = &k.t; }
            } else if (up->type == TrackType::Property) {
                auto* t = static_cast<AssetPropertyTrack*>(up.get());
                auto bindFloat = [&](auto& curve){ for (auto& k : curve.keys) if (k.id == sel) { if constexpr (std::is_same_v<decltype(k.v), float>) pFloat = &k.v; else if constexpr (std::is_same_v<decltype(k.v), glm::vec2>) pV2 = &k.v; else if constexpr (std::is_same_v<decltype(k.v), glm::vec3>) pV3 = &k.v; else if constexpr (std::is_same_v<decltype(k.v), glm::quat>) pQ = &k.v; else if constexpr (std::is_same_v<decltype(k.v), glm::vec4>) pC = &k.v; pT = &k.t; } };
                switch (t->binding.type) {
                    case cm::animation::PropertyType::Float: bindFloat(std::get<cm::animation::CurveFloat>(t->curve)); break;
                    case cm::animation::PropertyType::Vec2:  bindFloat(std::get<cm::animation::CurveVec2>(t->curve)); break;
                    case cm::animation::PropertyType::Vec3:  bindFloat(std::get<cm::animation::CurveVec3>(t->curve)); break;
                    case cm::animation::PropertyType::Quat:  bindFloat(std::get<cm::animation::CurveQuat>(t->curve)); break;
                    case cm::animation::PropertyType::Color: bindFloat(std::get<cm::animation::CurveColor>(t->curve)); break;
                }
            } else if (up->type == TrackType::ScriptEvent) {
                auto* t = static_cast<AssetScriptEventTrack*>(up.get());
                for (auto& e : t->events) if (e.id == sel) { pT = &e.time; pScriptClass = &e.className; pScriptMethod = &e.method; }
            }
        }

        if (pT) { ImGui::DragFloat("Time", pT, 0.001f, 0.0f, m_Doc.Duration(), "%.3fs"); }
        if (pFloat) { ImGui::DragFloat("Value", pFloat, 0.01f); }
        if (pV2) { ImGui::DragFloat2("Value", &pV2->x, 0.01f); }
        if (pV3) { ImGui::DragFloat3("Value", &pV3->x, 0.01f); }
        if (pQ) { ImGui::DragFloat4("Quat (x,y,z,w)", &pQ->x, 0.01f); }
        if (pC) { ImGui::ColorEdit4("Color", &pC->x); }
        if (pScriptClass) {
            char buf[128] = {};
            std::strncpy(buf, pScriptClass->c_str(), sizeof(buf) - 1);
            if (ImGui::InputText("Script Class", buf, sizeof(buf))) { *pScriptClass = buf; m_Doc.MarkDirty(); }
        }
        if (pScriptMethod) {
            char buf[128] = {};
            std::strncpy(buf, pScriptMethod->c_str(), sizeof(buf) - 1);
            if (ImGui::InputText("Method", buf, sizeof(buf))) { *pScriptMethod = buf; m_Doc.MarkDirty(); }
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) m_Doc.MarkDirty();
    } else if (!m_Doc.selectedTracks.empty()) {
        ImGui::Text("Track selected: %zu", m_Doc.selectedTracks.size());
    } else {
        ImGui::TextDisabled("No selection");
    }
    ImGui::EndChild();
}


void AnimTimelinePanel::OnImGuiRender()
{
    ImGui::Begin("Animation Timeline", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

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

    // Layout: Left tree + lanes (combined), right inspector; bottom preview
    const float rightW = 300.0f;
    const float splitter = 6.0f;

    // Make this panel occupy the full available space in its dock node
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Once);
    float fullH = std::max(1.0f, ImGui::GetContentRegionAvail().y);
    ImGui::BeginChild("TopRegion", ImVec2(0, fullH), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    // Left+Center combined (track tree + sequencer lanes)
    ImGui::BeginChild("LeftAndCenter", ImVec2(-rightW - splitter, 0), true);
    DrawTrackTreeAndLanes();
    ImGui::EndChild();
    // Splitter (needs non-zero height for ImGui::InvisibleButton) to separate from inspector
    ImGui::SameLine();
    float topAvailH = ImGui::GetContentRegionAvail().y;
    if (topAvailH < 1.0f) topAvailH = 1.0f;
    ImGui::InvisibleButton("split1", ImVec2(splitter, topAvailH));
    // Right
    ImGui::SameLine(); ImGui::BeginChild("RightPane", ImVec2(rightW, 0), true);
    DrawInspector();
    ImGui::EndChild();
    ImGui::EndChild();

    ImGui::End();
}


