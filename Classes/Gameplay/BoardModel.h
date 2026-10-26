#pragma once
#include "Gameplay/TrafficModel.h"

namespace farm {
enum class FruitPhase { Future, Settled, Collecting, Moving, Collected };
struct FruitPose {cocos2d::Vec2 position;cocos2d::Size size;float rotation=0,opacity=255;};
struct FruitState {
    bool spawning=false;
    std::string id,color,vehicle;
    FruitPhase phase=FruitPhase::Future;
    int slot=-1;
    double start=0,end=0;
    unsigned serial=0;
    FruitPose from,to;
};
class BoardModel {
public:
    BoardModel(const Level& level,const TrafficRules& rules,uint32_t spawnSeed=0);
    void reset();
    bool directContact(int slot) const;
    bool canTake(int slot,const std::string& color,double time) const;
    std::vector<int> collectionCandidates(int front,const std::string& color,double time,int capacity) const;
    void reserveFruit(int slot,const std::string& vehicle,double time);
    void finishEvents(double time,TrafficModel& traffic);
    double nextEventTime() const;
    bool idle() const;
    int remaining() const {return level_.total-collected_;}
    int collected() const {return collected_;}
    int futureCount(int entrance) const;
    const std::vector<int>& entranceCursors() const {return cursors_;}
    uint32_t spawnSeed() const {return spawnSeed_;}
    const std::vector<std::string>& slots() const {return slots_;}
    const std::map<std::string,FruitState>& fruits() const {return fruits_;}
    FruitPose pose(const FruitState& fruit,const TrafficModel& traffic) const;
    void checkInvariants(const TrafficModel& traffic) const;
private:
    FruitPose slotPose(int index) const;
    void moveTo(FruitState& fruit,int dest,const FruitPose& from,double time);
    void refill(int group,double time,int radialColumn=-1);
    const Level& level_;
    TrafficRules rules_;
    std::map<std::string,FruitState> fruits_;
    std::vector<std::string> slots_;
    std::vector<int> cursors_;
    uint32_t spawnSeed_;
    std::vector<std::vector<std::string>> spawnBags_;
    std::vector<double> groupReady_;
    int collected_=0;
    unsigned serial_=0;
};
}
