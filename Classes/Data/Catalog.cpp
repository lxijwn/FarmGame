#include "Data/Catalog.h"
#include "json/document.h"
#include "json/error/en.h"
#include <cmath>
#include <set>
#include <stdexcept>

namespace farm {
namespace {
using J = rapidjson::Value;
void need(bool condition, const std::string& error) {
    if (!condition) throw std::runtime_error(error);
}
const J& field(const J& v, const char* name) {
    need(v.IsObject() && v.HasMember(name), std::string("Missing field: ")+name);
    return v[name];
}
const J& array(const J& v) { need(v.IsArray(),"Expected JSON array"); return v; }
const J& object(const J& v) { need(v.IsObject(),"Expected JSON object"); return v; }
std::string str(const J& v) { need(v.IsString(),"Expected JSON string"); return v.GetString(); }
std::string str(const J& v, const char* k) { return str(field(v,k)); }
float num(const J& v) { need(v.IsNumber() && std::isfinite(v.GetDouble()),"Expected finite number"); return float(v.GetDouble()); }
int integer(const J& v) { need(v.IsInt() && v.GetInt()>=0,"Expected nonnegative integer"); return v.GetInt(); }
cocos2d::Vec2 point(const J& v) {
    need(array(v).Size()==2,"Expected 2D coordinate");
    return {num(v[0]),num(v[1])};
}
rapidjson::Document read(const std::string& path) {
    auto file=cocos2d::FileUtils::getInstance();
    need(file->isFileExist(path),"Missing resource: "+path);
    auto text=file->getStringFromFile(path);
    rapidjson::Document d;
    d.Parse<rapidjson::kParseValidateEncodingFlag>(text.c_str());
    need(!d.HasParseError(),"Invalid JSON: "+path+" ("+
        rapidjson::GetParseError_En(d.GetParseError())+")");
    need(d.IsObject(),"JSON root must be object: "+path);
    return d;
}
std::vector<float> values(const J& a) {
    std::vector<float> out;
    for(const auto& v:array(a).GetArray()) out.push_back(num(v));
    return out;
}
}

const Asset& Catalog::asset(const std::string& key) const {
    auto it=assets_.find(key);
    need(it!=assets_.end(),"Unknown assetKey: "+key);
    return it->second;
}
const Level& Catalog::level(const std::string& id) const {
    for(const auto& item:levels_) if(item.id==id) return item;
    throw std::runtime_error("Unknown level: "+id);
}
cocos2d::Sprite* Catalog::sprite(const std::string& key) const {
    auto result=cocos2d::Sprite::createWithSpriteFrameName(asset(key).frame);
    need(result!=nullptr,"Could not create sprite: "+key);
    return result;
}

void Catalog::load(const std::string& root) {
    root_=root;
    if(!root_.empty() && root_.back()!='/') root_+='/';
    assets_.clear(); levels_.clear();
    auto rules=read(root_+"config/rules.v1.json");
    need(str(rules,"rulesVersion")=="1.2.0","Unsupported rules version");
    need(str(field(rules,"circular"),"refillPolicy")=="stable_radial_columns","Unsupported circular refill policy");
    const auto& admission=field(rules,"admission");
    const auto& animation=field(rules,"animations");
    trafficRules_.roadCapacity=integer(field(admission,"roadCapacity"));
    trafficRules_.waitingCapacity=integer(field(admission,"waitingCapacity"));
    trafficRules_.launchInterval=num(field(admission,"minimumLaunchIntervalSeconds"));
    trafficRules_.enterSeconds=num(field(animation,"enterSeconds"));
    trafficRules_.returnSeconds=num(field(animation,"returnSeconds"));
    trafficRules_.exitSeconds=num(field(animation,"completedExitSeconds"));
    trafficRules_.queueSeconds=num(field(animation,"refillSeconds"));
    trafficRules_.collectSeconds=num(field(animation,"collectSeconds"));
    trafficRules_.refillSeconds=num(field(animation,"refillSeconds"));
    trafficRules_.contactTolerance=num(field(field(rules,"rectangular"),"contactToleranceGridUnits"));
    need(trafficRules_.roadCapacity>0 && trafficRules_.waitingCapacity>0 && trafficRules_.launchInterval>0 &&
         trafficRules_.enterSeconds>0 && trafficRules_.returnSeconds>0 && trafficRules_.exitSeconds>0 &&
         trafficRules_.queueSeconds>0 && trafficRules_.collectSeconds>0 && trafficRules_.refillSeconds>0 &&
         trafficRules_.contactTolerance>0,"Invalid traffic timing or capacity");
    auto manifest=read(root_+"art/asset_manifest.json");
    need(integer(field(manifest,"schemaVersion"))==1,"Unsupported manifest schema");
    atlasPath_=root_+str(manifest,"atlas");
    need(cocos2d::FileUtils::getInstance()->isFileExist(atlasPath_),"Missing atlas plist");
    need(cocos2d::FileUtils::getInstance()->isFileExist(root_+str(manifest,"texture")),"Missing atlas texture");
    std::set<std::string> frames;
    for(auto it=object(field(manifest,"assets")).MemberBegin();it!=field(manifest,"assets").MemberEnd();++it) {
        Asset a;
        a.frame=str(it->value,"frame");
        auto size=point(field(it->value,"size"));
        need(size.x>0 && size.y>0,"Invalid asset size");
        a.size=cocos2d::Size(size.x,size.y);
        if(it->value.HasMember("worldCenter")) a.center=point(it->value["worldCenter"]);
        if(it->value.HasMember("capacityTextPosition")) a.textPosition=point(it->value["capacityTextPosition"]);
        need(frames.insert(a.frame).second,"Duplicate atlas frame: "+a.frame);
        assets_.emplace(it->name.GetString(),a);
    }
    need(!assets_.empty(),"Empty asset manifest");
    auto index=read(root_+"levels/index.json");
    std::set<std::string> levelIds;
    const std::set<std::string> colors={"green","yellow","orange","purple"};
    for(const auto& entry:array(field(index,"levels")).GetArray()) {
        auto data=read(root_+str(entry,"path"));
        Level l;
        l.id=str(data,"id"); l.title=str(data,"title"); l.mode=str(data,"mode");
        need(integer(field(data,"schemaVersion"))==1,"Unsupported level schema");
        need(str(data,"rulesVersion")=="1.2.0","Level rules mismatch");
        need(l.id==str(entry,"id") && levelIds.insert(l.id).second,"Invalid/duplicate level ID");
        need(l.mode=="circular" || l.mode=="rectangular","Unknown layout mode");
        const auto& road=field(data,"road");
        l.road.circular=l.mode=="circular";
        l.road.lapSeconds=num(field(road,"lapDurationSeconds"));
        l.road.width=num(field(road,"width"));
        need(l.road.lapSeconds>0 && str(road,"direction")== (l.road.circular?"clockwise":"forward"),"Invalid road timing/direction");
        if(l.road.circular) {
            need(str(road,"type")=="circle","Expected circular road");
            l.road.center=point(field(road,"center"));
            l.road.radius=num(field(road,"radius"));
            l.road.startAngle=num(field(road,"startAngleDegrees"));
            need(l.road.radius>0,"Invalid road radius");
        } else {
            need(str(road,"type")=="open_polyline","Expected open folded road");
            l.road.closed=false;
            for(const auto& p:array(field(road,"points")).GetArray()) l.road.points.push_back(point(p));
            need(l.road.points.size()>=3,"Not enough road points");
            for(size_t i=0;i+1<l.road.points.size();++i)
                need(l.road.points[i].distance(l.road.points[i+1])>.001f && l.road.points[i+1].y>l.road.points[i].y,"Road must advance from entrance to endpoint");
            const auto& b=field(road,"outerBoundary");
            l.road.left=num(field(b,"left"));l.road.right=num(field(b,"right"));
            l.road.bottom=num(field(b,"bottom"));l.road.top=num(field(b,"top"));
            need(l.road.left<l.road.right && l.road.bottom<l.road.top,"Invalid road boundary");
        }
        need(point(field(data,"designSize"))==cocos2d::Vec2(600,900),"Unexpected design size");
        l.backgroundKey=str(data,"backgroundAssetKey"); l.boardKey=str(data,"boardAssetKey");
        asset(l.backgroundKey); asset(l.boardKey);
        l.columnX=values(field(data,"parkingColumnX")); l.rowY=values(field(data,"parkingRowY"));
        for(const auto& p:array(field(data,"waitingPositions")).GetArray()) l.waiting.push_back(point(p));
        need(l.waiting.size()==5 && l.columnX.size()==3,"Invalid parking/waiting configuration");
        std::set<std::string> ids;
        auto unique=[&](const std::string& id){ need(!id.empty() && ids.insert(id).second,"Duplicate entity/slot ID: "+id); };
        std::map<std::string,int> targets, capacities;
        auto fruitSlots=[&](const J& slots,int group) {
            for(const auto& slot:array(slots).GetArray()) {
                FruitSlot f;
                f.id=str(slot,"id"); unique(f.id);
                const auto& fruit=field(slot,"fruit");
                f.entityId=str(fruit,"id"); unique(f.entityId);
                f.color=str(fruit,"color");
                need(colors.count(f.color)!=0,"Unknown fruit color");
                asset("fruit."+f.color);
                ++targets[f.color];
                f.center=point(field(slot,"center"));
                auto size=point(field(slot,"size")); f.size=cocos2d::Size(size.x,size.y);
                need(size.x>0 && size.y>0,"Invalid fruit dimensions");
                if(slot.HasMember("rotationDegrees")) f.rotation=num(slot["rotationDegrees"]);
                f.group=group; f.order=int(l.groups[group].slots.size());
                if(slot.HasMember("contactProgress") && !slot["contactProgress"].IsNull()) f.contact=num(slot["contactProgress"]);
                l.groups[group].slots.push_back(int(l.fruit.size()));
                l.fruit.push_back(f);
            }
        };
        const auto& board=field(data,"board");
        if(l.mode=="circular") {
            const auto& rings=array(field(board,"rings"));
            need(rings.Size()==3,"Circular layout requires three rings");
            for(const auto& ring:rings.GetArray()) {
                need(array(field(ring,"slots")).Size()==16,"Circular ring requires 16 slots");
                BoardGroup group;group.radius=num(field(ring,"radius"));l.groups.push_back(group);
                fruitSlots(ring["slots"],int(l.groups.size()-1));
            }
        } else {
            for(const auto& chain:array(field(board,"chains")).GetArray()) {
                unique(str(chain,"id"));
                BoardGroup group;group.contact=num(field(chain,"contactProgress"));group.region=str(chain,"region");
                group.anchor=point(field(chain,"contactAnchor"));group.normal=point(field(chain,"outwardNormal"));
                l.groups.push_back(group);
                int gi=int(l.groups.size()-1);fruitSlots(field(chain,"slots"),gi);
                const auto& entrance=field(chain,"entrance");
                if(entrance.IsNull()) continue;
                unique(str(entrance,"id"));
                Entrance e;
                e.group=gi;l.groups[gi].entrance=int(l.entrances.size());
                e.position=point(field(entrance,"position")); e.inward=point(field(entrance,"inwardDirection"));
                need(std::abs(e.inward.x)+std::abs(e.inward.y)==1,"Invalid entrance direction");
                e.assetKey=str(entrance,"assetKey"); asset(e.assetKey);
                std::set<std::string> pool;
                for(const auto& color:array(field(entrance,"candidatePool")).GetArray()) pool.insert(str(color));
                for(const auto& fruit:array(field(entrance,"sequence")).GetArray()) {
                    auto id=str(fruit,"id");unique(id); auto color=str(fruit,"color");
                    need(colors.count(color) && pool.count(color),"Entrance color outside candidate pool");
                    ++targets[color]; ++e.futureCount; ++l.future;
                    e.sequence.push_back({id,color});
                }
                l.entrances.push_back(e);
            }
        }
        for(const auto& vehicle:array(field(data,"vehicles")).GetArray()) {
            Vehicle v;
            v.id=str(vehicle,"id"); unique(v.id); v.color=str(vehicle,"color");
            v.assetKey=str(vehicle,"assetKey"); asset(v.assetKey);
            auto size=str(vehicle,"size");
            v.capacity=integer(field(vehicle,"capacity"));
            need(colors.count(v.color) && (size=="small" || size=="medium" || size=="large"),"Invalid vehicle color/size");
            need(v.capacity==integer(field(field(rules,"vehicleCapacities"),size.c_str())),"Vehicle capacity mismatch");
            capacities[v.color]+=v.capacity; l.vehicles.emplace(v.id,v);
        }
        std::set<std::string> queued;
        for(const auto& col:array(field(data,"parkingColumns")).GetArray()) {
            std::vector<std::string> idsInColumn;
            for(const auto& vehicle:array(col).GetArray()) {
                auto id=str(vehicle);
                need(l.vehicles.count(id) && queued.insert(id).second,"Invalid/duplicate queued vehicle");
                idsInColumn.push_back(id);
            }
            need(idsInColumn.size()<=l.rowY.size(),"Not enough parking rows");
            l.columns.push_back(idsInColumn);
        }
        need(l.columns.size()==3 && queued.size()==l.vehicles.size(),"Parking queue mismatch");
        l.initial=int(l.fruit.size()); l.total=l.initial+l.future;
        const auto& expected=field(data,"expectedTotals");
        need(l.initial==integer(field(expected,"initialFruit")) && l.future==integer(field(expected,"entranceFruit")) &&
             l.total==integer(field(expected,"totalFruit")),"Level totals mismatch");
        need(targets==capacities,"Vehicle capacities do not balance fruit colors");
        for(const auto& color:colors) need(targets[color]==integer(field(field(expected,"byColor"),color.c_str())),"Color total mismatch");
        auto witnessPath=str(entry,"path");
        witnessPath=witnessPath.substr(0,witnessPath.size()-5)+"_solution.json";
        auto witness=read(root_+witnessPath);
        need(str(witness,"levelId")==l.id,"Witness level mismatch");
        for(const auto& step:array(field(witness,"steps")).GetArray()) {
            WitnessStep w;w.vehicle=str(step,"vehicleId");w.action=str(step,"action");w.result=str(step,"result");
            w.before=integer(field(step,"loadedBefore"));w.after=integer(field(step,"loadedAfter"));
            w.remaining=integer(field(step,"remainingTargets"));w.waitingSlot=integer(field(step,"waitingSlot"))-1;
            need(l.vehicles.count(w.vehicle)!=0 && w.waitingSlot>=0 && w.waitingSlot<trafficRules_.waitingCapacity,"Invalid witness vehicle/slot");
            for(const auto& event:array(field(step,"events")).GetArray()) {
                w.fruitIds.push_back(str(event,"fruit"));w.slotIndices.push_back(integer(field(event,"slotIndex")));
            }
            l.witness.push_back(std::move(w));
        }
        need(!l.witness.empty(),"Empty witness");
        levels_.push_back(std::move(l));
    }
    need(levels_.size()==2,"Expected two authored levels");
}

void Catalog::loadAtlas() {
    auto cache=cocos2d::SpriteFrameCache::getInstance();
    cache->addSpriteFramesWithFile(atlasPath_);
    for(const auto& pair:assets_) {
        auto frame=cache->getSpriteFrameByName(pair.second.frame);
        need(frame!=nullptr,"Atlas frame missing: "+pair.second.frame);
        auto size=frame->getOriginalSize();
        need(size.equals(pair.second.size),"Atlas frame dimensions mismatch: "+pair.second.frame);
        need(frame->getTexture()!=nullptr,"Atlas texture unavailable");
        frame->getTexture()->setAntiAliasTexParameters();
    }
}
}
