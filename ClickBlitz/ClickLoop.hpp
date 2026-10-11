#pragma once
#include "PrecisionWait.hpp"
#include <cmath>

// Calls click(scheduledTime) once per period until stopped, or until clickLimit clicks were made (0 = no limit).
// Returns the number of clicks made.
//
// Clicks are scheduled against fixed points in time (start + n * period) instead of "one period after the
// previous click". That way neither the time a click itself takes nor any rounding adds up into drift.
template <typename Click>
uint64_t RunClickLoop(PrecisionWait& wait, double periodTicks, uint64_t clickLimit, HANDLE stopEvent,
	const std::atomic<bool>& stopRequested, Click&& click)
{
	int64_t start = PrecisionWait::Now();
	uint64_t slot = 0;
	int64_t deadline = start;
	uint64_t clicks = 0;

	while (!stopRequested.load(std::memory_order_relaxed))
	{
		click(deadline);
		clicks++;
		if (clickLimit != 0 && clicks >= clickLimit)
			break;

		slot++;
		deadline = start + std::llround(static_cast<double>(slot) * periodTicks);

		const int64_t now = PrecisionWait::Now();
		if (static_cast<double>(now - deadline) > periodTicks)
		{
			// More than a whole period behind, because clicking takes longer than the period or the thread
			// was held up. Start a new schedule from here instead of firing a burst of clicks to catch up.
			start = now;
			slot = 0;
			deadline = now;
		}
		else if (!wait.Until(deadline, stopEvent, stopRequested))
			break;
	}
	return clicks;
}
