# 词条系统重构 (Affix System Overhaul)

日期: 2026-03-22

## 概述

对装备词条系统进行了全面重构，从"仅文本展示"升级为"全链路战斗生效"。参考 Diablo II 的装备词条设计，扩展词条种类、引入槽位限制和百分比词条，并将所有词条效果接入战斗计算。

---

## 一、词条扩展 (affixes.ts)

### 改动前
- 31 条词条（16 前缀 + 15 后缀）
- 仅覆盖 damage / defense / str / dex / int / maxHp / maxMana / lifeSteal / fireDamage / iceDamage / critRate / critDamage / lck
- 无槽位限制，任何词条可出现在任何装备上
- 无百分比类词条

### 改动后 (68 条)

| 分类 | 新增词条 | 说明 |
|------|---------|------|
| **百分比伤害前缀** | `damagePercent` T2-T5 (10%-100%) | 仅限武器 |
| **百分比防御前缀** | `defensePercent` T2/T4 (10%-50%) | 仅限铠甲/头盔/副手 |
| **新属性前缀** | `vit` (体力), `spi` (精神) T1-T2 | 任意槽位 |
| **百分比生命/法力后缀** | `maxHpPercent`, `maxManaPercent` | 腰带/铠甲/项链/头盔 |
| **法力偷取/回复** | `manaSteal`, `manaRegen`, `hpRegen` | 武器/戒指/项链/腰带 |
| **新元素伤害** | `lightningDamage`, `poisonDamage` | 武器/戒指/项链 |
| **五种抗性** | `fireResist`, `iceResist`, `lightningResist`, `poisonResist`, `allResist` | 防具/副手/戒指 |
| **实用词条** | `moveSpeed` (仅靴子), `magicFind`, `expBonus`, `cooldownReduction` | 各有槽位限制 |

### `allowedSlots` 限制
所有词条添加了合理的 `allowedSlots` 限制，例如：
- 攻击速度 → 武器/手套
- 移动速度 → 靴子
- 暴击率 → 武器/手套/戒指/项链
- 抗性 → 防具/副手/戒指

### `STAT_DISPLAY` 映射表
新增导出 `STAT_DISPLAY: Record<string, { label: string; isPercent: boolean }>`，用于 UI 层将 stat key 翻译为中文标签并区分百分比/固定值显示。

---

## 二、套装重设计 (sets.ts) — D2 风格

### 设计原则
1. **每件套装件有独立固定词条** (`pieceAffixes`)，类似 D2 中套装件本身就是小型暗金
2. **套装奖励层叠**: 2件=基础/防御, 3件=进攻/build-defining, 4件=强力capstone
3. **数值平衡**: 全套奖励强力但不碾压，鼓励混搭选择

### 套装列表

| 套装 | 件数 | 适配职业 | 2件奖励 | 3件奖励 | 4件奖励 |
|------|------|---------|---------|---------|---------|
| 铁壁守护者 | 4 | 战士 | +30%生命 | 受击回血+全抗 | 死亡豁免 |
| 暗影刺客 | 4 | 盗贼 | +20%暴击+攻速 | +50%暴伤+击杀回血 | 暴击连击 |
| 大法师 | 4 | 法师 | +25%法力+回复 | -20%CD+元素伤害 | 15%免费施法+元素30% |
| 荒野猎手 | 4 | 弓手 | +攻速+移速 | +掉宝+击杀回血 | 30%双倍箭矢 |
| 渊火之誓 | 3 | 通用 | +全抗+伤害 | 击杀回血+CDR | - |

### 传奇装备

新增/改进 9 件传奇，每件都有 `specialEffectValue` 数值化：

| 传奇 | 基底 | 特殊效果 |
|------|------|---------|
| 灵魂收割者 | 恶魔之刃 | 击杀回复5%生命 |
| 霜火之杖 | 奥术法杖 | 所有元素伤害+25% |
| 风之力 | 战争之弓 | 25%双倍箭矢 |
| 悲伤 | 阔剑 | 忽略目标20%防御 |
| 暗影之履 | 皮靴 | 闪避后必暴 |
| 不灭之盾 | 铁盾 | 10%死亡豁免 |
| 大天使之铠 | 板甲 | 受伤减少10% |
| 乔丹之石 | 金戒指 | 技能CD-10% |
| 玛拉的万花筒 | 翡翠项链 | 升级+2全属性 |

---

## 三、战斗系统集成 (CombatSystem.ts)

### 新增 `EquipStats` 接口
强类型装备属性聚合，包含 30+ 个字段，覆盖所有词条效果和套装特殊效果。

### 伤害计算管线 (按顺序)

```
基础伤害 + 属性加成
  → + 装备平伤 (flat damage)
  → × (1 + 百分比伤害%)
  → + 元素伤害 (fire/ice/lightning/poison)
  → × 元素伤害加成%
  → × 暴击倍率 (含装备暴击伤害%)
  → - 有效防御 × 0.5 (含忽略防御%)
  → × (1 - 减伤%)
  → × (1 - 抗性%) [非物理伤害]
  → 输出 finalDamage + lifeStolen + manaStolen
```

### 抗性系统
- 特定抗性 + `allResist` 叠加
- 上限 75% (D2 风格)
- 仅对非物理伤害生效

### `DamageResult` 扩展
新增 `damageType`, `lifeStolen`, `manaStolen` 字段，供调用方直接应用。

---

## 四、效果生效链路

### Player.ts
- `recalcDerived(equipStats?)`: 应用 maxHp% / maxMana% / 移速% / 攻速%
- `update(... equipStats?)`: 装备 hpRegen/manaRegen 加成到自然回复
- `useSkill(... cdr)`: 冷却缩减从装备传入
- `toCombatEntity(equipStats?)`: 注入装备词条到战斗实体

### ZoneScene.ts
- **每帧**: 刷新 equipStats 缓存 → recalcDerived → 传入 update
- **攻击后**: `applySteal()` 执行吸血/吸魔
- **击杀后**: `killHealPercent` 回复 + `expBonus` 经验加成
- **受击后**: `thornsHeal` 受击回血
- **致死伤害**: `deathSave` 死亡豁免 (回复30%生命, 60秒CD)
- **技能使用**: `cooldownReduction` CDR 应用

### InventorySystem.ts
- `getEquipmentStats()`: 聚合装备词条 + 套装奖励
- `getTypedEquipStats()`: 返回强类型 `EquipStats`

### LootSystem.ts
- `addRandomAffixes()`: 检查 `allowedSlots` 与装备槽位匹配

### UIScene.ts
- Tooltip: 使用 `STAT_DISPLAY` 显示 `+15% 暴击率` 而非 `暴击: +15`
- 角色面板: 显示暴击率/暴击伤害详细数值
- 装备加成: 过滤零值，中文标签+百分号

---

## 五、类型变更 (types.ts)

```typescript
// SetDefinition 新增
pieceAffixes?: Record<string, ItemAffix[]>;

// LegendaryDefinition 新增
specialEffectValue?: number;
```

---

## 六、涉及文件

| 文件 | 改动 |
|------|------|
| `src/data/items/affixes.ts` | 全面重写，68条词条 + STAT_DISPLAY |
| `src/data/items/sets.ts` | 全面重写，5套装 + 9传奇 |
| `src/data/types.ts` | SetDefinition/LegendaryDefinition 扩展 |
| `src/systems/CombatSystem.ts` | EquipStats 接口 + 伤害公式重做 |
| `src/systems/InventorySystem.ts` | 套装奖励聚合 + getTypedEquipStats |
| `src/systems/LootSystem.ts` | allowedSlots 过滤 |
| `src/entities/Player.ts` | recalcDerived/update/useSkill 装备加成 |
| `src/scenes/ZoneScene.ts` | 吸血/击杀回血/死亡豁免/CDR/经验加成 |
| `src/scenes/UIScene.ts` | 中文词条显示 + 角色面板增强 |
