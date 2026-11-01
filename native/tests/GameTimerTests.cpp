#include "GameTimer.h"

#include <cassert>
#include <chrono>
#include <stdexcept>

int main() {
    using namespace std::chrono_literals;
    const auto start=gtasa::native::GameTimer::Clock::time_point{};
    gtasa::native::GameTimer timer(start);
    const auto first=timer.Tick(start+20ms);
    assert(first.realSeconds==0.02 && first.timeStep==1.0f);
    assert(first.gameMilliseconds==20.0 && first.frameNumber==1);

    const auto slow=timer.Tick(start+120ms);
    assert(slow.realSeconds==0.1 && slow.timeStep==3.0f);
    assert(slow.simulationSeconds==0.06 && slow.gameMilliseconds==120.0);

    timer.SetTimeScale(0.5f);
    const auto scaled=timer.Advance(0.04);
    assert(scaled.timeStep==1.0f && scaled.gameMilliseconds==140.0);

    timer.SetPaused(true);
    const auto paused=timer.Advance(1.0);
    assert(paused.realSeconds==1.0 && paused.simulationSeconds==0.0);
    assert(paused.gameMilliseconds==140.0 && paused.frameNumber==4);

    timer.SetPaused(false);
    timer.SetTimeScale(1.0f);
    const auto longFrame=timer.Advance(1.0);
    assert(longFrame.timeStep==3.0f && longFrame.gameMilliseconds==440.0);

    bool rejected=false;
    try { timer.SetTimeScale(-1.0f); }
    catch (const std::invalid_argument&) { rejected=true; }
    assert(rejected);
    rejected=false;
    try { (void)timer.Advance(-0.01); }
    catch (const std::invalid_argument&) { rejected=true; }
    assert(rejected);
    return 0;
}
