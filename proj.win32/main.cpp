#include "AppDelegate.h"
#include <shellapi.h>
#include <stdexcept>

namespace {
std::string utf8(const wchar_t* text) {
    const int n=WideCharToMultiByte(CP_UTF8,0,text,-1,nullptr,0,nullptr,nullptr);
    std::string result(n,'\0');
    WideCharToMultiByte(CP_UTF8,0,text,-1,&result[0],n,nullptr,nullptr);
    result.pop_back(); return result;
}
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,LPWSTR,int) {
    try {
        wchar_t filename[32768]={}; GetModuleFileNameW(nullptr,filename,32768);
        auto path=utf8(filename); auto exeRoot=path.substr(0,path.find_last_of("\\/"));
        farm::Options options;
        options.resources=exeRoot+"/Resources";
        options.reportDirectory=exeRoot+"/reports/manual";
        int argc=0;
        auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);
        if(!argv) return 2;
        std::vector<std::string> args;
        for(int i=1;i<argc;++i) args.push_back(utf8(argv[i]));
        LocalFree(argv);
        for(size_t i=0;i<args.size();++i) {
            if(args[i]=="--smoke") options.smoke=true;
            else if(args[i]=="--demo") {options.demo=true;options.smoke=true;}
            else if(args[i]=="--resources" || args[i]=="--report-dir" || args[i]=="--level") {
                const auto key=args[i];
                if(++i==args.size()) throw std::runtime_error("Missing value for "+key);
                if(key=="--resources") options.resources=args[i];
                else if(key=="--report-dir") options.reportDirectory=args[i];
                else options.startLevel=args[i];
            } else throw std::runtime_error("Unknown argument: "+args[i]);
        }
        if(options.smoke && !options.startLevel.empty()) throw std::runtime_error("--smoke starts from mode selection; omit --level");
        farm::AppDelegate app(options);
        const int applicationResult=cocos2d::Application::getInstance()->run();
        return app.exitCode() ? app.exitCode() : applicationResult;
    } catch(const std::exception& e) {
        OutputDebugStringA(e.what()); return 2;
    }
}
