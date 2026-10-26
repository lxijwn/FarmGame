# Farm 首版关卡与资源接入

本目录为 cocos2d-x 3.17.2 首版游戏准备的运行资源。设计尺寸600×900，坐标采用左下原点，长度均为设计单位。关卡预览与工具在上级目录，发布时不用随游戏打包。

## 文件入口

| 路径 | 用途 |
| --- | --- |
| `config/rules.v1.json` | 已确认的规则快照，版本1.3.0，圆形径向补位、矩形开放折返与横向收集 |
| `levels/index.json` | 模式选择的关卡列表 |
| `levels/circular_01.json` | 圆形关卡：道路、3环×16槽、6辆车和三列停车队列 |
| `levels/rectangular_01.json` | 矩形关卡：开放折返路、8横排14条左右链、4个入口、8辆车和三列停车队列 |
| `levels/*_solution.json` | 调试回放输入与逐次收集/补位记录；游戏界面不展示 |
| `art/asset_manifest.json` | assetKey到帧名、尺寸、锚点、容量文字位置的映射 |
| `art/farm_atlas.plist` / `.png` | format-3图集，2048×1024，RGBA8888，未旋转、未裁边 |
| `art/source/*.png` | 33张独立原始PNG，可用于修改或重新打包；使用图集时不必发布 |

图集按cocos2d-x 3.17.2 `CCSpriteFrameCache.cpp` 的format-3字段准备，已通过结构与像素核对，也已由正式C++工程实际加载33个帧并渲染两关。最新运行证据见上级`Validation/latest-verification.json`。

## C++接入约定

将本目录整体复制到可执行文件旁的`Resources`目录，并将其绝对路径加入搜索路径。例如由启动代码确定资源根路径后：

```cpp
auto files = cocos2d::FileUtils::getInstance();
files->addSearchPath(resourceRoot); // 指向上述Resources目录
auto cache = cocos2d::SpriteFrameCache::getInstance();
cache->addSpriteFramesWithFile("art/farm_atlas.plist");
auto fruit = cocos2d::Sprite::createWithSpriteFrameName("fruit_green.png");
fruit->setPosition(300, 585);
fruit->setScale(28.0f / fruit->getContentSize().width,
                28.0f / fruit->getContentSize().height);
```

这是接口示意，`resourceRoot`由实际应用初始化。关卡与清单可用`FileUtils::getStringFromFile`读取，并用引擎自带RapidJSON解析。运行时优先通过assetKey查manifest，避免在玩法代码中散落PNG名称。

果蔬按`fruit.<color>`取得贴图。先用槽位`size`设置显示尺寸，再设置槽位中心及`rotationDegrees`。尺寸变化不得改变一个实体等于1个目标的计数。

车辆的assetKey含颜色和规格，原图朝上，规格与容量对应small=4、medium=8、large=12。沿道路行驶时根据切线旋转。容量数字不在图片中：在车辆的本地`capacityTextPosition`位置创建文字，显示`capacity - loaded - reserved`；位置原点为车辆贴图左下角。三种车辆底部条纹数量不同，四色果蔬也分别采用卷心菜、玉米、胡萝卜和葡萄图形。

图集帧坐标为左上原点，由SpriteFrameCache处理，不能手动按游戏世界坐标反转。所有帧锚点为(0.5,0.5)。使用线性采样，图集包含2px边缘扩展和4px分配边距；PNG未预乘Alpha，不在代码中再次预乘颜色。

道路背景放在manifest给出的`worldCenter`，按原始尺寸显示；它仅承担美术表现。车辆路径、矩形接触边界、果蔬逻辑方块都读取关卡，不能以PNG透明像素推导碰撞。道路精灵尺寸包含阴影，不能把整张道路图片当可行驶区域。

入口箭头原图朝上，按`inwardDirection`旋转：(0,1)=0°、(1,0)=90°、(0,-1)=180°、(-1,0)=270°，使用Cocos顺时针正角度。入口剩余数由队列游标计算，耗尽后调低不透明度。等待位使用`waiting.free`、`waiting.reserved`、`waiting.docked`，行驶时保留车辆绑定。

按钮文案、容量数字、道路名额、目标进度、等待位编号、教学提示、暂停及胜负文案均由运行时代码绘制。预览中这些文字仅为排版示例。系统微软雅黑用于生成预览，包内不附字体文件；游戏中可先用系统字体。收集特效可复用果蔬精灵做移动/缩放，加`fx.sparkle`和`fx.collect_ring`，不需要视频贴图。

## 数据与路径

`parkingColumns`内的ID顺序是队首到队尾；初始位置依次取`parkingColumnX`和`parkingRowY`，不根据车辆在JSON数组中的顺序排列。等待位从左到右为1—5，对应JSON数组索引0—4。

圆形从底部角度−90°开始顺时针，车辆位置为：`center + radius * (cos(a), sin(a))`，其中`a = -pi/2 - progress*2*pi`。只有第0环槽位参与接触，其`contactProgress`落在[0,1)。外环的该字段为null。各环相同槽位编号构成独立径向列；收集后只将本列剩余实体由内到外稳定压紧，中层补内层、外层补中层，沿半径直线移动并使用目标槽位的位置、大小和旋转。本列耗尽留空，不跨角度取块。

矩形`road.type=open_polyline`，路径依次为(452,390)入口、(172,590)拐点、(452,790)终点。进度1表示终点，不能取模回到入口。`contactProgress`按中心线累计路程计算；8横排的高度依次为415、465、515、565、615、665、715、765。

每横排分左右两条链，空侧不建链，共14条。果蔬尺寸44×44、链内步距44。扫描锚点位于该高度路径中心左右各76单位，以避开斜路和方块角点。每条链`slots`按从通道向外排列，前块靠近通道的边与该链扫描锚点对齐。车经过该排时，左右链按槽位ID顺序共享剩余容量，各自只收连续同色前缀，异色、空位或移动中的块会截断；不越过阻挡取后块。同批提交完毕后每链仅补位一次，剩余果蔬保持顺序横向靠近通道，随后从有限随机袋补充，不跨行、不改色。sequence字段现表示待生成实体库存，不表示固定生成次序；每入口库存由种子打乱，抽取不放回，无空位不消耗。已有果蔬先完成横移，新果蔬再从本链最后一个槽位以零尺寸开始放大弹出，然后沿同一横排滑至目标空位，多个新块依次弹出。

未满车辆到终点后缩小消失，再在绑定等待槽放大出现；保留装载且需玩家点击再发，不从终点斜穿果蔬回场。若到终点仍有收集事务，先提交后决定满载驶出或回场。

## 回放文件

`levelSha256`绑定关卡JSON原始字节。修改关卡后应重新生成通关序列。每一步含车辆ID、首次发车或等待车再发、绑定等待位、前后装载、剩余目标和收集事件；事件中的槽位索引是按关卡中的环/链顺序展平后得到的零基索引。

回放是串行输入：每一步等待道路、棋盘事务和停车列前移全部空闲，再点击下一辆。圆形相邻接触最小间隔0.625秒，矩形不同横排间隔1.25秒，都大于收集0.18秒加补位0.24秒。矩形同排左右两次接触在同一时刻进行，模拟器按同色前缀整批收取，再稳定补位。`earliestNextInputSeconds`从第一步开始累计，包含入场、回场/驶出和尾部补位；实际游戏回放应以空闲状态门控输入。

该序列已在C++ GameModel及实际Cocos场景中逐步回放，逐项核对果蔬ID、接触槽位、装载、绑定和剩余数，两关均达到胜利。另有独立测试检查并发车辆仲裁、容量预占、暂停、重开、细步/慢帧、满位与卡关；证据见上级`Validation/latest-verification.json`。

随机规则v1.3：board.randomSeed=20261026用于自动验证及录制，xorshift32配合Fisher-Yates打乱各入口有限库存，C++和独立Python模型使用相同算法。手动打开或重开关卡使用新的随机种子，runtime-report.json的traffic.spawnSeed记录当局种子。四入口仍各4块、每色1块，合计16块，未来目标始终计入64块总目标。
