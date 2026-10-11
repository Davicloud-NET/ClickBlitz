#pragma once
#include <Windows.h>
#include <atomic>
#include <cstddef>
#include <cstdint>

// Blocks until an absolute QueryPerformanceCounter deadline with the accuracy of a busy-wait,
// but without burning a CPU core for the whole interval: the thread sleeps on a waitable timer
// for most of the wait and only spins through the last stretch. How early it has to be awake
// for that is learned from how late the timer really fires on this machine.
class PrecisionWait
{
public:
	struct Stats
	{
		uint64_t sleeps = 0;    // Timer waits performed
		uint64_t lateWakes = 0; // Timer waits that only returned after the deadline had passed
	};

	explicit PrecisionWait(bool allowHighResolutionTimer = true);
	~PrecisionWait();
	PrecisionWait(const PrecisionWait&) = delete;
	PrecisionWait& operator=(const PrecisionWait&) = delete;

	static int64_t Now(); // QueryPerformanceCounter ticks
	int64_t TicksPerSecond() const { return m_ticksPerSecond; }

	// Returns false if the wait got cancelled before the deadline was reached.
	// cancelEvent interrupts the sleeping part, the flag is polled while spinning.
	bool Until(int64_t deadline, HANDLE cancelEvent, const std::atomic<bool>& cancelled);

	bool UsesHighResolutionTimer() const { return m_highResolution; }
	int64_t Margin() const; // How long before a deadline the thread currently wants to be awake, in ticks
	const Stats& GetStats() const { return m_stats; }

private:
	// Number of sleeps the margin is based on. About 1 in 1000 wake-ups is far later than the rest,
	// so the window has to be long enough to usually contain one of those.
	static const size_t WindowSize = 512;

	int64_t FromMicroseconds(int64_t us) const { return us * m_ticksPerSecond / 1000000; }
	void RecordLateness(int64_t lateness);
	void RetrySleeping(int64_t now, int64_t available);

	HANDLE m_timer = nullptr;
	bool m_highResolution = false;
	bool m_tickRaised = false;
	int64_t m_ticksPerSecond = 0;

	int64_t m_minSleep = 0;        // Shorter sleeps are not worth the system call
	int64_t m_headroom = 0;        // Fixed part of the safety distance on top of the worst lateness seen
	int64_t m_initialLateness = 0; // Assumed worst lateness until the window is filled with real measurements
	int64_t m_tightSpin = 0;       // Final stretch before the deadline in which the clock is polled without pausing

	// How late the timer woke the thread on its most recent sleeps (ring buffer once full)
	int64_t m_lateness[WindowSize] = {};
	size_t m_count = 0;
	size_t m_next = 0;
	int64_t m_worst = 0;

	int64_t m_spinningSince = 0; // Start of the current run of waits that could not sleep at all, 0 if none
	int64_t m_retryAfter = 0;

	Stats m_stats;
};
