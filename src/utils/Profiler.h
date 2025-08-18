#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <cstdint>

// Lightweight per-frame CPU profiler and memory sampler for the editor
class Profiler {
public:
	struct Entry {
		std::string name;
		double totalMs = 0.0;   // sum for this frame
		uint32_t callCount = 0; // calls this frame
	};

	struct MemoryStats {
		uint64_t workingSetBytes = 0;   // Resident Set Size (private + shareable)
		uint64_t privateBytes    = 0;   // Private bytes/commit
	};

	static Profiler& Get();

	void SetEnabled(bool enabled);
	bool IsEnabled() const;

	// Called once per frame at the very beginning of the loop
	void BeginFrame();
	// Called once per frame near the end of the loop
	void EndFrame();

	// Record a completed timing sample (in milliseconds)
	void Record(const std::string& name, double durationMs);

	// Convenience for script timings
	void RecordScriptSample(const std::string& scriptClassName, double durationMs);

	// Current frame entries (unsorted)
	const std::unordered_map<std::string, Entry>& GetEntries() const;
	// Last completed frame entries (unsorted)
	const std::unordered_map<std::string, Entry>& GetLastFrameEntries() const;

	// Sorted copy of current frame entries by totalMs desc
	std::vector<Entry> GetSortedEntriesByTimeDesc() const;
	std::vector<Entry> GetSortedLastFrameEntriesByTimeDesc() const;

	// Process memory at the moment of the call
	MemoryStats GetProcessMemory() const;

private:
	Profiler() = default;
	std::unordered_map<std::string, Entry> m_CurrentEntries;
	std::unordered_map<std::string, Entry> m_LastEntries;
	bool m_Enabled = true;
};

// RAII helper to time a scope and submit to Profiler on destruction
class ScopedTimer {
public:
	explicit ScopedTimer(const char* label)
		: m_Label(label), m_Start(std::chrono::high_resolution_clock::now()) {}
	~ScopedTimer();

private:
	std::string m_Label;
	std::chrono::high_resolution_clock::time_point m_Start;
};


