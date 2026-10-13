# SmartTank

离线 Windows 储罐设计应用，采用 C++ / Qt Widgets / OCCT。

## 构建与运行

开发环境准备见 [开发环境文档](docs/03-开发环境.md)。在项目根目录执行：

```powershell
cmake --preset windows-msvc
cmake --build --preset release
ctest --preset release
./tools/run.ps1
```

## 当前实现

已实现应用框架、三部位参数面板、组件树、参数驱动的 OCCT 外形预览、共同板件数据及设计修订/结果失效机制。
参数初始为空，标准、材料与制造规则明确显示为未确定。

固定排板、厚度公式、工程保存重开及制造输出尚未实现，等待 OpenSpec 中对应前置条件。
输入罐体直径和高度可查看罐底与罐壁，输入锥顶升高可查看完整罐体。支持旋转缩放、组件高亮/显隐、等轴测/正视/俯视。
点击“载入示例尺寸”，或运行 `./tools/run.ps1 -Example`，可查看示例外形。新工程仍不预填任何尺寸或工程依据。
外形预览不包含板件接缝和制造厚度，不能用于下料或校核；当前没有真实排板可供检查。

若已有窗口打开，可使用独立构建目录：`cmake --preset windows-preview`、`cmake --build --preset preview`、`ctest --preset preview`。
启动脚本会选择两个构建目录中最近编译的应用。

OCCT 原生查看器已通过 Windows 渲染及旋转缩放测试。操作与当前限制见 [当前版本操作说明](docs/04-当前版本操作说明.md)。
