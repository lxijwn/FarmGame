#pragma once
#include "cocos2d.h"
#include <map>
#include <string>
#include <vector>

namespace farm {
struct Asset {
    std::string frame;
    cocos2d::Size size;
    cocos2d::Vec2 center, textPosition;
};
struct FruitSlot {
    std::string id, entityId, color;
    cocos2d::Vec2 center;
    cocos2d::Size size;
    float rotation = 0;
    int group=-1, order=0;
    double contact=-1;
};
struct FruitDefinition {std::string id,color;};
struct Entrance {
    cocos2d::Vec2 position, inward;
    std::string assetKey;
    int futureCount = 0;
    int group=-1;
    std::vector<FruitDefinition> sequence;
};
struct BoardGroup {
    std::vector<int> slots;
    int entrance=-1;
    float radius=0;
    double contact=-1;
    std::string region;
    cocos2d::Vec2 anchor,normal;
};
struct WitnessStep {
    std::string vehicle,action,result;
    int before=0,after=0,remaining=0,waitingSlot=0;
    std::vector<std::string> fruitIds;
    std::vector<int> slotIndices;
};
struct Vehicle {
    std::string id, color, assetKey;
    int capacity = 0;
};
struct TrafficRules {
    int roadCapacity=5, waitingCapacity=5;
    double launchInterval=.8, enterSeconds=.25, returnSeconds=.3, exitSeconds=.3, queueSeconds=.24;
    double collectSeconds=.18, refillSeconds=.24, contactTolerance=.0001;
};
struct Road {
    bool circular=false, closed=true;
    cocos2d::Vec2 center;
    float radius=0, startAngle=-90, width=0;
    float left=0,right=0,bottom=0,top=0;
    double lapSeconds=10;
    std::vector<cocos2d::Vec2> points;
};
struct Level {
    std::string id, title, mode, backgroundKey, boardKey;
    int initial = 0, future = 0, total = 0;
    std::vector<FruitSlot> fruit;
    std::vector<Entrance> entrances;
    std::map<std::string, Vehicle> vehicles;
    std::vector<std::vector<std::string>> columns;
    std::vector<float> columnX, rowY;
    std::vector<cocos2d::Vec2> waiting;
    Road road;
    std::vector<BoardGroup> groups;
    std::vector<WitnessStep> witness;
};
class Catalog {
public:
    void load(const std::string& resourceRoot);
    void loadAtlas();
    const Asset& asset(const std::string& key) const;
    const Level& level(const std::string& id) const;
    cocos2d::Sprite* sprite(const std::string& key) const;
    const std::vector<Level>& levels() const { return levels_; }
    size_t frameCount() const { return assets_.size(); }
    const std::string& root() const { return root_; }
    const TrafficRules& trafficRules() const { return trafficRules_; }
private:
    std::string root_, atlasPath_;
    std::map<std::string, Asset> assets_;
    std::vector<Level> levels_;
    TrafficRules trafficRules_;
};
}
