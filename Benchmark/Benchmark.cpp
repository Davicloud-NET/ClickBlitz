// Measures how precisely the click loop hits its schedule on this machine and how much CPU it needs for that,
// and compares it with the busy-wait loop ClickBlitz used up to version 1.1.0.
//
// Nothing is clicked: the "click" is only a timestamp. With --inject it additionally sends a mouse move by
// zero pixels through SendInput, to include what going through the input stack costs.

#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>
#include "../ClickBlitz/ClickLoop.hpp"

namespace
{
	enum class Mode { Hybrid, BusyWait };

	struct Options
	{
		Mode mode = Mode::Hybrid;
		int cps = 150;
		int intervalMs = 0; // Used instead of cps when > 0
		double seconds = 10;
		int priority = THREAD_PRIORITY_TIME_CRITICAL;
		bool highResolutionTimer = true;
		bool inject = false;
		bool window = false;
		bool minimized = false;
	};

	struct Result
	{
		std::vector<int64_t> at;        // When each click happened
		std::vector<int64_t> scheduled; // When it should have happened
		double cpuShare = 0;
		PrecisionWait::Stats waitStats;
		int64_t margin = 0;
		bool highResolution = false;
	};

	int64_t g_ticksPerSecond;

	void Click(const Options& options)
	{
		if (!options.inject)
			return;
		INPUT input = {};
		input.type = INPUT_MOUSE;
		input.mi.dwFlags = MOUSEEVENTF_MOVE;
		SendInput(1, &input, sizeof(INPUT));
	}

	// The loop of ClickBlitz 1.1.0 (AutoClicker::AutoClickThread + Utils::Sleep), kept as it was
	void RunBusyWait(const Options& options, uint64_t clickLimit, Result& result)
	{
		long long SleepTime_ms;
		if (options.intervalMs == 0)
			SleepTime_ms = static_cast<long long>((1.0 / options.cps) * 1000);
		else
			SleepTime_ms = options.intervalMs;

		int64_t target = PrecisionWait::Now();
		for (uint64_t i = 0; i < clickLimit; i++)
		{
			result.at.push_back(PrecisionWait::Now());
			result.scheduled.push_back(target);
			Click(options);

			target = PrecisionWait::Now() + SleepTime_ms * g_ticksPerSecond / 1000;
			auto start = std::chrono::high_resolution_clock::now();
			while (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count() < SleepTime_ms)
			{
			}
		}
	}

	void RunHybrid(const Options& options, uint64_t clickLimit, Result& result)
	{
		PrecisionWait wait(options.highResolutionTimer);
		const double periodTicks = options.intervalMs == 0
			? static_cast<double>(g_ticksPerSecond) / options.cps
			: static_cast<double>(g_ticksPerSecond) * options.intervalMs / 1000.0;

		const HANDLE stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
		std::atomic<bool> stopRequested(false);
		RunClickLoop(wait, periodTicks, clickLimit, stopEvent, stopRequested, [&](int64_t scheduled)
		{
			result.at.push_back(PrecisionWait::Now());
			result.scheduled.push_back(scheduled);
			Click(options);
		});
		CloseHandle(stopEvent);

		result.waitStats = wait.GetStats();
		result.margin = wait.Margin();
		result.highResolution = wait.UsesHighResolutionTimer();
	}

	// CPU cycles this thread gets charged per performance counter tick while it runs flat out
	double CalibrateCyclesPerTick()
	{
		ULONG64 cyclesBefore, cyclesAfter;
		const int64_t start = PrecisionWait::Now();
		QueryThreadCycleTime(GetCurrentThread(), &cyclesBefore);
		int64_t now = start;
		while (now - start < g_ticksPerSecond / 5)
			now = PrecisionWait::Now();
		QueryThreadCycleTime(GetCurrentThread(), &cyclesAfter);
		return static_cast<double>(cyclesAfter - cyclesBefore) / static_cast<double>(now - start);
	}

	void Worker(const Options& options, Result& result)
	{
		SetThreadPriority(GetCurrentThread(), options.priority);

		const double period = options.intervalMs == 0 ? 1.0 / options.cps : options.intervalMs / 1000.0;
		const uint64_t clickLimit = std::max<uint64_t>(2, static_cast<uint64_t>(options.seconds / period));
		result.at.reserve(static_cast<size_t>(clickLimit));
		result.scheduled.reserve(static_cast<size_t>(clickLimit));

		const double cyclesPerTick = CalibrateCyclesPerTick();

		ULONG64 cyclesBefore, cyclesAfter;
		QueryThreadCycleTime(GetCurrentThread(), &cyclesBefore);
		const int64_t start = PrecisionWait::Now();

		if (options.mode == Mode::BusyWait)
			RunBusyWait(options, clickLimit, result);
		else
			RunHybrid(options, clickLimit, result);

		const int64_t end = PrecisionWait::Now();
		QueryThreadCycleTime(GetCurrentThread(), &cyclesAfter);
		result.cpuShare = static_cast<double>(cyclesAfter - cyclesBefore) / cyclesPerTick / static_cast<double>(end - start);
	}

	double Microseconds(double ticks)
	{
		return ticks * 1e6 / static_cast<double>(g_ticksPerSecond);
	}

	double Percentile(const std::vector<int64_t>& sorted, double p)
	{
		const size_t index = std::min(sorted.size() - 1, static_cast<size_t>(p * static_cast<double>(sorted.size())));
		return Microseconds(static_cast<double>(sorted[index]));
	}

	void Report(const Options& options, const Result& result)
	{
		const size_t clicks = result.at.size();
		const double requestedRate = options.intervalMs == 0 ? options.cps : 1000.0 / options.intervalMs;

		// How long after its scheduled time each click happened
		std::vector<int64_t> lateness(clicks);
		for (size_t i = 0; i < clicks; i++)
			lateness[i] = result.at[i] - result.scheduled[i];
		std::sort(lateness.begin(), lateness.end());
		const int64_t noticeable = g_ticksPerSecond / 10000; // 100 us
		const size_t late = static_cast<size_t>(lateness.end() - std::upper_bound(lateness.begin(), lateness.end(), noticeable));

		// Intervals between consecutive clicks
		const double mean = static_cast<double>(result.at.back() - result.at.front()) / static_cast<double>(clicks - 1);
		double variance = 0;
		for (size_t i = 1; i < clicks; i++)
		{
			const double deviation = static_cast<double>(result.at[i] - result.at[i - 1]) - mean;
			variance += deviation * deviation;
		}
		variance /= static_cast<double>(clicks - 1);

		const char* priority = options.priority == THREAD_PRIORITY_TIME_CRITICAL ? "time critical"
			: options.priority == THREAD_PRIORITY_HIGHEST ? "highest" : "normal";

		std::printf("%s, %g clicks per second for %g s, thread priority %s\n",
			options.mode == Mode::BusyWait ? "Busy-wait loop (ClickBlitz 1.1.0)" : "Sleep-then-spin loop", requestedRate, options.seconds, priority);
		std::printf("  clicks        %zu\n", clicks);
		std::printf("  rate          %.3f per second\n", static_cast<double>(g_ticksPerSecond) / mean);
		std::printf("  interval      %.2f us on average, standard deviation %.2f us\n", Microseconds(mean), Microseconds(std::sqrt(variance)));
		std::printf("  lateness      median %.1f us, 99%% within %.1f us, 99.9%% within %.1f us, worst %.1f us\n",
			Percentile(lateness, 0.5), Percentile(lateness, 0.99), Percentile(lateness, 0.999), Microseconds(static_cast<double>(lateness.back())));
		std::printf("  over 100 us   %zu clicks (%.3f %%)\n", late, 100.0 * static_cast<double>(late) / static_cast<double>(clicks));
		std::printf("  CPU           %.1f %% of one core\n", result.cpuShare * 100.0);
		if (options.mode == Mode::Hybrid)
		{
			std::printf("  timer         %s, %llu sleeps of which %llu ended after the click was due, now waking up %.0f us early\n",
				result.highResolution ? "high resolution" : "1 ms tick",
				static_cast<unsigned long long>(result.waitStats.sleeps), static_cast<unsigned long long>(result.waitStats.lateWakes),
				Microseconds(static_cast<double>(result.margin)));
		}
		std::printf("\n");
		std::fflush(stdout);
	}

	bool ParseOptions(int argc, char** argv, Options& options)
	{
		for (int i = 1; i < argc; i++)
		{
			const std::string argument = argv[i];
			const char* value = i + 1 < argc ? argv[i + 1] : "";
			if (argument == "--busywait") options.mode = Mode::BusyWait;
			else if (argument == "--hybrid") options.mode = Mode::Hybrid;
			else if (argument == "--cps") { options.cps = std::atoi(value); i++; }
			else if (argument == "--interval") { options.intervalMs = std::atoi(value); i++; }
			else if (argument == "--seconds") { options.seconds = std::atof(value); i++; }
			else if (argument == "--priority")
			{
				options.priority = std::strcmp(value, "critical") == 0 ? THREAD_PRIORITY_TIME_CRITICAL
					: std::strcmp(value, "highest") == 0 ? THREAD_PRIORITY_HIGHEST : THREAD_PRIORITY_NORMAL;
				i++;
			}
			else if (argument == "--legacy-timer") options.highResolutionTimer = false;
			else if (argument == "--inject") options.inject = true;
			else if (argument == "--window") options.window = true;
			else if (argument == "--minimized") options.window = options.minimized = true;
			else return false;
		}
		return options.cps > 0 && options.seconds > 0;
	}

	void PrintUsage()
	{
		std::printf(
			"Usage: Benchmark [--hybrid | --busywait] [--cps N | --interval MS] [--seconds S]\n"
			"                 [--priority normal|highest|critical] [--legacy-timer] [--inject]\n"
			"                 [--window | --minimized]\n\n"
			"  --hybrid        Loop used by ClickBlitz now: sleep, then spin (default)\n"
			"  --busywait      Loop used by ClickBlitz up to 1.1.0\n"
			"  --priority      Thread priority. ClickBlitz clicks at critical now (the default here) and did at\n"
			"                  normal up to 1.1.0\n"
			"  --legacy-timer  Do not use the Windows 10 high resolution timer, as on older Windows versions\n"
			"  --inject        Send a zero-distance mouse move through SendInput on every click\n"
			"  --window        Own a visible top-level window while measuring, like the real application\n"
			"  --minimized     Same, but minimized (Windows 11 treats such processes differently)\n");
	}
}

int main(int argc, char** argv)
{
	Options options;
	if (!ParseOptions(argc, argv, options))
	{
		PrintUsage();
		return 1;
	}

	LARGE_INTEGER frequency;
	QueryPerformanceFrequency(&frequency);
	g_ticksPerSecond = frequency.QuadPart;

	HWND window = nullptr;
	if (options.window)
	{
		window = CreateWindowExW(0, L"STATIC", L"ClickBlitz Benchmark", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 320, 120,
			nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
		ShowWindow(window, options.minimized ? SW_SHOWMINNOACTIVE : SW_SHOWNOACTIVATE);
	}

	Result result;
	std::thread worker(Worker, std::cref(options), std::ref(result));
	const HANDLE workerHandle = worker.native_handle();
	while (MsgWaitForMultipleObjects(1, &workerHandle, FALSE, INFINITE, QS_ALLINPUT) != WAIT_OBJECT_0)
	{
		MSG message;
		while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
			DispatchMessageW(&message);
	}
	worker.join();
	if (window != nullptr)
		DestroyWindow(window);

	Report(options, result);
	return 0;
}
