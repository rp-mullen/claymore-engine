#include "SceneHierarchyPanel.h"
#include <iostream>
#include "ecs/EntityData.h"
#include "rendering/TextureLoader.h"
#include <rendering/StandardMeshManager.h>
#include <rendering/MaterialManager.h>
#include <cstring>

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

    // Background context menu for hierarchy window
    if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_MouseButtonRight)) {
        if (ImGui::BeginMenu("Create")) {
            if (ImGui::MenuItem("Empty")) {
                auto e = m_Context->CreateEntity("Empty Entity");
                *m_SelectedEntity = e.GetID();
            }
            if (ImGui::MenuItem("Camera")) {
                auto e = m_Context->CreateEntity("Camera");
                if (auto* ed = m_Context->GetEntityData(e.GetID())) {
                    ed->Camera = std::make_unique<CameraComponent>();
                }
                *m_SelectedEntity = e.GetID();
            }
            if (ImGui::MenuItem("Cube")) {
                auto e = m_Context->CreateEntity("Cube");
                if (auto* ed = m_Context->GetEntityData(e.GetID())) {
                    ed->Mesh = std::make_unique<MeshComponent>();
                    ed->Mesh->mesh = StandardMeshManager::Instance().GetCubeMesh();
                    ed->Mesh->material = MaterialManager::Instance().CreateDefaultPBRMaterial();
                    ed->Mesh->MeshName = "Cube";
                }
                *m_SelectedEntity = e.GetID();
            }
            if (ImGui::MenuItem("Sphere")) {
                auto e = m_Context->CreateEntity("Sphere");
                if (auto* ed = m_Context->GetEntityData(e.GetID())) {
                    ed->Mesh = std::make_unique<MeshComponent>();
                    ed->Mesh->mesh = StandardMeshManager::Instance().GetSphereMesh();
                    ed->Mesh->material = MaterialManager::Instance().CreateDefaultPBRMaterial();
                    ed->Mesh->MeshName = "Sphere";
                }
                *m_SelectedEntity = e.GetID();
            }
            if (ImGui::MenuItem("Plane")) {
                auto e = m_Context->CreateEntity("Plane");
                if (auto* ed = m_Context->GetEntityData(e.GetID())) {
                    ed->Mesh = std::make_unique<MeshComponent>();
                    ed->Mesh->mesh = StandardMeshManager::Instance().GetPlaneMesh();
                    ed->Mesh->material = MaterialManager::Instance().CreateDefaultPBRMaterial();
                    ed->Mesh->MeshName = "Plane";
                }
                *m_SelectedEntity = e.GetID();
            }
            if (ImGui::BeginMenu("Light")) {
                if (ImGui::MenuItem("Directional")) {
                    auto e = m_Context->CreateEntity("Directional Light");
                    if (auto* ed = m_Context->GetEntityData(e.GetID())) {
                        ed->Light = std::make_unique<LightComponent>(LightType::Directional, glm::vec3(1.0f), 1.0f);
                    }
                    *m_SelectedEntity = e.GetID();
                }
                if (ImGui::MenuItem("Point")) {
                    auto e = m_Context->CreateEntity("Point Light");
                    if (auto* ed = m_Context->GetEntityData(e.GetID())) {
                        ed->Light = std::make_unique<LightComponent>(LightType::Point, glm::vec3(1.0f), 1.0f);
                    }
                    *m_SelectedEntity = e.GetID();
                }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Terrain")) {
                auto e = m_Context->CreateEntity("Terrain");
                if (auto* ed = m_Context->GetEntityData(e.GetID())) {
                    ed->Terrain = std::make_unique<TerrainComponent>();
                    ed->Transform.Position = glm::vec3(-0.5f * ed->Terrain->Size, 0.0f, -0.5f * ed->Terrain->Size);
                }
                *m_SelectedEntity = e.GetID();
            }
            if (ImGui::MenuItem("Particle Emitter")) {
                auto e = m_Context->CreateEntity("Particle Emitter");
                if (auto* ed = m_Context->GetEntityData(e.GetID())) {
                    ed->Emitter = std::make_unique<ParticleEmitterComponent>();
                }
                *m_SelectedEntity = e.GetID();
            }
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }
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
        opened = ImGui::TreeNodeEx((void*)(intptr_t)id, flags, "%s", entity.GetName().c_str());
        // TreeNodeEx pushes only when it returns true
        hasTreePush = opened;
    }

   // Selection
    if (ImGui::IsItemClicked()) {
      *m_SelectedEntity = id;
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
    if (opened && hasTreePush) {
      for (EntityID childID : data->Children) {
         DrawEntityNode(m_Context->FindEntityByID(childID));
         }
      ImGui::TreePop();
      }

    ImGui::PopID();
   }

void SceneHierarchyPanel::EnsureIconsLoaded() {
    if (m_IconsLoaded) return;
    m_VisibleIcon    = TextureLoader::ToImGuiTextureID(TextureLoader::LoadIconTexture("assets/icons/visible.svg"));
    m_NotVisibleIcon = TextureLoader::ToImGuiTextureID(TextureLoader::LoadIconTexture("assets/icons/not_visible.svg"));
    m_IconsLoaded = true;
}

