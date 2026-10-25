#pragma once
#include "cocos2d.h"
#include "Validation/Runtime.h"
namespace farm {
class AppDelegate final : public cocos2d::Application {
public:
    explicit AppDelegate(Options options) : runtime_(std::move(options)) {}
    void initGLContextAttrs() override;
    bool applicationDidFinishLaunching() override;
    void applicationDidEnterBackground() override;
    void applicationWillEnterForeground() override;
    int exitCode() const { return runtime_.exitCode(); }
private:
    Runtime runtime_;
};
}
