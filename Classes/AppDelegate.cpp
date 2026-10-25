#include "AppDelegate.h"
namespace farm {
using namespace cocos2d;
void AppDelegate::initGLContextAttrs() {
    GLContextAttrs attrs={8,8,8,8,24,8,0}; GLView::setGLContextAttrs(attrs);
}
bool AppDelegate::applicationDidFinishLaunching() {
    auto d=Director::getInstance();
    auto view=GLViewImpl::createWithRect("Farm - cocos2d-x 3.17.2",Rect(0,0,480,720));
    if(!view) return false;
    d->setOpenGLView(view);
    view->setDesignResolutionSize(600,900,ResolutionPolicy::SHOW_ALL);
    d->setAnimationInterval(1.0/60.0); d->setDisplayStats(false);
    return runtime_.start();
}
void AppDelegate::applicationDidEnterBackground() { Director::getInstance()->stopAnimation(); }
void AppDelegate::applicationWillEnterForeground() { Director::getInstance()->startAnimation(); }
}
