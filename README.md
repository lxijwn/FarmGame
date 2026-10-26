# FarmGame

Windows / C++14 / cocos2d-x 3.17.2 正式工程。两种布局均已实现车辆发车、同色果蔬收集、补位、有限入口、单圈回场再发、满载驶出、暂停、重开及胜负。圆形48块、矩形64块均已通过实际C++场景通关验证。

当前规则v1.3.0：矩形箭头入口在已有果蔬横向补位后，从本链最远端槽位随机弹出新果蔬，再滑向空位。颜色使用有限随机袋，每局重开更换顺序，仍保留每色数量配平。自动验证固定种子，手动试玩随机种子。矩形改为从下右入口经左侧拐点到上右终点的开放折返通道；每横排从通道两侧连续收同色，异色阻挡，剩余果蔬横向补到通道边。未满车辆到终点回到绑定等待槽，保留装载并等待点击再次出发。圆形保留径向补位。

[项目运行演示（当前v1.3）](FarmDemo_双关通关演示_v1.2.mp4)：真实Cocos画面，正常速度30fps，自动输入、无音轨，包含圆形径向补位、矩形最远端随机弹出、未满回槽和两关胜利。为保持已有链接有效，保留原文件名中的v1.2；视频内容已更新为v1.3，版本及文件校验见同名JSON。

## 使用

- 双击“重新编译.cmd”：增量编译Release并同步Resources。
- 双击“启动预览.cmd”：打开480×720窗口，设计坐标600×900。鼠标点击每列队首发车，点击等待车再次出发。车上数字为剩余容量，满载提前驶出。
- 双击“完整验证.cmd”：检查关卡与资源、编译、资源副本SHA256、C++模型、两关实际场景通关、截图和3种错误包。每次运行生成独立证据目录，自动运行期间忽略外部操作。
- 右上角：暂停/继续、返回、重开。菜单按1/2选择布局；关卡按空格暂停/继续、R重开、Esc返回；菜单按Esc退出。

PowerShell入口：

~~~powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/build.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/verify.ps1
# 已编译且资源未改动时：
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/verify.ps1 -SkipBuild
# 直接进入矩形：
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/run.ps1 -Level rectangular_01
~~~

验收以Release为准。完整验证需要Python 3与Pillow，优先使用本机Codex已有Python，也可用“-Python <完整路径>”指定。源码不依赖旧环境验证工程的二进制产物。

## 结构

| 目录/文件 | 责任 |
| --- | --- |
| CMakeLists.txt | 独立应用目标、引擎链接、资源与CRT/DLL复制 |
| proj.win32/main.cpp | Unicode参数、资源根路径、退出码 |
| Classes/AppDelegate.* | OpenGL窗口、设计分辨率、前后台生命周期 |
| Classes/Data/Catalog.* | JSON解析、完整性检查、图集及回放输入 |
| Classes/Gameplay/TrafficModel.* | 车辆状态、道路采样、队列、容量预占和名额 |
| Classes/Gameplay/BoardModel.* | 果蔬身份与槽位、飞入、补位、有限入口 |
| Classes/Gameplay/GameModel.* | 统一事件时钟、接触仲裁、事务提交、胜负 |
| Classes/Scenes/FarmScenes.* | 模式选择、模型驱动画面、鼠标/键盘、结算界面 |
| Classes/Validation/TrafficChecks.* | 10组独立车辆检查 |
| Classes/Validation/GameplayChecks.* | 21组真实玩法与见证回放检查 |
| Classes/Validation/Runtime.* | 场景路由、实际场景回放、截图与JSON证据 |
| Resources | 关卡、规则、图集和调试输入，构建时同步到exe旁 |
| Tools | 数据与图集生成、验证、错误包构造 |
| Scripts | 构建、运行、整体验证 |
| build-Release/bin | exe、依赖DLL及Resources |
| Validation/runs/<时间戳> | 本次数据快照、运行报告/截图和错误包 |
| Validation/manual/<时间戳> | 手动试玩状态，不能作为自动验收通过凭据 |

编译工具链复用上级“编译环境验证/toolchain.ps1”，工具在“笔试一/工具链”，引擎在“游戏实习笔试题/cocos2d-x-3.17.2”。构建临时映射S:工程、T:工具链、U:引擎，结束或失败后撤销；遇到已占用盘符会停止，不覆盖。运行不依赖这些盘符和启动目录。SDK保持原样，PCH/Ninja兼容处理在项目CMake中。

资源生成和坐标说明见 Resources/README.md；玩法操作及本次证据见上级“果蔬收集与补位说明.md”。修改资源后要重新构建以同步，通常只需复制资源。

## 验证含义

Validation/latest-verification.json指向最新结果；只有所有阶段通过才为passed，失败证据独立保留。

C++模型用0.017秒和0.73秒两种步长执行相同的通关输入；实际Cocos场景再执行圆形10步、矩形15步（验证种子20261026），逐步比较收集果蔬ID和槽位、装载、等待绑定及剩余目标，最终收完48/64块并显示胜利。额外规则测试包括：

- 同色同时接触的确定性仲裁、实体和容量同时预占、不重复/超量收集。
- 内环收集限制、径向逐层补位、同半径直线动画、本列耗尽不跨列借块。
- 矩形横向连续同色批量收集、异色阻挡、左右同排共享容量、整批提交后稳定补位、不跨横排、入口处间隙及角点拒绝。
- 补位期间错过接触后不远程补收，入口随机袋、最远端弹出、有限耗尽和同种子重现。
- 暂停冻结、飞入/补位中重开、圈末先提交再回场。
- 满位仍可推进时继续，所有等待车无法推进时卡关；车辆独立，不合并。

实际场景保存13张非空帧缓冲截图及对应模型状态，覆盖收集、补位、最远端弹出、部分装载回场和两种胜利；重开恢复初始画面。自动回放加速运行，手动窗口保持正常速度。导航通过1/R/Esc/2/Esc的Cocos事件执行；此项覆盖引擎事件路由，实体键盘曾由用户确认可用，新的鼠标体验需要试玩。

源Resources与打包副本逐文件做SHA256核对。错误包只修改本次证据目录的副本，要求缺失资源、JSON语法损坏及图集缺帧返回非零退出码和对应错误信息。单次运行超过45秒会被终止并记为失败。

## 继续开发

保持用户确认的首版规则。新增玩法接入GameModel的同一套事件与状态，避免画面回调单独修改计数；修改后执行完整验证。当前使用原创占位美术，两关核心流程已可玩。提交版答题、可运行包、源码包和正常速度通关演示在上级“交付材料”目录。音效、美术细化及更多关卡仍待后续制作。

Tools/export_demo.py接收--ffmpeg与--output参数，用程序--demo模式导出实际Cocos帧缓冲，再编码为30fps静音MP4。每帧推进1/30秒，按当前关卡通关序列发车，保留两关胜利；与手动版使用相同模型及渲染。导出原始帧保存在Validation/demo，避免混入源码或运行包。Tools/package_release.py生成压缩包，并从新解压目录、独立工作目录和最小PATH启动程序复核实际通关。本轮已替换项目根目录及交付材料中的v1.2命名视频，保留路径兼容；ZIP归档包尚未重打。

Validation/random_spawn_report.json另记录64个随机种子的独立数据模型通关抽查，均找到可行路线。自动验证路线不代表所有种子或任意玩家顺序都能通关。视频和自动验证使用种子20261026，两关分别10次和15次发车；手动关卡每次重新进入或重开使用新随机种子。
