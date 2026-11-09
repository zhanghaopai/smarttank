# SmartTank

离线 Windows 储罐设计应用，采用 C++ / Qt Widgets / OCCT。

项目按工程数据（`core`）、几何生成（`geometry`）、三维查看器（`view`）和窗口交互（`ui`）组织。目录职责、依赖边界及架构审查见 [项目结构与架构](docs/05-项目结构与架构.md)。

## 构建与运行

开发环境准备见 [开发环境文档](docs/03-开发环境.md)。在项目根目录执行：

```powershell
./devops/run.ps1
```

使用 `/bin/bash` 或 Git Bash 时：

```bash
bash ./devops/run.sh
# 载入示例尺寸
bash ./devops/run.sh --example
# 在独立目录构建和测试，不启动窗口
bash ./devops/run.sh --preview --no-launch
```

Bash 入口复用 `run.ps1` 的完整流程，支持 `--example`、`--preview`、`--skip-build`、`--no-launch`；用 `--` 分隔应用参数。
仍需 Windows/MSVC 开发环境及可调用的 `powershell.exe`。WSL Bash 需启用 Windows 互操作并将项目放在 Windows 可访问的目录；不支持原生 Linux/macOS 构建。

## 当前实现

已实现应用框架、三部位参数面板、组件树、参数驱动的 OCCT 外形预览、共同板件数据及设计修订/结果失效机制。
参数初始为空，标准、材料与制造规则明确显示为未确定。

固定排板、厚度公式、工程保存重开及制造输出尚未实现，等待 OpenSpec 中对应前置条件。
输入罐体直径和高度可查看罐底与罐壁，输入锥顶升高可查看完整罐体。支持旋转缩放、组件高亮/显隐、等轴测/正视/俯视。
点击“载入示例尺寸”，或运行 `./devops/run.ps1 -Example`，可查看示例外形。新工程仍不预填任何尺寸或工程依据。
外形预览不包含板件接缝和制造厚度，不能用于下料或校核；当前没有真实排板可供检查。

脚本依次配置、编译、运行测试，通过后启动应用；任一步失败都会停止。可从任意工作目录调用脚本。
若默认构建的窗口已打开，可运行 `./devops/run.ps1 -Preview`，使用独立构建目录。
仅启动已有默认构建用 `./devops/run.ps1 -SkipBuild`；仅构建和测试用 `./devops/run.ps1 -NoLaunch`。这些选项可与 `-Example`、`-Preview` 组合使用。

OCCT 原生查看器已通过 Windows 渲染及旋转缩放测试。操作与当前限制见 [当前版本操作说明](docs/04-当前版本操作说明.md)。
