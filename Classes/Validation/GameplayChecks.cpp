#include "Validation/GameplayChecks.h"
#include <set>
#include <stdexcept>

namespace farm {
namespace {
void check(bool ok,const std::string& text) {if(!ok) throw std::runtime_error("Gameplay check: "+text);}
void settle(GameModel& g,double dt) {
    for(int i=0;i<10000 && !g.idle();++i) g.advance(dt);
    check(g.idle(),"trip did not settle");g.checkInvariants();
}
}
void beginWitness(GameModel& game,const WitnessStep& step) {
    const auto& before=game.traffic().state(step.vehicle);
    check(game.idle() && before.loaded==step.before,"witness before "+step.vehicle);
    check(before.phase==(step.action=="launch_from_parking"?VehiclePhase::Parked:VehiclePhase::Waiting),"witness source phase");
    check(game.launch(step.vehicle)==LaunchResult::Accepted,"witness launch "+step.vehicle);
    check(game.traffic().state(step.vehicle).waitingSlot==step.waitingSlot,"witness reservation "+step.vehicle);
}
void finishWitness(const GameModel& game,const WitnessStep& step,size_t pickupStart) {
    const auto& state=game.traffic().state(step.vehicle);
    check(game.idle() && state.loaded==step.after && game.board().remaining()==step.remaining,"witness result "+step.vehicle);
    check(state.phase==(step.result=="completed_exit"?VehiclePhase::Completed:VehiclePhase::Waiting),"witness final phase");
    check(state.waitingSlot==(state.phase==VehiclePhase::Completed?-1:step.waitingSlot),"witness final binding");
    check(game.pickups().size()-pickupStart==step.fruitIds.size(),"witness pickup count");
    for(size_t i=0;i<step.fruitIds.size();++i) {
        const auto& event=game.pickups()[pickupStart+i];
        check(event.vehicle==step.vehicle && event.fruit==step.fruitIds[i] && event.slot==step.slotIndices[i],"witness fruit/slot order "+step.vehicle+" "+event.fruit);
    }
}
std::vector<std::string> checkGameplay(const Catalog& catalog) {
    std::vector<std::string> checks;
    const auto rules=catalog.trafficRules();
    for(const auto& level:catalog.levels()) {
        for(double dt:{.017,.73}) {
            GameModel g(level,rules);
            for(const auto& step:level.witness) {
                size_t start=g.pickups().size();beginWitness(g,step);settle(g,dt);finishWitness(g,step,start);
            }
            check(g.outcome()==Outcome::Won && g.board().collected()==level.total,"witness win");
            check(g.traffic().waitingUsed()==0 && g.traffic().roadUsed()==0,"win releases all capacity");
            for(size_t i=0;i<level.entrances.size();++i) check(g.board().futureCount(int(i))==0,"finite entrance exhausted");
            checks.push_back(level.id+": exact fruit IDs/slots, loads, reservations and win at dt="+std::to_string(dt));
        }
        {
            GameModel g(level,rules);g.launch(level.columns[0][0]);
            bool collected=false,moving=false;
            for(int i=0;i<2000 && !(collected&&moving);++i) {
                g.advance(.01);bool flight=false,refill=false;
                for(const auto& f:g.board().fruits()) {flight|=f.second.phase==FruitPhase::Collecting;refill|=f.second.phase==FruitPhase::Moving;}
                if(!flight && !refill) continue;
                auto time=g.traffic().now();auto count=g.pickups().size();auto remaining=g.board().remaining();
                std::map<std::string,FruitPose> poses;
                for(const auto& f:g.board().fruits()) poses.emplace(f.first,g.board().pose(f.second,g.traffic()));
                g.traffic().setPaused(true);g.advance(10);
                check(g.traffic().now()==time && g.pickups().size()==count && g.board().remaining()==remaining,"pause transaction clock");
                for(const auto& f:g.board().fruits()) check(g.board().pose(f.second,g.traffic()).position==poses.at(f.first).position,"pause fruit pose");
                g.traffic().setPaused(false);collected|=flight;moving|=refill;
            }
            check(collected && moving,"observe flight and refill");
            g.reset();g.advance(20);
            check(g.pickups().empty() && g.board().collected()==0 && g.traffic().waitingUsed()==0,"reset cancels pending transactions");
            checks.push_back(level.id+": pause freezes flight/refill; reset removes transactions");
        }
        {
            GameModel g(level,rules);
            for(int i=0;i<3;++i) {check(g.launch(level.columns[i][0])==LaunchResult::Accepted,"concurrent admission");g.advance(.8);}
            settle(g,.61);std::set<std::string> ids;
            for(const auto& e:g.pickups()) check(ids.insert(e.fruit).second,"concurrent duplicate fruit");
            g.checkInvariants();checks.push_back(level.id+": concurrent trips conserve unique fruit and reserved capacity");
        }
    }
    const auto& circular=catalog.level("circular_01");
    {
        auto level=circular;auto r=rules;r.launchInterval=0;r.collectSeconds=2.1;
        const auto a=level.columns[0][0],b=level.columns[1][0];level.vehicles[b].color="green";
        GameModel g(level,r);g.launch(a);g.launch(b);g.advance(1.9);
        check(g.traffic().state(a).reserved==3 && g.traffic().state(a).loaded==0,"in-flight capacity is reserved before commit");
        check(g.traffic().state(b).reserved==0 && g.traffic().state(b).loaded==0,"same-time arbitration uses vehicle ID");
        g.advance(.6);
        check(g.traffic().available(a)==0 && g.traffic().state(a).reserved==4,"reservation prevents over-capacity");
        std::set<std::string> fruit;for(const auto& e:g.pickups()) check(fruit.insert(e.fruit).second,"same-time fruit allocated once");
        g.checkInvariants();g.reset();g.advance(30);
        check(g.board().collected()==0 && g.pickups().empty() && g.traffic().waitingUsed()==0,"reset mid-flight discards old commits");
        checks.push_back("same-color simultaneous contact: stable arbitration, capacity reservation, reset mid-flight");
    }
    {
        auto level=catalog.level("rectangular_01");auto r=rules;r.collectSeconds=1.1;r.refillSeconds=1.2;
        for(auto& f:level.fruit) f.color="purple";
        const int front=level.groups[0].slots[0],back=level.groups[0].slots[1];
        level.fruit[front].color=level.fruit[back].color="green";
        const auto a=level.columns[0][0],b=level.columns[1][0];level.vehicles[b].color="green";
        level.vehicles[a].capacity=1; // The first car leaves the same-color back fruit for the next trip.
        GameModel g(level,r);g.launch(a);g.advance(.8);g.launch(b);settle(g,.07);
        check(g.pickups().size()==1 && g.traffic().state(b).loaded==0,"passed vehicle cannot remotely collect after refill");
        check(g.board().canTake(front,"green",g.traffic().now()),"refill settles with same fruit color");
        g.launch(b);settle(g,.07);check(g.traffic().state(b).loaded==1,"next lap can collect newly settled fruit");
        checks.push_back("rectangle: busy chain skips contact; only a later lap collects replenished fruit");
    }
    {
        auto level=catalog.level("rectangular_01");
        for(auto& f:level.fruit) f.color="purple";
        const auto group=level.groups[0];
        for(int i=0;i<3;++i) level.fruit[group.slots[i]].color="green";
        level.fruit[group.slots[4]].color="green";
        BoardModel board(level,rules);TrafficModel traffic(level,rules);
        auto id=level.columns[0][0];traffic.launch(id);traffic.advance(rules.enterSeconds);
        auto batch=board.collectionCandidates(group.slots[0],"green",traffic.now(),4);
        check(batch==std::vector<int>(group.slots.begin(),group.slots.begin()+3),"horizontal prefix stops at different color");
        check(board.collectionCandidates(group.slots[0],"green",traffic.now(),2).size()==2,"capacity truncates prefix");
        for(int slot:batch) {check(traffic.reserveCapacity(id,1),"batch reservation");board.reserveFruit(slot,id,traffic.now());}
        traffic.advance(rules.collectSeconds);board.finishEvents(traffic.now(),traffic);
        check(traffic.state(id).loaded==3 && board.slots()[group.slots[0]]==level.fruit[group.slots[3]].entityId,"batch commits then compacts once");
        check(board.slots()[group.slots[1]]==level.fruit[group.slots[4]].entityId,"horizontal refill preserves order");
        check(board.slots()[level.groups[1].slots[0]]==level.fruit[level.groups[1].slots[0]].entityId,"refill cannot cross rows");
        traffic.advance(rules.refillSeconds);board.finishEvents(traffic.now(),traffic);
        check(board.collectionCandidates(group.slots[0],"green",traffic.now(),4).empty(),"blocking color hides matching back fruit");
        board.checkInvariants(traffic);
        checks.push_back("rectangle: same-color batch, different-color blocking, capacity clipping, atomic horizontal refill and row isolation");
    }
    {
        auto level=catalog.level("rectangular_01");for(auto& f:level.fruit) f.color="purple";
        const auto left=level.groups[1],right=level.groups[2];
        for(int slot:left.slots) level.fruit[slot].color="green";
        for(int slot:right.slots) level.fruit[slot].color="green";
        auto id=level.columns[0][0];level.vehicles[id].capacity=5;
        GameModel g(level,rules);g.launch(id);settle(g,.73);
        check(g.pickups().size()==5 && g.traffic().state(id).phase==VehiclePhase::Completed,"both sides share the remaining capacity");
        for(size_t i=0;i<left.slots.size();++i) check(g.pickups()[i].slot==left.slots[i],"left side prefix ordering");
        check(g.pickups().back().slot==right.slots[0] && g.pickups().front().time==g.pickups().back().time,"both sides collect at same horizontal crossing");
        check(g.board().collected()==5,"both sides commit without overflow");
        checks.push_back("rectangle: simultaneous left/right contact shares capacity without duplicate or excess collection");
    }
    {
        auto level=catalog.level("rectangular_01");auto r=rules;r.collectSeconds=.6;
        for(auto& f:level.fruit) f.color="purple";
        level.groups.back().contact=.999;
        level.fruit[level.groups.back().slots[0]].color="green";
        auto id=level.columns[0][0];GameModel g(level,r);g.launch(id);g.advance(10.3);
        check(g.traffic().state(id).lapEnded && g.traffic().state(id).reserved==1,"open endpoint holds pending transaction");
        settle(g,.13);
        check(g.traffic().state(id).phase==VehiclePhase::Waiting && g.traffic().state(id).loaded==1,"open endpoint commits before partial return");
        check(g.traffic().pose(id).position==level.waiting[g.traffic().state(id).waitingSlot],"return ends at bound waiting position");
        checks.push_back("rectangle: endpoint waits for collection and returns partial load to bound waiting slot");
    }
    for(bool progress:{false,true}) {
        auto level=circular;for(auto& f:level.fruit) f.color="purple";
        level.columns={{"v0","v3"},{"v1","v4"},{"v2","v5"}};level.vehicles.clear();
        for(int i=0;i<6;++i) {auto id="v"+std::to_string(i);Vehicle v;v.id=id;v.color=(progress && i==4)?"purple":"green";v.capacity=100;level.vehicles[id]=v;}
        GameModel g(level,rules);
        for(int i=0;i<5;++i) {check(g.launch("v"+std::to_string(i))==LaunchResult::Accepted,"fill waiting slots");settle(g,.2);}
        check(g.traffic().waitingUsed()==5,"five bound waiting vehicles");
        if(progress) {
            check(g.outcome()==Outcome::Running && g.launch("v5")==LaunchResult::WaitingFull,"full but progress remains");
            check(g.launch("v4")==LaunchResult::Accepted,"waiting vehicle can relaunch at full occupancy");
        } else check(g.outcome()==Outcome::Stuck,"full idle mismatch is definitively stuck");
        checks.push_back(progress?"full waiting area: eligible partial vehicle can relaunch; no premature failure":"full waiting area: idle mismatch ends game even with blocked parking vehicle");
    }
    {
        BoardModel b(circular,rules);
        for(size_t i=16;i<circular.fruit.size();++i) check(!b.directContact(int(i)),"outer ring cannot collect");
        auto level=circular;
        // Two rays have a middle entity; the third must draw from its own
        // outer slot. Neighbouring angular columns must remain untouched.
        for(int i=18;i<32;++i) level.fruit[i].entityId.clear();
        level.total-=14;level.initial-=14;
        TrafficModel t(level,rules);BoardModel board(level,rules);
        auto vehicle=level.columns[0][0];t.launch(vehicle);t.advance(rules.enterSeconds);
        for(int slot=0;slot<3;++slot) {t.reserveCapacity(vehicle,1);board.reserveFruit(slot,vehicle,t.now());}
        t.advance(rules.collectSeconds);board.finishEvents(t.now(),t);
        check(board.slots()[0]==level.fruit[16].entityId && board.slots()[1]==level.fruit[17].entityId && board.slots()[2]==level.fruit[34].entityId,"refill preserves angular column");
        check(board.slots()[16]==level.fruit[32].entityId && board.slots()[17]==level.fruit[33].entityId,"outer entities shift to the middle on their own rays");
        check(board.slots()[35]==level.fruit[35].entityId && board.slots()[3]==level.fruit[3].entityId,"unaffected ray stays still");
        for(int slot=0;slot<3;++slot) check(!board.canTake(slot,board.fruits().at(board.slots()[slot]).color,t.now()),"radial refill must settle before collection");
        t.advance(rules.refillSeconds*.5);
        for(const auto& item:board.fruits()) if(item.second.phase==FruitPhase::Moving) {
            const auto& f=item.second;auto a=f.from.position-level.road.center,b=board.pose(f,t).position-level.road.center;
            check(std::abs(a.cross(b))<.05 && a.dot(b)>0 && b.length()<a.length(),"animation travels straight inward on the same radius");
        }
        board.checkInvariants(t);
        auto empty=circular;empty.fruit[16].entityId.clear();empty.fruit[32].entityId.clear();empty.total-=2;empty.initial-=2;
        TrafficModel t2(empty,rules);BoardModel b2(empty,rules);t2.launch(vehicle);t2.advance(rules.enterSeconds);
        t2.reserveCapacity(vehicle,1);b2.reserveFruit(0,vehicle,t2.now());t2.advance(rules.collectSeconds);b2.finishEvents(t2.now(),t2);
        check(b2.slots()[0].empty() && b2.slots()[17]==empty.fruit[17].entityId && b2.slots()[33]==empty.fruit[33].entityId,"exhausted ray cannot borrow from neighbouring rays");
        b2.checkInvariants(t2);checks.push_back("circle: radial-only stable refill, collinear inward animation, no borrowing across rays, inner-only collection");
    }
    {
        auto level=catalog.level("rectangular_01");const auto slot=level.groups[0].slots[0];
        BoardModel original(level,rules);check(original.directContact(slot),"authored rectangle contact");
        level.fruit[slot].center+=level.groups[0].normal*2;
        BoardModel gap(level,rules);check(!gap.directContact(slot),"gap cannot collect");
        level.fruit[slot].center.y=level.groups[0].anchor.y+level.fruit[slot].size.height;
        level.fruit[slot].center-=level.groups[0].normal*2;
        BoardModel corner(level,rules);check(!corner.directContact(slot),"corner-only contact cannot collect");
        for(size_t i=0;i<original.entranceCursors().size();++i) check(original.entranceCursors()[i]==0,"full chain must not consume entrance");
        checks.push_back("rectangle: shared edge required; gaps/corners rejected; full chain preserves finite entrance");
    }
    {
        auto r=rules;r.collectSeconds=.6;
        auto level=circular;level.fruit[0].contact=.99;
        GameModel g(level,r);g.launch(level.columns[0][0]);g.advance(10.3);
        // Other early green fruit may fill a small vehicle; isolate one green
        // at the seam by recoloring the remaining inner ring in this fixture.
        for(int i=1;i<16;++i) level.fruit[i].color="purple";
        GameModel seam(level,r);auto id=level.columns[0][0];seam.launch(id);seam.advance(10.3);
        check(seam.traffic().state(id).lapEnded && seam.traffic().state(id).reserved==1,"seam waits for pending fruit");
        settle(seam,.13);check(seam.traffic().state(id).loaded==1 && seam.traffic().state(id).phase==VehiclePhase::Waiting,"pending commit precedes return");
        checks.push_back("lap seam: pending fruit commits before returning with partial load");
    }
    {
        auto level=circular;for(auto& f:level.fruit) f.color="purple";
        level.columns={{level.columns[0][0]},{},{}};level.vehicles.erase("c_purple_l");
        // Retain only the queued vehicle, so a color mismatch has no further move.
        auto first=level.columns[0][0];for(auto it=level.vehicles.begin();it!=level.vehicles.end();) if(it->first!=first) it=level.vehicles.erase(it);else ++it;
        GameModel g(level,rules);g.launch(first);settle(g,.5);
        check(g.outcome()==Outcome::Stuck,"true no-progress state ends game");
        checks.push_back("stuck: idle mismatch with no new vehicles is detected");
    }
    return checks;
}
}
