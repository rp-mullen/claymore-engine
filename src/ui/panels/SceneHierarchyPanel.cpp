#include "SceneHierarchyPanel.h"
#include <iostream>
#include "ecs/EntityData.h"
#include "rendering/TextureLoader.h"
#include <rendering/StandardMeshManager.h>
#include <rendering/MaterialManager.h>
#include <cstring>
#include <editor/Input.h>
#include "ui/utility/CreateEntityMenu.h"
#include "serialization/Serializer.h"
#include <editor/Project.h>
#include <filesystem>

SceneHierarchyPanel::SceneHierarchyPanel(Scene* scene, EntityID* selectedEntity)
   : m_SelectedEntity(selectedEntity) {
   SetContext(scene);
   }

void SceneHierarchyPanel::OnImGuiRender() {
   ImGui::Begin("Scene Hierarchy");
   DrawHierarchyContents();
   ImGui::End();
   }

void SceneHierarchyPanel::OnImGuiRenderEmbedded() {
   DrawHierarchyContents();
}

void SceneHierarchyPanel::DrawHierarchyContents() {
   if (!m_Context) {
      ImGui::Text("No scene loaded.");
      return;
      }

    EnsureIconsLoaded();

    // Draw root-level entities (those with no parent)
   for (auto& entity : m_Context->GetEntities()) {
      EntityData* data = m_Context->GetEntityData(entity.GetID());
      if (data && data->Parent == -1) {
         DrawEntityNode(entity);
         }
      }

    if (ImGui::Button("Add Entity")) {
      Entity newEntity = m_Context->CreateEntity("Empty");
      std::cout << "Added Entity: " << newEntity.GetName() << std::endl;
      }

    // Background context menu for hierarchy window (only when not over an item)
    if (ImGui::BeginPopupContextWindow("HierarchyBlankCtx", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        if (ImGui::BeginMenu("Create")) {
            extern bool DrawCreateEntityMenuItems(Scene* context, EntityID* selectedEntityOut);
            DrawCreateEntityMenuItems(m_Context, m_SelectedEntity);
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }

    // Delete key handling when the hierarchy window is focused and no text field is active
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
        && !ImGui::IsAnyItemActive()
        && m_SelectedEntity && *m_SelectedEntity != -1
        && Input::WasKeyPressedThisFrame(GLFW_KEY_DELETE)) {
        m_Context->QueueRemoveEntity(*m_SelectedEntity);
        *m_SelectedEntity = -1;
    }

    // Clear any one-shot expand target after drawing the list once
    m_ExpandTarget = -1;
}


void SceneHierarchyPanel::DrawEntityNode(const Entity& entity) {
   EntityID id = entity.GetID();
   EntityData* data = m_Context->GetEntityData(id);
   if (!data) return;

    ImGuiTreeNodeFlags flags = ((*m_SelectedEntity == id) ? ImGuiTreeNodeFlags_Selected : 0)
      | ImGuiTreeNodeFlags_OpenOnArrow
      | ImGuiTreeNodeFlags_SpanAvailWidth;

   bool hasChildren = !data->Children.empty();
   if (!hasChildren)
      flags |= ImGuiTreeNodeFlags_Leaf;

   // Highlight background for selected entity row
   if (*m_SelectedEntity == id) {
       ImU32 bg = ImGui::GetColorU32(ImGuiCol_HeaderActive);
       ImVec2 start = ImGui::GetCursorScreenPos();
       ImVec2 end   = ImVec2(start.x + ImGui::GetContentRegionAvail().x, start.y + ImGui::GetTextLineHeightWithSpacing());
       ImGui::GetWindowDrawList()->AddRectFilled(start, end, bg, 4.0f);
   }

    // Layout: [visibility button] [tree node label]
    ImGui::PushID((int)id);
    ImVec2 iconSize(16, 16);
    ImTextureID icon = data->Visible ? m_VisibleIcon : m_NotVisibleIcon;
    // Transparent ImageButton for visibility (no background)
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1,1,1,0.15f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1,1,1,0.25f));
    if (ImGui::ImageButton("##vis", icon, iconSize)) {
        data->Visible = !data->Visible;
    }
    ImGui::PopStyleColor(3);
    ImGui::SameLine();
    // Name or rename field
    bool opened = false;
    bool hasTreePush = true;
    if (m_RenamingEntity == id) {
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
        ImGui::SetKeyboardFocusHere();
        if (ImGui::InputText("##rename", m_RenameBuffer, IM_ARRAYSIZE(m_RenameBuffer), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
            std::string desired = m_RenameBuffer;
            if (desired.empty()) desired = "Entity";
            // ensure unique name within scene
            int suffix = 1;
            std::string finalName = desired;
            bool unique = false;
            while (!unique) {
                unique = true;
                for (const auto& e : m_Context->GetEntities()) {
                    if (e.GetID() == id) continue;
                    auto* ed = m_Context->GetEntityData(e.GetID());
                    if (ed && ed->Name == finalName) { unique = false; break; }
                }
                if (!unique) { finalName = desired + "_" + std::to_string(suffix++); }
            }
            data->Name = finalName;
            m_RenamingEntity = -1;
        }
        // Exit rename on click outside or escape
        if (!ImGui::IsItemActive() && ImGui::IsMouseClicked(0)) m_RenamingEntity = -1;
        opened = ImGui::TreeNodeEx((void*)(intptr_t)id, flags | ImGuiTreeNodeFlags_NoTreePushOnOpen, "%s", "");
        hasTreePush = false;
    } else {
        // If we have a pending expand target and this node is on the path to it, force it open
        if (m_ExpandTarget != -1) {
            EntityID cur = m_ExpandTarget;
            while (cur != -1) {
                if (cur == id) { ImGui::SetNextItemOpen(true); break; }
                auto* d2 = m_Context->GetEntityData(cur);
                if (!d2) break;
                cur = d2->Parent;
            }
        }
        opened = ImGui::TreeNodeEx((void*)(intptr_t)id, flags, "%s", entity.GetName().c_str());
        // TreeNodeEx pushes only when it returns true
        hasTreePush = opened;
    }

    // Selection: single click selects, but don't change selection when this click starts a drag
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        m_PendingSelect = id;
    }

    // Double-click to rename
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
        m_RenamingEntity = id;
        strncpy(m_RenameBuffer, data->Name.c_str(), sizeof(m_RenameBuffer));
        m_RenameBuffer[sizeof(m_RenameBuffer)-1] = 0;
    }

    // Context menu
   bool entityDeleted = false;
   if (ImGui::BeginPopupContextItem()) {
      if (ImGui::MenuItem("Rename")) {
         m_RenamingEntity = id;
         strncpy(m_RenameBuffer, data->Name.c_str(), sizeof(m_RenameBuffer));
         m_RenameBuffer[sizeof(m_RenameBuffer)-1] = 0;
         }
      if (ImGui::MenuItem("Duplicate")) {
         // TODO: implement duplication
         }
      if (ImGui::MenuItem("Delete")) {
         // Defer deletion to avoid invalidation during render
         m_Context->QueueRemoveEntity(id);
         if (*m_SelectedEntity == id)
            *m_SelectedEntity = -1;
         entityDeleted = true;
         }
      if (ImGui::MenuItem("Convert to Prefab")) {
         namespace fs = std::filesystem;
         fs::path folder = Project::GetProjectDirectory() / "assets/prefabs";
         std::error_code ec; fs::create_directories(folder, ec);
         auto sanitize = [](std::string s){
            const std::string invalid = "<>:\"/\\|?*";
            for (char& c : s) if (invalid.find(c) != std::string::npos) c = '_';
            size_t start = s.find_first_not_of(' ');
            size_t end = s.find_last_not_of(' ');
            if (start == std::string::npos) return std::string("Prefab");
            return s.substr(start, end - start + 1);
         };
         std::string desired = sanitize(data->Name.empty() ? std::string("Prefab") : data->Name);
         if (desired.empty()) desired = "Prefab";
         fs::path out = folder / (desired + ".prefab");
         int counter = 1;
         while (fs::exists(out)) out = folder / (desired + "_" + std::to_string(counter++) + ".prefab");
         if (Serializer::SavePrefabSubtreeToFile(*m_Context, id, out.string())) {
            std::cout << "[Hierarchy] Prefab saved: " << out.string() << std::endl;
         } else {
            std::cerr << "[Hierarchy] Failed to save prefab: " << out.string() << std::endl;
         }
      }
      // No Create submenu on entity context menu
      ImGui::EndPopup();
      }

    // If entity was deleted, close the tree node if it was opened and return
    if (entityDeleted) {
      if (opened) {
         ImGui::TreePop();
         }
       ImGui::PopID();
      return;
      }

    // Drag source (keep current selection unchanged while dragging)
    if (ImGui::BeginDragDropSource()) {
       // Cancel any pending selection when a drag actually starts
       if (m_PendingSelect == id) m_PendingSelect = -1;
      ImGui::SetDragDropPayload("ENTITY_ID", &id, sizeof(EntityID));
      ImGui::Text("Drag %s", entity.GetName().c_str());
      ImGui::EndDragDropSource();
      }

    // If a pending select exists and we are not dragging, commit selection on mouse release
    if (m_PendingSelect == id && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        *m_SelectedEntity = id;
        m_PendingSelect = -1;
    }

   // Drop target
   if (ImGui::BeginDragDropTarget()) {
      if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_ID")) {
         EntityID draggedID = *(EntityID*)payload->Data;
         if (draggedID != id)
            m_Context->SetParent(draggedID, id);
         }
      ImGui::EndDragDropTarget();
      }

   // Children
    if (opened && hasTreePush) {
      for (EntityID childID : data->Children) {
         DrawEntityNode(m_Context->FindEntityByID(childID));
         }
      ImGui::TreePop();
      }

    ImGui::PopID();
   }

void SceneHierarchyPanel::ExpandTo(EntityID id) {
    m_ExpandTarget = id;
}

void SceneHierarchyPanel::EnsureIconsLoaded() {
    if (m_IconsLoaded) return;
    m_VisibleIcon    = TextureLoader::ToImGuiTextureID(TextureLoader::LoadIconTexture("assets/icons/visible.svg"));
    m_NotVisibleIcon = TextureLoader::ToImGuiTextureID(TextureLoader::LoadIconTexture("assets/icons/not_visible.svg"));
    m_IconsLoaded = true;
}

