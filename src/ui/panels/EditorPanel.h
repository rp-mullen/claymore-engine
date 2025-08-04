#pragma once
#include "ecs/Scene.h"

class EditorPanel {

public:
   void SetContext(Scene* context) {
      m_Context = context;
      }
   Scene* m_Context = nullptr;
   };