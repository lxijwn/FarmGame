#include "Gameplay/GameModel.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace farm {
namespace {
constexpr double eps=1e-7;
void require(bool condition,const char* message) {if(!condition) throw std::logic_error(message);}
}
const char* outcomeName(Outcome o) {return o==Outcome::Won?"won":o==Outcome::Stuck?"stuck":"running";}
GameModel::GameModel(const Level& level,const TrafficRules& rules)
    :level_(level),rules_(rules),traffic_(level,rules),board_(level,rules) {
    if(level.road.circular) for(int slot:level.groups.at(0).slots) contacts_.push_back({level.fruit[slot].contact,slot});
    else for(const auto& group:level.groups) {require(!group.slots.empty(),"Empty refill chain");contacts_.push_back({group.contact,group.slots[0]});}
    for(const auto& c:contacts_) require(c.progress>=0 && c.progress<1,"Contact progress outside single lap");
    std::sort(contacts_.begin(),contacts_.end(),[&](const Contact& a,const Contact& b) {
        return a.progress!=b.progress?a.progress<b.progress:level_.fruit[a.slot].id<level_.fruit[b.slot].id;
    });
    reset();
}
void GameModel::reset() {
    traffic_.reset();board_.reset();nextContact_.clear();pickups_.clear();outcome_=Outcome::Running;
    for(const auto& v:traffic_.states()) nextContact_[v.first]=0;
    updateOutcome();checkInvariants();
}
LaunchResult GameModel::launch(const std::string& id) {
    if(outcome_!=Outcome::Running) return LaunchResult::Finished;
    auto result=traffic_.launch(id);
    if(result==LaunchResult::Accepted) nextContact_[id]=0;
    checkInvariants();return result;
}
bool GameModel::idle() const {return traffic_.roadUsed()==0 && traffic_.queuesSettled() && board_.idle();}
void GameModel::processDue() {
    traffic_.advance(0);board_.finishEvents(traffic_.now(),traffic_);
    std::vector<PickupEvent> due;
    for(const auto& item:traffic_.states()) {
        const auto& s=item.second;
        if(s.phase!=VehiclePhase::Driving || s.lapEnded) continue;
        auto& next=nextContact_.at(item.first);
        while(next<contacts_.size() && s.started+contacts_[next].progress*level_.road.lapSeconds<=traffic_.now()+eps) {
            const auto& c=contacts_[next++];due.push_back({s.started+c.progress*level_.road.lapSeconds,item.first,"",c.slot});
        }
    }
    std::sort(due.begin(),due.end(),[&](const PickupEvent& a,const PickupEvent& b) {
        if(a.time!=b.time) return a.time<b.time;
        if(a.vehicle!=b.vehicle) return a.vehicle<b.vehicle;
        return level_.fruit[a.slot].id<level_.fruit[b.slot].id;
    });
    for(auto event:due) {
        const auto& v=level_.vehicles.at(event.vehicle);
        auto candidates=board_.collectionCandidates(event.slot,v.color,traffic_.now(),traffic_.available(event.vehicle));
        for(int slot:candidates) {
            event.slot=slot;event.fruit=board_.slots()[slot];
            require(traffic_.reserveCapacity(event.vehicle,1),"Batch exceeded available capacity");
            board_.reserveFruit(slot,event.vehicle,traffic_.now());pickups_.push_back(event);
        }
    }
    updateOutcome();
}
void GameModel::advance(double seconds) {
    require(std::isfinite(seconds) && seconds>=0,"Invalid game time step");
    if(traffic_.paused() || outcome_!=Outcome::Running) return;
    const auto target=traffic_.now()+seconds;
    for(int iteration=0;iteration<1000000;++iteration) {
        processDue();
        if(outcome_!=Outcome::Running || traffic_.now()>=target-eps) {checkInvariants();return;}
        double next=std::min(target,std::min(traffic_.nextTransitionTime(),board_.nextEventTime()));
        for(const auto& item:traffic_.states()) {
            if(item.second.phase!=VehiclePhase::Driving || item.second.lapEnded) continue;
            const auto index=nextContact_.at(item.first);
            if(index<contacts_.size()) next=std::min(next,item.second.started+contacts_[index].progress*level_.road.lapSeconds);
        }
        require(next>traffic_.now(),"Game event clock stalled");
        traffic_.advance(next-traffic_.now());
    }
    throw std::logic_error("Game event iteration limit exceeded");
}
void GameModel::updateOutcome() {
    if(!idle()) return;
    if(board_.remaining()==0) {outcome_=Outcome::Won;return;}
    bool hasNew=false;
    for(const auto& q:traffic_.queues()) if(!q.empty()) hasNew=true;
    if(hasNew && traffic_.waitingUsed()<rules_.waitingCapacity) return;
    for(const auto& v:traffic_.states()) {
        if(v.second.phase!=VehiclePhase::Waiting || traffic_.available(v.first)<=0) continue;
        for(const auto& contact:contacts_)
            if(board_.canTake(contact.slot,level_.vehicles.at(v.first).color,traffic_.now())) return;
    }
    outcome_=Outcome::Stuck;
}
void GameModel::checkInvariants() const {traffic_.checkInvariants();board_.checkInvariants(traffic_);}
}
