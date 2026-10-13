# Spec Delta

## Purpose

提供罐底、罐壁及自支撑锥顶的基础厚度计算与校核结果，明确所用标准、材料及适用边界，使用户能够理解不合格项和未覆盖内容，而不将部分校核误认为整罐合规结论。

## ADDED Requirements

### Requirement: Confirmed calculation basis
系统 MUST 使用已确认的 API 650 版次及增补、单一材料牌号与性能数据、公式及适用边界进行校核；依据未确认或输入不适用时 MUST 不生成通过结论。

#### Scenario: Missing basis
- **WHEN** 用户点击计算且所需标准、材料或规则缺失
- **THEN** 系统指出缺失依据，不使用猜测默认值并不生成通过结论

#### Scenario: Outside applicability
- **WHEN** 输入不满足已确认计算规则的适用条件
- **THEN** 系统明确提示不适用，不能把该情况视为校核通过

### Requirement: Three part thickness checks
系统 SHALL 在点击计算后同时执行罐底、罐壁和自支撑锥顶已确认的基础厚度计算与校核，逐项提供结果；MVP MUST 不将风、地震、锚固或梁柱支撑校核纳入已完成范围。

#### Scenario: Calculate valid design
- **WHEN** 三部位输入有效且基础计算依据已确认
- **THEN** 用户可分别查看罐底、罐壁和锥顶的计算及校核结果

### Requirement: Failed parameter indication
系统 SHALL 在相关参数或组件上对不合格项提供标红等可识别提示，并允许用户查看对应校核项及失败原因。

#### Scenario: Thickness failure
- **WHEN** 某部位厚度不满足已确认校核条件
- **THEN** 对应参数或组件被标示为不合格，用户可查看失败原因

### Requirement: Current design results
系统 SHALL 将计算结果关联到其输入设计状态；影响计算的参数变化后 MUST 不把旧结果作为当前设计有效结论。

#### Scenario: Change after calculation
- **WHEN** 计算完成后用户修改影响校核的参数
- **THEN** 原结果标识为需重新计算，不能用于当前设计的有效计算报告

### Requirement: Explicit coverage
系统 SHALL 在计算结果与报告中说明基础校核覆盖范围及未覆盖专项；MUST 不因基础校核通过而宣称整罐满足 API 650 全部适用要求。

#### Scenario: Basic checks pass
- **WHEN** 已实现的基础厚度校核全部通过
- **THEN** 结果仍明确风、地震、锚固等未覆盖内容，不给出整罐完整合规结论
