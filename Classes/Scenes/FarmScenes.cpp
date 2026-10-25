#include "Scenes/FarmScenes.h"
#include "Validation/Runtime.h"
#include "Gameplay/TrafficModel.h"
#include "Gameplay/GameModel.h"
#include <cmath>
#include <stdexcept>

namespace farm {
using namespace cocos2d;
namespace {
const Color4B ink(50,77,66,255);
Label* label(Node* parent,const std::string& text,Vec2 position,float size,Vec2 anchor=Vec2(.5f,.5f)) {
    auto l=Label::createWithSystemFont(text,"Microsoft YaHei",size);
    if(!l) throw std::runtime_error("System font label creation failed");
    l->setTextColor(ink); l->setPosition(position); l->setAnchorPoint(anchor);
    parent->addChild(l); return l;
}
Sprite* placed(Node* parent,const Catalog& catalog,const std::string& key,Vec2 p) {
    auto s=catalog.sprite(key); s->setPosition(p); parent->addChild(s); return s;
}
MenuItemSprite* button(Node* parent,const Catalog& catalog,const std::string& key,const std::string& text,Vec2 p,const std::function<void()>& fn) {
    auto normal=catalog.sprite(key); auto selected=catalog.sprite(key);
    selected->setColor(Color3B(210,225,180));
    auto item=MenuItemSprite::create(normal,selected,[fn](Ref*){fn();});
    item->setPosition(p);
    if(!text.empty()) label(item,text,Vec2(normal->getContentSize().width/2,normal->getContentSize().height/2),20);
    auto menu=Menu::create(item,nullptr); menu->setPosition(Vec2::ZERO); parent->addChild(menu,10);
    return item;
}
void navigation(Scene* scene,Runtime& runtime,bool isMenu) {
    auto keyboard=EventListenerKeyboard::create();
    keyboard->onKeyPressed=[&runtime](EventKeyboard::KeyCode code,Event*) {
        if(runtime.keyInputEnabled()) runtime.guard([&]{runtime.recordKey(code,true);});
    };
    keyboard->onKeyReleased=[&runtime,isMenu](EventKeyboard::KeyCode code,Event*) {
        if(!runtime.keyInputEnabled()) return;
        runtime.guard([&] {
            runtime.recordKey(code,false);
            if(code==EventKeyboard::KeyCode::KEY_ESCAPE) {
                if(isMenu) Director::getInstance()->end(); else runtime.showMenu();
            } else if(isMenu && (code==EventKeyboard::KeyCode::KEY_1 || code==EventKeyboard::KeyCode::KEY_2)) {
                runtime.openLevel(runtime.catalog().levels()[code==EventKeyboard::KeyCode::KEY_1?0:1].id);
            } else if(!isMenu && code==EventKeyboard::KeyCode::KEY_R) runtime.reloadLevel();
            else if(!isMenu && code==EventKeyboard::KeyCode::KEY_SPACE) {
                auto model=sceneTraffic(Director::getInstance()->getRunningScene());
                if(model) model->setPaused(!model->paused());
            }
        });
    };
    scene->getEventDispatcher()->addEventListenerWithSceneGraphPriority(keyboard,scene);
}
}

Scene* makeModeScene(Runtime& runtime) {
    auto scene=Scene::create(); auto& catalog=runtime.catalog();
    placed(scene,catalog,"background.farm",Vec2(300,450));
    label(scene,"FARM",Vec2(300,810),48);
    label(scene,"选择布局",Vec2(300,704),27);
    auto panel=placed(scene,catalog,"panel.dialog",Vec2(300,506));
    panel->setScale(1,1.15f);
    label(scene,"选择农场",Vec2(300,622),17);
    for(size_t i=0;i<catalog.levels().size();++i) {
        const auto id=catalog.levels()[i].id;
        button(scene,catalog,"button.primary",catalog.levels()[i].title,Vec2(300,550-float(i)*100),
            [&runtime,id] { if(runtime.manualInputEnabled()) runtime.guard([&]{runtime.openLevel(id);}); });
    }
    label(scene,"1 / 2 选择农场    Esc 退出",Vec2(300,378),15);
    navigation(scene,runtime,true); return scene;
}

namespace {
class TrafficScene final : public Scene {
public:
    TrafficScene(Runtime& runtime,const Level& level) : runtime_(runtime),level_(level),model_(level,runtime.catalog().trafficRules()) {}
    GameModel& model() { return model_; }
    bool init() override;
    void update(float dt) override {
        runtime_.guard([&]{model_.advance(runtime_.simulationDelta(dt)); renderTraffic();});
    }
private:
    void clicked(const std::string& id) {
        auto result=model_.launch(id);
        hint_->setString(launchMessage(result)); hintUntil_=model_.traffic().now()+2;
        renderTraffic();
        runtime_.recordTrafficInput(id,result);
    }
    void renderTraffic();
    void showOutcome();
    Runtime& runtime_;
    const Level& level_;
    GameModel model_;
    std::map<std::string,Sprite*> vehicles_;
    std::map<std::string,Label*> digits_;
    std::map<std::string,Sprite*> fruit_;
    std::vector<Sprite*> entranceSprites_;
    std::vector<Label*> entranceLabels_;
    std::vector<Sprite*> waiting_;
    Label* hud_=nullptr;
    Label* hint_=nullptr;
    MenuItemSprite* pause_=nullptr;
    double hintUntil_=0;
    bool wasPaused_=false;
    Outcome shownOutcome_=Outcome::Running;
};
bool TrafficScene::init() {
    if(!Scene::init()) return false;
    auto scene=this; auto& runtime=runtime_; const auto& level=level_;
    auto& catalog=runtime.catalog(); ViewCounts counts;
    placed(scene,catalog,level.backgroundKey,Vec2(300,450));
    placed(scene,catalog,level.boardKey,catalog.asset(level.boardKey).center);
    for(const auto& item:model_.board().fruits()) {
        auto s=placed(scene,catalog,"fruit."+item.second.color,Vec2::ZERO);
        fruit_[item.first]=s;
    }
    counts.fruit=level.initial;
    for(const auto& e:level.entrances) {
        auto s=placed(scene,catalog,e.assetKey,e.position);
        s->setRotation(CC_RADIANS_TO_DEGREES(std::atan2(e.inward.x,e.inward.y)));
        entranceSprites_.push_back(s);
        entranceLabels_.push_back(label(scene,std::to_string(e.futureCount),e.position+Vec2(-e.inward.y,e.inward.x)*20,10));
        ++counts.entrances;
    }
    for(size_t i=0;i<level.waiting.size();++i) {
        waiting_.push_back(placed(scene,catalog,"waiting.free",level.waiting[i]));
        auto l=label(scene,std::to_string(i+1),level.waiting[i]+Vec2(-29,24),11);
        l->setTextColor(Color4B(166,184,142,255)); ++counts.waiting;
    }
    for(size_t ci=0;ci<level.columns.size();++ci) {
        for(size_t ri=0;ri<level.columns[ci].size();++ri) {
            const auto& v=level.vehicles.at(level.columns[ci][ri]);
            auto s=placed(scene,catalog,v.assetKey,Vec2(level.columnX[ci],level.rowY[ri]));
            vehicles_[v.id]=s;
            digits_[v.id]=label(s,std::to_string(v.capacity),catalog.asset(v.assetKey).textPosition,16);
            ++counts.vehicles;
        }
    }
    auto header=placed(scene,catalog,"panel.dialog",Vec2(300,849));
    header->setScale(550.0f/480,68.0f/300);
    label(scene,level.title,Vec2(48,861),22,Vec2(0,.5f));
    hud_=label(scene,"",Vec2(48,835),13,Vec2(0,.5f));
    hint_=label(scene,"",Vec2(300,level_.road.circular?538:348),11);
    hint_->setDimensions(level_.road.circular?134:460,level_.road.circular?40:24); hint_->setAlignment(TextHAlignment::CENTER,TextVAlignment::CENTER);
    label(scene,"停车场",Vec2(300,247),14);
    button(scene,catalog,"icon.back","",Vec2(488,849),[&runtime]{if(runtime.manualInputEnabled()) runtime.guard([&]{runtime.showMenu();});});
    button(scene,catalog,"icon.restart","",Vec2(545,849),[&runtime]{if(runtime.manualInputEnabled()) runtime.guard([&]{runtime.reloadLevel();});});
    pause_=button(scene,catalog,"icon.pause","",Vec2(430,849),[this]{
        if(!runtime_.manualInputEnabled()) return;
        runtime_.guard([&]{model_.traffic().setPaused(!model_.traffic().paused());renderTraffic();});
    });
    auto touch=EventListenerTouchOneByOne::create();
    touch->setSwallowTouches(true);
    touch->onTouchBegan=[this](Touch* t,Event*) {
        if(!runtime_.manualInputEnabled() || model_.outcome()!=Outcome::Running) return false;
        const auto point=t->getLocation();
        bool handled=false;
        runtime_.guard([&] {
            // Waiting slots can be clicked anywhere inside their visible box.
            for(size_t i=0;i<waiting_.size();++i) {
                if(!waiting_[i]->getBoundingBox().containsPoint(point)) continue;
                const auto& owner=model_.traffic().waitingOwners()[i];
                if(!owner.empty()) clicked(owner);
                handled=true; return;
            }
            // Road sprites are drawn above the yard. Hit-test in that order.
            for(int layer=1;layer>=0;--layer) for(const auto& v:vehicles_) {
                const auto phase=model_.traffic().state(v.first).phase;
                if(phase==VehiclePhase::Completed || (phase==VehiclePhase::Parked?0:1)!=layer) continue;
                if(v.second->getBoundingBox().containsPoint(point)) {clicked(v.first);handled=true;return;}
            }
        });
        return handled;
    };
    getEventDispatcher()->addEventListenerWithSceneGraphPriority(touch,this);
    navigation(scene,runtime,false);
    runtime.recordView(level,counts);
    renderTraffic(); scheduleUpdate(); return true;
}
void TrafficScene::renderTraffic() {
    auto& catalog=runtime_.catalog();
    auto& traffic=model_.traffic();
    if(wasPaused_!=traffic.paused()) {wasPaused_=traffic.paused();hintUntil_=0;}
    for(const auto& item:vehicles_) {
        const auto& id=item.first; auto sprite=item.second;
        const auto& state=traffic.state(id); const auto pose=traffic.pose(id);
        sprite->setPosition(pose.position); sprite->setRotation(pose.rotation); sprite->setScale(pose.scale);
        sprite->setVisible(state.phase!=VehiclePhase::Completed);
        sprite->setLocalZOrder(state.phase==VehiclePhase::Parked?2:5);
        bool ready=state.phase==VehiclePhase::Waiting;
        if(state.phase==VehiclePhase::Parked) {
            const auto& queue=traffic.queues()[state.column]; ready=!queue.empty() && queue.front()==id;
        }
        sprite->setOpacity(state.phase==VehiclePhase::Parked && !ready?150:255);
        digits_.at(id)->setString(std::to_string(traffic.available(id)));
    }
    for(const auto& item:fruit_) {
        const auto& state=model_.board().fruits().at(item.first);auto sprite=item.second;
        const bool visible=state.phase!=FruitPhase::Future && state.phase!=FruitPhase::Collected;
        sprite->setVisible(visible);if(!visible) continue;
        const auto p=model_.board().pose(state,traffic);
        sprite->setPosition(p.position);sprite->setRotation(p.rotation);sprite->setOpacity(GLubyte(p.opacity));
        sprite->setScale(p.size.width/sprite->getContentSize().width,p.size.height/sprite->getContentSize().height);
        sprite->setLocalZOrder(state.phase==FruitPhase::Collecting?6:1);
    }
    for(size_t i=0;i<entranceLabels_.size();++i) {
        int left=model_.board().futureCount(int(i));entranceLabels_[i]->setString(std::to_string(left));
        entranceSprites_[i]->setOpacity(left?255:90);
    }
    for(size_t i=0;i<waiting_.size();++i) {
        const auto& owner=traffic.waitingOwners()[i];
        auto key=owner.empty()?"waiting.free":traffic.state(owner).phase==VehiclePhase::Waiting?"waiting.docked":"waiting.reserved";
        auto frame=SpriteFrameCache::getInstance()->getSpriteFrameByName(catalog.asset(key).frame);
        if(waiting_[i]->getSpriteFrame()!=frame) waiting_[i]->setSpriteFrame(frame);
    }
    hud_->setString("剩余 "+std::to_string(model_.board().remaining())+"    道路 "+std::to_string(traffic.roadUsed())+
        "/5    等待 "+std::to_string(traffic.waitingUsed())+"/5");
    auto pauseFrame=SpriteFrameCache::getInstance()->getSpriteFrameByName(catalog.asset(traffic.paused()?"icon.play":"icon.pause").frame);
    static_cast<Sprite*>(pause_->getNormalImage())->setSpriteFrame(pauseFrame);
    static_cast<Sprite*>(pause_->getSelectedImage())->setSpriteFrame(pauseFrame);
    if(traffic.paused()) hint_->setString("已暂停\n点击继续或按空格");
    else if(traffic.now()>=hintUntil_) hint_->setString(level_.road.circular?"收集沿路同色果蔬\n回场后点击再次出发":"横向收同色 · 异色阻挡 · 未满到终点回槽");
    if(model_.outcome()!=shownOutcome_) showOutcome();
}
void TrafficScene::showOutcome() {
    shownOutcome_=model_.outcome();if(shownOutcome_==Outcome::Running) return;
    auto root=Node::create();addChild(root,30);
    root->addChild(LayerColor::create(Color4B(38,55,38,135),600,900));
    const auto& catalog=runtime_.catalog();placed(root,catalog,"panel.dialog",Vec2(300,500));
    const bool won=shownOutcome_==Outcome::Won;
    label(root,won?"农场收集完成！":"当前无可推进的车辆",Vec2(300,602),25);
    label(root,won?"所有果蔬已装车运走":"调整发车顺序，再试一次",Vec2(300,558),17);
    button(root,catalog,"button.primary","重新开始",Vec2(300,484),[this]{if(runtime_.manualInputEnabled()) runtime_.guard([&]{runtime_.reloadLevel();});});
    button(root,catalog,"button.primary","选择农场",Vec2(300,405),[this]{if(runtime_.manualInputEnabled()) runtime_.guard([&]{runtime_.showMenu();});});
}
}
Scene* makeLevelScene(Runtime& runtime,const Level& level) {
    auto scene=new TrafficScene(runtime,level);
    try {
        if(scene->init()) {scene->autorelease();return scene;}
    } catch(...) {delete scene;throw;}
    delete scene; throw std::runtime_error("Could not create traffic scene");
}
TrafficModel* sceneTraffic(Scene* scene) {
    auto traffic=dynamic_cast<TrafficScene*>(scene);
    return traffic?&traffic->model().traffic():nullptr;
}
GameModel* sceneGame(Scene* scene) {
    auto traffic=dynamic_cast<TrafficScene*>(scene);return traffic?&traffic->model():nullptr;
}
}
