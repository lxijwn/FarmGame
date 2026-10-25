#pragma once
#include "cocos2d.h"
namespace farm {
class Runtime;
struct Level;
class TrafficModel;
class GameModel;
cocos2d::Scene* makeModeScene(Runtime& runtime);
cocos2d::Scene* makeLevelScene(Runtime& runtime, const Level& level);
TrafficModel* sceneTraffic(cocos2d::Scene* scene);
GameModel* sceneGame(cocos2d::Scene* scene);
}
