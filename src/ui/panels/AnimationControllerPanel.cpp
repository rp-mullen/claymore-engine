#include "AnimationControllerPanel.h"
#include <imnodes.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include "animation/AnimatorController.h"
#include "ui/FileDialogs.h"
#include <windows.h>
#include <editor/Project.h>
#include <algorithm>

using nlohmann::json;
using cm::animation::AnimatorController;
using cm::animation::AnimatorState;
using cm::animation::AnimatorTransition;
using cm::animation::AnimatorParameter;

AnimationControllerPanel::AnimationControllerPanel() {
}

AnimationControllerPanel::~AnimationControllerPanel() {
}

bool AnimationControllerPanel::Load(const std::string& path) {
    std::ifstream in(path);
    if (!in) return false;
    json j; in >> j;
    auto ctrl = std::make_shared<AnimatorController>();
    nlohmann::from_json(j, *ctrl);
    m_Controller = ctrl;
    m_OpenPath = path;
    m_NextStateId = 1;
    for (const auto& s : ctrl->States) m_NextStateId = std::max(m_NextStateId, s.Id + 1);
    return true;
}

bool AnimationControllerPanel::Save(const std::string& path) {
    if (!m_Controller) return false;
    // Sanitize transitions: coerce condition modes to match parameter types before saving
    {
        std::unordered_map<std::string, int> ptype; // 0=Bool,1=Int,2=Float,3=Trigger
        for (const auto& p : m_Controller->Parameters) ptype[p.Name] = (int)p.Type;
        for (auto& t : m_Controller->Transitions) {
            for (auto& c : t.Conditions) {
                auto it = ptype.find(c.Parameter);
                if (it == ptype.end()) continue;
                int pt = it->second;
                switch (pt) {
                    case 0: { // Bool
                        if (!(c.Mode == cm::animation::ConditionMode::If || c.Mode == cm::animation::ConditionMode::IfNot))
                            c.Mode = cm::animation::ConditionMode::If;
                    } break;
                    case 1: // Int
                    case 2: { // Float
                        if (!(c.Mode == cm::animation::ConditionMode::Greater ||
                              c.Mode == cm::animation::ConditionMode::Less ||
                              c.Mode == cm::animation::ConditionMode::Equals ||
                              c.Mode == cm::animation::ConditionMode::NotEquals))
                            c.Mode = cm::animation::ConditionMode::Greater;
                    } break;
                    case 3: { // Trigger
                        if (c.Mode != cm::animation::ConditionMode::Trigger)
                            c.Mode = cm::animation::ConditionMode::Trigger;
                    } break;
                }
            }
        }
    }
    json j; nlohmann::to_json(j, *m_Controller);
    std::ofstream out(path);
    if (!out) return false;
    out << j.dump(2);
    m_OpenPath = path;
    return true;
}

void AnimationControllerPanel::DrawToolbar() {
    if (ImGui::Button("New")) {
        m_Controller = std::make_shared<AnimatorController>();
        m_Controller->Name = "New Controller";
        m_Controller->DefaultState = -1;
        m_NextStateId = 1;
    }
    ImGui::SameLine();
    static char ctrlPathBuf[512] = {0};
    ImGui::SetNextItemWidth(240);
    ImGui::InputText("##ctrlPath", ctrlPathBuf, sizeof(ctrlPathBuf));
    ImGui::SameLine();
    if (ImGui::Button("Open")) {
        std::string p = ShowOpenFileDialogExt(L"Animation Controllers (*.animctrl)", L"animctrl");
        if (!p.empty()) { strncpy(ctrlPathBuf, p.c_str(), sizeof(ctrlPathBuf)); ctrlPathBuf[sizeof(ctrlPathBuf)-1]=0; Load(p); }
        else if (ctrlPathBuf[0]) Load(ctrlPathBuf); // fallback to typed path
    }
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        if (m_OpenPath.empty()) {
            std::string p = ShowSaveFileDialogExt(L"NewController.animctrl", L"Animation Controllers (*.animctrl)", L"animctrl");
            if (!p.empty()) { Save(p); }
        } else {
            Save(m_OpenPath);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Save As")) {
        std::wstring defNameW;
        if (ctrlPathBuf[0]) {
            int requiredW = MultiByteToWideChar(CP_UTF8, 0, ctrlPathBuf, -1, nullptr, 0);
            defNameW.assign((size_t)(requiredW > 0 ? requiredW - 1 : 0), L'\0');
            if (requiredW > 0) MultiByteToWideChar(CP_UTF8, 0, ctrlPathBuf, -1, defNameW.data(), requiredW);
        } else {
            defNameW = L"NewController.animctrl";
        }
        std::string p = ShowSaveFileDialogExt(defNameW.c_str(), L"Animation Controllers (*.animctrl)", L"animctrl");
        if (!p.empty()) { strncpy(ctrlPathBuf, p.c_str(), sizeof(ctrlPathBuf)); ctrlPathBuf[sizeof(ctrlPathBuf)-1]=0; Save(p); }
    }
}

void AnimationControllerPanel::DrawParameterList() {
    if (!m_Controller) return;
    ImGui::BeginChild("Params", ImVec2(220, 0), true);
    ImGui::TextUnformatted("Parameters");
    ImGui::Separator();
    for (size_t i = 0; i < m_Controller->Parameters.size(); ++i) {
        auto& p = m_Controller->Parameters[i];
        ImGui::PushID((int)i);
        char buf[128]; strncpy(buf, p.Name.c_str(), sizeof(buf)); buf[sizeof(buf)-1] = 0;
        ImGui::InputText("Name", buf, sizeof(buf)); p.Name = buf;
        const char* types[] = {"Bool","Int","Float","Trigger"};
        int t = (int)p.Type; if (ImGui::Combo("Type", &t, types, 4)) p.Type = (cm::animation::AnimatorParamType)t;
        ImGui::Separator();
        ImGui::PopID();
    }
    if (ImGui::Button("+ Add Parameter")) {
        AnimatorParameter p; p.Name = "Param" + std::to_string(m_Controller->Parameters.size()+1);
        m_Controller->Parameters.push_back(p);
    }
    ImGui::EndChild();
}

void AnimationControllerPanel::DrawNodeEditor() {
    if (!m_Controller) return;
    // Use a child without eating inputs globally
    ImGui::BeginChild("Graph", ImVec2(0, 0), true, ImGuiWindowFlags_NoNav);
    // Capture editor's top-left in screen space for coordinate conversions
    ImVec2 editorScreenOrigin = ImGui::GetCursorScreenPos();
    ImNodes::BeginNodeEditor();

    static int pendingNewStateId = -1;
    static ImVec2 pendingNewStateGridPos = ImVec2(0,0);

    // Context menu on grid
    if (ImNodes::IsEditorHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        ImGui::OpenPopup("AnimNodeContext");
    }
    if (ImGui::BeginPopup("AnimNodeContext")) {
        if (ImGui::BeginMenu("Create")) {
            if (ImGui::MenuItem("State")) {
                ImVec2 mouse = ImGui::GetMousePos();
                ImVec2 pan = ImNodes::EditorContextGetPanning();
                pendingNewStateGridPos = ImVec2(mouse.x - editorScreenOrigin.x - pan.x,
                                               mouse.y - editorScreenOrigin.y - pan.y);
                AnimatorState s; s.Id = m_NextStateId++; s.Name = "State" + std::to_string(s.Id);
                s.EditorPosX = 0.0f; s.EditorPosY = 0.0f;
                m_Controller->States.push_back(s);
                pendingNewStateId = s.Id;
                if (m_Controller->DefaultState < 0) m_Controller->DefaultState = s.Id;
            }
            if (ImGui::MenuItem("Blend1D")) {
                ImVec2 mouse = ImGui::GetMousePos();
                ImVec2 pan = ImNodes::EditorContextGetPanning();
                pendingNewStateGridPos = ImVec2(mouse.x - editorScreenOrigin.x - pan.x,
                                               mouse.y - editorScreenOrigin.y - pan.y);
                AnimatorState s; s.Id = m_NextStateId++; s.Name = "Blend1D" + std::to_string(s.Id);
                s.Kind = cm::animation::AnimatorStateKind::Blend1D;
                s.EditorPosX = 0.0f; s.EditorPosY = 0.0f;
                cm::animation::Blend1DEntry e0; e0.Key = 0.0f; s.Blend1DEntries.push_back(e0);
                cm::animation::Blend1DEntry e1; e1.Key = 1.0f; s.Blend1DEntries.push_back(e1);
                m_Controller->States.push_back(s);
                pendingNewStateId = s.Id;
                if (m_Controller->DefaultState < 0) m_Controller->DefaultState = s.Id;
            }
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }

    // Left-side inspectors for selected state/transition (persist across panes)
    int& selectedStateId = m_SelectedStateId;
    int& selectedLinkId = m_SelectedLinkId;

    // Nodes for states
    for (auto& s : m_Controller->States) {
        // If a new node is pending placement, set its position before drawing
        if (pendingNewStateId == s.Id) {
            ImNodes::SetNodeGridSpacePos(s.Id, pendingNewStateGridPos);
        }
        ImNodes::BeginNode(s.Id);
        ImNodes::BeginNodeTitleBar();
        // Editable name
        char nameBuf[128]; strncpy(nameBuf, s.Name.c_str(), sizeof(nameBuf)); nameBuf[sizeof(nameBuf)-1] = 0;
        if (ImGui::InputText("##name", nameBuf, sizeof(nameBuf))) s.Name = nameBuf;
        if (s.Kind == cm::animation::AnimatorStateKind::Blend1D) { ImGui::SameLine(); ImGui::TextDisabled("[Blend1D]"); }
        ImNodes::EndNodeTitleBar();

        ImNodes::BeginInputAttribute(s.Id * 1000 + 1);
        ImGui::Text("In");
        ImNodes::EndInputAttribute();

        ImNodes::BeginOutputAttribute(s.Id * 1000 + 2);
        ImGui::Text("Out");
        ImNodes::EndOutputAttribute();

        ImNodes::EndNode();
    }

    // Links for transitions
    for (const auto& t : m_Controller->Transitions) {
        int start = t.FromState < 0 ? 0 : t.FromState;
        int startSlot = start * 1000 + 2;
        int endSlot = t.ToState * 1000 + 1;
        int lid = (t.Id >= 0 ? t.Id : (t.FromState + 1) * 100000 + t.ToState + 1);
        ImNodes::Link(lid, startSlot, endSlot);
    }

    // Clear pending id after this frame; persist new position next block
    if (pendingNewStateId != -1) {
        pendingNewStateId = -1;
    }

    ImNodes::EndNodeEditor();

    // Persist positions of nodes (avoid setting them each frame)
    for (auto& s : m_Controller->States) {
        ImVec2 ep = ImNodes::GetNodeEditorSpacePos(s.Id);
        s.EditorPosX = ep.x;
        s.EditorPosY = ep.y;
    }

    // Create links by drag (MVP): query ImNodes
    int startAttr, endAttr;
    if (ImNodes::IsLinkCreated(&startAttr, &endAttr)) {
        auto decode = [](int attr)->int { return (attr/1000); };
        int from = decode(startAttr);
        int to = decode(endAttr);
        if (from != to && to >= 0) {
            AnimatorTransition tr; tr.FromState = from; tr.ToState = to; tr.HasExitTime = false; tr.Id = m_NextStateId++ * 1000 + (int)m_Controller->Transitions.size();
            m_Controller->Transitions.push_back(tr);
        }
    }

    // Selection
    int hoveredNode = -1, hoveredLink = -1;
    if (ImNodes::IsNodeHovered(&hoveredNode) && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        selectedStateId = hoveredNode;
        selectedLinkId = -1;
    }
    if (ImNodes::IsLinkHovered(&hoveredLink) && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        selectedLinkId = hoveredLink;
        selectedStateId = -1;
    }

    ImGui::EndChild();

    // Forward selection to Inspector when a state is clicked
    if (selectedStateId >= 0 && m_Inspector) {
        for (auto& s : m_Controller->States) if (s.Id == selectedStateId) {
            bool isDefault = (m_Controller->DefaultState == s.Id);
            InspectorPanel::AnimatorStateBinding binding;
            binding.Name = &s.Name;
            binding.ClipPath = &s.ClipPath;
            binding.AssetPath = &s.AnimationAssetPath;
            binding.Speed = &s.Speed;
            binding.Loop = &s.Loop;
            binding.IsDefault = isDefault;
            binding.MakeDefault = [this, &s]() { m_Controller->DefaultState = s.Id; };
            m_Inspector->SetAnimatorStateBinding(binding);
            break;
        }
    } else if (m_Inspector) {
        m_Inspector->ClearAnimatorBinding();
    }

    // Transition inspector no longer drawn here; moved to properties pane
    if (selectedLinkId >= 0) {
        for (auto& t : m_Controller->Transitions) {
            int lid = (t.Id >= 0 ? t.Id : (t.FromState + 1) * 100000 + t.ToState + 1);
            if (lid == selectedLinkId) {
                // Cache selection hit for properties pane rendering
                // Build parameter name list from controller
                static std::vector<std::string> paramNames;
                static std::vector<int> paramTypes; // 0=Bool,1=Int,2=Float,3=Trigger
                paramNames.clear(); paramTypes.clear();
                if (m_Controller) {
                    for (const auto& p : m_Controller->Parameters) {
                        paramNames.push_back(p.Name);
                        paramTypes.push_back((int)p.Type);
                    }
                }
                auto comboParam = [&](std::string& target){
                    int sel = -1; for (int i=0;i<(int)paramNames.size();++i) if (paramNames[i]==target) { sel=i; break; }
                    const char* label = sel>=0? paramNames[sel].c_str(): "<Param>";
                    if (ImGui::BeginCombo("Parameter", label)) {
                        for (int i=0;i<(int)paramNames.size();++i) {
                            bool isSel = (i==sel);
                            if (ImGui::Selectable(paramNames[i].c_str(), isSel)) { target = paramNames[i]; sel=i; }
                            if (isSel) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                    return sel;
                };

                for (size_t i = 0; i < t.Conditions.size(); ++i) {
                    auto& c = t.Conditions[i];
                    ImGui::PushID((int)i);
                    int sel = comboParam(c.Parameter);

                    // Mode options filtered by parameter type
                    const char* allModes[] = {"if","if_not","greater","less","equals","not_equals","trigger"};
                    int allCount = 7;
                    // Determine allowed indices
                    std::vector<int> allowed;
                    if (sel >= 0 && sel < (int)paramTypes.size()) {
                        int tcode = paramTypes[sel];
                        if (tcode == 0) { // Bool
                            allowed = {0,1};
                        } else if (tcode == 1 || tcode == 2) { // Int/Float
                            allowed = {2,3,4,5};
                        } else { // Trigger
                            allowed = {6};
                        }
                    } else {
                        // Fallback allow all
                        for (int k=0;k<allCount;++k) allowed.push_back(k);
                    }

                    // Map current mode to filtered index; coerce to a valid mode if needed
                    int curIdx = 0; int modeRaw = (int)cm::animation::ConditionMode::If;
                    switch (c.Mode) {
                        case cm::animation::ConditionMode::If: modeRaw=0; break;
                        case cm::animation::ConditionMode::IfNot: modeRaw=1; break;
                        case cm::animation::ConditionMode::Greater: modeRaw=2; break;
                        case cm::animation::ConditionMode::Less: modeRaw=3; break;
                        case cm::animation::ConditionMode::Equals: modeRaw=4; break;
                        case cm::animation::ConditionMode::NotEquals: modeRaw=5; break;
                        case cm::animation::ConditionMode::Trigger: modeRaw=6; break;
                    }
                    bool modeAllowed = std::find(allowed.begin(), allowed.end(), modeRaw) != allowed.end();
                    if (!modeAllowed && !allowed.empty()) {
                        c.Mode = (cm::animation::ConditionMode)allowed[0];
                        modeRaw = allowed[0];
                    }
                    for (int k=0;k<(int)allowed.size();++k) if (allowed[k]==modeRaw) { curIdx=k; break; }

                    // Build filtered labels
                    std::vector<const char*> labels; labels.reserve(allowed.size());
                    for (int idx : allowed) labels.push_back(allModes[idx]);
                    if (ImGui::Combo("Mode", &curIdx, labels.data(), (int)labels.size())) {
                        c.Mode = (cm::animation::ConditionMode)allowed[curIdx];
                    }

                    // Show thresholds based on type
                    if (sel >= 0 && sel < (int)paramTypes.size()) {
                        int tcode = paramTypes[sel];
                        if (tcode == 1) { ImGui::DragInt("Int Threshold", &c.IntThreshold); }
                        else if (tcode == 2) { ImGui::DragFloat("Threshold", &c.Threshold, 0.01f); }
                    } else {
                        // Unknown: show both for safety
                        ImGui::DragFloat("Threshold", &c.Threshold, 0.01f);
                        ImGui::DragInt("Int Threshold", &c.IntThreshold);
                    }

                    if (ImGui::Button("Remove")) { t.Conditions.erase(t.Conditions.begin()+i); ImGui::PopID(); break; }
                    ImGui::PopID();
                }
                if (ImGui::Button("+ Add Condition")) { t.Conditions.push_back({"", cm::animation::ConditionMode::If, 0.0f, 0}); }
                break;
            }
        }
    }
}

void AnimationControllerPanel::OnImGuiRender() {
    ImGui::Begin("Animation Controller");
    DrawToolbar();
    ImGui::Separator();

    if (!m_Controller) {
        ImGui::TextDisabled("No controller loaded. Click New or Open.");
        ImGui::End();
        return;
    }

    ImGui::Columns(3);
    // Column 1: Parameters list
    ImGui::SetColumnWidth(0, 240.0f);
    DrawParameterList();
    ImGui::NextColumn();
    // Column 2: Graph editor
    DrawNodeEditor();
    ImGui::NextColumn();
    // Column 3: Properties for selection (state or transition)
    DrawPropertiesPane();
    ImGui::Columns(1);

    ImGui::End();
}

void AnimationControllerPanel::DrawPropertiesPane() {
    if (!m_Controller) return;
    ImGui::BeginChild("Properties", ImVec2(0, 0), true);
    // State properties
    if (m_SelectedStateId >= 0) {
        for (auto& s : m_Controller->States) if (s.Id == m_SelectedStateId) {
            bool isDefault = (m_Controller->DefaultState == s.Id);
            ImGui::Text("State Properties");
            char nameBuf[128]; strncpy(nameBuf, s.Name.c_str(), sizeof(nameBuf)); nameBuf[sizeof(nameBuf)-1]=0;
            if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) s.Name = nameBuf;
            if (s.Kind == cm::animation::AnimatorStateKind::Blend1D) {
                // Blend parameter (float)
                int sel = -1; std::vector<std::string> floatParams;
                for (const auto& p : m_Controller->Parameters) if (p.Type == cm::animation::AnimatorParamType::Float) floatParams.push_back(p.Name);
                for (int i=0;i<(int)floatParams.size();++i) if (floatParams[i]==s.Blend1DParam) { sel=i; break; }
                const char* cur = sel>=0? floatParams[sel].c_str(): "<Float Param>";
                if (ImGui::BeginCombo("Blend Param", cur)) {
                    for (int i=0;i<(int)floatParams.size();++i) { bool isSel=(i==sel); if (ImGui::Selectable(floatParams[i].c_str(), isSel)) { s.Blend1DParam=floatParams[i]; sel=i; } if (isSel) ImGui::SetItemDefaultFocus(); }
                    ImGui::EndCombo();
                }
                ImGui::Separator(); ImGui::Text("Entries");
                for (size_t ei=0; ei<s.Blend1DEntries.size(); ++ei) {
                    auto& e = s.Blend1DEntries[ei]; ImGui::PushID((int)ei);
                    ImGui::DragFloat("Key", &e.Key, 0.01f, 0.0f, 1.0f);
                    struct AnimOption { std::string name; std::string path; }; std::vector<AnimOption> options;
                    auto root = Project::GetAssetDirectory(); if (root.empty()) root = std::filesystem::path("assets");
                    if (std::filesystem::exists(root)) { for (auto& p : std::filesystem::recursive_directory_iterator(root)) { if (!p.is_regular_file()) continue; auto ext = p.path().extension().string(); std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower); if (ext == ".anim") options.push_back({ p.path().stem().string(), p.path().string() }); } }
                    int sidx=-1; for (int i=0;i<(int)options.size();++i) if (options[i].path==e.AssetPath || options[i].path==e.ClipPath) { sidx=i; break; }
                    const char* lab = sidx>=0? options[sidx].name.c_str(): "<Select Clip>";
                    if (ImGui::BeginCombo("Clip", lab)) { for (int i=0;i<(int)options.size();++i) { bool isSel=(i==sidx); if (ImGui::Selectable(options[i].name.c_str(), isSel)) { sidx=i; e.AssetPath=options[i].path; e.ClipPath=options[i].path; } if (isSel) ImGui::SetItemDefaultFocus(); } ImGui::EndCombo(); }
                    ImGui::SameLine(); if (ImGui::Button("Remove")) { s.Blend1DEntries.erase(s.Blend1DEntries.begin()+ei); ImGui::PopID(); break; }
                    ImGui::PopID();
                }
                if (ImGui::Button("+ Add Entry")) { cm::animation::Blend1DEntry ne; ne.Key = 0.5f; s.Blend1DEntries.push_back(ne); }
                std::sort(s.Blend1DEntries.begin(), s.Blend1DEntries.end(), [](const auto& a, const auto& b){ return a.Key < b.Key; });
            } else {
                // Clip selection dropdown identical to Inspector
                static int selectedIndex = -1;
                struct AnimOption { std::string name; std::string path; };
                static std::vector<AnimOption> s_options;
                s_options.clear();
                auto root = Project::GetAssetDirectory();
                if (root.empty()) root = std::filesystem::path("assets");
                if (std::filesystem::exists(root)) {
                    for (auto& p : std::filesystem::recursive_directory_iterator(root)) {
                        if (!p.is_regular_file()) continue;
                        auto ext = p.path().extension().string();
                        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                        if (ext == ".anim") s_options.push_back({ p.path().stem().string(), p.path().string() });
                    }
                }
                selectedIndex = -1;
                for (int i=0;i<(int)s_options.size();++i) if (s_options[i].path == s.ClipPath) { selectedIndex = i; break; }
                const char* currentLabel = (selectedIndex >= 0 ? s_options[selectedIndex].name.c_str() : "<Select Clip>");
                if (ImGui::BeginCombo("Clip", currentLabel)) {
                    for (int i = 0; i < (int)s_options.size(); ++i) {
                        bool isSelected = (i == selectedIndex);
                        if (ImGui::Selectable(s_options[i].name.c_str(), isSelected)) {
                            selectedIndex = i;
                            s.ClipPath = s_options[i].path;
                            s.AnimationAssetPath = s_options[i].path; // mirror
                        }
                        if (isSelected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            }
            ImGui::DragFloat("Speed", &s.Speed, 0.01f, 0.0f, 10.0f);
            ImGui::Checkbox("Loop", &s.Loop);
            if (isDefault) ImGui::TextDisabled("(Default Entry)"); else if (ImGui::Button("Make Default")) m_Controller->DefaultState = s.Id;
            ImGui::EndChild();
            return;
        }
    }
    // Transition properties
    if (m_SelectedLinkId >= 0) {
        for (auto& t : m_Controller->Transitions) {
            int lid = (t.Id >= 0 ? t.Id : (t.FromState + 1) * 100000 + t.ToState + 1);
            if (lid == m_SelectedLinkId) {
                ImGui::Text("Transition Properties");
                ImGui::Checkbox("Has Exit Time", &t.HasExitTime);
                ImGui::DragFloat("Exit Time", &t.ExitTime, 0.01f, 0.0f, 1.0f);
                ImGui::DragFloat("Duration", &t.Duration, 0.01f, 0.0f, 5.0f);
                ImGui::Separator();
                ImGui::Text("Conditions");
                // Parameter lists
                std::vector<std::string> paramNames; std::vector<int> paramTypes;
                if (m_Controller) {
                    for (const auto& p : m_Controller->Parameters) { paramNames.push_back(p.Name); paramTypes.push_back((int)p.Type); }
                }
                auto comboParam = [&](std::string& target){
                    int sel=-1; for (int i=0;i<(int)paramNames.size();++i) if (paramNames[i]==target) { sel=i; break; }
                    const char* label = sel>=0? paramNames[sel].c_str(): "<Param>";
                    if (ImGui::BeginCombo("Parameter", label)) {
                        for (int i=0;i<(int)paramNames.size();++i) { bool isSel=(i==sel); if (ImGui::Selectable(paramNames[i].c_str(), isSel)) { target=paramNames[i]; sel=i; } if (isSel) ImGui::SetItemDefaultFocus(); }
                        ImGui::EndCombo();
                    }
                    return sel;
                };
                for (size_t i = 0; i < t.Conditions.size(); ++i) {
                    auto& c = t.Conditions[i];
                    ImGui::PushID((int)i);
                    int sel = comboParam(c.Parameter);
                    const char* allModes[] = {"if","if_not","greater","less","equals","not_equals","trigger"};
                    std::vector<int> allowed;
                    if (sel>=0 && sel<(int)paramTypes.size()) {
                        int tc=paramTypes[sel];
                        if (tc==0) allowed={0,1}; else if (tc==1||tc==2) allowed={2,3,4,5}; else allowed={6};
                    } else { allowed={0,1,2,3,4,5,6}; }
                    int modeRaw=(int)c.Mode; int curIdx=0; for (int k=0;k<(int)allowed.size();++k) if (allowed[k]==modeRaw) { curIdx=k; break; }
                    std::vector<const char*> labels; for (int idx:allowed) labels.push_back(allModes[idx]);
                    if (ImGui::Combo("Mode", &curIdx, labels.data(), (int)labels.size())) c.Mode=(cm::animation::ConditionMode)allowed[curIdx];
                    if (sel>=0 && sel<(int)paramTypes.size()) { int tc=paramTypes[sel]; if (tc==1) ImGui::DragInt("Int Threshold", &c.IntThreshold); else if (tc==2) ImGui::DragFloat("Threshold", &c.Threshold, 0.01f); }
                    else { ImGui::DragFloat("Threshold", &c.Threshold, 0.01f); ImGui::DragInt("Int Threshold", &c.IntThreshold); }
                    if (ImGui::Button("Remove")) { t.Conditions.erase(t.Conditions.begin()+i); ImGui::PopID(); break; }
                    ImGui::PopID();
                }
                if (ImGui::Button("+ Add Condition")) t.Conditions.push_back({"", cm::animation::ConditionMode::If, 0.0f, 0});
                ImGui::EndChild();
                return;
            }
        }
    }
    ImGui::TextDisabled("Select a state or transition to edit.");
    ImGui::EndChild();
}


