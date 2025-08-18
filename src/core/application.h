#pragma once
#include <string>
#include <GLFW/glfw3.h>
#include <memory>
#include "ui/UILayer.h"
#include "pipeline/AssetWatcher.h"
#include "pipeline/AssetPipeline.h"
#include "jobs/JobSystem.h"
#include "ecs/Scene.h"

class Application {
public:
    Application(int width, int height, const std::string& title); 
    ~Application();

    static Application& Get();

    JobSystem& Jobs() { return *m_Jobs; }
    AssetWatcher* GetAssetWatcher() const { return m_AssetWatcher.get(); }
    AssetPipeline* GetAssetPipeline() const { return m_AssetPipeline.get(); }

    void Run();

    void StartPlayMode();
    void StopPlayMode();
    bool IsPlaying() const { return m_IsPlaying; }

    std::filesystem::path defaultProjPath;
    bool m_RunEditorUI = true;
private:
    static Application* s_Instance;

    std::unique_ptr<JobSystem> m_Jobs;

    void InitWindow(int width, int height, const std::string& title);
    void InitBgfx();
    void Shutdown();
    void InitImGui();

    std::shared_ptr<Scene> m_EditScene;
    std::shared_ptr<Scene> m_RuntimeScene;
    bool m_IsPlaying = false;

    GLFWwindow* m_window;
    int m_width;
    int m_height;
    std::unique_ptr<UILayer> uiLayer;
	std::unique_ptr<AssetPipeline> m_AssetPipeline;
	std::unique_ptr<AssetWatcher> m_AssetWatcher;

    // Runtime without editor UI
    
    std::unique_ptr<Scene> m_GameScene;
};
