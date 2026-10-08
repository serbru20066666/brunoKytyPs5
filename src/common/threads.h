#ifndef KYTY_COMMON_THREADS_H_
#define KYTY_COMMON_THREADS_H_

#include "common/common.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

namespace Common {

void InitializeThreads();

using thread_func_t    = void (*)(void*);
using wait_poll_func_t = void (*)();

struct ThreadPrivate;
struct MutexPrivate;
struct CondVarPrivate;

class Thread {
public:
	Thread(thread_func_t func, void* arg);
	~Thread();

	void Join();
	void Detach();

	// Once a thread has finished, the id may be reused by another thread.
	[[nodiscard]] std::string GetId() const;

	// The id is unique and can't be reused by another thread.
	[[nodiscard]] int GetUniqueId() const;

	static void SleepMicro(uint32_t micros);
	static void SleepNano(uint64_t nanos);
	static bool IsMainThread();

	// Get current thread id
	// Once a thread has finished, the id may be reused by another thread.
	static std::string GetThreadId();

	// Get current thread id
	// The id is unique and can't be reused by another thread.
	static int GetThreadIdUnique();

	KYTY_CLASS_NO_COPY(Thread);

private:
	std::unique_ptr<ThreadPrivate> m_thread;
};

class Mutex {
public:
	Mutex();
	~Mutex();

	void Lock();
	void Unlock();
	bool TryLock();

	friend class CondVar;

	KYTY_CLASS_NO_COPY(Mutex);

private:
	std::unique_ptr<MutexPrivate> m_mutex;
};

class CondVar {
public:
	CondVar();
	~CondVar();

	void Wait(Mutex* mutex);
	bool WaitFor(Mutex* mutex, uint32_t micros);
	void Signal();
	void SignalAll();

	static void SetWaitPollCallback(wait_poll_func_t callback);

	KYTY_CLASS_NO_COPY(CondVar);

private:
	std::unique_ptr<CondVarPrivate> m_cond_var;
};

// A recursive lock for the few mutexes one thread takes for every draw and other threads take
// now and then: taking and releasing it uncontended is one compare-and-swap and one store, in
// line, where the critical section behind Mutex is two calls into the system for each. In GPU
// Jungle's ten thousand draws a frame those calls were 6% of the GPU thread. A thread that finds
// it taken spins, then yields; nothing sleeps on it, so it is not for locks held across waits and
// cannot be used with CondVar.
class SpinMutex {
public:
	SpinMutex() = default;

	void Lock() noexcept {
		const auto self = Token();
		if (m_owner.load(std::memory_order_relaxed) == self) {
			m_depth++;
			return;
		}
		uintptr_t expected = 0;
		for (uint32_t spins = 0;
		     !m_owner.compare_exchange_weak(expected, self, std::memory_order_acquire,
		                                    std::memory_order_relaxed);
		     spins++) {
			expected = 0;
			Relax(spins);
		}
		m_depth = 1;
	}

	void Unlock() noexcept {
		if (--m_depth == 0) {
			m_owner.store(0, std::memory_order_release);
		}
	}

	KYTY_CLASS_NO_COPY(SpinMutex);

private:
	// Any address that only the calling thread has.
	static uintptr_t Token() noexcept {
		static thread_local char token;
		return reinterpret_cast<uintptr_t>(&token);
	}
	static void Relax(uint32_t spins) noexcept;

	// On a line of its own: other threads' writes beside it would make every lock wait for it.
	alignas(64) std::atomic<uintptr_t> m_owner {0};
	uint32_t                           m_depth = 0; // The owner's.
};

template <typename MutexType>
class LockGuard {
public:
	using mutex_type = MutexType;

	// NOLINTNEXTLINE(google-runtime-references)
	explicit LockGuard(mutex_type& m): m_mutex(m) { m_mutex.Lock(); }

	~LockGuard() { m_mutex.Unlock(); }

	KYTY_CLASS_NO_COPY(LockGuard);

private:
	mutex_type& m_mutex;
};

} // namespace Common

#endif /* KYTY_COMMON_THREADS_H_ */
