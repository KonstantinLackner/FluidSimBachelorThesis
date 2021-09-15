#include "FPSLimiter.h"

#include <thread>

constexpr auto FrameDelayAdjustmentMod = 4;

FPSLimiter::FPSLimiter(int fps)
    : simFrameTime(1000.0f / fps)
    , adjustmentCtr(FrameDelayAdjustmentMod)
    , avgFrameTime(0)
    , fpsDelay(int(simFrameTime * 0.6))
{
}

void FPSLimiter::Regulate()
{
    if (fpsDelay != 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(fpsDelay));

    if (--adjustmentCtr == 0)
    {
        auto now = std::chrono::high_resolution_clock::now();

        if (lastTime.time_since_epoch().count() != 0)
        {
            std::chrono::duration<float, std::milli> time_taken = now - lastTime;
            avgFrameTime = time_taken.count() / FrameDelayAdjustmentMod;

            float diff = simFrameTime - avgFrameTime;
            if (diff >= 1.0f)
                fpsDelay++;
            else if (diff <= -1.0f)
                fpsDelay = fpsDelay > 0 ? fpsDelay - 1 : 0;
        }

        lastTime = now;
        adjustmentCtr = FrameDelayAdjustmentMod;
    }
}
