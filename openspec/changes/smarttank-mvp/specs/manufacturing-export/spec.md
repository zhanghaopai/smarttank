# Spec Delta

## Purpose

提供基于当前工程板件数据的二维制造图、板件明细与计算报告输出，让用户取得可以对应三维设计的制造信息，并维持单向 CAD 导出及明确计算覆盖范围的首版边界。

## ADDED Requirements

### Requirement: DXF manufacturing drawings
系统 SHALL 单向导出 DXF，包含罐底排板图、罐壁展开及排板图、锥顶排板图，以及板件下料轮廓、尺寸和制造编号；MVP MUST 不要求 DWG 输出或附件详图。

#### Scenario: Export valid manufacturing design
- **WHEN** 当前设计具有按已确认规则生成的有效制造几何并执行导出
- **THEN** DXF 包含三个部位的排板表达、板件轮廓、尺寸及编号

### Requirement: No CAD reimport
系统 MUST 不提供将外部修改的 CAD 图纸导回工程、参数或三维模型的功能。

#### Scenario: External modification
- **WHEN** 用户在外部 CAD 软件修改已导出的 DXF
- **THEN** SmartTank 工程保持原设计，不通过回导更新工程

### Requirement: Consistent drawing and plate data
系统 SHALL 从同一当前设计状态生成制造图与明细；板件身份、制造编号、尺寸和材料 SHALL 与工程及三维表达对应。

#### Scenario: Compare outputs
- **WHEN** 用户对照当前工程板件、DXF 与 CSV
- **THEN** 同一板件的编号、制造尺寸和材料一致，可以追溯到共同板件身份

### Requirement: CSV plate schedule
系统 SHALL 导出 CSV 板件明细，提供足以对应工程及制造图的板件身份、制造编号、所属部位、制造尺寸、厚度、材料与数量信息。

#### Scenario: Export schedule
- **WHEN** 用户为当前有效排板导出明细
- **THEN** CSV 覆盖当前生成的板件，每项可与制造图及工程板件对应

### Requirement: Fixed template calculation report
系统 SHALL 按固定模板导出计算报告，包含设计输入、所用计算依据、三部位基础校核结果以及覆盖和未覆盖范围；具体文件格式待确认，MUST 不默认选定 PDF、DOCX 或其他格式。

#### Scenario: Export current calculation
- **WHEN** 当前输入已完成有效计算，且报告格式与模板已确认
- **THEN** 报告记录当前输入及校核结果，明确未覆盖专项，不宣称整罐完整符合 API 650

### Requirement: Reject unavailable current output
系统 MUST 不将缺失制造规则、无效排板或过期计算结果作为有效当前输出导出；失败时 SHALL 说明原因，不能提示导出成功。

#### Scenario: Missing manufacturing rules
- **WHEN** 相关制造规则未确认且用户要求导出下料图
- **THEN** 系统说明缺失规则，不输出冒充有效制造图的结果

#### Scenario: Stale report
- **WHEN** 用户修改影响计算的参数后未重新计算即导出报告
- **THEN** 系统提示重新计算，不把旧结果作为当前有效报告

#### Scenario: Write failure
- **WHEN** 导出目标不可写
- **THEN** 系统说明写入失败，不提示成功
