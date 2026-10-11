#include "PrecisionWait.hpp"
#include <mmsystem.h>
#include <algorithm>

namespace
{
	const int64_t FirstRetrySeconds = 2;
	const int64_t LastRetrySeconds = 64;

	// CreateWaitableTimerExW is Vista+ and its high resolution flag Windows 10 1803+. The function is
	// looked up at runtime so that the executable still loads on Windows XP.
	HANDLE CreateHighResolutionTimer()
	{
		typedef HANDLE(WINAPI* CreateWaitableTimerExFn)(LPSECURITY_ATTRIBUTES, LPCWSTR, DWORD, DWORD);
		const DWORD HighResolution = 0x00000002; // CREATE_WAITABLE_TIMER_HIGH_RESOLUTION

		const FARPROC function = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "CreateWaitableTimerExW");
		if (function == nullptr)
			return nullptr;
		return reinterpret_cast<CreateWaitableTimerExFn>(function)(nullptr, nullptr, HighResolution, TIMER_ALL_ACCESS);
	}
}

PrecisionWait::PrecisionWait(bool allowHighResolutionTimer)
{
	LARGE_INTEGER frequency;
	QueryPerformanceFrequency(&frequency);
	m_ticksPerSecond = frequency.QuadPart;

	if (allowHighResolutionTimer)
		m_timer = CreateHighResolutionTimer();
	m_highResolution = m_timer != nullptr;

	if (m_timer == nullptr)
	{
		// Ordinary waitable timers only fire on the scheduler tick, so shorten that from 15.6 ms to 1 ms
		m_timer = CreateWaitableTimerW(nullptr, FALSE, nullptr);
		m_tickRaised = timeBeginPeriod(1) == TIMERR_NOERROR;
	}

	m_minSleep = FromMicroseconds(100);
	m_headroom = FromMicroseconds(20);
	m_tightSpin = FromMicroseconds(20);
	// High resolution timers fire on a 0.5 ms grid, ordinary ones on the 1 ms tick, and both are late on top of that at times
	m_initialLateness = FromMicroseconds(m_highResolution ? 1000 : 2500);
	m_retryAfter = FirstRetrySeconds * m_ticksPerSecond;
}

PrecisionWait::~PrecisionWait()
{
	if (m_tickRaised)
		timeEndPeriod(1);
	if (m_timer != nullptr)
		CloseHandle(m_timer);
}

int64_t PrecisionWait::Now()
{
	LARGE_INTEGER counter;
	QueryPerformanceCounter(&counter);
	return counter.QuadPart;
}

int64_t PrecisionWait::Margin() const
{
	int64_t worst = m_worst;
	if (m_count < WindowSize)
		worst = std::max(worst, m_initialLateness);
	return worst + worst / 8 + m_headroom;
}

bool PrecisionWait::Until(int64_t deadline, HANDLE cancelEvent, const std::atomic<bool>& cancelled)
{
	int64_t now = Now();
	bool slept = false;

	// Usually this runs once. It repeats when the timer fired early, or for the tail of a long sleep.
	while (m_timer != nullptr)
	{
		const int64_t remaining = deadline - now;
		// The timer does not run off the performance counter, so long sleeps leave a sliver of
		// themselves unslept in case the two clocks do not tick at exactly the same rate
		const int64_t sleep = remaining - Margin() - remaining / 512;
		if (sleep < m_minSleep)
			break;

		LARGE_INTEGER dueTime;
		dueTime.QuadPart = -static_cast<int64_t>(static_cast<double>(sleep) * 1e7 / static_cast<double>(m_ticksPerSecond));
		SetWaitableTimer(m_timer, &dueTime, 0, nullptr, nullptr, FALSE);

		const HANDLE handles[] = { m_timer, cancelEvent };
		if (WaitForMultipleObjects(2, handles, FALSE, INFINITE) != WAIT_OBJECT_0)
			return false;

		const int64_t wokenAt = Now();
		RecordLateness(wokenAt - (now + sleep));
		m_stats.sleeps++;
		if (wokenAt > deadline)
			m_stats.lateWakes++;

		now = wokenAt;
		slept = true;
	}

	if (!slept)
		RetrySleeping(now, deadline - now);
	else
	{
		m_spinningSince = 0;
		if (m_count == WindowSize)
			m_retryAfter = FirstRetrySeconds * m_ticksPerSecond;
	}

	// Spin through what is left. Pausing between looks at the clock leaves more of the core to its
	// hyper-thread sibling, but right before the deadline the clock is polled as fast as possible.
	const int64_t tightSpinFrom = deadline - m_tightSpin;
	while (now < deadline)
	{
		if (cancelled.load(std::memory_order_relaxed))
			return false;
		if (now < tightSpinFrom)
			YieldProcessor();
		now = Now();
	}
	return true;
}

void PrecisionWait::RecordLateness(int64_t lateness)
{
	lateness = std::max<int64_t>(lateness, 0);

	const bool evictsWorst = m_count == WindowSize && m_lateness[m_next] == m_worst;
	m_lateness[m_next] = lateness;
	m_next = (m_next + 1) % WindowSize;
	m_count = std::min(m_count + 1, WindowSize);

	if (lateness >= m_worst)
		m_worst = lateness;
	else if (evictsWorst)
		m_worst = *std::max_element(m_lateness, m_lateness + m_count);
}

// Called for every wait that was too short to sleep in. That is the normal state for very short
// intervals, but it is also where a single freak wake-up (preemption, resume from standby) leaves
// us: the margin it caused no longer fits into the interval, so no sleep happens, so no new
// measurement can ever push it out of the window again. Every now and then, drop such
// measurements, provided they really are the odd ones out.
void PrecisionWait::RetrySleeping(int64_t now, int64_t available)
{
	if (m_spinningSince == 0)
	{
		m_spinningSince = now;
		return;
	}
	if (now - m_spinningSince < m_retryAfter)
		return;

	m_spinningSince = now;
	m_retryAfter = std::min(m_retryAfter * 2, LastRetrySeconds * m_ticksPerSecond);

	// Largest lateness whose margin would still leave room for a sleep (inverse of Margin())
	const int64_t tolerable = (available - m_minSleep - m_headroom) * 8 / 9;
	const auto blocks = [tolerable](int64_t lateness) { return lateness > tolerable; };
	const size_t blocking = static_cast<size_t>(std::count_if(m_lateness, m_lateness + m_count, blocks));
	if (blocking == 0)
		return;
	// If they are not a small minority, the timer simply is too coarse for this interval
	if (m_count >= 32 && blocking > m_count / 8)
		return;

	// Put the oldest measurement first again, so the ring buffer keeps evicting in order of age
	std::rotate(m_lateness, m_lateness + (m_count == WindowSize ? m_next : 0), m_lateness + m_count);
	m_count = static_cast<size_t>(std::remove_if(m_lateness, m_lateness + m_count, blocks) - m_lateness);
	m_next = m_count % WindowSize;
	m_worst = m_count > 0 ? *std::max_element(m_lateness, m_lateness + m_count) : 0;
}
