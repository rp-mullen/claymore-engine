#pragma once
#include <string>
#include <queue>
#include <mutex>
#include <functional>
#include <vector>
#include "AssetMetadata.h"
#include "AssetRegistry.h"
#include <rendering/ShaderManager.h>
#include <bgfx/bgfx.h>

// ---------------------------
// GPU Upload Job Struct
// ---------------------------
struct PendingGPUUpload {
    enum class Type { Texture, Model, Shader };
    Type type;

    std::string sourcePath;

    // Texture
    std::vector<uint8_t> pixelData;
    int width = 0, height = 0;
    bgfx::TextureFormat::Enum format = bgfx::TextureFormat::RGBA8;

    // Model
    std::vector<float> vertices;
    std::vector<uint16_t> indices;

    // Shader
    std::string compiledShaderPath;
    ShaderType shaderType;

    // Upload callback
    std::function<void()> Upload;
};

class AssetPipeline {
public:

	static AssetPipeline& Instance() {
		static AssetPipeline instance;
		return instance;
	}

    AssetPipeline() = default;

    // File scanning and import queue
    void ScanProject(const std::string& rootPath);
    void EnqueueAssetImport(const std::string& path);

    // Main-thread execution
    void ProcessMainThreadTasks();
    void ProcessGPUUploads();
    void EnqueueMainThreadTask(std::function<void()> task);
    void EnqueueGPUUpload(PendingGPUUpload&& task);

    // Asset importing
    void ImportAsset(const std::string& path);
    void ImportModel(const std::string& path);
    void ImportTexture(const std::string& path);
    void ImportShader(const std::string& path);

    void ImportScript(const std::string& path);

    // CPU-side texture pre-process for async
    void ImportTextureCPU(const std::string& path);

    // Utility helpers
    bool IsSupportedAsset(const std::string& ext) const;
    std::string DetermineType(const std::string& ext);
    std::string ComputeHash(const std::string& path) const;
    std::string ComputeFileHash(const std::string& path);
    std::string GetCurrentTimestamp() const;

    void CheckAndCompileScriptsAtStartup();

    const AssetMetadata* GetMetadata(const std::string& path) const {
        return AssetRegistry::Instance().GetMetadata(path);
    }

private:
    // Queues
    std::queue<std::string> m_ImportQueue;
    std::mutex m_QueueMutex;

    std::queue<std::function<void()>> m_MainThreadTasks;
    std::mutex m_MainThreadQueueMutex;

    std::queue<PendingGPUUpload> m_GPUUploadQueue;
    std::mutex m_GPUQueueMutex;
};
