#pragma once

#include <chrono>

class FPSLimiter
{
public:
	FPSLimiter(int fps);
	void Regulate();
	float AverageFPS() const { return 1000 / avgFrameTime; }
private:
	float simFrameTime;
	float avgFrameTime;
	int fpsDelay;
	int adjustmentCtr;
	std::chrono::time_point<std::chrono::high_resolution_clock> lastTime;
};