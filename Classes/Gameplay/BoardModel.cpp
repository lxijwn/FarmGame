#include "Gameplay/BoardModel.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace farm {
namespace {
constexpr double eps=1e-7;
void require(bool test,const char* error) {if(!test) throw std::logic_error(error);}
float blend(float a,float b,float t) {return a+(b-a)*t;}
FruitPose interpolate(const FruitPose& a,const FruitPose& b,float t) {
    FruitPose p;p.position=a.position.lerp(b.position,t);
    p.size=cocos2d::Size(blend(a.size.width,b.size.width,t),blend(a.size.height,b.size.height,t));
    float delta=std::fmod(b.rotation-a.rotation+540.f,360.f)-180.f;
    p.rotation=a.rotation+delta*t;p.opacity=blend(a.opacity,b.opacity,t);return p;
}
}
BoardModel::BoardModel(const Level& level,const TrafficRules& rules):level_(level),rules_(rules) {reset();}
void BoardModel::reset() {
    fruits_.clear();slots_.assign(level_.fruit.size(),"");cursors_.assign(level_.entrances.size(),0);
    groupReady_.assign(level_.groups.size(),0);collected_=0;serial_=0;
    for(size_t i=0;i<level_.fruit.size();++i) {
        const auto& def=level_.fruit[i];if(def.entityId.empty()) continue;
        FruitState fruit;fruit.id=def.entityId;fruit.color=def.color;fruit.slot=int(i);fruit.phase=FruitPhase::Settled;
        require(fruits_.emplace(fruit.id,fruit).second,"Duplicate initial fruit");slots_[i]=fruit.id;
    }
    for(const auto& entrance:level_.entrances) for(const auto& def:entrance.sequence) {
        FruitState fruit;fruit.id=def.id;fruit.color=def.color;
        require(fruits_.emplace(fruit.id,fruit).second,"Duplicate future fruit");
    }
    require(int(fruits_.size())==level_.total,"Fruit definitions/target mismatch");
    if(!level_.road.circular) for(size_t i=0;i<level_.groups.size();++i) refill(int(i),0);
}
FruitPose BoardModel::slotPose(int index) const {
    const auto& s=level_.fruit.at(index);return {s.center,s.size,s.rotation,255};
}
bool BoardModel::directContact(int index) const {
    const auto& s=level_.fruit.at(index);const auto& g=level_.groups.at(s.group);const auto& r=level_.road;
    if(r.circular) return s.group==0 && std::abs(g.radius-s.size.width*.5f-r.radius-r.width*.5f)<=rules_.contactTolerance;
    const auto half=s.size*.5f;
    const double tol=rules_.contactTolerance;
    if(!r.closed) {
        const double nearEdge=s.center.x-g.normal.x*half.width;
        const auto frontHalf=level_.fruit[g.slots.front()].size.height*.5;
        return std::abs(g.normal.x)==1 && g.normal.y==0 && std::abs(nearEdge-g.anchor.x)<=tol &&
            std::min(double(s.center.y+half.height),double(g.anchor.y+frontHalf))-
            std::max(double(s.center.y-half.height),double(g.anchor.y-frontHalf))>tol;
    }
    if(g.region=="left" || g.region=="right") {
        const double edge=g.region=="left"?s.center.x+half.width:s.center.x-half.width;
        const double boundary=g.region=="left"?r.left:r.right;
        return std::abs(edge-boundary)<=tol &&
            std::min(double(s.center.y+half.height),double(r.top))-std::max(double(s.center.y-half.height),double(r.bottom))>tol;
    }
    const double edge=g.region=="bottom"?s.center.y+half.height:s.center.y-half.height;
    const double boundary=g.region=="bottom"?r.bottom:r.top;
    return std::abs(edge-boundary)<=tol &&
        std::min(double(s.center.x+half.width),double(r.right))-std::max(double(s.center.x-half.width),double(r.left))>tol;
}
bool BoardModel::canTake(int index,const std::string& color,double time) const {
    if(index<0 || index>=int(slots_.size()) || slots_[index].empty() || !directContact(index)) return false;
    const auto& s=level_.fruit[index];const auto& f=fruits_.at(slots_[index]);
    if(!level_.road.circular && (level_.groups[s.group].slots.front()!=index || time+eps<groupReady_[s.group])) return false;
    return f.phase==FruitPhase::Settled && f.color==color;
}
std::vector<int> BoardModel::collectionCandidates(int front,const std::string& color,double time,int capacity) const {
    std::vector<int> result;
    if(capacity<=0 || !canTake(front,color,time)) return result;
    if(level_.road.circular) {result.push_back(front);return result;}
    const auto& group=level_.groups.at(level_.fruit.at(front).group);
    for(int slot:group.slots) {
        if(int(result.size())==capacity || slots_[slot].empty()) break;
        const auto& f=fruits_.at(slots_[slot]);
        if(f.phase!=FruitPhase::Settled || f.color!=color) break;
        result.push_back(slot);
    }
    return result;
}
void BoardModel::reserveFruit(int index,const std::string& vehicle,double time) {
    auto& f=fruits_.at(slots_.at(index));
    bool allowed=canTake(index,f.color,time);
    if(!allowed && !level_.road.circular && f.phase==FruitPhase::Settled) {
        const auto& group=level_.groups.at(level_.fruit[index].group);
        allowed=directContact(group.slots.front()) && time+eps>=groupReady_[level_.fruit[index].group];
        for(int slot:group.slots) {
            if(slot==index) break;
            if(slots_[slot].empty()) {allowed=false;break;}
            const auto& prefix=fruits_.at(slots_[slot]);
            if(prefix.phase!=FruitPhase::Collecting || prefix.vehicle!=vehicle || prefix.color!=f.color) {allowed=false;break;}
        }
    }
    require(allowed,"Fruit is not collectable");
    f.phase=FruitPhase::Collecting;f.vehicle=vehicle;f.start=time;f.end=time+rules_.collectSeconds;
    f.from=slotPose(index);f.serial=++serial_;
}
void BoardModel::moveTo(FruitState& fruit,int dest,const FruitPose& from,double time) {
    require(slots_.at(dest).empty(),"Refill destination occupied");
    fruit.phase=FruitPhase::Moving;fruit.slot=dest;fruit.start=time;fruit.end=time+rules_.refillSeconds;
    fruit.from=from;fruit.to=slotPose(dest);fruit.serial=++serial_;slots_[dest]=fruit.id;
    groupReady_[level_.fruit[dest].group]=fruit.end;
}
void BoardModel::refill(int group,double time,int radialColumn) {
    if(level_.road.circular) {
        require(radialColumn>=0,"Radial refill needs the collected column");
        std::vector<std::pair<int,std::string>> existing;
        for(const auto& ring:level_.groups) {
            const int slot=ring.slots.at(radialColumn);
            if(!slots_[slot].empty()) {
                require(fruits_.at(slots_[slot]).phase==FruitPhase::Settled,"Radial chain still busy");
                existing.emplace_back(slot,slots_[slot]);
            }
            slots_[slot].clear();
        }
        // Compact only this ray. Every entity retains its angular column;
        // middle -> inner and outer -> middle move together along the radius.
        for(size_t depth=0;depth<existing.size();++depth) {
            const int dest=level_.groups[depth].slots.at(radialColumn);
            auto& fruit=fruits_.at(existing[depth].second);
            if(dest==existing[depth].first) slots_[dest]=fruit.id;
            else moveTo(fruit,dest,slotPose(existing[depth].first),time);
        }
        return;
    }
    const auto& g=level_.groups.at(group);
    std::vector<std::pair<int,std::string>> existing;
    for(int slot:g.slots) if(!slots_[slot].empty()) existing.emplace_back(slot,slots_[slot]);
    for(int slot:g.slots) slots_[slot].clear();
    size_t count=0;
    for(const auto& old:existing) {
        int dest=g.slots[count++];auto& fruit=fruits_.at(old.second);
        if(dest==old.first) {slots_[dest]=fruit.id;continue;}
        require(fruit.phase==FruitPhase::Settled,"Refill chain still busy");
        moveTo(fruit,dest,slotPose(old.first),time);
    }
    if(g.entrance<0) return;
    const auto& e=level_.entrances.at(g.entrance);auto& cursor=cursors_[g.entrance];
    while(count<g.slots.size() && cursor<int(e.sequence.size())) {
        auto& f=fruits_.at(e.sequence[cursor++].id);
        require(f.phase==FruitPhase::Future,"Entrance sequence consumed twice");
        const int dest=g.slots[count++];auto from=slotPose(dest);from.position=e.position;
        moveTo(f,dest,from,time);
    }
}
void BoardModel::finishEvents(double time,TrafficModel& traffic) {
    std::vector<std::string> due;
    for(const auto& f:fruits_) if((f.second.phase==FruitPhase::Moving || f.second.phase==FruitPhase::Collecting) && f.second.end<=time+eps) due.push_back(f.first);
    std::sort(due.begin(),due.end(),[&](const std::string& a,const std::string& b) {
        const auto& x=fruits_.at(a);const auto& y=fruits_.at(b);
        return x.end!=y.end?x.end<y.end:x.serial<y.serial;
    });
    std::set<int> refillGroups,refillRays;
    for(const auto& id:due) {
        auto& f=fruits_.at(id);
        if(f.phase==FruitPhase::Moving) {f.phase=FruitPhase::Settled;continue;}
        const int old=f.slot;const int group=level_.fruit[old].group;
        require(slots_[old]==f.id,"Collected fruit lost its slot");
        slots_[old].clear();f.slot=-1;f.phase=FruitPhase::Collected;++collected_;
        traffic.commitReserved(f.vehicle,1);
        if(level_.road.circular) refillRays.insert(level_.fruit[old].order);
        else refillGroups.insert(group);
    }
    for(int group:refillGroups) refill(group,time);
    for(int ray:refillRays) refill(0,time,ray);
}
double BoardModel::nextEventTime() const {
    double next=1e30;for(const auto& item:fruits_) {
        const auto& f=item.second;
        if(f.phase==FruitPhase::Moving || f.phase==FruitPhase::Collecting) next=std::min(next,f.end);
    }return next;
}
bool BoardModel::idle() const {return nextEventTime()>1e29;}
int BoardModel::futureCount(int entrance) const {return int(level_.entrances.at(entrance).sequence.size())-cursors_.at(entrance);}
FruitPose BoardModel::pose(const FruitState& f,const TrafficModel& traffic) const {
    if(f.phase==FruitPhase::Settled) return slotPose(f.slot);
    if(f.phase==FruitPhase::Future || f.phase==FruitPhase::Collected) return {};
    const float t=float(std::max(0.0,std::min(1.0,(traffic.now()-f.start)/(f.end-f.start))));
    auto to=f.to;
    if(f.phase==FruitPhase::Collecting) {to.position=traffic.pose(f.vehicle).position;to.size=cocos2d::Size(7,7);to.rotation=f.from.rotation;to.opacity=170;}
    auto result=interpolate(f.from,to,t);
    if(f.phase==FruitPhase::Collecting) result.position.y+=std::sin(float(3.141592653589793*t))*22;
    return result;
}
void BoardModel::checkInvariants(const TrafficModel& traffic) const {
    std::set<std::string> occupied;int collected=0,reserved=0;
    std::map<std::string,int> reservations;
    for(size_t i=0;i<slots_.size();++i) if(!slots_[i].empty()) {
        const auto& f=fruits_.at(slots_[i]);
        require(occupied.insert(f.id).second && f.slot==int(i),"Fruit duplicated across slots");
        require(f.phase==FruitPhase::Settled || f.phase==FruitPhase::Moving || f.phase==FruitPhase::Collecting,"Invalid occupied fruit");
    }
    for(const auto& item:fruits_) {
        const auto& f=item.second;
        if(f.phase==FruitPhase::Collected) {++collected;require(f.slot==-1,"Collected fruit still occupies slot");}
        else if(f.phase==FruitPhase::Future) require(f.slot==-1,"Future fruit already placed");
        else require(occupied.count(f.id)==1,"Fruit disappeared from board");
        if(f.phase==FruitPhase::Collecting) {++reserved;++reservations[f.vehicle];}
    }
    int loaded=0,vehicleReserved=0;
    for(const auto& v:traffic.states()) {loaded+=v.second.loaded;vehicleReserved+=v.second.reserved;
        require(v.second.reserved==reservations[v.first],"Fruit/vehicle reservation mismatch");}
    require(int(fruits_.size())==level_.total && collected==collected_ && loaded==collected && vehicleReserved==reserved,"Fruit/load conservation failed");
    for(size_t i=0;i<level_.entrances.size();++i) for(size_t j=0;j<level_.entrances[i].sequence.size();++j) {
        const auto& f=fruits_.at(level_.entrances[i].sequence[j].id);
        require((f.phase==FruitPhase::Future)==(int(j)>=cursors_[i]),"Entrance order/cursor mismatch");
    }
}
}
