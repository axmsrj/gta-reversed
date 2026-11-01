#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace gtasa::native {

struct FrameTiming {
    double realSeconds{};
    double simulationSeconds{};
    float timeStep{}; // GTA units: one unit is 20 ms (50 Hz).
    double gameMilliseconds{};
    std::uint64_t frameNumber{};
};

class GameTimer {
public:
    using Clock=std::chrono::steady_clock;

    explicit GameTimer(Clock::time_point started) : previous_(started) {}

    void SetPaused(bool paused) noexcept { paused_=paused; }
    void SetTimeScale(float scale) {
        if (!std::isfinite(scale) || scale<0.0f)
            throw std::invalid_argument("Invalid game time scale");
        scale_=scale;
    }

    FrameTiming Tick(Clock::time_point now) {
        if (now<previous_) now=previous_;
        const double seconds=std::chrono::duration<double>(now-previous_).count();
        previous_=now;
        return Advance(seconds);
    }

    FrameTiming Advance(double realSeconds) {
        if (!std::isfinite(realSeconds) || realSeconds<0.0)
            throw std::invalid_argument("Invalid elapsed time");
        const double scaled=paused_ ? 0.0 : realSeconds*scale_;
        const double simulation=std::min(scaled,0.06); // CTimer clips to three 20 ms steps.
        gameMilliseconds_+=std::min(scaled,0.3)*1000.0;
        return {realSeconds,simulation,static_cast<float>(simulation*50.0),
                gameMilliseconds_,++frameNumber_};
    }

private:
    Clock::time_point previous_;
    double gameMilliseconds_{};
    std::uint64_t frameNumber_{};
    float scale_{1.0f};
    bool paused_{};
};

} // namespace gtasa::native
