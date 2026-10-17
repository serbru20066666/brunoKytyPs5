#ifndef EMULATOR_SRC_LOCAL_WRITEBACK_VERIFY_H_
#define EMULATOR_SRC_LOCAL_WRITEBACK_VERIFY_H_

// (Diagnostic, KYTY_WRITEBACK_VERIFY=1, issue #31: a GPU write-back replaced bytes the game wrote.) The bytes of a page
// the GPU owns do not change until a write-back: a guest store faults (the page is no-access, write-protected before),
// the emulator's own writes give the page back to the CPU first, and write-backs hash it again. Each page that becomes
// GPU-owned is hashed (RegionManager::UpdateGpuProtection) and each write-back checks its pages before writing
// (Memory::WriteBacking): a page whose bytes changed took a store nothing saw, which the write-back is about to
// replace. The pages are read through the backing view (no fault, no system call: unlike the protection audit, nothing
// here reads page table entries).

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <unordered_map>

namespace Libs::LibKernel::Memory {
const uint8_t* TryGetBackingPointer(uint64_t vaddr, uint64_t size);
} // namespace Libs::LibKernel::Memory

namespace WritebackVerify {

inline constexpr uint64_t PageSize = 4096;

[[nodiscard]] inline bool Enabled() noexcept {
	static const bool on = [] {
		const char* value = std::getenv("KYTY_WRITEBACK_VERIFY");
		const bool  set   = value != nullptr && value[0] != '\0' && value[0] != '0';
		if (set) std::printf("WRITEBACK-VERIFY on: GPU-owned pages hashed, checked before each write-back\n");
		return set;
	}();
	return on;
}

// Four independent lanes (a single multiply chain made the hash 4x slower); only a change has to show.
[[nodiscard]] inline uint64_t HashPage(const uint8_t* page) noexcept {
	uint64_t lanes[4] = {0x9e3779b97f4a7c15ull, 0xc2b2ae3d27d4eb4full, 0x165667b19e3779f9ull, 0x27d4eb2f165667c5ull};
	for (uint64_t i = 0; i < PageSize; i += 32) {
		uint64_t words[4];
		std::memcpy(words, page + i, sizeof(words));
		for (int lane = 0; lane < 4; ++lane) {
			lanes[lane] = (lanes[lane] ^ words[lane]) * 0xff51afd7ed558ccdull;
			lanes[lane] ^= lanes[lane] >> 32u;
		}
	}
	return lanes[0] ^ (lanes[1] * 3) ^ (lanes[2] * 5) ^ (lanes[3] * 7);
}

// The GPU-owned pages' hashes in shards by page (the GPU thread hashes the pages it marks while guest threads check
// theirs).
struct Shard {
	std::mutex                             mutex;
	std::unordered_map<uint64_t, uint64_t> hashes;
};
struct State {
	std::array<Shard, 64>  shards;
	std::atomic<uint64_t> owned {0}, released {0}, checked {0}, mismatches {0}, unbacked {0}, rehashed {0},
	    reported_at {0};
};
[[nodiscard]] inline State& GetState() {
	static State state;
	return state;
}
[[nodiscard]] inline Shard& ShardOf(State& state, uint64_t page) {
	return state.shards[(page / PageSize) % state.shards.size()];
}

inline void Summary(State& state) {
	const auto checked = state.checked.load(std::memory_order_relaxed);
	auto       at      = state.reported_at.load(std::memory_order_relaxed);
	if (checked < at + 1000000 || !state.reported_at.compare_exchange_strong(at, checked)) return;
	uint64_t held = 0;
	for (auto& shard: state.shards) {
		std::lock_guard lock(shard.mutex);
		held += shard.hashes.size();
	}
	std::printf("WRITEBACK-VERIFY pages owned %llu released %llu held %llu | checked %llu changed %llu | unbacked %llu "
	            "rehashed %llu\n",
	            static_cast<unsigned long long>(state.owned.load()), static_cast<unsigned long long>(state.released.load()),
	            static_cast<unsigned long long>(held), static_cast<unsigned long long>(checked),
	            static_cast<unsigned long long>(state.mismatches.load()),
	            static_cast<unsigned long long>(state.unbacked.load()),
	            static_cast<unsigned long long>(state.rehashed.load()));
}

// The pages [first, first + count * PageSize) became GPU-owned.
inline void Owned(uint64_t first, uint64_t count) {
	auto& state = GetState();
	for (uint64_t page = first; page < first + count * PageSize; page += PageSize) {
		const auto* bytes = Libs::LibKernel::Memory::TryGetBackingPointer(page, PageSize);
		if (bytes == nullptr) {
			state.unbacked.fetch_add(1, std::memory_order_relaxed);
			continue;
		}
		// (Under the shard's lock, as Rehash: a write-back's hash cannot be replaced by one taken before it.)
		auto&           shard = ShardOf(state, page);
		std::lock_guard lock(shard.mutex);
		shard.hashes[page] = HashPage(bytes);
		state.owned.fetch_add(1, std::memory_order_relaxed);
	}
}

// The pages [first, first + count * PageSize) were given back to the CPU.
inline void Released(uint64_t first, uint64_t count) {
	auto& state = GetState();
	for (uint64_t page = first; page < first + count * PageSize; page += PageSize) {
		auto&           shard = ShardOf(state, page);
		std::lock_guard lock(shard.mutex);
		state.released.fetch_add(shard.hashes.erase(page), std::memory_order_relaxed);
	}
}

// Before a write-back of `data` over [address, address + size): pages whose bytes changed since they became GPU-owned
// (or since the last write-back) are reported with the bytes this one replaces.
inline void Check(uint64_t address, const uint8_t* data, uint64_t size, const char* source) {
	if (size == 0) return;
	auto& state = GetState();
	for (uint64_t page = address & ~(PageSize - 1); page < address + size; page += PageSize) {
		auto&           shard = ShardOf(state, page);
		std::lock_guard lock(shard.mutex);
		const auto      it = shard.hashes.find(page);
		if (it == shard.hashes.end()) continue;
		const auto* bytes = Libs::LibKernel::Memory::TryGetBackingPointer(page, PageSize);
		if (bytes == nullptr) continue;
		state.checked.fetch_add(1, std::memory_order_relaxed);
		if (HashPage(bytes) == it->second) continue;
		if (state.mismatches.fetch_add(1, std::memory_order_relaxed) >= 64) continue;
		const auto begin = std::max(address, page), end = std::min(address + size, page + PageSize);
		uint64_t   replaced = 0, first = 0;
		for (auto at = begin; at < end; ++at) {
			if (bytes[at - page] == data[at - address]) continue;
			if (replaced++ == 0) first = at;
		}
		char text[512];
		int  length = std::snprintf(text, sizeof(text),
		                            "WRITEBACK-VERIFY page 0x%llx changed while GPU-owned: the %s write-back of 0x%llx+0x%llx "
		                            "replaces %llu of its bytes",
		                            static_cast<unsigned long long>(page), source, static_cast<unsigned long long>(address),
		                            static_cast<unsigned long long>(size), static_cast<unsigned long long>(replaced));
		if (replaced != 0) {
			const auto line = std::max(begin, first & ~uint64_t {15});
			length += std::snprintf(text + length, sizeof(text) - length, ", first at 0x%llx: backing",
			                        static_cast<unsigned long long>(first));
			for (auto at = line; at < std::min(end, line + 16); ++at)
				length += std::snprintf(text + length, sizeof(text) - length, " %02x", bytes[at - page]);
			length += std::snprintf(text + length, sizeof(text) - length, " | write-back");
			for (auto at = line; at < std::min(end, line + 16); ++at)
				length += std::snprintf(text + length, sizeof(text) - length, " %02x", data[at - address]);
		}
		std::printf("%s\n", text);
		std::fflush(stdout);
	}
	Summary(state);
}

// After a write of the backing over [address, address + size): its pages still GPU-owned are hashed again.
inline void Rehash(uint64_t address, uint64_t size) {
	if (size == 0) return;
	auto& state = GetState();
	for (uint64_t page = address & ~(PageSize - 1); page < address + size; page += PageSize) {
		auto&           shard = ShardOf(state, page);
		std::lock_guard lock(shard.mutex);
		const auto      it = shard.hashes.find(page);
		if (it == shard.hashes.end()) continue;
		if (const auto* bytes = Libs::LibKernel::Memory::TryGetBackingPointer(page, PageSize); bytes != nullptr) {
			it->second = HashPage(bytes);
			state.rehashed.fetch_add(1, std::memory_order_relaxed);
		}
	}
}

} // namespace WritebackVerify

#endif // EMULATOR_SRC_LOCAL_WRITEBACK_VERIFY_H_
