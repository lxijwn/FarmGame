#include "Validation/Runtime.h"
#include "Scenes/FarmScenes.h"
#include "Validation/TrafficChecks.h"
#include "Validation/GameplayChecks.h"
#include "json/document.h"
#include "json/prettywriter.h"
#include "json/stringbuffer.h"
#include "base/ccUtils.h"
#include <fstream>
#include <stdexcept>

namespace farm {
using namespace cocos2d;
namespace {
std::wstring wide(const std::string& s) {
    int length=MultiByteToWideChar(CP_UTF8,0,s.c_str(),-1,nullptr,0);
    std::wstring result(length,L'\0');
    MultiByteToWideChar(CP_UTF8,0,s.c_str(),-1,&result[0],length);
    result.pop_back(); return result;
}
std::string glText(GLenum name) {
    auto text=reinterpret_cast<const char*>(glGetString(name));
    return text?text:"unavailable";
}
}
bool Runtime::start() {
    try {
        if(!FileUtils::getInstance()->createDirectory(options_.reportDirectory))
            throw std::runtime_error("Cannot create report directory");
        if(options_.demo && !FileUtils::getInstance()->createDirectory(options_.reportDirectory+"/frames"))
            throw std::runtime_error("Cannot create demo frames directory");
        catalog_.load(options_.resources);
        catalog_.loadAtlas();
        if(options_.smoke) {trafficChecks_=checkTraffic(catalog_);gameplayChecks_=checkGameplay(catalog_);}
        vendor_=glText(GL_VENDOR); renderer_=glText(GL_RENDERER); glVersion_=glText(GL_VERSION);
        if(options_.startLevel.empty()) showMenu(); else openLevel(options_.startLevel);
        Director::getInstance()->getScheduler()->schedule([this](float dt){tick(dt);},this,0,false,"farm-runtime");
        writeReport(); return true;
    } catch(const std::exception& e) { fail(e.what()); return false; }
}
void Runtime::guard(const std::function<void()>& action) {
    if(failed_) return;
    try { action(); } catch(const std::exception& e) {fail(e.what());}
}
void Runtime::showMenu() {
    auto scene=makeModeScene(*this);
    current_="mode_selection"; ++menuVisits_;
    auto d=Director::getInstance();
    if(d->getRunningScene()) d->replaceScene(scene); else d->runWithScene(scene);
    writeReport();
}
void Runtime::openLevel(const std::string& id) {
    auto scene=makeLevelScene(*this,catalog_.level(id));
    current_=id;
    auto d=Director::getInstance();
    if(d->getRunningScene()) d->replaceScene(scene); else d->runWithScene(scene);
    writeReport();
}
void Runtime::reloadLevel() {
    if(current_=="mode_selection") throw std::runtime_error("No level to reload");
    const auto id=current_; openLevel(id); ++reloads_; writeReport();
}
void Runtime::recordView(const Level& level,const ViewCounts& counts) {
    if(counts.fruit!=level.initial || counts.vehicles!=int(level.vehicles.size()) ||
       counts.waiting!=5 || counts.entrances!=int(level.entrances.size()))
        throw std::runtime_error("Rendered object count mismatch: "+level.id);
    views_.push_back({level.id,counts});
}
void Runtime::recordKey(EventKeyboard::KeyCode key,bool pressed) {
    ++keyEvents_; lastKey_=int(key); lastKeyPressed_=pressed; writeReport();
}
void Runtime::recordTrafficInput(const std::string& id,LaunchResult result) {
    ++trafficInputs_; lastTrafficInput_=id;lastLaunchResult_=int(result);writeReport();
}
Runtime::TrafficAudit Runtime::audit(const std::string& capture,const GameModel& game) const {
    const auto& model=game.traffic();
    TrafficAudit a;
    a.capture=capture;a.levelId=model.levelId();a.time=model.now();a.road=model.roadUsed();a.waiting=model.waitingUsed();a.paused=model.paused();
    a.remaining=game.board().remaining();a.collected=game.board().collected();a.pickups=int(game.pickups().size());a.outcome=outcomeName(game.outcome());
    a.entranceCursors=game.board().entranceCursors();
    a.spawnSeed=game.board().spawnSeed();
    for(const auto& f:game.board().fruits()) if(f.second.spawning) ++a.spawning;
    for(const auto& item:model.states()) {
        VehicleAudit v;v.id=item.first;v.phase=phaseName(item.second.phase);
        v.slot=item.second.waitingSlot;v.loaded=item.second.loaded;v.trips=item.second.trips;v.pose=model.pose(item.first);
        a.vehicles.push_back(v);
        a.reserved+=item.second.reserved;
    }
    return a;
}
void Runtime::capture(const std::string& name,const std::function<void()>& next) {
    captureBusy_=true;
    utils::captureScreen([this,name,next](bool success,const std::string&) {
        captureBusy_=false;
        guard([&]{
            if(!success) throw std::runtime_error("Screenshot failed: "+name);
            auto traffic=sceneGame(Director::getInstance()->getRunningScene());
            if(traffic) trafficObservations_.push_back(audit(name,*traffic));
            screenshots_.push_back(name); next(); stageFrame_=frames_; writeReport();
        });
    },options_.reportDirectory+"/"+name);
}
void Runtime::tick(float dt) {
    ++frames_; elapsed_+=dt;
    if(options_.demo) {tickDemo();return;}
    if(!options_.smoke) { if(frames_%120==0) writeReport(); return; }
    if(failed_ || passed_) return;
    if(elapsed_>40) { fail("Runtime smoke timed out"); return; }
    if(captureBusy_ || frames_-stageFrame_<3) return;
    guard([&] {
        // Exercise the same scene-graph keyboard callbacks used by the UI.
        // This checks Cocos event routing, not OS/GLFW physical key translation.
        auto key=[this](EventKeyboard::KeyCode code) {
            EventKeyboard event(code,false);
            injectingKey_=true;
            Director::getInstance()->getEventDispatcher()->dispatchEvent(&event);
            injectingKey_=false;
        };
        auto game=[]() {
            auto model=sceneGame(Director::getInstance()->getRunningScene());
            if(!model) throw std::runtime_error("Game scene not active");return model;
        };
        switch(stage_) {
        case 0: capture("01_mode_selection.png",[this,key]{key(EventKeyboard::KeyCode::KEY_1);stage_=1;}); break;
        case 1: capture("02_circular.png",[this]{stage_=10;}); break;
        case 10: case 13: {
            auto g=game();const auto& level=catalog_.level(g->traffic().levelId());
            const std::string prefix=stage_==10?"circular":"rectangular";
            bool flight=false,refill=false;
            for(const auto& f:g->board().fruits()) {flight|=f.second.phase==FruitPhase::Collecting;refill|=f.second.phase==FruitPhase::Moving;}
            if(flight && !sawFlight_) {sawFlight_=true;capture(prefix+"_collecting.png",[]{});break;}
            if(refill && !sawRefill_) {sawRefill_=true;capture(prefix+"_refilling.png",[]{});break;}
            if(stage_==13 && !sawSpawn_) {
                for(const auto& f:g->board().fruits()) if(f.second.spawning &&
                    g->traffic().now()>=f.second.start+.04 && g->traffic().now()<f.second.start+.10) {
                    sawSpawn_=true;capture("rectangular_spawning.png",[]{});return;
                }
            }
            if(!g->idle()) break;
            if(tripActive_) {
                const auto& w=level.witness.at(witnessIndex_);finishWitness(*g,w,pickupStart_);
                recordWitness(*g,w);++witnessIndex_;tripActive_=false;
                if(g->traffic().state(w.vehicle).phase==VehiclePhase::Waiting && !sawWaiting_) {sawWaiting_=true;capture(prefix+"_partial_waiting.png",[]{});break;}
            }
            if(witnessIndex_==level.witness.size()) {
                if(g->outcome()!=Outcome::Won || !sawFlight_ || !sawRefill_ || !sawWaiting_ || (stage_==13 && !sawSpawn_)) throw std::runtime_error("Incomplete gameplay scene coverage");
                if(stage_==10) capture("circular_won.png",[this,key]{key(EventKeyboard::KeyCode::KEY_R);stage_=2;});
                else capture("rectangular_won.png",[this,key]{key(EventKeyboard::KeyCode::KEY_ESCAPE);stage_=5;});
                break;
            }
            pickupStart_=g->pickups().size();beginWitness(*g,level.witness[witnessIndex_]);tripActive_=true;
            break;
        }
        case 2: capture("circular_reloaded.png",[this,key]{key(EventKeyboard::KeyCode::KEY_ESCAPE);stage_=3;}); break;
        case 3: key(EventKeyboard::KeyCode::KEY_2); stage_=4; stageFrame_=frames_; break;
        case 4: capture("rectangular_initial.png",[this]{witnessIndex_=0;tripActive_=false;sawFlight_=sawRefill_=sawWaiting_=false;stage_=13;}); break;
        case 5: {
            size_t expectedSteps=0;for(const auto& level:catalog_.levels()) expectedSteps+=level.witness.size();
            if(menuVisits_!=3 || reloads_!=1 || views_.size()!=3 || screenshots_.size()!=13 || keyEvents_!=5 || trafficChecks_.size()!=10 || stepObservations_.size()!=expectedSteps)
                throw std::runtime_error("Navigation coverage incomplete");
            passed_=true; writeReport(); Director::getInstance()->end(); break;
        }
        }
    });
}
void Runtime::recordWitness(const GameModel& game,const WitnessStep& w) {
    StepAudit a;a.level=game.traffic().levelId();a.vehicle=w.vehicle;a.before=w.before;a.after=game.traffic().state(w.vehicle).loaded;
    a.remaining=game.board().remaining();a.slot=w.waitingSlot;a.phase=phaseName(game.traffic().state(w.vehicle).phase);
    for(size_t i=pickupStart_;i<game.pickups().size();++i) {a.fruit.push_back(game.pickups()[i].fruit);a.slots.push_back(game.pickups()[i].slot);}
    stepObservations_.push_back(a);
}
void Runtime::tickDemo() {
    if(failed_ || passed_) return;
    if(elapsed_>600) {fail("Demo export timed out");return;}
    if(captureBusy_) return;
    guard([&] {
        if(current_=="mode_selection" && demoFrames_>=demoHoldUntil_) {
            openLevel(catalog_.levels().at(stage_==0?0:1).id);stage_=stage_==0?1:3;
            witnessIndex_=0;tripActive_=false;return;
        }
        auto game=sceneGame(Director::getInstance()->getRunningScene());
        if(game && game->traffic().levelId()==current_ && game->idle()) {
            const auto& level=catalog_.level(current_);
            if(tripActive_) {
                const auto& w=level.witness.at(witnessIndex_);finishWitness(*game,w,pickupStart_);recordWitness(*game,w);
                ++witnessIndex_;tripActive_=false;
                demoHoldUntil_=demoFrames_+(witnessIndex_==level.witness.size()?75:15);
                writeReport();
            }
            if(demoFrames_>=demoHoldUntil_) {
                if(witnessIndex_==level.witness.size()) {
                    if(game->outcome()!=Outcome::Won) throw std::runtime_error("Demo did not win");
                    trafficObservations_.push_back(audit("demo_win",*game));
                    if(stage_==1) {showMenu();stage_=2;demoHoldUntil_=demoFrames_+30;return;}
                    size_t expectedSteps=0;for(const auto& l:catalog_.levels()) expectedSteps+=l.witness.size();
                    if(stepObservations_.size()!=expectedSteps) throw std::runtime_error("Incomplete demo witness");
                    passed_=true;writeReport();Director::getInstance()->end();return;
                }
                pickupStart_=game->pickups().size();beginWitness(*game,level.witness.at(witnessIndex_));tripActive_=true;
            }
        }
        const auto name=cocos2d::StringUtils::format("frame_%06d.png",demoFrames_);
        captureBusy_=true;
        utils::captureScreen([this](bool success,const std::string&) {
            captureBusy_=false;
            if(!success) {fail("Demo framebuffer capture failed");return;}
            ++demoFrames_;
        },options_.reportDirectory+"/frames/"+name);
    });
}
void Runtime::fail(const std::string& error) {
    failed_=true; error_=error;
    try {writeReport();} catch(...) {}
    CCLOG("Farm runtime error: %s",error.c_str());
    if(!options_.smoke) MessageBox(error.c_str(),"Farm - resource error");
    Director::getInstance()->end();
}
void Runtime::writeReport() {
    using namespace rapidjson;
    Document report(kObjectType); auto& a=report.GetAllocator();
    auto s=[&](const std::string& text){return rapidjson::Value(text.c_str(),SizeType(text.size()),a);};
    report.AddMember("schemaVersion",1,a);
    report.AddMember("status",s(failed_?"failed":passed_?"passed":"running"),a);
    report.AddMember("mode",s(options_.demo?"demo":options_.smoke?"smoke":"interactive"),a);
    report.AddMember("demoFrames",demoFrames_,a);report.AddMember("demoFps",30,a);
    report.AddMember("scope",s("C++ collection, refill, finite entrances, vehicle lifecycle and two exact witness wins"),a);
    report.AddMember("engine",s(cocos2dVersion()),a);
    report.AddMember("frames",frames_,a); report.AddMember("elapsedSeconds",elapsed_,a);
    report.AddMember("assetFrames",int(catalog_.frameCount()),a);
    report.AddMember("levelCount",int(catalog_.levels().size()),a);
    report.AddMember("currentScene",s(current_),a); report.AddMember("menuVisits",menuVisits_,a);
    report.AddMember("reloads",reloads_,a); report.AddMember("error",s(error_),a);
    report.AddMember("keyEvents",keyEvents_,a); report.AddMember("lastKeyCode",lastKey_,a);
    report.AddMember("lastKeyPressed",lastKeyPressed_,a);
    report.AddMember("trafficInputs",trafficInputs_,a);report.AddMember("lastTrafficVehicle",s(lastTrafficInput_),a);
    report.AddMember("lastLaunchResult",lastLaunchResult_,a);
    report.AddMember("resourceRoot",s(options_.resources),a);
    report.AddMember("glVendor",s(vendor_),a); report.AddMember("glRenderer",s(renderer_),a);
    report.AddMember("glVersion",s(glVersion_),a);
    rapidjson::Value views(kArrayType);
    for(const auto& view:views_) {
        rapidjson::Value item(kObjectType); item.AddMember("levelId",s(view.id),a);
        item.AddMember("fruitSprites",view.counts.fruit,a); item.AddMember("vehicleSprites",view.counts.vehicles,a);
        item.AddMember("waitingSprites",view.counts.waiting,a); item.AddMember("entranceSprites",view.counts.entrances,a);
        views.PushBack(item,a);
    }
    report.AddMember("views",views,a);
    rapidjson::Value captures(kArrayType);
    for(const auto& filename:screenshots_) captures.PushBack(s(filename),a);
    report.AddMember("screenshots",captures,a);
    rapidjson::Value checks(kArrayType);
    for(const auto& check:trafficChecks_) checks.PushBack(s(check),a);
    report.AddMember("trafficChecks",checks,a);
    rapidjson::Value gameplay(kArrayType);
    for(const auto& check:gameplayChecks_) gameplay.PushBack(s(check),a);
    report.AddMember("gameplayChecks",gameplay,a);
    rapidjson::Value steps(kArrayType);
    for(const auto& o:stepObservations_) {
        rapidjson::Value v(kObjectType);v.AddMember("levelId",s(o.level),a);v.AddMember("vehicleId",s(o.vehicle),a);
        v.AddMember("phase",s(o.phase),a);v.AddMember("loadedBefore",o.before,a);v.AddMember("loadedAfter",o.after,a);
        v.AddMember("remainingTargets",o.remaining,a);v.AddMember("waitingSlot",o.slot,a);
        rapidjson::Value fruits(kArrayType),slots(kArrayType);
        for(const auto& f:o.fruit) fruits.PushBack(s(f),a);for(int slot:o.slots) slots.PushBack(slot,a);
        v.AddMember("fruitIds",fruits,a);v.AddMember("slotIndices",slots,a);steps.PushBack(v,a);
    }
    report.AddMember("witnessObservations",steps,a);
    auto trafficJson=[&](const TrafficAudit& audit) {
        rapidjson::Value item(kObjectType);
        item.AddMember("capture",s(audit.capture),a);item.AddMember("levelId",s(audit.levelId),a);
        item.AddMember("time",audit.time,a);item.AddMember("roadUsed",audit.road,a);item.AddMember("waitingUsed",audit.waiting,a);
        item.AddMember("paused",audit.paused,a);
        item.AddMember("spawnSeed",audit.spawnSeed,a);item.AddMember("spawning",audit.spawning,a);
        item.AddMember("remaining",audit.remaining,a);item.AddMember("collected",audit.collected,a);item.AddMember("reserved",audit.reserved,a);
        item.AddMember("pickups",audit.pickups,a);item.AddMember("outcome",s(audit.outcome),a);
        rapidjson::Value cursors(kArrayType);for(int n:audit.entranceCursors) cursors.PushBack(n,a);item.AddMember("entranceCursors",cursors,a);
        rapidjson::Value vehicles(kArrayType);
        for(const auto& v:audit.vehicles) {
            rapidjson::Value state(kObjectType);
            state.AddMember("id",s(v.id),a);state.AddMember("phase",s(v.phase),a);
            state.AddMember("waitingSlot",v.slot,a);state.AddMember("loaded",v.loaded,a);state.AddMember("trips",v.trips,a);
            state.AddMember("x",v.pose.position.x,a);state.AddMember("y",v.pose.position.y,a);
            vehicles.PushBack(state,a);
        }
        item.AddMember("vehicles",vehicles,a);return item;
    };
    rapidjson::Value observations(kArrayType);
    for(const auto& observation:trafficObservations_) observations.PushBack(trafficJson(observation),a);
    report.AddMember("trafficObservations",observations,a);
    auto active=sceneGame(Director::getInstance()->getRunningScene());
    if(active && active->traffic().levelId()==current_) report.AddMember("traffic",trafficJson(audit("live",*active)),a);
    StringBuffer buffer; PrettyWriter<StringBuffer> writer(buffer); report.Accept(writer);
    std::ofstream out(wide(options_.reportDirectory+"/runtime-report.json").c_str(),std::ios::binary|std::ios::trunc);
    if(!out) throw std::runtime_error("Cannot write runtime report");
    out<<buffer.GetString()<<'\n';
    if(!out) throw std::runtime_error("Runtime report write failed");
}
}
