#pragma once
#include "Gameplay/BoardModel.h"

namespace farm {
enum class Outcome {Running,Won,Stuck};
const char* outcomeName(Outcome outcome);
struct PickupEvent {double time=0;std::string vehicle,fruit;int slot=-1;};
class GameModel {
public:
    GameModel(const Level& level,const TrafficRules& rules);
    void reset();
    LaunchResult launch(const std::string& vehicle);
    void advance(double seconds);
    TrafficModel& traffic() {return traffic_;}
    const TrafficModel& traffic() const {return traffic_;}
    const BoardModel& board() const {return board_;}
    bool idle() const;
    Outcome outcome() const {return outcome_;}
    const std::vector<PickupEvent>& pickups() const {return pickups_;}
    void checkInvariants() const;
private:
    void processDue();
    void updateOutcome();
    const Level& level_;
    TrafficRules rules_;
    TrafficModel traffic_;
    BoardModel board_;
    Outcome outcome_=Outcome::Running;
    struct Contact {double progress=0;int slot=-1;};
    std::vector<Contact> contacts_;
    std::map<std::string,size_t> nextContact_;
    std::vector<PickupEvent> pickups_;
};
}
