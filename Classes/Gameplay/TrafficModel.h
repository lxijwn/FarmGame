#pragma once
#include "Data/Catalog.h"
#include <deque>

namespace farm {
enum class VehiclePhase { Parked, Entering, Driving, Returning, Waiting, Exiting, Completed };
const char* phaseName(VehiclePhase phase);
struct Pose { cocos2d::Vec2 position; float rotation=0, scale=1; };
Pose roadPose(const Road& road,double progress);
struct VehicleState {
    VehiclePhase phase=VehiclePhase::Parked;
    int column=-1, waitingSlot=-1, loaded=0, reserved=0, trips=0;
    bool lapEnded=false;
    double started=0, parkingStarted=0;
    Pose from, to;
    cocos2d::Vec2 parkingFrom, parkingTo;
};
enum class LaunchResult { Accepted, Paused, UnknownVehicle, Busy, NotQueueHead, QueueMoving, RoadFull, WaitingFull, Cooldown, Finished };
const char* launchMessage(LaunchResult result);

// Owns traffic state and clocks; no actions or asynchronous callbacks mutate it.
// Scene replacement destroys this model, so old animations cannot affect a new run.
class TrafficModel {
public:
    TrafficModel(const Level& level,const TrafficRules& rules);
    LaunchResult launch(const std::string& id);
    void advance(double seconds);
    void reset();
    void setPaused(bool paused) { paused_=paused; }
    bool paused() const { return paused_; }
    Pose pose(const std::string& id) const;
    double progress(const std::string& id) const;
    const VehicleState& state(const std::string& id) const { return states_.at(id); }
    const std::map<std::string,VehicleState>& states() const { return states_; }
    const std::vector<std::string>& waitingOwners() const { return owners_; }
    const std::vector<std::deque<std::string>>& queues() const { return queues_; }
    int roadUsed() const;
    int waitingUsed() const;
    double now() const { return now_; }
    const std::string& levelId() const { return level_.id; }
    void checkInvariants() const;
    // Hook for the future collection system. No UI control grants fruit.
    void commitCollected(const std::string& id,int amount);
    bool reserveCapacity(const std::string& id,int amount);
    void commitReserved(const std::string& id,int amount);
    int available(const std::string& id) const;
    double nextTransitionTime() const;
    bool queuesSettled() const;
private:
    void afterCollection(const std::string& id,VehicleState& state);
    void beginReturn(VehicleState& state,double time);
    Pose poseAt(const VehicleState& state,double time) const;
    double duration(VehiclePhase phase) const;
    const Level& level_;
    TrafficRules rules_;
    std::map<std::string,VehicleState> states_;
    std::vector<std::deque<std::string>> queues_;
    std::vector<double> queueReady_;
    std::vector<std::string> owners_;
    double now_=0, lastLaunch_=-1e30;
    bool paused_=false;
};
}
