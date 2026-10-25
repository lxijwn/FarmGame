#include "Gameplay/TrafficModel.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace farm {
using namespace cocos2d;
namespace {
constexpr double pi=3.14159265358979323846;
constexpr double epsilon=1e-7;
bool active(VehiclePhase p) {
    return p==VehiclePhase::Entering || p==VehiclePhase::Driving || p==VehiclePhase::Returning || p==VehiclePhase::Exiting;
}
float fraction(double elapsed,double seconds) { return float(std::max(0.0,std::min(1.0,elapsed/seconds))); }
Pose blend(const Pose& from,const Pose& to,float t) {
    auto delta=std::fmod(to.rotation-from.rotation+540.0f,360.0f)-180;
    return {from.position.lerp(to.position,t),from.rotation+delta*t,from.scale+(to.scale-from.scale)*t};
}
void require(bool condition,const char* message) { if(!condition) throw std::logic_error(message); }
}
const char* phaseName(VehiclePhase p) {
    switch(p) {
    case VehiclePhase::Parked:return "parked";
    case VehiclePhase::Entering:return "entering";
    case VehiclePhase::Driving:return "driving";
    case VehiclePhase::Returning:return "returning";
    case VehiclePhase::Waiting:return "waiting";
    case VehiclePhase::Exiting:return "exiting";
    default:return "completed";
    }
}
const char* launchMessage(LaunchResult r) {
    switch(r) {
    case LaunchResult::Accepted:return "已出发，未满到终点后回场";
    case LaunchResult::Paused:return "已暂停，请先继续";
    case LaunchResult::UnknownVehicle:return "车辆不存在";
    case LaunchResult::Busy:return "这辆车正在行驶，请等待回场";
    case LaunchResult::NotQueueHead:return "请先发出同列前方的车辆";
    case LaunchResult::QueueMoving:return "车辆正在前移，请稍候";
    case LaunchResult::RoadFull:return "道路已满，请等待车辆回场";
    case LaunchResult::WaitingFull:return "等待位已全部绑定，请点击等待区车辆";
    case LaunchResult::Finished:return "本局已结束，请重新开始";
    default:return "发车间隔为0.8秒，请稍候";
    }
}
Pose roadPose(const Road& r,double progress) {
    double p=r.closed?progress-std::floor(progress):std::max(0.0,std::min(1.0,progress));
    Pose pose; pose.scale=.5f;
    if(r.circular) {
        const double a=r.startAngle*pi/180-p*2*pi;
        pose.position=r.center+Vec2(float(std::cos(a)*r.radius),float(std::sin(a)*r.radius));
        pose.rotation=float(std::atan2(std::sin(a),-std::cos(a))*180/pi);
    } else {
        double length=0;
        const size_t segments=r.closed?r.points.size():r.points.size()-1;
        for(size_t i=0;i<segments;++i) length+=r.points[i].distance(r.points[(i+1)%r.points.size()]);
        double distance=p*length;
        for(size_t i=0;i<segments;++i) {
            const auto a=r.points[i], b=r.points[(i+1)%r.points.size()];
            const auto segment=a.distance(b);
            if(distance<segment || i+1==segments) {
                pose.position=a.lerp(b,float(distance/segment));
                const auto d=b-a;
                pose.rotation=float(std::atan2(d.x,d.y)*180/pi); break;
            }
            distance-=segment;
        }
    }
    return pose;
}
TrafficModel::TrafficModel(const Level& level,const TrafficRules& rules) : level_(level),rules_(rules) { reset(); }
void TrafficModel::reset() {
    states_.clear(); queues_.clear(); queueReady_.assign(level_.columns.size(),0);
    owners_.assign(rules_.waitingCapacity,""); now_=0; lastLaunch_=-1e30; paused_=false;
    require(owners_.size()==level_.waiting.size(),"Waiting positions/capacity mismatch");
    for(size_t ci=0;ci<level_.columns.size();++ci) {
        queues_.emplace_back(level_.columns[ci].begin(),level_.columns[ci].end());
        for(size_t ri=0;ri<level_.columns[ci].size();++ri) {
            VehicleState state; state.column=int(ci);
            state.parkingFrom=state.parkingTo=Vec2(level_.columnX[ci],level_.rowY[ri]);
            states_.emplace(level_.columns[ci][ri],state);
        }
    }
    checkInvariants();
}
int TrafficModel::roadUsed() const {
    int n=0; for(const auto& v:states_) if(active(v.second.phase)) ++n; return n;
}
int TrafficModel::waitingUsed() const {
    return int(std::count_if(owners_.begin(),owners_.end(),[](const std::string& id){return !id.empty();}));
}
double TrafficModel::duration(VehiclePhase p) const {
    switch(p) {
    case VehiclePhase::Entering:return rules_.enterSeconds;
    case VehiclePhase::Driving:return level_.road.lapSeconds;
    case VehiclePhase::Returning:return rules_.returnSeconds;
    case VehiclePhase::Exiting:return rules_.exitSeconds;
    default:return 1e30;
    }
}
Pose TrafficModel::poseAt(const VehicleState& s,double time) const {
    if(s.phase==VehiclePhase::Parked)
        return {s.parkingFrom.lerp(s.parkingTo,fraction(time-s.parkingStarted,rules_.queueSeconds)),0,1};
    if(s.phase==VehiclePhase::Waiting) return {level_.waiting.at(s.waitingSlot),0,.8f};
    if(s.phase==VehiclePhase::Driving) return roadPose(level_.road,fraction(time-s.started,level_.road.lapSeconds));
    if(s.phase==VehiclePhase::Completed) return s.to;
    if(s.phase==VehiclePhase::Returning && !level_.road.closed) {
        const float t=fraction(time-s.started,rules_.returnSeconds);
        if(t<.5f) return {s.from.position,s.from.rotation,s.from.scale*(1-2*t)};
        return {s.to.position,0,s.to.scale*(2*t-1)};
    }
    return blend(s.from,s.to,fraction(time-s.started,duration(s.phase)));
}
Pose TrafficModel::pose(const std::string& id) const { return poseAt(states_.at(id),now_); }
double TrafficModel::progress(const std::string& id) const {
    const auto& s=state(id);
    return s.phase==VehiclePhase::Driving?std::max(0.0,std::min(1.0,(now_-s.started)/level_.road.lapSeconds)):0;
}
LaunchResult TrafficModel::launch(const std::string& id) {
    if(paused_) return LaunchResult::Paused;
    auto found=states_.find(id);
    if(found==states_.end()) return LaunchResult::UnknownVehicle;
    auto& s=found->second;
    const bool fresh=s.phase==VehiclePhase::Parked;
    if(!fresh && s.phase!=VehiclePhase::Waiting) return LaunchResult::Busy;
    if(fresh) {
        if(queues_[s.column].empty() || queues_[s.column].front()!=id) return LaunchResult::NotQueueHead;
        if(now_+epsilon<queueReady_[s.column]) return LaunchResult::QueueMoving;
    }
    if(roadUsed()>=rules_.roadCapacity) return LaunchResult::RoadFull;
    auto free=std::find(owners_.begin(),owners_.end(),"");
    if(fresh && free==owners_.end()) return LaunchResult::WaitingFull;
    if(now_+epsilon<lastLaunch_+rules_.launchInterval) return LaunchResult::Cooldown;
    // All rejection paths above leave occupancy, load and queues unchanged.
    const auto from=poseAt(s,now_);
    if(fresh) {
        s.waitingSlot=int(free-owners_.begin()); *free=id;
        queues_[s.column].pop_front();
        for(size_t row=0;row<queues_[s.column].size();++row) {
            auto& follower=states_.at(queues_[s.column][row]);
            follower.parkingFrom=poseAt(follower,now_).position;
            follower.parkingTo=Vec2(level_.columnX[s.column],level_.rowY[row]);
            follower.parkingStarted=now_;
        }
        queueReady_[s.column]=now_+rules_.queueSeconds;
    }
    s.from=from; s.to=roadPose(level_.road,0); s.started=now_;
    s.phase=VehiclePhase::Entering; s.lapEnded=false; ++s.trips; lastLaunch_=now_;
    checkInvariants(); return LaunchResult::Accepted;
}
void TrafficModel::advance(double seconds) {
    require(std::isfinite(seconds) && seconds>=0,"Invalid traffic time step");
    if(paused_) return;
    now_+=seconds;
    for(auto& item:states_) {
        auto& s=item.second;
        // Process exact boundaries, including several transitions in one slow frame.
        while(active(s.phase) && s.started+duration(s.phase)<=now_+epsilon) {
            const auto boundary=s.started+duration(s.phase);
            switch(s.phase) {
            case VehiclePhase::Entering:s.phase=VehiclePhase::Driving; break;
            case VehiclePhase::Driving:
                if(s.reserved>0) {s.lapEnded=true;break;}
                beginReturn(s,boundary); break;
            case VehiclePhase::Returning:s.phase=VehiclePhase::Waiting; break;
            case VehiclePhase::Exiting:
                s.phase=VehiclePhase::Completed; owners_.at(s.waitingSlot).clear(); s.waitingSlot=-1; break;
            default:break;
            }
            if(s.phase==VehiclePhase::Driving && s.lapEnded) break;
            s.started=boundary;
        }
    }
    checkInvariants();
}
void TrafficModel::commitCollected(const std::string& id,int amount) {
    auto& s=states_.at(id);
    require(s.phase==VehiclePhase::Driving && amount>0 && s.loaded+s.reserved+amount<=level_.vehicles.at(id).capacity,
            "Invalid collection commit");
    s.loaded+=amount;
    afterCollection(id,s);
    checkInvariants();
}
void TrafficModel::beginReturn(VehicleState& s,double time) {
    s.from=poseAt(s,time);s.to={level_.waiting.at(s.waitingSlot),0,.8f};
    s.phase=VehiclePhase::Returning;s.started=time;
}
void TrafficModel::afterCollection(const std::string& id,VehicleState& s) {
    if(s.loaded==level_.vehicles.at(id).capacity && s.reserved==0) {
        s.from=poseAt(s,now_); s.to=s.from;
        s.to.position+=Vec2(0,75); s.to.scale=0;
        s.phase=VehiclePhase::Exiting; s.started=now_;
    } else if(s.lapEnded && s.reserved==0) beginReturn(s,now_);
}
int TrafficModel::available(const std::string& id) const {
    const auto& s=state(id);return level_.vehicles.at(id).capacity-s.loaded-s.reserved;
}
bool TrafficModel::reserveCapacity(const std::string& id,int amount) {
    auto& s=states_.at(id);
    if(s.phase!=VehiclePhase::Driving || s.lapEnded || amount<=0 || available(id)<amount) return false;
    s.reserved+=amount;checkInvariants();return true;
}
void TrafficModel::commitReserved(const std::string& id,int amount) {
    auto& s=states_.at(id);
    require(s.phase==VehiclePhase::Driving && amount>0 && s.reserved>=amount,"Invalid reserved commit");
    s.reserved-=amount;s.loaded+=amount;afterCollection(id,s);
    checkInvariants();
}
double TrafficModel::nextTransitionTime() const {
    double next=1e30;
    for(const auto& item:states_) {
        const auto& s=item.second;
        if(active(s.phase) && !(s.phase==VehiclePhase::Driving && s.lapEnded)) next=std::min(next,s.started+duration(s.phase));
    }
    return next;
}
bool TrafficModel::queuesSettled() const {
    for(auto time:queueReady_) if(time>now_+epsilon) return false;
    return true;
}
void TrafficModel::checkInvariants() const {
    require(roadUsed()<=rules_.roadCapacity && waitingUsed()<=rules_.waitingCapacity,"Traffic capacity overflow");
    std::set<std::string> queued, bound;
    for(size_t ci=0;ci<queues_.size();++ci) for(const auto& id:queues_[ci]) {
        require(queued.insert(id).second,"Vehicle queued twice");
        require(states_.at(id).phase==VehiclePhase::Parked && states_.at(id).column==int(ci),"Invalid parked vehicle");
    }
    for(size_t slot=0;slot<owners_.size();++slot) if(!owners_[slot].empty()) {
        const auto& id=owners_[slot]; const auto& s=states_.at(id);
        require(bound.insert(id).second && s.waitingSlot==int(slot),"Invalid waiting reservation");
        require(active(s.phase) || s.phase==VehiclePhase::Waiting,"Orphaned waiting reservation");
    }
    for(const auto& item:states_) {
        const auto& s=item.second;
        require(s.loaded>=0 && s.reserved>=0 && s.loaded+s.reserved<=level_.vehicles.at(item.first).capacity,"Invalid vehicle load");
        if(s.reserved>0) require(s.phase==VehiclePhase::Driving,"Reservation outside road trip");
        if(s.phase==VehiclePhase::Parked) require(queued.count(item.first)==1 && s.waitingSlot==-1,"Lost parked vehicle");
        else if(s.phase==VehiclePhase::Completed) require(s.waitingSlot==-1 && s.loaded==level_.vehicles.at(item.first).capacity,"Invalid completion");
        else require(bound.count(item.first)==1,"Active vehicle has no reserved return position");
    }
    require(waitingUsed()>=roadUsed(),"Road vehicle has no return space");
}
}
