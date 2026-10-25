#pragma once
#include "Data/Catalog.h"
#include "Gameplay/GameModel.h"
#include <functional>

namespace farm {
struct Options {
    bool smoke = false, demo = false;
    std::string resources, reportDirectory, startLevel;
};
struct ViewCounts { int fruit=0, vehicles=0, waiting=0, entrances=0; };
class Runtime {
public:
    explicit Runtime(Options options) : options_(std::move(options)) {}
    bool start();
    void showMenu();
    void openLevel(const std::string& id);
    void reloadLevel();
    void guard(const std::function<void()>& action);
    Catalog& catalog() { return catalog_; }
    void recordView(const Level& level, const ViewCounts& counts);
    void recordKey(cocos2d::EventKeyboard::KeyCode key, bool pressed);
    void recordTrafficInput(const std::string& id,LaunchResult result);
    int exitCode() const { return failed_ ? 1 : 0; }
    bool manualInputEnabled() const { return !options_.smoke; }
    bool keyInputEnabled() const { return !options_.smoke || injectingKey_; }
    float simulationSpeed() const {return !options_.smoke?1.f:captureBusy_?0.f:(!sawFlight_ || !sawRefill_)?3.f:12.f;}
    double simulationDelta(float dt) const {return options_.demo?(captureBusy_?0.0:1.0/30.0):dt*simulationSpeed();}
private:
    void tick(float dt);
    void tickDemo();
    void recordWitness(const GameModel& game,const WitnessStep& step);
    void capture(const std::string& name, const std::function<void()>& next);
    void fail(const std::string& error);
    void writeReport();
    struct VehicleAudit {std::string id,phase;int slot=0,loaded=0,trips=0;Pose pose;};
    struct TrafficAudit {
        std::string capture,levelId;
        double time=0;
        int road=0,waiting=0,remaining=0,collected=0,reserved=0,pickups=0;
        std::string outcome;
        std::vector<int> entranceCursors;
        bool paused=false;
        std::vector<VehicleAudit> vehicles;
    };
    TrafficAudit audit(const std::string& capture,const GameModel& game) const;
    Options options_;
    Catalog catalog_;
    bool failed_=false, passed_=false, captureBusy_=false;
    bool injectingKey_=false;
    int frames_=0, stage_=0, stageFrame_=0, menuVisits_=0, reloads_=0;
    int keyEvents_=0, lastKey_=0;
    bool lastKeyPressed_=false;
    float elapsed_=0;
    std::string current_, error_, vendor_, renderer_, glVersion_;
    std::vector<std::string> screenshots_;
    std::vector<std::string> trafficChecks_;
    std::vector<std::string> gameplayChecks_;
    struct StepAudit {std::string level,vehicle,phase;int before=0,after=0,remaining=0,slot=0;std::vector<std::string> fruit;std::vector<int> slots;};
    std::vector<StepAudit> stepObservations_;
    size_t witnessIndex_=0,pickupStart_=0;
    bool tripActive_=false,sawFlight_=false,sawRefill_=false,sawWaiting_=false;
    int demoFrames_=0,demoHoldUntil_=30;
    std::vector<TrafficAudit> trafficObservations_;
    std::string trafficVehicle_,lastTrafficInput_;
    int trafficInputs_=0,lastLaunchResult_=0;
    struct View { std::string id; ViewCounts counts; };
    std::vector<View> views_;
};
}
