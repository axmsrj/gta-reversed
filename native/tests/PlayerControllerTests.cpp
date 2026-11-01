#include "PlayerController.h"

#include <cassert>
#include <cmath>
#include <optional>

int main() {
    gtasa::native::PlayerController player({0,0,1},-1);
    const auto flat=[](float,float,float) -> std::optional<float> { return 0.0f; };
    assert(player.Step(0.02f,1,0,flat));
    assert(std::abs(player.Position()[0]-0.031f)<1.0e-5f);
    assert(player.Position()[2]==1.0f);
    assert(player.Heading()==0.0f);
    assert(player.Step(0.02f,1,1,flat));
    assert(player.Position()[0]>0.031f && player.Position()[1]>0.0f);
    assert(std::abs(player.Heading()-0.7853982f)<1.0e-5f);
    const auto before=player.Position();
    const float heading=player.Heading();
    const auto gap=[](float,float,float) -> std::optional<float> { return {}; };
    assert(!player.Step(0.02f,1,0,gap) && player.Position()==before);
    assert(player.Heading()==heading);
    const auto step=[](float,float,float) -> std::optional<float> { return 0.5f; };
    assert(player.Step(0.02f,1,0,step));
    assert(player.Position()[2]==1.5f);
    const auto wall=[](float,float,float) -> std::optional<float> { return 3.0f; };
    assert(!player.Step(0.02f,1,0,wall));
    const auto drop=[](float,float,float) -> std::optional<float> { return -3.0f; };
    assert(!player.Step(0.02f,1,0,drop));
    const auto blocked=[](std::array<float,3> from,std::array<float,3> to) {
        assert(to[0]>from[0]);
        assert(to[2]==0.0f);
        return true;
    };
    const auto beforeObstacle=player.Position();
    const float beforeHeading=player.Heading();
    assert(!player.Step(0.02f,1,0,flat,blocked));
    assert(player.Position()==beforeObstacle && player.Heading()==beforeHeading);
    gtasa::native::PlayerController sliding({0,0,1},-1);
    const auto verticalWall=[](std::array<float,3>,std::array<float,3> to) {
        return to[0]>0.02f;
    };
    assert(sliding.Step(0.02f,1,1,flat,verticalWall));
    assert(sliding.Position()[0]==0.0f && sliding.Position()[1]>0.0f);
    assert(std::abs(sliding.Heading()-1.5707963f)<1.0e-5f);
    const auto noPath=[](std::array<float,3>,std::array<float,3>) { return true; };
    const auto stopped=sliding.Position();
    assert(!sliding.Step(0.02f,1,1,flat,noPath));
    assert(sliding.Position()==stopped);
    gtasa::native::PlayerController clipSpeed({0,0,1},-1,2.0f);
    assert(clipSpeed.Step(0.05f,1,0,flat));
    assert(std::abs(clipSpeed.Position()[0]-0.1f)<1.0e-5f);
    assert(clipSpeed.Step(0.05f,1,0,flat,{},0.04f));
    assert(std::abs(clipSpeed.Position()[0]-0.14f)<1.0e-5f);
    return 0;
}
