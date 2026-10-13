# Spec Delta

## Purpose

提供 SmartTank 离线储罐设计工程的创建、编辑与保存重开行为，明确自支撑锥顶首版边界，使参数、组件与设计数据能够在 Windows 本地完成完整使用流程。

## ADDED Requirements

### Requirement: Offline Windows operation
系统 MUST 使用 C++ / Qt 技术栈提供离线 Windows 应用；核心工程流程 SHALL 不依赖网络、用户授权、许可证或激活机制。

#### Scenario: Offline startup
- **WHEN** 用户在断网 Windows 环境启动软件
- **THEN** 用户可以进入工程流程，无登录、许可证或激活前置条件

### Requirement: MVP tank boundary
系统 SHALL 支持自支撑锥顶储罐工程，其设计包含罐底、罐壁和罐顶；MVP MUST 不将球顶、带支撑结构锥顶及附件建模计算作为已支持能力。

#### Scenario: Start supported project
- **WHEN** 用户创建 MVP 储罐工程
- **THEN** 工程包含罐底、罐壁与自支撑锥顶，不提供球顶或梁柱支撑设计流程

### Requirement: Engineering workspace
系统 SHALL 提供左侧参数面板、中间三维可视面板、右侧组件树及顶部菜单工具栏；选择组件后 SHALL 显示相应参数，修改有效参数后 SHALL 更新受影响设计表达。

#### Scenario: Edit selected component
- **WHEN** 用户选择罐壁组件并修改有效参数
- **THEN** 参数面板显示罐壁参数，受影响排板与三维表达更新

### Requirement: Project persistence
系统 SHALL 将工程设计数据保存为 `.st` 文件并允许重新打开，恢复参数、排板定义、板件身份及材料引用，使设计能够继续编辑与生成输出。

#### Scenario: Save and reopen
- **WHEN** 用户保存工程、关闭后重新打开该 `.st` 文件
- **THEN** 工程参数和板件身份恢复，重新生成的结果与保存时设计一致

### Requirement: Unconfirmed basis remains explicit
系统 MUST 不以未经确认的 API 650 版次、材料牌号或制造规则作为默认工程依据；尚未配置的依据 SHALL 被标识为未确定，而不是有效工程结果。

#### Scenario: Missing design basis
- **WHEN** 工程缺少相关计算或制造输出所需的已确认依据
- **THEN** 系统明确指出缺失项，不将相关输出标记为已验证结果
