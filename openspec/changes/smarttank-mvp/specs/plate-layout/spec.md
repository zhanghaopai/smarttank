# Spec Delta

## Purpose

提供由参数驱动的罐底、罐壁和锥顶自动排板能力，使板件身份、尺寸及材料在三维预览和二维制造表达中保持一致，并明确显示几何与制造尺寸之间的边界。

## ADDED Requirements

### Requirement: Parameter driven layout
系统 SHALL 按已确认的固定规则从工程参数生成罐底、罐壁和锥顶板件；MVP MUST 不允许直接修改板件边界、接缝或拖动板件，且 MUST 不执行材料利用率优化。

#### Scenario: Regenerate from parameters
- **WHEN** 用户修改有效排板参数
- **THEN** 系统重新生成受影响板件及表达，用户不能直接编辑生成的边界或接缝

#### Scenario: Repeat generation
- **WHEN** 工程参数与排板规则均未改变并重复生成
- **THEN** 板件划分、编号及制造尺寸保持一致

### Requirement: Shared plate correspondence
系统 SHALL 从共同的工程、排板和板件数据生成三维与二维结果，并通过稳定板件 ID 关联；同一设计状态下的编号、尺寸及材料 SHALL 一致。

#### Scenario: Match representations
- **WHEN** 对照三维板件与对应二维轮廓及明细
- **THEN** 可依据板件 ID 确定对应关系，制造编号、尺寸及材料一致

#### Scenario: Preserve unchanged identity
- **WHEN** 未改变排板拓扑的设计被保存重开或重新生成
- **THEN** 对应板件保留其身份，编号关联不因重绘丢失

### Requirement: Manufacturing geometry source
系统 MUST 根据板件定义、展开及已确认制造规则生成下料轮廓；MUST 不使用三维截图或显示网格投影作为下料尺寸依据。罐底 SHALL 使用平面排板表达，罐壁 SHALL 使用圆柱面展开，锥顶 SHALL 使用锥面展开。

#### Scenario: Rendering precision changes
- **WHEN** 三维显示网格精度或观察方向改变且工程数据不变
- **THEN** 二维下料轮廓及制造尺寸不变

#### Scenario: Generate supported parts
- **WHEN** 三部位的排板和制造规则已确认且输入有效
- **THEN** 各自生成平面排板、圆柱展开和锥面展开结果

### Requirement: Three dimensional preview
系统 SHALL 使用 Open CASCADE Technology（OCCT）实现三维预览，从共同工程及板件数据生成储罐及板件的三维表达，显示板件划分，并支持旋转和缩放查看；查看操作 MUST 不改变工程参数或制造数据。OCCT 显示对象 SHALL 通过板件 ID 与共同板件数据关联，MUST 不作为工程或制造尺寸的主数据。

#### Scenario: Generate OCCT preview
- **WHEN** 当前设计具有有效的罐底、罐壁和自支撑锥顶板件数据
- **THEN** 系统通过 OCCT 生成并显示三维表达，显示对象可关联至对应板件 ID，板件边界可辨识

#### Scenario: Inspect plates
- **WHEN** 用户旋转或缩放三维视图
- **THEN** 可检查板件划分，工程数据与二维制造尺寸不变

### Requirement: Invalid layout indication
系统 SHALL 对无法按已确认规则生成有效排板的参数说明失败原因；MUST 不将旧排板作为当前参数的有效制造结果。

#### Scenario: Invalid input
- **WHEN** 输入超出已确认排板规则的适用范围
- **THEN** 系统提示原因，相关当前制造结果不可作为有效结果导出
