#include "Profiler.h"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#endif

#include <algorithm>

Profiler& Profiler::Get() {
	static Profiler instance;
	return instance;
}

void Profiler::SetEnabled(bool enabled) { m_Enabled = enabled; }
bool Profiler::IsEnabled() const { return m_Enabled; }

void Profiler::BeginFrame() {
	if (!m_Enabled) return;
	m_CurrentEntries.clear();
}

void Profiler::EndFrame() {
	if (!m_Enabled) return;
	m_LastEntries = m_CurrentEntries;
}

void Profiler::Record(const std::string& name, double durationMs) {
	if (!m_Enabled) return;
	Entry& e = m_CurrentEntries[name];
	if (e.name.empty()) e.name = name;
	e.totalMs += durationMs;
	e.callCount += 1;
}

void Profiler::RecordScriptSample(const std::string& scriptClassName, double durationMs) {
	Record(std::string("Script/") + scriptClassName, durationMs);
}

const std::unordered_map<std::string, Profiler::Entry>& Profiler::GetEntries() const {
	return m_CurrentEntries;
}

std::vector<Profiler::Entry> Profiler::GetSortedEntriesByTimeDesc() const {
	std::vector<Entry> list;
	list.reserve(m_CurrentEntries.size());
	for (const auto& kv : m_CurrentEntries) list.push_back(kv.second);
	std::sort(list.begin(), list.end(), [](const Entry& a, const Entry& b){ return a.totalMs > b.totalMs; });
	return list;
}

const std::unordered_map<std::string, Profiler::Entry>& Profiler::GetLastFrameEntries() const {
	return m_LastEntries.empty() ? m_CurrentEntries : m_LastEntries;
}

std::vector<Profiler::Entry> Profiler::GetSortedLastFrameEntriesByTimeDesc() const {
	const auto& src = m_LastEntries.empty() ? m_CurrentEntries : m_LastEntries;
	std::vector<Entry> list;
	list.reserve(src.size());
	for (const auto& kv : src) list.push_back(kv.second);
	std::sort(list.begin(), list.end(), [](const Entry& a, const Entry& b){ return a.totalMs > b.totalMs; });
	return list;
}

Profiler::MemoryStats Profiler::GetProcessMemory() const {
	MemoryStats m{};
#if defined(_WIN32)
	PROCESS_MEMORY_COUNTERS_EX pmc{};
	pmc.cb = sizeof(pmc);
	if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PPROCESS_MEMORY_COUNTERS>(&pmc), sizeof(pmc))) {
		m.workingSetBytes = static_cast<uint64_t>(pmc.WorkingSetSize);
		m.privateBytes    = static_cast<uint64_t>(pmc.PrivateUsage);
	}
#endif
	return m;
}

ScopedTimer::~ScopedTimer() {
	auto end = std::chrono::high_resolution_clock::now();
	double ms = std::chrono::duration<double, std::milli>(end - m_Start).count();
	Profiler::Get().Record(m_Label, ms);
}


