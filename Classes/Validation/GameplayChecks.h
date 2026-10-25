#pragma once
#include "Gameplay/GameModel.h"
namespace farm {
void beginWitness(GameModel& game,const WitnessStep& step);
void finishWitness(const GameModel& game,const WitnessStep& step,size_t pickupStart);
std::vector<std::string> checkGameplay(const Catalog& catalog);
}
