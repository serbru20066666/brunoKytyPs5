#include "graphics/host_gpu/renderer/pipeline/pipelineLibrary.h"

#include "common/assert.h"
#include "common/logging/log.h"
#include "common/profiler.h"
#include "graphics/host_gpu/graphicContext.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>

#if KYTY_PLATFORM == KYTY_PLATFORM_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace Libs::Graphics {

namespace {

// Prefetched parts compile while Thread_Gpu is stalled on a loading burst's first pipeline; six
// threads leave the game two cores of an 8-core CPU during loads, when it needs little.
constexpr uint32_t CompileThreadCount = 6;

// Background compiles run below the game's priority: while the game runs (with asynchronous
// pipelines, or during play), they use spare cores instead of slowing its frames. The link thread
// keeps normal priority: until it relinks a pipeline with link-time optimization, draws run the
// slower fast-linked one.
void LowerThreadPriority() {
#if KYTY_PLATFORM == KYTY_PLATFORM_WINDOWS
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
#endif
}

int64_t SteadyNowNs() {
	return std::chrono::duration_cast<std::chrono::nanoseconds>(
	           std::chrono::steady_clock::now().time_since_epoch())
	    .count();
}

// How long the draw path must build no pipeline before an optimized link starts.
// KYTY_LTO_QUIET_MS=<ms> changes it (0 links at once, as before); KYTY_NO_LTO=1 never relinks.
int64_t LinkQuietNs() {
	static const int64_t quiet = [] {
		const char* text = std::getenv("KYTY_LTO_QUIET_MS");
		const auto  ms   = text != nullptr ? std::strtoll(text, nullptr, 10) : 2500;
		return std::max<int64_t>(ms, 0) * 1000000;
	}();
	return quiet;
}

} // namespace

PipelineLibraryCache::PipelineLibraryCache(GraphicContext& graphics, vk::PipelineCache driver_cache)
    : m_graphics(graphics), m_driver_cache(driver_cache), m_running_keys(CompileThreadCount),
      m_link_thread([this](const std::stop_token& stop) { LinkThread(stop); }) {
	for (size_t i = 0; i < CompileThreadCount; i++) {
		m_compile_threads.emplace_back(
		    [this, i](const std::stop_token& stop) { CompileThread(stop, i); });
	}
}

void PipelineLibraryCache::NoteForegroundLink() {
	m_last_foreground_ns.store(SteadyNowNs(), std::memory_order_relaxed);
}

void PipelineLibraryCache::BoostRunning(const std::string& key) {
#if KYTY_PLATFORM == KYTY_PLATFORM_WINDOWS
	std::lock_guard lock(m_compile_mutex);
	for (size_t i = 0; i < m_running_keys.size() && i < m_compile_threads.size(); i++) {
		if (m_running_keys[i] == key) {
			// The worker lowers itself again before its next job.
			SetThreadPriority(static_cast<HANDLE>(m_compile_threads[i].native_handle()),
			                  THREAD_PRIORITY_ABOVE_NORMAL);
			return;
		}
	}
#else
	(void)key;
#endif
}

PipelineLibraryCache::~PipelineLibraryCache() {
	Stop();
	const auto& device = m_graphics.device;
	for (const auto& [target, pipeline]: m_optimized) {
		(void)target;
		if (pipeline != nullptr) {
			device.destroyPipeline(pipeline, nullptr);
		}
	}
	for (auto& [key, entry]: m_libraries) {
		(void)key;
		// Stop resolved every queued job, and the threads are joined.
		const auto pipeline = entry.pending.valid() ? entry.pending.get() : entry.pipeline;
		if (pipeline != nullptr) {
			device.destroyPipeline(pipeline, nullptr);
		}
	}
	for (const auto layout: m_kept_layouts) {
		device.destroyPipelineLayout(layout, nullptr);
	}
	for (const auto set_layout: m_kept_set_layouts) {
		device.destroyDescriptorSetLayout(set_layout, nullptr);
	}
}

PipelineLibraryCache::Found PipelineLibraryCache::Find(const std::string& key) {
	const auto iter = m_libraries.find(key);
	if (iter == m_libraries.end()) {
		return {};
	}
	auto& entry = iter->second;
	if (entry.pending.valid()) {
		KYTY_PROFILER_BLOCK("PipelineLibraryCache::WaitForPrefetch");
		if (entry.pending.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
			RunIfQueued(key);
			// A below-normal worker loses its core to the game's threads while the draw path
			// waits for it.
			if (entry.pending.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
				BoostRunning(key);
			}
		}
		entry.pipeline = entry.pending.get();
		entry.pending  = {};
		if (entry.pipeline == nullptr) {
			// The prefetch failed or was dropped; the caller compiles the part itself.
			m_libraries.erase(iter);
			return {};
		}
	}
	return {entry.pipeline, entry.prefetched};
}

bool PipelineLibraryCache::Ready(const std::string& key) const {
	const auto iter = m_libraries.find(key);
	if (iter == m_libraries.end()) {
		return false;
	}
	const auto& pending = iter->second.pending;
	return !pending.valid() || pending.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

vk::Pipeline PipelineLibraryCache::Take(const std::string& key) {
	const auto found = Find(key);
	m_libraries.erase(key);
	return found.pipeline;
}

vk::Pipeline PipelineLibraryCache::Insert(std::string key, vk::Pipeline library) {
	EXIT_IF(library == nullptr);
	const auto [iter, inserted] = m_libraries.emplace(std::move(key), Entry {library, {}, false});
	EXIT_IF(!inserted);
	return iter->second.pipeline;
}

bool PipelineLibraryCache::Prefetch(std::string key, Common::UniqueFunction<vk::Pipeline>&& compile,
                                    bool urgent) {
	if (m_libraries.contains(key)) {
		return false;
	}
	CompileJob job;
	job.key     = key;
	job.compile = std::move(compile);
	Entry entry;
	entry.pending    = job.result.get_future().share();
	entry.prefetched = true;
	{
		std::lock_guard lock(m_compile_mutex);
		if (m_compile_stopped) {
			return false;
		}
		EnqueueLocked(std::move(job), urgent);
	}
	m_libraries.emplace(std::move(key), std::move(entry));
	m_compile_available.notify_one();
	return true;
}

void PipelineLibraryCache::EnqueueLocked(CompileJob&& job, bool urgent) {
	// Urgent jobs run first, in the order they came, then the others in theirs.
	job.urgent = urgent;
	if (urgent) {
		const auto first_normal = std::ranges::find(m_compile_jobs, false, &CompileJob::urgent);
		m_compile_jobs.insert(first_normal, std::move(job));
	} else {
		m_compile_jobs.push_back(std::move(job));
	}
}

void PipelineLibraryCache::RunIfQueued(const std::string& key) {
	static const bool disabled = std::getenv("KYTY_DEBUG_NO_STEAL_PREFETCH") != nullptr;
	if (disabled) {
		return;
	}
	CompileJob job;
	{
		std::lock_guard lock(m_compile_mutex);
		const auto      queued = std::ranges::find(m_compile_jobs, key, &CompileJob::key);
		if (queued == m_compile_jobs.end()) {
			return;
		}
		job = std::move(*queued);
		m_compile_jobs.erase(queued);
	}
	job.result.set_value(job.compile());
}

void PipelineLibraryCache::Promote(const std::string& key) {
	std::lock_guard lock(m_compile_mutex);
	const auto      queued = std::ranges::find(m_compile_jobs, key, &CompileJob::key);
	if (queued != m_compile_jobs.end() && !queued->urgent) {
		auto job = std::move(*queued);
		m_compile_jobs.erase(queued);
		EnqueueLocked(std::move(job), true);
	}
}

bool PipelineLibraryCache::Post(Common::UniqueFunction<void>&& job, bool urgent) {
	CompileJob compile_job;
	compile_job.compile = [job = std::move(job)]() mutable {
		job();
		return vk::Pipeline {};
	};
	{
		std::lock_guard lock(m_compile_mutex);
		if (m_compile_stopped) {
			return false;
		}
		EnqueueLocked(std::move(compile_job), urgent);
	}
	m_compile_available.notify_one();
	return true;
}

void PipelineLibraryCache::KeepLayout(vk::PipelineLayout                       layout,
                                      std::span<const vk::DescriptorSetLayout> set_layouts) {
	m_kept_layouts.push_back(layout);
	m_kept_set_layouts.insert(m_kept_set_layouts.end(), set_layouts.begin(), set_layouts.end());
}

void PipelineLibraryCache::QueueOptimizedLink(const void*                   target,
                                              std::span<const vk::Pipeline> parts,
                                              vk::PipelineLayout            layout) {
	LinkJob job;
	EXIT_IF(parts.size() > job.parts.size());
	job.target     = target;
	job.part_count = static_cast<uint32_t>(parts.size());
	job.layout     = layout;
	std::ranges::copy(parts, job.parts.begin());
	{
		std::lock_guard lock(m_link_mutex);
		if (m_link_stopped) {
			// Shutting down: the fast-linked pipeline stays.
			m_optimized.emplace(target, nullptr);
			return;
		}
		m_link_jobs.push_back(job);
	}
	m_link_available.notify_one();
}

vk::PipelineCache PipelineLibraryCache::FastLinkCache() const {
	static const bool disabled = std::getenv("KYTY_DEBUG_FAST_LINK_NO_CACHE") != nullptr;
	return disabled ? nullptr : m_driver_cache;
}

std::shared_future<vk::Pipeline> PipelineLibraryCache::LinkLater(const void*                   target,
                                                                 std::span<const vk::Pipeline> parts,
                                                                 vk::PipelineLayout            layout) {
	std::array<vk::Pipeline, 4> kept {};
	EXIT_IF(parts.size() > kept.size());
	std::ranges::copy(parts, kept.begin());
	const auto count = static_cast<uint32_t>(parts.size());
	CompileJob job;
	job.compile = [this, target, kept, count, layout] {
		vk::PipelineLibraryCreateInfoKHR libraries {};
		libraries.libraryCount = count;
		libraries.pLibraries   = kept.data();
		vk::GraphicsPipelineCreateInfo info {};
		info.pNext             = &libraries;
		info.layout            = layout;
		info.basePipelineIndex = -1;
		vk::Pipeline linked    = nullptr;
		if (m_graphics.device.createGraphicsPipelines(FastLinkCache(), 1, &info, nullptr,
		                                              &linked) != vk::Result::eSuccess) {
			LOGF("PipelineLibrary: fast link failed\n");
			return vk::Pipeline {};
		}
		QueueOptimizedLink(target, std::span(kept).first(count), layout);
		return linked;
	};
	auto result = job.result.get_future().share();
	{
		std::lock_guard lock(m_compile_mutex);
		if (m_compile_stopped) {
			job.result.set_value(nullptr);
			return result;
		}
		EnqueueLocked(std::move(job), true);
	}
	m_compile_available.notify_one();
	return result;
}

std::optional<vk::Pipeline> PipelineLibraryCache::TakeOptimized(const void* target) {
	std::lock_guard lock(m_link_mutex);
	const auto      iter = m_optimized.find(target);
	if (iter == m_optimized.end()) {
		return std::nullopt;
	}
	const auto pipeline = iter->second;
	m_optimized.erase(iter);
	return pipeline;
}

void PipelineLibraryCache::Stop() {
	{
		std::lock_guard lock(m_link_mutex);
		m_link_stopped = true;
		for (const auto& job: m_link_jobs) {
			m_optimized.emplace(job.target, nullptr);
		}
		m_link_jobs.clear();
	}
	{
		std::lock_guard lock(m_compile_mutex);
		m_compile_stopped = true;
		for (auto& job: m_compile_jobs) {
			job.result.set_value(nullptr);
		}
		m_compile_jobs.clear();
	}
	m_link_thread.request_stop();
	for (auto& thread: m_compile_threads) {
		thread.request_stop();
	}
	if (m_link_thread.joinable()) {
		m_link_thread.join();
	}
	for (auto& thread: m_compile_threads) {
		if (thread.joinable()) {
			thread.join();
		}
	}
}

void PipelineLibraryCache::LinkThread(const std::stop_token& stop) {
	KYTY_PROFILER_THREAD("PipelineLink");
	for (;;) {
		LinkJob job;
		{
			std::unique_lock lock(m_link_mutex);
			if (!m_link_available.wait(lock, stop, [this] { return !m_link_jobs.empty(); })) {
				return;
			}
			job = m_link_jobs.front();
			m_link_jobs.pop_front();
		}
		static const bool no_lto = std::getenv("KYTY_NO_LTO") != nullptr;
		if (no_lto) {
			std::lock_guard lock(m_link_mutex);
			m_optimized.emplace(job.target, nullptr);
			continue;
		}
		// Wait for a pause in the draw path's pipeline builds; see NoteForegroundLink.
		bool stopping = false;
		for (;;) {
			const auto since = SteadyNowNs() - m_last_foreground_ns.load(std::memory_order_relaxed);
			std::unique_lock lock(m_link_mutex);
			if (m_link_stopped || stop.stop_requested()) {
				stopping = true;
				break;
			}
			if (since >= LinkQuietNs()) {
				break;
			}
			const auto wait =
			    std::chrono::nanoseconds(std::max<int64_t>(LinkQuietNs() - since, 1000000));
			(void)m_link_available.wait_for(lock, stop, wait, [this] { return m_link_stopped; });
		}
		if (stopping) {
			// Shutting down: the fast-linked pipeline stays.
			std::lock_guard lock(m_link_mutex);
			m_optimized.emplace(job.target, nullptr);
			return;
		}
		vk::PipelineLibraryCreateInfoKHR libraries {};
		libraries.libraryCount = job.part_count;
		libraries.pLibraries   = job.parts.data();
		vk::GraphicsPipelineCreateInfo info {};
		info.pNext  = &libraries;
		info.flags  = vk::PipelineCreateFlagBits::eLinkTimeOptimizationEXT;
		info.layout = job.layout;
		vk::Pipeline optimized = nullptr;
		m_link_busy            = true;
		const auto result =
		    m_graphics.device.createGraphicsPipelines(m_driver_cache, 1, &info, nullptr, &optimized);
		m_link_busy = false;
		if (result != vk::Result::eSuccess) {
			LOGF("PipelineLibrary: optimized link failed (%s); keeping the fast-linked pipeline\n",
			     vk::to_string(result).c_str());
			optimized = nullptr;
		}
		std::lock_guard lock(m_link_mutex);
		m_optimized.emplace(job.target, optimized);
	}
}

void PipelineLibraryCache::CompileThread(const std::stop_token& stop, size_t index) {
	KYTY_PROFILER_THREAD("PipelinePrefetch");
	for (;;) {
		// Also undoes a BoostRunning of the previous job.
		LowerThreadPriority();
		CompileJob job;
		{
			std::unique_lock lock(m_compile_mutex);
			m_running_keys[index].clear();
			if (!m_compile_available.wait(lock, stop, [this] { return !m_compile_jobs.empty(); })) {
				return;
			}
			job = std::move(m_compile_jobs.front());
			m_compile_jobs.pop_front();
			m_running_keys[index] = job.key;
		}
		m_busy_compiles++;
		auto pipeline = job.compile();
		m_busy_compiles--;
		job.result.set_value(pipeline);
	}
}

} // namespace Libs::Graphics
