#include "Validation/TrafficChecks.h"
#include "Gameplay/TrafficModel.h"
#include <cmath>
#include <stdexcept>

namespace farm {
namespace {
void check(bool condition,const char* message) {if(!condition) throw std::runtime_error(std::string("Traffic check: ")+message);}
bool close(cocos2d::Vec2 a,cocos2d::Vec2 b) {return a.distance(b)<.02f;}
}
std::vector<std::string> checkTraffic(const Catalog& catalog) {
    std::vector<std::string> result;
    for(const auto& level:catalog.levels()) {
        const auto& rules=catalog.trafficRules();
        const auto first=level.columns[0][0], other=level.columns[1][0], tail=level.columns[0][1];
        const auto cap=level.vehicles.at(first).capacity;
        const double trip=rules.enterSeconds+level.road.lapSeconds+rules.returnSeconds;
        const auto r=level.road;
        if(r.circular) {
            check(close(roadPose(r,0).position,r.center+cocos2d::Vec2(0,-r.radius)),"circle start");
            check(close(roadPose(r,.25).position,r.center+cocos2d::Vec2(-r.radius,0)),"circle clockwise");
            check(close(roadPose(r,.5).position,r.center+cocos2d::Vec2(0,r.radius)),"circle half lap");
        } else {
            check(!r.closed && close(roadPose(r,0).position,{452,390}),"open road entrance");
            check(close(roadPose(r,.25).position,{312,490}),"open first leg");
            check(close(roadPose(r,.5).position,{172,590}),"open road bend");
            check(close(roadPose(r,.75).position,{312,690}),"open second leg");
            check(close(roadPose(r,1).position,{452,790}),"open road endpoint");
        }
        check(close(roadPose(r,0).position,roadPose(r,1).position)==r.closed,"road endpoint topology");
        result.push_back(level.id+": arc-length road samples and correct open/closed endpoints");
        {
            TrafficModel m(level,rules);
            check(m.launch(tail)==LaunchResult::NotQueueHead && m.waitingUsed()==0,"tail cannot launch or reserve");
            check(m.launch(first)==LaunchResult::Accepted,"first launch");
            const auto slot=m.state(first).waitingSlot;
            check(m.roadUsed()==1 && m.waitingUsed()==1 && m.waitingOwners()[slot]==first,"reserve at admission");
            check(m.launch(first)==LaunchResult::Busy && m.state(first).trips==1,"duplicate click");
            check(m.launch(tail)==LaunchResult::QueueMoving,"queue animation admission lock");
            check(m.launch(other)==LaunchResult::Cooldown,"global launch interval");
            m.advance(rules.enterSeconds+.01);
            check(m.state(first).phase==VehiclePhase::Driving,"enter -> driving");
            check(close(m.pose(tail).position,{level.columnX[0],level.rowY[0]}),"queue follower settles at front");
            auto before=m.pose(first).position; auto time=m.now();
            m.setPaused(true); m.advance(100);
            check(m.now()==time && close(m.pose(first).position,before) && m.launch(other)==LaunchResult::Paused,"pause freezes motion and admission");
            m.setPaused(false); m.advance(trip);
            check(m.state(first).phase==VehiclePhase::Waiting && m.roadUsed()==0 && m.waitingUsed()==1,"empty lap returns and releases road only");
            check(m.state(first).loaded==0 && m.state(first).trips==1,"empty trip preserves load and needs click");
            m.advance(20); check(m.state(first).trips==1,"no automatic relaunch");
            check(m.launch(first)==LaunchResult::Accepted && m.state(first).waitingSlot==slot && m.state(first).trips==2,"relaunch keeps reservation");
            m.reset(); m.advance(30);
            check(m.roadUsed()==0 && m.waitingUsed()==0 && m.state(first).phase==VehiclePhase::Parked && m.state(first).trips==0,"reset removes old trip");
        }
        result.push_back(level.id+": queue heads, cooldown, duplicate clicks, queue shift, pause, zero-load return/relaunch/reset");
        {
            TrafficModel m(level,rules); m.launch(first); m.advance(rules.enterSeconds+.01);
            bool rejected=false;
            try {m.commitCollected(first,cap+1);} catch(const std::logic_error&) {rejected=true;}
            check(rejected && m.state(first).loaded==0,"over-capacity commit has no side effect");
            m.commitCollected(first,cap-1); m.advance(trip);
            const auto slot=m.state(first).waitingSlot;
            check(m.state(first).phase==VehiclePhase::Waiting && m.state(first).loaded==cap-1,"partial load survives return");
            m.launch(first); m.advance(rules.enterSeconds+.01);
            check(m.state(first).loaded==cap-1 && m.state(first).waitingSlot==slot,"partial load survives relaunch");
            m.commitCollected(first,1);
            check(m.state(first).phase==VehiclePhase::Exiting && m.roadUsed()==1 && m.waitingUsed()==1,"full car reserves until exit completes");
            m.advance(rules.exitSeconds*.5); check(m.waitingUsed()==1,"no early release during exit");
            m.advance(rules.exitSeconds);
            check(m.state(first).phase==VehiclePhase::Completed && m.waitingUsed()==0 && m.roadUsed()==0,"full exit releases both quotas");
        }
        result.push_back(level.id+": isolated traffic hook checks partial-load persistence, capacity guard and full-exit release");
        {
            TrafficModel m(level,rules);
            for(int i=0;i<rules.waitingCapacity;++i) {
                std::string id;
                for(const auto& q:m.queues()) if(!q.empty()) {id=q.front();break;}
                check(!id.empty() && m.launch(id)==LaunchResult::Accepted,"fill road with legal front vehicles");
                m.advance(rules.launchInterval+.001);
            }
            check(m.roadUsed()==5 && m.waitingUsed()==5,"five travelling vehicles reserve all return positions");
            std::string sixth;
            for(const auto& q:m.queues()) if(!q.empty()) {sixth=q.front();break;}
            check(!sixth.empty() && m.launch(sixth)==LaunchResult::RoadFull,"sixth car cannot overfill road");
            m.advance(20);
            check(m.roadUsed()==0 && m.waitingUsed()==5 && m.launch(sixth)==LaunchResult::WaitingFull,"full waiting blocks new admission after return");
            const auto id=m.waitingOwners()[0];
            check(m.launch(id)==LaunchResult::Accepted && m.waitingUsed()==5,"waiting car can relaunch at full binding capacity");
        }
        result.push_back(level.id+": five reserved slots, sixth-car refusal and relaunch with waiting full");
        {
            TrafficModel slow(level,rules),fast(level,rules);
            slow.launch(first); fast.launch(first); slow.advance(25);
            for(int i=0;i<2500;++i) fast.advance(.01);
            check(slow.state(first).phase==fast.state(first).phase && close(slow.pose(first).position,fast.pose(first).position) && slow.waitingUsed()==fast.waitingUsed(),"large frame and fine steps have same final state");
            for(double t:{.1,rules.enterSeconds+1,rules.enterSeconds+level.road.lapSeconds+.1}) {
                TrafficModel m(level,rules); m.launch(first); m.advance(t); m.reset(); m.advance(50);
                check(m.waitingUsed()==0 && m.roadUsed()==0 && m.state(first).phase==VehiclePhase::Parked,"reset during moving phase invalidates old motion");
            }
            TrafficModel m(level,rules); m.launch(first); m.advance(rules.launchInterval+.01); m.launch(other); m.advance(1);
            double gap=m.progress(first)-m.progress(other); m.advance(2);
            check(gap>0 && std::abs(m.progress(first)-m.progress(other)-gap)<1e-7,"same speed preserves order and spacing");
        }
        result.push_back(level.id+": slow-frame equivalence, in-flight reset and no overtaking");
    }
    return result;
}
}
