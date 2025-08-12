#include "AnimationControllerPanel.h"
#include <imnodes.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include "animation/AnimatorController.h"

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
        if (ctrlPathBuf[0]) Load(ctrlPathBuf);
    }
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        if (!m_OpenPath.empty()) Save(m_OpenPath);
    }
    ImGui::SameLine();
    if (ImGui::Button("Save As")) {
        if (ctrlPathBuf[0]) { m_OpenPath = ctrlPathBuf; Save(m_OpenPath); }
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
        if (ImGui::MenuItem("Add State")) {
            ImVec2 mouse = ImGui::GetMousePos();
            // Convert mouse screen-space to grid-space using editor origin and panning
            ImVec2 pan = ImNodes::EditorContextGetPanning();
            pendingNewStateGridPos = ImVec2(mouse.x - editorScreenOrigin.x - pan.x,
                                           mouse.y - editorScreenOrigin.y - pan.y);
            AnimatorState s; s.Id = m_NextStateId++; s.Name = "State" + std::to_string(s.Id);
            s.EditorPosX = 0.0f; s.EditorPosY = 0.0f; // will set precise position below
            m_Controller->States.push_back(s);
            pendingNewStateId = s.Id;
            if (m_Controller->DefaultState < 0) m_Controller->DefaultState = s.Id;
        }
        ImGui::EndPopup();
    }

    // Left-side inspectors for selected state/transition
    static int selectedStateId = -1;
    static int selectedLinkId = -1;

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

    // Transition inspector drawn below graph for now
    ImGui::Separator();
    if (selectedLinkId >= 0) {
        for (auto& t : m_Controller->Transitions) {
            int lid = (t.Id >= 0 ? t.Id : (t.FromState + 1) * 100000 + t.ToState + 1);
            if (lid == selectedLinkId) {
                ImGui::Text("Transition Properties");
                ImGui::Checkbox("Has Exit Time", &t.HasExitTime);
                ImGui::DragFloat("Exit Time", &t.ExitTime, 0.01f, 0.0f, 1.0f);
                ImGui::DragFloat("Duration", &t.Duration, 0.01f, 0.0f, 5.0f);
                ImGui::Separator();
                ImGui::Text("Conditions");
                for (size_t i = 0; i < t.Conditions.size(); ++i) {
                    auto& c = t.Conditions[i];
                    ImGui::PushID((int)i);
                    char pname[128]; strncpy(pname, c.Parameter.c_str(), sizeof(pname)); pname[sizeof(pname)-1]=0;
                    if (ImGui::InputText("Param", pname, sizeof(pname))) c.Parameter = pname;
                    const char* modes[] = {"if","if_not","greater","less","equals","not_equals","trigger"};
                    int mode = (int)cm::animation::ConditionMode::If;
                    switch (c.Mode) {
                        case cm::animation::ConditionMode::If: mode=0; break;
                        case cm::animation::ConditionMode::IfNot: mode=1; break;
                        case cm::animation::ConditionMode::Greater: mode=2; break;
                        case cm::animation::ConditionMode::Less: mode=3; break;
                        case cm::animation::ConditionMode::Equals: mode=4; break;
                        case cm::animation::ConditionMode::NotEquals: mode=5; break;
                        case cm::animation::ConditionMode::Trigger: mode=6; break;
                    }
                    if (ImGui::Combo("Mode", &mode, modes, 7)) {
                        c.Mode = (cm::animation::ConditionMode)mode;
                    }
                    ImGui::DragFloat("Threshold", &c.Threshold, 0.01f);
                    ImGui::DragInt("Int Threshold", &c.IntThreshold);
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

    ImGui::Columns(2);
    DrawParameterList();
    ImGui::NextColumn();
    DrawNodeEditor();
    ImGui::Columns(1);

    ImGui::End();
}


