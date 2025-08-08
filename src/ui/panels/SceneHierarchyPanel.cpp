#include "SceneHierarchyPanel.h"
#include <iostream>
#include "ecs/EntityData.h"

SceneHierarchyPanel::SceneHierarchyPanel(Scene* scene, EntityID* selectedEntity)
   : m_SelectedEntity(selectedEntity) {
   SetContext(scene);
   }

void SceneHierarchyPanel::OnImGuiRender() {
   ImGui::Begin("Scene Hierarchy");

   if (!m_Context) {
      ImGui::Text("No scene loaded.");
      ImGui::End();
      return;
      }

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

   ImGui::End();
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

   bool opened = ImGui::TreeNodeEx((void*)(intptr_t)id, flags, "%s", entity.GetName().c_str());

   // Selection
   if (ImGui::IsItemClicked()) {
      *m_SelectedEntity = id;
      }

   // Context menu
   bool entityDeleted = false;
   if (ImGui::BeginPopupContextItem()) {
      if (ImGui::MenuItem("Rename")) {
         // TODO: implement rename UI
         }
      if (ImGui::MenuItem("Duplicate")) {
         // TODO: implement duplication
         }
      if (ImGui::MenuItem("Delete")) {
         m_Context->RemoveEntity(id);
         if (*m_SelectedEntity == id)
            *m_SelectedEntity = -1;
         entityDeleted = true;
         }
      ImGui::EndPopup();
      }

   // If entity was deleted, close the tree node if it was opened and return
   if (entityDeleted) {
      if (opened) {
         ImGui::TreePop();
         }
      return;
      }

   // Drag source
   if (ImGui::BeginDragDropSource()) {
      ImGui::SetDragDropPayload("ENTITY_ID", &id, sizeof(EntityID));
      ImGui::Text("Drag %s", entity.GetName().c_str());
      ImGui::EndDragDropSource();
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
   if (opened) {
      for (EntityID childID : data->Children) {
         DrawEntityNode(m_Context->FindEntityByID(childID));
         }
      ImGui::TreePop();
      }
   }

