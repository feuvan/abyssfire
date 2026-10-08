# Port Spec — Items, Loot, Inventory, Shops & Crafting

Area owner: items & loot. Web source of truth: branch `claude/unreal-rebuild`, TypeScript under `src/`.
Target: portable C++20 core (`AbyssCore`, no UE types, no exceptions/RTTI) + thin UE5 module (render/input/UI).

This document describes **what the web game does today**, precisely enough to re-implement it without reading the
TypeScript. Where the web behaviour is a bug, dead code or a 2D artefact it is called out as a **QUIRK** with a
recommendation. Default is *keep the behaviour*; a recommendation that says **FIX** is only applied if the open
question at the end is resolved that way. Citations are `path:line` (line numbers at the time of writing).

Sibling specs: `combat-feel.md` (consumes `EquipStats`, kill flow, death), `classes-stats-skills.md` (hero derived
stats from `EquipStats`, `autoLootMode` field). Quest objective plumbing, pets, homestead, the Abyss Labyrinth and
mercenaries have their own specs; this one only documents the item-facing hooks they use.

---

## 0. Scope and Chapter 1 requirements

### 0.1 Systems in this spec (all general; nothing below is Chapter-1 specific unless marked)
| System | Web source | Core class (suggested) |
|---|---|---|
| Item data (bases, affixes, gems, materials, sets, legendaries) | `src/data/items/*.ts`, `src/data/dungeonData.ts:131-211` | `ItemDb` (loaded from JSON) |
| Item generation + loot rolls | `src/systems/LootSystem.ts` | `LootGenerator` |
| Bag / equipment / stash / buyback / gems / consumables | `src/systems/InventorySystem.ts` | `Inventory` |
| Blacksmith crafting | `src/systems/CraftingSystem.ts` | `Crafting` (already pure in web) |
| Item comparison | `src/systems/ItemCompare.ts` | `ItemCompare` (pure) |
| Quest reward gear picking | `src/systems/QuestRewards.ts:13-91` | `QuestRewardGear` (pure) |
| World drops, pickup, auto-loot, potion pickups | `src/scenes/ZoneScene.ts` (various, cited below) | `WorldLoot` (core) + UE actors (render) |
| Shops, sell/buyback, forge UI, stash UI, tooltips | `src/scenes/UIScene.ts` (cited below) | `ShopService` (core rules) + UMG |

### 0.2 What Chapter 1 (zone `emerald_plains`, hero Lv 1–10) needs
* **All item data tables** (they are small; exporting everything keeps later chapters data-only). The drop pools a
  Chapter 1 hero can actually see are listed in §5.9 (item levels 1–13).
* Loot rolls on kill for `slime_green` (L1), `goblin` (L3), `goblin_chief` (L5, elite, 1 elite affix), mini-boss
  `miniboss_goblin_shaman` (L6, elite, `isMiniBoss` → magic floor, `src/data/miniBosses.ts:9-31`), and quest hunts
  (`makeHuntDefinition`, `src/systems/QuestHunts.ts:48-65`: base monster + `elite: true`, `isMiniBoss: true` → magic
  floor, gold ×3; they also roll elite affixes, 1 in Chapter 1, `ZoneScene.ts:6618-6621`). The `lootTable` field on
  monster/mini-boss data is **never read** (dead data, QUIRK Q11).
  Difficulty is always `normal` in Chapter 1 (Nightmare unlocks after the Chapter 5 final boss).
* Gold on kill, gold/items from Chapter 1 quests (fixed items + pick-one gear, §5.5–5.6), hidden-area chest (`rare`,
  item level 7) + gold pile (200) (`src/data/maps/emerald_plains.ts:57-59`), random-event treasure cache.
* Shops in Chapter 1: `blacksmith` (wares + forge tab), `merchant` (camp at 15,15 and 95,100), field merchant
  `plains_herbalist` (`src/data/maps/emerald_plains.ts:32-45`, `src/data/npcs.ts:5-18,89-99`).
* Inventory (100), 10 equipment slots, gems & sockets, potions (ground pickups + bag use), sell/buyback, forge.
* **Stash:** no stash keeper exists in Chapter 1 (first `stash` NPC is in `anvil_mountains`, Chapter 3, and in the
  Ember Tower). The stash data + rules are still needed in Chapter 1 because quest rewards overflow into it (§10).
* Save/load of all of the above (§16).

---

## 1. Conventions

| Topic | Web | Port rule |
|---|---|---|
| RNG | `Math.random()`. `chance(p) = rand*100 < p` (p in %), `randomInt(a,b) = floor(rand*(b-a+1))+a` inclusive (`src/utils/MathUtils.ts:5-15`). | Same helpers on the core's injectable PRNG (see `combat-feel.md` §0). Sequence parity with JS not required; draw **conditions** and distributions are. Crafting already takes an injectable `rng` (`CraftingSystem.ts:257`). |
| Rounding | `Math.round` (half → +∞), `Math.floor`. | `floor(x+0.5)` for `Math.round`. |
| Numbers | All stats/prices are JS numbers; every shipped value is an integer. | Store stat values as `double` (matches `EquipStats` in `combat-feel.md` §1.1); gold and quantities as `int64`/`int32`. |
| Ids | String ids everywhere (`w_short_sword`, `pre_sharp`, …). | Keep string ids in data + saves; intern to `uint16` indices at load for speed. |
| Item uid | `item_${Date.now()}_${counter}` (`LootSystem.ts:10-11`), `craft_${now36}_${n36}` (`CraftingSystem.ts:90-93`), `gem_${now}_${8 hex}` (`InventorySystem.ts:356`). Counter resets per page load. | Any globally unique string. Recommend a persisted 64-bit counter (`"i" + hex`) saved in the save file, so uids are unique and deterministic in tests. Uids are only compared for equality. |
| Text | Item names are **stored zh-CN strings** in `ItemInstance.name`, affix `name`, gem `name`, `legendaryEffect`; display re-localises via i18n keys (§15). | Core never formats text; it stores ids. UE resolves display names (§15). Keep `name` in saves for compatibility only. |
| Time | `scene.time` ms (drop despawn, auto-loot cadence). | Core monotonic `nowMs` (`combat-feel.md` §0). |
| Distances | Tiles (col,row floats), `distanceSq` Euclidean. | Same; UE converts with the tile→uu constant. |

---

## 2. Data model

### 2.1 Enumerations
* `ItemQuality = 'normal' | 'magic' | 'rare' | 'legendary' | 'set'` (`src/data/types.ts:103`).
  * **Quality order (loot / floors):** normal < magic < rare < legendary < set (`LootSystem.ts:15-20`,
    `qualityMeetsFloor(q, floor) = idx(q) >= idx(floor)`).
  * **Sort order (bag/stash sort):** legendary 0, set 1, rare 2, magic 3, normal 4, unknown 5 (`InventorySystem.ts:492`).
  * **Auto-loot rank:** normal 0, magic 1, rare 2, legendary 3, set 3 (`ZoneScene.ts:3720`).
  * Colours (art direction, `docs/art-direction.md:42-43`, `src/ui/UiKit.ts:45-51`): normal `#c8c8c8`, magic
    `#4f8cff`, rare `#ffd84a`, legendary `#ff8a2a`, set `#3ecf6a`. (The world drop label/tint uses an older palette —
    QUIRK Q16.)
* `EquipSlot = helmet | armor | gloves | boots | weapon | offhand | necklace | ring1 | ring2 | belt` (10 slots,
  `types.ts:104`). Ring bases all declare `slot: 'ring1'`; `ring2` is only reachable through the ring rule (§8.1).
* `ItemType = weapon | armor | accessory | consumable | gem | material | scroll` (`types.ts:111`).
  Sort order: weapon 0, armor 1, accessory 2, consumable 3, gem 4, material 5, scroll 6, unknown 7 (`InventorySystem.ts:493`).
* `WeaponType = sword | axe | mace | dagger | bow | staff | wand | shield` (`types.ts:125`). `mace` and `wand` have no
  bases yet (quest reward class tables reference them, `QuestRewards.ts:13-17`).

### 2.2 Item bases (`types.ts:106-139`)
```
ItemBase      { id, name(zh), nameEn, description(zh), type: ItemType, slot?: EquipSlot, icon: string,
                levelReq: int, sellPrice: int, stackable: bool, maxStack: int }
WeaponBase    : ItemBase { type='weapon', slot: 'weapon'|'offhand', baseDamage: [min,max], attackSpeed: int ms,
                weaponType, sockets: int }
ArmorBase     : ItemBase { type='armor', slot: helmet|armor|gloves|boots|belt, baseDefense: int, sockets: int }
AccessoryBase : ItemBase { type='accessory', slot: necklace|ring1|ring2 }            // no sockets field → 0
```
* Shields are `WeaponBase` with `slot: 'offhand'`, `baseDamage [0,0]`, `attackSpeed 0`, and **no `baseDefense`**.
* "Is equipment" everywhere = `base.slot` is defined (`CraftingSystem.ts:109-111`, `LootSystem.ts:88-91`).
* Lookup: `getItemBase(id)` = linear find over `[...Weapons, ...Armors, ...Accessories, ...Consumables, ...Gems,
  ...Materials]` (`bases.ts:160-166`). Ids are unique. Unknown ids return undefined and every caller tolerates it.
* `levelReq` is used **only** for loot/shop/reward pools; equipping never checks it (QUIRK Q1).
* `WeaponBase.attackSpeed` and `baseDamage` do **not** reach combat (QUIRK Q2); `baseDefense` does (§8.2).

### 2.3 Affix definitions (`types.ts:141-152`)
```
AffixDefinition { id, name(zh), nameEn, type: 'prefix'|'suffix', tier: 1..5, stat: StatKey,
                  minValue: int, maxValue: int, levelReq: int, allowedSlots?: EquipSlot[] }   // absent = any slot
```

### 2.4 Item instance (`types.ts:154-185`) — also the save format
```
ItemAffix     { affixId: string, name: string(zh), stat: StatKey, value: number }
GemInstance   { gemId: string(base id), name: string(zh), stat: StatKey, value: number, tier: int }
ItemInstance  {
  uid: string; baseId: string;
  name: string;              // zh display name baked at creation (see §4.5); display re-localises (§15)
  quality: ItemQuality;
  level: int;                // "item level": drives affix tiers, legendary scaling, craft cost, salvage yield
  affixes: ItemAffix[];      // ordered; order matters for naming
  sockets: GemInstance[];    // filled sockets, in insertion order
  bonusSockets?: int;        // sockets punched by the blacksmith (0/absent or 1)
  setId?: string;            // set membership (set pieces only)
  legendaryEffect?: string;  // zh (or locale-at-creation) effect description; text only (QUIRK Q5)
  identified: bool;          // always true in practice (§7.6)
  quantity: int;             // ≥1; >1 only for stackables
  stats: Record<StatKey, number>  // cache = Σ affix values + Σ gem values per stat (§4.5)
}
```
Invariants: `stats` must be recomputed whenever `affixes` or `sockets` change. `sockets.length ≤ capacity` (§9.1).
Port: add optional `legendaryId` and `setPieceId` fields (absent in web saves; fill on load by matching
`baseId`+`quality` if needed) so display/effects need not parse strings.

### 2.5 Sets & legendaries (`types.ts:187-205`)
```
SetDefinition       { id, name(zh), nameEn, pieces: pieceId[], pieceAffixes?: Record<pieceId, ItemAffix[]>,
                      bonuses: { count: int, description(zh), stats: Record<StatKey, number> }[] }
SetPieceBases       : Record<pieceId, baseId>        // sets.ts:7-32 + dungeonData.ts:208-211
LegendaryDefinition { id, baseId, name(zh), nameEn, fixedAffixes: ItemAffix[], specialEffect: string,
                      specialEffectValue?: number, specialEffectDescription(zh) }
```
Bonuses are cumulative (2pc + 3pc + 4pc all apply at 4 pieces).

### 2.6 Stat keys (the item stat vocabulary)
Display table `STAT_DISPLAY` (`src/data/items/affixes.ts:115-151`): key → zh label + `isPercent`. Localised label =
i18n `ui.stat.<key>` falling back to that table (`src/i18n/gameAccessors.ts:122-134`).

| Key | Shown as | % | Key | Shown as | % |
|---|---|---|---|---|---|
| damage | 伤害 | | fireDamage | 火焰伤害 | |
| damagePercent | 伤害 | ✓ | iceDamage | 冰霜伤害 | |
| defense | 防御 | | lightningDamage | 闪电伤害 | |
| defensePercent | 防御 | ✓ | poisonDamage | 毒素伤害 | |
| str / dex / int / vit / spi / lck | 力量/敏捷/智力/体力/精神/幸运 | | fireResist / iceResist / lightningResist / poisonResist / allResist | …抗性 | ✓ |
| maxHp | 生命 | | moveSpeed | 移动速度 | ✓ |
| maxHpPercent | 生命 | ✓ | magicFind | 掉宝率 | ✓ |
| maxMana | 法力 | | expBonus | 经验加成 | ✓ |
| maxManaPercent | 法力 | ✓ | cooldownReduction | 冷却缩减 | ✓ |
| lifeSteal / manaSteal | 生命偷取/法力偷取 | ✓ | knockback | 击退 | |
| hpRegen / manaRegen | 生命回复/秒, 法力回复/秒 | | critRate / critDamage / attackSpeed | 暴击率/暴击伤害/攻击速度 | ✓ |

Extra keys that appear on items/aggregates but not in `STAT_DISPLAY`:
* `allStats` — diamond gems; expanded at equipment aggregation into +v to each of str, dex, int, vit, spi, **lck** (§8.2).
* Set-bonus specials: `thornsHeal`, `deathSave`, `killHealPercent`, `critDoubleStrike`, `freeCast`,
  `elementalDamagePercent`, `doubleShot` (`sets.ts:67-190`). These are `EquipStats` fields (`combat-feel.md` §1.1).
* `weaponDamageMin`, `weaponDamageMax` — produced by aggregation (§8.2), have i18n labels
  (`zh-CN.ts:1649-1650`), but are **not** `EquipStats` fields → dropped before combat (QUIRK Q2).

`EquipStats` (`src/systems/CombatSystem.ts:6-54`) = the fixed key set that survives into combat:
damage, damagePercent, defense, defensePercent, maxHp, maxHpPercent, maxMana, maxManaPercent, critRate, critDamage,
attackSpeed, lifeSteal, manaSteal, hpRegen, manaRegen, fireDamage, iceDamage, lightningDamage, poisonDamage,
fireResist, iceResist, lightningResist, poisonResist, allResist, moveSpeed, magicFind, expBonus, cooldownReduction,
knockback, str, dex, int, vit, spi, lck, killHealPercent, deathSave, critDoubleStrike, doubleShot, freeCast,
elementalDamagePercent, ignoreDefense, damageReduction, thornsHeal, dodgeCounter.

Port: define `enum class Stat : uint8_t` over the union of all keys above with a string table for JSON/i18n;
`EquipStats` = `std::array<double, kEquipStatCount>` over the EquipStats subset.

---

## 3. Data tables (full) and JSON export

All tables below are generated from the TS source (row order = array order, which **matters** for uniform picks and
weighted lists). The descriptions (`description` zh) and per-locale names live in i18n
(`data.item.<id>.name|desc`, `data.affix.<id>`, `data.set.<id>.name|bonus.<count>`, `data.setAffix.<affixId>`,
`data.legendary.<id>.name|effect`, `data.legAffix.<affixId>` in `src/i18n/locales/{zh-CN,en}.ts`).

### 3.1 JSON files to export (one-time script from the TS modules)
| File | Source | Shape |
|---|---|---|
| `items/bases.json` | `bases.ts` Weapons, Armors, Accessories, Consumables, Gems, Materials (keep group + order) | array of `ItemBase` (+weapon/armor fields) |
| `items/gems.json` | `GEM_STAT_MAP` (`bases.ts:140-158`) | `{ baseId: {stat, value, tier} }` |
| `items/affixes.json` | `Prefixes`, `Suffixes` (`affixes.ts`) — two ordered arrays | `AffixDefinition[]` |
| `items/stat_display.json` | `STAT_DISPLAY` (`affixes.ts:115-151`) | `{ key: {label, isPercent} }` |
| `items/sets.json` | `SetDefinitions` + `DUNGEON_EXCLUSIVE_SETS`, `SetPieceBases` + `DUNGEON_SET_PIECE_BASES` (flag `dungeonExclusive`) | `SetDefinition[]`, piece→base map |
| `items/legendaries.json` | `LegendaryItems` + `DUNGEON_EXCLUSIVE_LEGENDARIES` (flag `dungeonExclusive`; keep order: overworld first) | `LegendaryDefinition[]` |
| `items/consumable_effects.json` | `InventorySystem.useConsumable` switch (`:417-426`) + ground-potion map (`ZoneScene.ts:3900-3906`) | `{ baseId: {effect, value} }` |
| `items/loot_tuning.json` | constants in §5 (drop rates, quality thresholds, windows, tier bands, difficulty mods, lifetimes) | flat object |
| `items/crafting.json` | constants in §13 | flat object |
| `items/economy.json` | buy ×3, buyback ×5, buyback size 5, bag 100, stash 80 (+10/warehouse level) | flat object |
| `shops` | `NPCDefinition.shopItems` (`npcs.ts`) — exported with the NPC table (NPC spec) | `string[]` per NPC |
| i18n | the key families listed above, zh-CN + en | string tables |

### 3.2 Bases, gems, materials, affixes

#### Weapons (`WeaponBase`, `src/data/items/bases.ts:3-40`)
| id | zh name | en name | slot | weaponType | levelReq | baseDamage | attackSpeed ms | sockets | sellPrice | icon |
|---|---|---|---|---|---|---|---|---|---|---|
| `w_rusty_sword` | 生锈的剑 | Rusty Sword | weapon | sword | 1 | 3–7 | 1000 | 0 | 5 | w_sword |
| `w_short_sword` | 短剑 | Short Sword | weapon | sword | 3 | 5–10 | 900 | 1 | 15 | w_sword |
| `w_broad_sword` | 阔剑 | Broad Sword | weapon | sword | 8 | 10–18 | 1100 | 1 | 40 | w_sword |
| `w_battle_axe` | 战斧 | Battle Axe | weapon | axe | 10 | 14–22 | 1300 | 1 | 55 | w_axe |
| `w_dagger` | 匕首 | Dagger | weapon | dagger | 1 | 2–6 | 700 | 0 | 8 | w_dagger |
| `w_stiletto` | 细剑 | Stiletto | weapon | dagger | 6 | 4–10 | 650 | 1 | 30 | w_dagger |
| `w_short_bow` | 短弓 | Short Bow | weapon | bow | 1 | 3–8 | 1100 | 0 | 10 | w_bow |
| `w_long_bow` | 长弓 | Long Bow | weapon | bow | 10 | 8–16 | 1200 | 1 | 50 | w_bow |
| `w_oak_staff` | 橡木法杖 | Oak Staff | weapon | staff | 1 | 4–9 | 1200 | 0 | 10 | w_staff |
| `w_arcane_staff` | 奥术法杖 | Arcane Staff | weapon | staff | 8 | 8–16 | 1300 | 2 | 45 | w_staff |
| `w_wooden_shield` | 木盾 | Wooden Shield | offhand | shield | 1 | 0–0 | 0 | 0 | 8 | w_shield |
| `w_iron_shield` | 铁盾 | Iron Shield | offhand | shield | 8 | 0–0 | 0 | 1 | 35 | w_shield |
| `w_militia_sword` | 民兵剑 | Militia Sword | weapon | sword | 15 | 12–20 | 1000 | 1 | 80 | w_sword |
| `w_kris` | 波纹短刀 | Kris | weapon | dagger | 15 | 8–16 | 600 | 1 | 75 | w_dagger |
| `w_composite_bow` | 复合弓 | Composite Bow | weapon | bow | 15 | 10–18 | 1100 | 1 | 78 | w_bow |
| `w_rune_staff` | 符文法杖 | Rune Staff | weapon | staff | 15 | 10–20 | 1250 | 1 | 82 | w_staff |
| `w_tower_shield` | 塔盾 | Tower Shield | offhand | shield | 15 | 0–0 | 0 | 1 | 70 | w_shield |
| `w_claymore` | 双手巨剑 | Claymore | weapon | sword | 20 | 18–30 | 1400 | 2 | 150 | w_sword |
| `w_assassin_blade` | 刺客之刃 | Assassin Blade | weapon | dagger | 20 | 12–22 | 600 | 2 | 140 | w_dagger |
| `w_war_bow` | 战争之弓 | War Bow | weapon | bow | 20 | 14–24 | 1100 | 2 | 145 | w_bow |
| `w_elder_staff` | 长老法杖 | Elder Staff | weapon | staff | 20 | 16–28 | 1300 | 2 | 155 | w_staff |
| `w_kite_shield` | 鸢盾 | Kite Shield | offhand | shield | 20 | 0–0 | 0 | 1 | 130 | w_shield |
| `w_flamberge` | 烈焰巨剑 | Flamberge | weapon | sword | 28 | 25–42 | 1300 | 2 | 250 | w_sword |
| `w_shadow_blade` | 暗影之刃 | Shadow Blade | weapon | dagger | 28 | 16–30 | 550 | 2 | 240 | w_dagger |
| `w_eagle_bow` | 苍鹰之弓 | Eagle Bow | weapon | bow | 28 | 20–35 | 1050 | 2 | 245 | w_bow |
| `w_lich_staff` | 巫妖法杖 | Lich Staff | weapon | staff | 28 | 22–38 | 1250 | 2 | 255 | w_staff |
| `w_runic_shield` | 符文盾 | Runic Shield | offhand | shield | 28 | 0–0 | 0 | 2 | 220 | w_shield |
| `w_demon_blade` | 恶魔之刃 | Demon Blade | weapon | sword | 35 | 32–52 | 1200 | 3 | 350 | w_sword |
| `w_abyssal_staff` | 深渊法杖 | Abyssal Staff | weapon | staff | 35 | 28–46 | 1300 | 3 | 360 | w_staff |
| `w_abyssal_bow` | 深渊之弓 | Abyssal Bow | weapon | bow | 35 | 26–44 | 1000 | 3 | 355 | w_bow |
| `w_abyssal_dagger` | 深渊匕首 | Abyssal Dagger | weapon | dagger | 35 | 22–38 | 500 | 3 | 345 | w_dagger |
| `w_abyssal_shield` | 深渊之盾 | Abyssal Shield | offhand | shield | 35 | 0–0 | 0 | 2 | 340 | w_shield |

#### Armour (`ArmorBase`, `src/data/items/bases.ts:42-76`)
| id | zh name | en name | slot | levelReq | baseDefense | sockets | sellPrice | icon |
|---|---|---|---|---|---|---|---|---|
| `a_cloth_cap` | 布帽 | Cloth Cap | helmet | 1 | 2 | 0 | 3 | a_helm |
| `a_leather_helm` | 皮盔 | Leather Helm | helmet | 5 | 5 | 0 | 15 | a_helm |
| `a_iron_helm` | 铁盔 | Iron Helm | helmet | 10 | 10 | 1 | 35 | a_helm |
| `a_quilted_armor` | 绗缝甲 | Quilted Armor | armor | 1 | 4 | 0 | 5 | a_armor |
| `a_leather_armor` | 皮甲 | Leather Armor | armor | 5 | 8 | 0 | 20 | a_armor |
| `a_chain_mail` | 锁子甲 | Chain Mail | armor | 10 | 15 | 1 | 50 | a_armor |
| `a_plate_armor` | 板甲 | Plate Armor | armor | 20 | 25 | 2 | 120 | a_armor |
| `a_leather_gloves` | 皮手套 | Leather Gloves | gloves | 1 | 1 | 0 | 3 | a_gloves |
| `a_chain_gloves` | 锁链手套 | Chain Gloves | gloves | 10 | 5 | 1 | 25 | a_gloves |
| `a_leather_boots` | 皮靴 | Leather Boots | boots | 1 | 1 | 0 | 3 | a_boots |
| `a_chain_boots` | 锁链靴 | Chain Boots | boots | 10 | 4 | 0 | 25 | a_boots |
| `a_leather_belt` | 皮带 | Leather Belt | belt | 1 | 1 | 0 | 2 | a_belt |
| `a_heavy_belt` | 重型腰带 | Heavy Belt | belt | 10 | 3 | 0 | 20 | a_belt |
| `a_full_helm` | 全盔 | Full Helm | helmet | 15 | 12 | 1 | 55 | a_helm |
| `a_scale_mail` | 鳞甲 | Scale Mail | armor | 15 | 18 | 1 | 65 | a_armor |
| `a_gauntlets` | 铁护手 | Gauntlets | gloves | 15 | 6 | 0 | 40 | a_gloves |
| `a_greaves` | 胫甲 | Greaves | boots | 15 | 5 | 0 | 40 | a_boots |
| `a_war_belt` | 战斗腰带 | War Belt | belt | 15 | 4 | 0 | 35 | a_belt |
| `a_heavy_plate_armor` | 重型板甲 | Heavy Plate Armor | armor | 20 | 20 | 2 | 120 | a_armor |
| `a_plate_helm` | 板甲头盔 | Plate Helm | helmet | 20 | 14 | 1 | 90 | a_helm |
| `a_plate_gloves` | 板甲手套 | Plate Gloves | gloves | 20 | 7 | 1 | 70 | a_gloves |
| `a_plate_boots` | 板甲靴 | Plate Boots | boots | 20 | 6 | 1 | 70 | a_boots |
| `a_plated_belt` | 镶板腰带 | Plated Belt | belt | 20 | 4 | 0 | 50 | a_belt |
| `a_demon_helm` | 恶魔头冠 | Demon Crown | helmet | 30 | 20 | 2 | 200 | a_helm |
| `a_demon_armor` | 恶魔铠甲 | Demon Plate | armor | 30 | 30 | 2 | 280 | a_armor |
| `a_demon_gloves` | 恶魔护手 | Demon Gauntlets | gloves | 30 | 10 | 1 | 150 | a_gloves |
| `a_demon_boots` | 恶魔战靴 | Demon Greaves | boots | 30 | 8 | 1 | 150 | a_boots |
| `a_dragon_armor` | 龙鳞甲 | Dragon Scale | armor | 35 | 38 | 3 | 350 | a_armor |
| `a_dragon_helm` | 龙角头盔 | Dragon Helm | helmet | 35 | 26 | 2 | 300 | a_helm |

#### Accessories (`AccessoryBase`, `src/data/items/bases.ts:78-86`) — no `sockets` field (capacity 0)
| id | zh name | en name | slot | levelReq | sellPrice | icon |
|---|---|---|---|---|---|---|
| `j_copper_ring` | 铜戒指 | Copper Ring | ring1 | 1 | 5 | j_ring |
| `j_silver_ring` | 银戒指 | Silver Ring | ring1 | 10 | 25 | j_ring |
| `j_gold_ring` | 金戒指 | Gold Ring | ring1 | 20 | 60 | j_ring |
| `j_platinum_ring` | 白金戒指 | Platinum Ring | ring1 | 30 | 120 | j_ring |
| `j_bone_amulet` | 骨项链 | Bone Amulet | necklace | 1 | 8 | j_amulet |
| `j_jade_amulet` | 翡翠项链 | Jade Amulet | necklace | 15 | 45 | j_amulet |
| `j_arcane_amulet` | 奥术项链 | Arcane Amulet | necklace | 30 | 100 | j_amulet |

#### Consumables & scrolls (`src/data/items/bases.ts:88-98`)
| id | zh name | en name | type | levelReq | sellPrice | maxStack | icon | zh description |
|---|---|---|---|---|---|---|---|---|
| `c_hp_potion_s` | 小型生命药水 | Minor HP Potion | consumable | 1 | 5 | 20 | c_hp | 恢复50生命 |
| `c_hp_potion_m` | 中型生命药水 | HP Potion | consumable | 10 | 15 | 20 | c_hp | 恢复150生命 |
| `c_hp_potion_l` | 大型生命药水 | Greater HP Potion | consumable | 25 | 40 | 20 | c_hp | 恢复400生命 |
| `c_mp_potion_s` | 小型法力药水 | Minor MP Potion | consumable | 1 | 5 | 20 | c_mp | 恢复30法力 |
| `c_mp_potion_m` | 中型法力药水 | MP Potion | consumable | 10 | 15 | 20 | c_mp | 恢复80法力 |
| `c_antidote` | 解毒药水 | Antidote | consumable | 1 | 8 | 10 | c_antidote | 解除毒性状态 |
| `c_tp_scroll` | 传送卷轴 | TP Scroll | scroll | 1 | 10 | 20 | c_scroll | 传送回营地 |
| `c_ley_fruit` | 灵脉果 | Ley Fruit | consumable | 1 | 12 | 20 | c_ley_fruit | 喂给灵兽：增加经验与羁绊 |
| `c_id_scroll` | 鉴定卷轴 | ID Scroll | scroll | 1 | 5 | 20 | c_scroll | 鉴定未知装备 |

#### Gems (`src/data/items/bases.ts:100-123`) joined with `GEM_STAT_MAP` (`:140-158`)
| id | zh name | en name | levelReq | sellPrice | maxStack | icon | stat | value | tier |
|---|---|---|---|---|---|---|---|---|---|
| `g_ruby_1` | 碎裂红宝石 | Chipped Ruby | 1 | 10 | 10 | g_ruby | str | 5 | 1 |
| `g_ruby_2` | 红宝石 | Ruby | 15 | 30 | 10 | g_ruby | str | 12 | 2 |
| `g_ruby_3` | 完美红宝石 | Perfect Ruby | 30 | 60 | 10 | g_ruby | str | 20 | 3 |
| `g_sapphire_1` | 碎裂蓝宝石 | Chipped Sapphire | 1 | 10 | 10 | g_sapphire | int | 5 | 1 |
| `g_sapphire_2` | 蓝宝石 | Sapphire | 15 | 30 | 10 | g_sapphire | int | 12 | 2 |
| `g_sapphire_3` | 完美蓝宝石 | Perfect Sapphire | 30 | 60 | 10 | g_sapphire | int | 20 | 3 |
| `g_emerald_1` | 碎裂翡翠 | Chipped Emerald | 1 | 10 | 10 | g_emerald | dex | 5 | 1 |
| `g_emerald_2` | 翡翠 | Emerald | 15 | 30 | 10 | g_emerald | dex | 12 | 2 |
| `g_emerald_3` | 完美翡翠 | Perfect Emerald | 30 | 60 | 10 | g_emerald | dex | 20 | 3 |
| `g_topaz_1` | 碎裂黄玉 | Chipped Topaz | 1 | 10 | 10 | g_topaz | magicFind | 5 | 1 |
| `g_topaz_2` | 黄玉 | Topaz | 15 | 30 | 10 | g_topaz | magicFind | 10 | 2 |
| `g_topaz_3` | 完美黄玉 | Perfect Topaz | 30 | 60 | 10 | g_topaz | magicFind | 18 | 3 |
| `g_diamond_1` | 碎裂钻石 | Chipped Diamond | 10 | 20 | 10 | g_diamond | allStats | 3 | 1 |
| `g_diamond_2` | 钻石 | Diamond | 18 | 50 | 10 | g_diamond | allStats | 5 | 2 |
| `g_diamond_3` | 完美钻石 | Perfect Diamond | 26 | 80 | 10 | g_diamond | allStats | 8 | 3 |
| `g_diamond_4` | 璀璨钻石 | Radiant Diamond | 34 | 120 | 10 | g_diamond | allStats | 12 | 4 |
| `g_diamond_5` | 至尊钻石 | Supreme Diamond | 40 | 180 | 10 | g_diamond | allStats | 18 | 5 |

#### Materials (`src/data/items/bases.ts:130-134`)
| id | zh name | en name | levelReq | sellPrice | maxStack | icon |
|---|---|---|---|---|---|---|
| `m_scrap` | 铁屑 | Iron Scrap | 1 | 2 | 50 | m_scrap |
| `m_dust` | 魔尘 | Magic Dust | 1 | 6 | 50 | m_dust |
| `m_essence` | 稀有精华 | Rare Essence | 1 | 20 | 50 | m_essence |

#### Prefixes (`src/data/items/affixes.ts:3-49`) — array order matters (pool order)
| # | id | zh name | en name | tier | stat | min–max | levelReq | allowedSlots (— = any) |
|---|---|---|---|---|---|---|---|---|
| 0 | `pre_sharp` | 锋利的 | Sharp | 1 | damage | 1–3 | 1 | weapon, offhand |
| 1 | `pre_quick` | 迅捷的 | Quick | 1 | attackSpeed | 3–5 | 1 | weapon, gloves |
| 2 | `pre_keen` | 敏锐的 | Keen | 2 | damage | 4–8 | 10 | weapon, offhand |
| 3 | `pre_swift` | 疾速的 | Swift | 2 | attackSpeed | 6–10 | 10 | weapon, gloves |
| 4 | `pre_deadly` | 致命的 | Deadly | 3 | damage | 10–18 | 20 | weapon, offhand |
| 5 | `pre_cruel` | 残忍的 | Cruel | 4 | damage | 20–35 | 30 | weapon, offhand |
| 6 | `pre_abyssal` | 深渊的 | Abyssal | 5 | damage | 35–55 | 40 | weapon, offhand |
| 7 | `pre_sturdy` | 坚韧的 | Sturdy | 1 | defense | 1–3 | 1 | helmet, armor, gloves, boots, belt, offhand |
| 8 | `pre_fortified` | 强化的 | Fortified | 2 | defense | 4–8 | 10 | helmet, armor, gloves, boots, belt, offhand |
| 9 | `pre_godly` | 神圣的 | Godly | 3 | defense | 10–18 | 20 | helmet, armor, gloves, boots, belt, offhand |
| 10 | `pre_mythic` | 传说的 | Mythic | 4 | defense | 20–30 | 30 | helmet, armor, gloves, boots, belt, offhand |
| 11 | `pre_strong` | 强壮的 | Strong | 1 | str | 2–4 | 1 | — |
| 12 | `pre_nimble` | 灵巧的 | Nimble | 1 | dex | 2–4 | 1 | — |
| 13 | `pre_wise` | 睿智的 | Wise | 1 | int | 2–4 | 1 | — |
| 14 | `pre_hardy` | 健壮的 | Hardy | 1 | vit | 2–4 | 1 | — |
| 15 | `pre_spiritual` | 通灵的 | Spiritual | 1 | spi | 2–4 | 1 | — |
| 16 | `pre_savage` | 凶猛的 | Savage | 2 | str | 5–10 | 10 | — |
| 17 | `pre_agile` | 敏捷的 | Agile | 2 | dex | 5–10 | 10 | — |
| 18 | `pre_arcane` | 奥术的 | Arcane | 2 | int | 5–10 | 10 | — |
| 19 | `pre_stalwart` | 刚毅的 | Stalwart | 2 | vit | 5–10 | 10 | — |
| 20 | `pre_titan` | 泰坦的 | Titan's | 3 | str | 12–20 | 20 | — |
| 21 | `pre_shadow` | 暗影的 | Shadow | 3 | dex | 12–20 | 20 | — |
| 22 | `pre_elder` | 长老的 | Elder's | 3 | int | 12–20 | 20 | — |
| 23 | `pre_empowered` | 赋能的 | Empowered | 2 | damagePercent | 10–20 | 10 | weapon |
| 24 | `pre_ferocious` | 凶残的 | Ferocious | 3 | damagePercent | 25–40 | 20 | weapon |
| 25 | `pre_merciless` | 无情的 | Merciless | 4 | damagePercent | 45–65 | 30 | weapon |
| 26 | `pre_annihilating` | 歼灭的 | Annihilating | 5 | damagePercent | 70–100 | 40 | weapon |
| 27 | `pre_reinforced` | 增幅的 | Reinforced | 2 | defensePercent | 10–20 | 10 | helmet, armor, offhand |
| 28 | `pre_indestructible` | 不灭的 | Indestructible | 4 | defensePercent | 30–50 | 30 | helmet, armor, offhand |

#### Suffixes (`src/data/items/affixes.ts:51-110`)
| # | id | zh name | en name | tier | stat | min–max | levelReq | allowedSlots (— = any) |
|---|---|---|---|---|---|---|---|---|
| 0 | `suf_life` | 生命 | of Life | 1 | maxHp | 5–15 | 1 | — |
| 1 | `suf_vitality` | 活力 | of Vitality | 2 | maxHp | 20–40 | 10 | — |
| 2 | `suf_titan_life` | 巨人生命 | of the Titan | 3 | maxHp | 50–80 | 20 | — |
| 3 | `suf_immortal` | 不朽 | of Immortality | 4 | maxHp | 100–160 | 30 | — |
| 4 | `suf_mana` | 法力 | of Mana | 1 | maxMana | 3–10 | 1 | — |
| 5 | `suf_energy` | 魔能 | of Energy | 2 | maxMana | 15–30 | 10 | — |
| 6 | `suf_vigor` | 精力 | of Vigor | 2 | maxHpPercent | 5–10 | 10 | belt, armor, necklace |
| 7 | `suf_colossus` | 巨像 | of the Colossus | 4 | maxHpPercent | 15–25 | 30 | belt, armor, necklace |
| 8 | `suf_brilliance` | 光辉 | of Brilliance | 3 | maxManaPercent | 10–20 | 20 | helmet, necklace, ring1, ring2 |
| 9 | `suf_leech` | 吸血 | of Leech | 1 | lifeSteal | 1–3 | 1 | weapon, ring1, ring2 |
| 10 | `suf_bloodthirst` | 嗜血 | of Bloodthirst | 3 | lifeSteal | 4–8 | 20 | weapon, ring1, ring2 |
| 11 | `suf_manaleech` | 噬魔 | of Sorcery | 2 | manaSteal | 2–5 | 10 | weapon, ring1, ring2 |
| 12 | `suf_regen` | 恢复 | of Regeneration | 1 | hpRegen | 1–3 | 1 | belt, ring1, ring2, necklace |
| 13 | `suf_rejuv` | 焕新 | of Rejuvenation | 3 | hpRegen | 5–10 | 20 | belt, ring1, ring2, necklace |
| 14 | `suf_meditation` | 冥想 | of Meditation | 2 | manaRegen | 2–5 | 10 | helmet, ring1, ring2, necklace |
| 15 | `suf_crit` | 暴击 | of Precision | 2 | critRate | 3–6 | 10 | weapon, gloves, ring1, ring2, necklace |
| 16 | `suf_crit_high` | 精准 | of Accuracy | 4 | critRate | 8–12 | 30 | weapon, gloves, ring1, ring2, necklace |
| 17 | `suf_devastation` | 毁灭 | of Devastation | 3 | critDamage | 10–25 | 20 | weapon, gloves, necklace |
| 18 | `suf_oblivion` | 湮灭 | of Oblivion | 5 | critDamage | 30–50 | 40 | weapon, gloves, necklace |
| 19 | `suf_flame` | 火焰 | of Flame | 1 | fireDamage | 1–4 | 1 | weapon, ring1, ring2, necklace |
| 20 | `suf_inferno` | 地狱火 | of Inferno | 2 | fireDamage | 5–12 | 10 | weapon, ring1, ring2, necklace |
| 21 | `suf_frost` | 冰霜 | of Frost | 1 | iceDamage | 1–4 | 1 | weapon, ring1, ring2, necklace |
| 22 | `suf_blizzard` | 暴风雪 | of the Blizzard | 3 | iceDamage | 8–18 | 20 | weapon, ring1, ring2, necklace |
| 23 | `suf_spark` | 雷击 | of Shock | 1 | lightningDamage | 1–5 | 1 | weapon, ring1, ring2, necklace |
| 24 | `suf_thunder` | 雷霆 | of Thunder | 3 | lightningDamage | 6–16 | 20 | weapon, ring1, ring2, necklace |
| 25 | `suf_venom` | 剧毒 | of Venom | 2 | poisonDamage | 3–8 | 10 | weapon, ring1, ring2, necklace |
| 26 | `suf_fire_res` | 抗火 | of Fire Resistance | 1 | fireResist | 5–10 | 1 | helmet, armor, boots, belt, offhand, ring1, ring2 |
| 27 | `suf_fire_res2` | 烈焰庇护 | of Fire Ward | 3 | fireResist | 15–25 | 20 | helmet, armor, boots, belt, offhand, ring1, ring2 |
| 28 | `suf_ice_res` | 抗冰 | of Cold Resistance | 1 | iceResist | 5–10 | 1 | helmet, armor, boots, belt, offhand, ring1, ring2 |
| 29 | `suf_ice_res2` | 冰霜庇护 | of Cold Ward | 3 | iceResist | 15–25 | 20 | helmet, armor, boots, belt, offhand, ring1, ring2 |
| 30 | `suf_lightning_res` | 抗雷 | of Lightning Resistance | 1 | lightningResist | 5–10 | 1 | helmet, armor, boots, belt, offhand, ring1, ring2 |
| 31 | `suf_poison_res` | 抗毒 | of Poison Resistance | 2 | poisonResist | 8–15 | 10 | helmet, armor, boots, belt, offhand, ring1, ring2 |
| 32 | `suf_all_res` | 全抗 | of the Zodiac | 4 | allResist | 5–12 | 30 | helmet, armor, offhand, ring1, ring2, necklace |
| 33 | `suf_speed` | 疾行 | of Speed | 1 | moveSpeed | 5–10 | 1 | boots |
| 34 | `suf_haste` | 神速 | of Haste | 3 | moveSpeed | 15–25 | 20 | boots |
| 35 | `suf_luck` | 幸运 | of Fortune | 1 | lck | 2–4 | 1 | — |
| 36 | `suf_fortune` | 财运 | of Wealth | 2 | magicFind | 5–15 | 10 | helmet, boots, ring1, ring2, necklace |
| 37 | `suf_exp` | 历练 | of Experience | 2 | expBonus | 3–8 | 10 | helmet, necklace, ring1, ring2 |
| 38 | `suf_honing` | 磨砺 | of Honing | 3 | cooldownReduction | 5–10 | 20 | helmet, weapon, necklace |

### 3.3 Sets and legendaries (`src/data/items/sets.ts`, `src/data/dungeonData.ts:131-211`)

Set pieces — the piece id is data only (never stored on the item); fixed piece affixes are **not** level-scaled.

| set id | zh / en | piece id | base (levelReq, slot) | fixed piece affixes (affixId stat value) |
|---|---|---|---|---|
| `set_iron_guardian` | 铁壁守护者 / Iron Guardian | `set_iron_helm` | `a_plate_helm` (20, helmet) | set_ig_1 defense 15 (守护者之坚); set_ig_2 allResist 8 (坚定意志) |
| `set_iron_guardian` | 铁壁守护者 / Iron Guardian | `set_iron_armor` | `a_plate_armor` (20, armor) | set_ig_3 maxHp 80 (铁壁之心); set_ig_4 defense 25 (铜皮铁骨) |
| `set_iron_guardian` | 铁壁守护者 / Iron Guardian | `set_iron_shield` | `w_tower_shield` (15, offhand) | set_ig_5 defense 20 (不动如山); set_ig_6 str 10 (磐石之力) |
| `set_iron_guardian` | 铁壁守护者 / Iron Guardian | `set_iron_belt` | `a_war_belt` (15, belt) | set_ig_7 maxHp 40 (守卫之韧); set_ig_8 vit 8 (体魄强健) |
| `set_shadow_assassin` | 暗影刺客 / Shadow Assassin | `set_shadow_helm` | `a_full_helm` (15, helmet) | set_sa_1 critRate 8 (鹰眼); set_sa_2 dex 12 (暗影潜行) |
| `set_shadow_assassin` | 暗影刺客 / Shadow Assassin | `set_shadow_armor` | `a_chain_mail` (10, armor) | set_sa_3 dex 10 (影遁之衣); set_sa_4 critDamage 15 (暗杀本能) |
| `set_shadow_assassin` | 暗影刺客 / Shadow Assassin | `set_shadow_gloves` | `a_chain_gloves` (10, gloves) | set_sa_5 poisonDamage 10 (毒刃); set_sa_6 attackSpeed 8 (迅捷之手) |
| `set_shadow_assassin` | 暗影刺客 / Shadow Assassin | `set_shadow_boots` | `a_chain_boots` (10, boots) | set_sa_7 moveSpeed 15 (无影步); set_sa_8 dex 8 (幻影闪避) |
| `set_archmage` | 大法师 / Archmage | `set_archmage_hat` | `a_demon_helm` (30, helmet) | set_am_1 int 15 (渊博学识); set_am_2 manaRegen 5 (冥思苦想) |
| `set_archmage` | 大法师 / Archmage | `set_archmage_robe` | `a_scale_mail` (15, armor) | set_am_3 maxMana 60 (魔力之泉); set_am_4 allResist 10 (奥术屏障) |
| `set_archmage` | 大法师 / Archmage | `set_archmage_staff` | `w_arcane_staff` (8, weapon) | set_am_5 damagePercent 30 (魔力增幅); set_am_6 fireDamage 12 (元素精通) |
| `set_archmage` | 大法师 / Archmage | `set_archmage_ring` | `j_gold_ring` (20, ring1) | set_am_7 manaSteal 5 (法力虹吸); set_am_8 int 10 (智者之环) |
| `set_hunter` | 荒野猎手 / Wilds Hunter | `set_hunter_helm` | `a_leather_helm` (5, helmet) | set_hu_1 critRate 5 (鹰眼视野); set_hu_2 dex 10 (猎手直觉) |
| `set_hunter` | 荒野猎手 / Wilds Hunter | `set_hunter_armor` | `a_leather_armor` (5, armor) | set_hu_3 defense 15 (野性皮甲); set_hu_4 hpRegen 4 (自然恩赐) |
| `set_hunter` | 荒野猎手 / Wilds Hunter | `set_hunter_boots` | `a_leather_boots` (1, boots) | set_hu_5 moveSpeed 20 (追风步); set_hu_6 dex 8 (丛林行者) |
| `set_hunter` | 荒野猎手 / Wilds Hunter | `set_hunter_bow` | `w_war_bow` (20, weapon) | set_hu_7 damage 20 (穿甲之矢); set_hu_8 attackSpeed 12 (连珠箭法) |
| `set_abyssfire` | 渊火之誓 / Abyssfire Oath | `set_abyss_ring` | `j_platinum_ring` (30, ring1) | set_af_1 damagePercent 15 (深渊共鸣); set_af_2 fireDamage 15 (渊火灼烧) |
| `set_abyssfire` | 渊火之誓 / Abyssfire Oath | `set_abyss_amulet` | `j_arcane_amulet` (30, necklace) | set_af_3 allResist 12 (渊火庇护); set_af_4 maxHpPercent 10 (不灭意志) |
| `set_abyssfire` | 渊火之誓 / Abyssfire Oath | `set_abyss_belt` | `a_plated_belt` (20, belt) | set_af_5 defense 18 (深渊束缚); set_af_6 str 8 (渊火之力) |
| `set_abyss_walker` *(dungeon)* | 深渊行者 / Abyss Walker | `set_aw_crown` | `a_dragon_helm` (35, helmet) | set_aw_1 allResist 15 (深渊凝视); set_aw_2 critRate 10 (虚空感知) |
| `set_abyss_walker` *(dungeon)* | 深渊行者 / Abyss Walker | `set_aw_blade` | `w_demon_blade` (35, weapon) | set_aw_3 damage 40 (深渊之刃); set_aw_4 critDamage 25 (虚空切裂) |

| set id | count | stats | zh description |
|---|---|---|---|
| `set_iron_guardian` | 2 | maxHpPercent 30 | +30% 最大生命 |
| `set_iron_guardian` | 3 | thornsHeal 2, allResist 15 | 受击回复2%最大生命，+15 全抗 |
| `set_iron_guardian` | 4 | deathSave 1 | 生命低于30%时免疫一次致死伤害（60秒冷却） |
| `set_shadow_assassin` | 2 | critRate 20, attackSpeed 10 | +20% 暴击率，+10% 攻击速度 |
| `set_shadow_assassin` | 3 | critDamage 50, killHealPercent 5 | 暴击伤害 +50%，击杀回复5%生命 |
| `set_shadow_assassin` | 4 | critDoubleStrike 25 | 暴击时25%概率连击（立即发动额外一次攻击） |
| `set_archmage` | 2 | maxManaPercent 25, manaRegen 3 | +25% 法力上限，法力回复 +3/秒 |
| `set_archmage` | 3 | cooldownReduction 20, fireDamage 20, iceDamage 20 | 技能冷却减少20%，+20 火焰/冰霜伤害 |
| `set_archmage` | 4 | freeCast 15, elementalDamagePercent 30 | 施法时15%概率不消耗法力，所有元素伤害+30% |
| `set_hunter` | 2 | attackSpeed 15, moveSpeed 20 | +15% 攻击速度，+20% 移动速度 |
| `set_hunter` | 3 | magicFind 30, killHealPercent 3 | +30% 掉宝率，击杀回复3%生命 |
| `set_hunter` | 4 | doubleShot 30, critDamage 20 | 普攻30%概率发射双倍箭矢，+20% 暴击伤害 |
| `set_abyssfire` | 2 | allResist 10, damagePercent 15 | +10% 全抗，+15% 伤害 |
| `set_abyssfire` | 3 | killHealPercent 8, cooldownReduction 10 | 击杀恢复8%最大生命，+10% 冷却缩减 |
| `set_abyss_walker` | 2 | damagePercent 20, allResist 15, killHealPercent 5 | 深渊迷宫中伤害+20%，全抗+15，击杀回复5%生命 |

| legendary id | zh / en | base (levelReq) | fixed affixes (affixId stat baseValue) | specialEffect = value | zh effect text |
|---|---|---|---|---|---|
| `leg_soulreaver` | 灵魂收割者 / Soulreaver | `w_demon_blade` (35) | leg_1 damage 45; leg_2 lifeSteal 8; leg_2b critDamage 20 | killHealPercent = 5 | 击杀回复5%最大生命 |
| `leg_frostburn` | 霜火之杖 / Frostburn | `w_arcane_staff` (8) | leg_3 fireDamage 20; leg_4 iceDamage 20; leg_4b int 15 | elementalDamagePercent = 25 | 所有元素伤害 +25% |
| `leg_windforce` | 风之力 / Windforce | `w_war_bow` (20) | leg_9 damage 35; leg_10 knockback 2; leg_10b attackSpeed 15 | doubleShot = 25 | 普攻25%概率发射双倍箭矢 |
| `leg_grief` | 悲伤 / Grief | `w_broad_sword` (8) | leg_gr1 damage 30; leg_gr2 attackSpeed 20; leg_gr3 lifeSteal 5 | ignoreDefense = 20 | 攻击忽略目标20%防御 |
| `leg_shadowstep` | 暗影之履 / Shadowstep | `a_leather_boots` (1) | leg_5 moveSpeed 25; leg_6 dex 15 | dodgeCounter = 1 | 闪避后下次攻击必定暴击 |
| `leg_aegis` | 不灭之盾 / Aegis | `w_iron_shield` (8) | leg_7 defense 30; leg_8 maxHp 100; leg_8b allResist 10 | deathDefiance = 10 | 受到致命伤害时10%概率免死并回复30%生命 |
| `leg_tyrael` | 大天使之铠 / Tyrael's Might | `a_plate_armor` (20) | leg_ty1 defense 35; leg_ty2 allResist 15; leg_ty3 hpRegen 8 | damageReduction = 10 | 受到的所有伤害减少10% |
| `leg_soj` | 乔丹之石 / Stone of Jordan | `j_gold_ring` (20) | leg_soj1 maxManaPercent 25; leg_soj2 damagePercent 20; leg_soj3 int 12 | cooldownReduction = 10 | 所有技能冷却减少10% |
| `leg_maras` | 玛拉的万花筒 / Mara's Kaleidoscope | `j_jade_amulet` (15) | leg_mk1 allResist 20; leg_mk2 str 5; leg_mk3 int 5; leg_mk4 dex 5 | allStatsBonus = 2 | 每次升级额外获得+2全属性 |
| `leg_abyss_crown` *(dungeon)* | 深渊之冠 / Crown of the Abyss | `a_dragon_helm` (35) | leg_ac1 damagePercent 25; leg_ac2 allResist 18; leg_ac3 maxHp 120; leg_ac4 defense 30 | dungeonDamageBonus = 15 | 在深渊迷宫中所有伤害额外增加15% |
| `leg_void_edge` *(dungeon)* | 虚空之刃 / Voidedge | `w_demon_blade` (35) | leg_ve1 damage 55; leg_ve2 lifeSteal 10; leg_ve3 critDamage 30; leg_ve4 str 15 | voidStrike = 20 | 每次攻击20%概率触发虚空打击，造成额外50%暗影伤害 |

### 3.4 Tunable constants (content of `loot_tuning.json`, `economy.json`, `crafting.json`)
| Constant | Value | Source |
|---|---|---|
| Equipment drop base chance (normal / elite) | 40 / 80 % | `LootSystem.ts:35` |
| Luck → drop chance | `+ luck × 0.5` % | `:36` |
| Elite second drop | 50 % + luck×0.5 | `:47` |
| Third drop gate / chance | affix bonus ≥ 10 / 30 % + luck×0.5 + affix bonus | `:54` |
| Consumable drop / gem drop | 30 % / 5 % + luck×0.1 + affix bonus×0.3 | `:68, :74` |
| Quality thresholds | see §5.3 | `:124-135` |
| Difficulty loot mods (levelBonus, qualityBonus, extraAffixes) | normal 0/0/0, nightmare 3/5/1, hell 6/12/2 | `:23-29` |
| Mini-boss floors | zone mini-boss `magic`, sub-dungeon mini-boss `rare` | `:62-65` |
| Base windows (equip / wide / set piece) | `[L-10, L+3]` / `[L-20, L+5]` / `[L-15, L+5]` (lower bound ≥ 1) | `:145, :112, :164` |
| Consumable/gem/affix level gate | `levelReq ≤ L + 5` | `:378, :396, :274` |
| Affix tier bands | L<8 [1,2]; <18 [1,3]; <28 [2,4]; <38 [3,5]; else [4,5]; allowed ±1; weight in-band 3 / out 1 | `:267-290` |
| Affix counts | magic 1–2, rare 3–4 (+ difficulty extra on the max), generic legendary 3–5, set piece +1–2 random | `:218-229, 323, 194` |
| Legendary value scale | `clamp(level/35, 0.6, 1.5)`, `Math.round` | `:314-318` |
| Dropped consumable quantity | 1–3 | `:390` |
| Ley fruit drop | elite 12 % / other 1.5 % | `PetSystem.ts:98-100` |
| Ground item lifetime / potion lifetime | 60 000 ms / 30 000 ms (treasure-cache items: none) | `ZoneScene.ts:3987, 4021` |
| Pickup radius (click, auto-loot, potions) | `distSq ≤ 4` (2 tiles) | `:4032, 3728, 1500` |
| Click hit box on loot | `abs(Δcol) < 1.5 && abs(Δrow) < 1.5` | `:5676` |
| Auto-loot cadence | > 300 ms between scans | `:1521` |
| Bag capacity | 100 entries | `InventorySystem.ts:10` |
| Stash capacity | 80 + 10 per warehouse level | `:12`, `homestead.ts:67` |
| Buy price / buyback price / buyback slots | `sellPrice × 3` / `sell × 5` / 5 (FIFO) | `UIScene.ts:1238`, `InventorySystem.ts:25, 128-131` |
| Craft gold unit | `6 × (max(1, floor(L)) + 5)` | `CraftingSystem.ts:114-116` |
| Socket caps | ≤ 3 total, ≤ 1 punched | `:79-81` |
| Craft bag capacity | 100 | `:83` |

---

## 4. Creating an item instance

### 4.1 `createItem(baseId, level, quality, extraAffixes = 0)` (`LootSystem.ts:199-235`)
Used for every item that is not a set piece rolled by `generateSetPiece` (drops, shop purchases, quest/dialogue
rewards, ley fruit, homestead yields).
```
base = getItemBase(baseId); if !base → null
item = { uid: newUid(), baseId, name: base.name /*zh*/, quality, level, affixes: [], sockets: [],
         identified: true, quantity: 1, stats: {} }
switch quality:
  magic:     addRandomAffixes(item, level, 1, 2 + extraAffixes)
  rare:      addRandomAffixes(item, level, 3, 4 + extraAffixes)
  legendary: makeLegendary(item, baseId)          // extraAffixes ignored
  set:       makeSetItem(item, baseId)            // dead path in practice, see 4.4
  normal:    (nothing)
buildItemName(item); computeStats(item)
```
`quantity` is always 1 here (consumables bought or rewarded arrive one at a time and stack in the bag).

### 4.2 Random affixes — `addRandomAffixes(item, L, min, max)` (`LootSystem.ts:252-306`)
Also exposed as the crafting hook `rollAffixes` (`:238-240`).
```
count = randomInt(min, max)
used  = { a.affixId for a in item.affixes }          // pre-existing affixes (set piece fixed affixes, upgrades)
nP = nS = 0                                         // NB: counts only affixes added by THIS call
slot = getItemBase(item.baseId)?.slot               // may be undefined (then no slot filter)
(minT, maxT) = L<8 ? (1,2) : L<18 ? (1,3) : L<28 ? (2,4) : L<38 ? (3,5) : (4,5)
loT = max(1, minT-1);  hiT = min(5, maxT+1)
repeat count times:
  wantPrefix = (nP <= nS)
  pool = [a in (wantPrefix ? Prefixes : Suffixes), in table order, where
            a.levelReq <= L + 5
            and a.id ∉ used
            and (a.allowedSlots absent or slot absent or slot ∈ a.allowedSlots)
            and loT <= a.tier <= hiT]
  if pool empty: continue          // nP/nS unchanged → the next iterations ask for the same side again
  weighted = for a in pool: append a ×(minT <= a.tier <= maxT ? 3 : 1)      // keep order
  a = weighted[randomInt(0, weighted.length-1)]
  item.affixes.push({ affixId: a.id, name: a.name /*zh*/, stat: a.stat, value: randomInt(a.minValue, a.maxValue) })
  used.add(a.id); if a.type == prefix: nP++ else nS++
```
Consequences worth testing: a 1-affix magic item always gets a **prefix**; 2 → P,S; rare 3 → P,S,P; 4 → P,S,P,S.
Two different affixes on the same stat (e.g. `pre_sharp` + `pre_keen`) are allowed and sum in `stats`.
The rolled value is uniform in `[min,max]`, independent of item level (level only gates tiers).
"Zone scaling" of affixes = item level → tier band; there is no other zone input.

### 4.3 Legendary — `makeLegendary(item, baseId)` (`LootSystem.ts:308-326`)
```
def = first d in [...LegendaryItems, ...DUNGEON_EXCLUSIVE_LEGENDARIES] with d.baseId == baseId
if def:
  item.name = def.name
  scale = clamp(item.level / 35, 0.6, 1.5)
  item.affixes = def.fixedAffixes.map(a => { ...a, value: Math.round(a.value * scale) })
  item.legendaryEffect = def.specialEffectDescription           // zh text, never applied as a stat (QUIRK Q5)
else:                                                           // "generic legendary"
  addRandomAffixes(item, item.level, 3, 5)
  item.legendaryEffect = t('sys.loot.genericLegendaryEffect')   // locale at creation time; zh '蕴含未知的力量'
```
Any equipment base can roll legendary quality; only bases with a definition become a named legendary. Because the
lookup is "first match", `w_demon_blade` always becomes Soulreaver and `leg_void_edge` can never be created; the
"dungeon-exclusive" Crown of the Abyss (`a_dragon_helm`) is created by any overworld legendary roll on that base
(QUIRK Q17). Named legendaries keep `item.name = def.name` because `buildItemName` skips legendary quality.

### 4.4 Set items
**Real path — `generateSetPiece(L)`** (`LootSystem.ts:152-197`), used whenever loot rolls quality `set`:
```
pieceBase = { ...SetPieceBases, ...DUNGEON_SET_PIECE_BASES }
candidates = []
for setDef in [...SetDefinitions, ...DUNGEON_EXCLUSIVE_SETS]:      // table order
  for pieceId in setDef.pieces:
    base = getItemBase(pieceBase[pieceId]); if !base continue
    if base.levelReq <= L + 5 and base.levelReq >= max(1, L - 15): candidates.push({pieceId, base, setDef})
if candidates empty → null                                          // the drop is simply lost
pick = candidates[randomInt(0, n-1)]
item = { uid, baseId: pick.base.id, name: `${setDef.name} ${base.name}` /*zh, space-separated*/, quality: 'set',
         level: L, affixes: copy(setDef.pieceAffixes[pieceId] ?? []) /*values NOT level-scaled*/, sockets: [],
         identified: true, quantity: 1, stats: {}, setId: setDef.id }
addRandomAffixes(item, L, 1, 2)       // fixed affix ids pre-seed `used`; the same STAT may repeat
computeStats(item)                    // no buildItemName
```
A set item does not record which piece it is; identity is `(setId, baseId)`. Dungeon-exclusive pieces are in the
same pool (bases L35 → reachable from overworld drops at item level ≥ 30, QUIRK Q17).

**Dead path — `makeSetItem`** (`LootSystem.ts:328-342`): looks for `setDef.pieces.includes(baseId)`, but `pieces`
holds piece ids, never base ids, so it always falls through to "add 2–3 random affixes, no `setId`". Only reachable via
`createItem(..., 'set')`, which no shipped caller does. Port: implement `createItem(set)` as "pick a piece whose base
is `baseId`, else behave like web fallback".

### 4.5 Naming and stats
`buildItemName(item)` (`LootSystem.ts:344-364`) — sets the stored zh `name`:
```
if quality in {normal, legendary, set}: return              // name unchanged
prefix = first affix whose affixId is in Prefixes;  suffix = first affix whose affixId is in Suffixes
name = base.name; if prefix: name = prefix.name + name; if suffix: name = name + ' (' + suffix.name + ')'
```
e.g. `锋利的短剑 (生命)`. The UI does **not** show this string; it rebuilds a localised name (§15).

`computeStats(item)` (`LootSystem.ts:366-375`) ≡ `InventorySystem.recomputeItemStats` (`InventorySystem.ts:396-405`):
`stats = {}`; `stats[a.stat] += a.value` for every affix; `stats[g.stat] += g.value` for every socketed gem.

`refreshItem(item)` (crafting finalize, `LootSystem.ts:243-250`): if quality is `normal` → `name = base.name`;
then `buildItemName`; then `computeStats`.

---

## 5. Loot generation

### 5.1 Monster kill → loot (`ZoneScene.onMonsterKilled`, `ZoneScene.ts:3789-3929`; loot part `:3884-3914`)
Order of operations relevant to items (the rest is in `combat-feel.md` §13.1):
1. `gold = randomInt(def.goldReward[0], def.goldReward[1])`, added straight to `player.gold` (never a ground item).
   `def` is the spawn-time, difficulty-scaled definition (`combat-feel.md` §14).
2. Ley fruit: `if rand() < (def.elite ? 0.12 : 0.015)` (`PetSystem.ts:98-100`) →
   `createItem('c_ley_fruit', player.level, 'normal')` → ground drop (§6.1).
3. Quest drops (`rollQuestDrops`, quest spec) — not inventory items.
4. Loot roll inputs:
   * `luck = player.stats.lck + homeBonus.magicFind + (inLabyrinth ? floorConfig.magicFindBonus : 0)` where
     `player.stats.lck` is the **raw** stat (class base + allocated points, no gear) and
     `homeBonus = mergeBonuses(homestead.getTotalBonuses(), pets.getBonuses())` (`ZoneScene.ts:3796, 3892-3893`).
     Gear `lck`/`magicFind` are not included (QUIRK Q3). Chapter 1: `luck = stats.lck` (5 warrior/mage, 8 rogue,
     `src/data/classes/*.ts:8`) + allocated points.
   * `affixLootBonus = Σ eliteAffix.lootQualityBonus` over the monster's elite affixes (3–8 each,
     `EliteAffixSystem.ts:53-180, 270-298`) `+ (inLabyrinth ? floorConfig.lootQualityBonus : 0)` (`:3895-3898`).
   * `difficulty` = current difficulty.
5. `loot = generateLoot(def, luck, affixLootBonus, difficulty)` (§5.2).
6. For each item in order: if `baseId` is one of the 5 potions below → spawn a **potion pickup** (§6.2) with that
   amount (stack quantity ignored); otherwise spawn a ground item (§6.1).

| Ground-potion id | type | amount |
|---|---|---|
| c_hp_potion_s / m / l | hp | 50 / 150 / 400 |
| c_mp_potion_s / m | mp | 30 / 80 |

(`ZoneScene.ts:3900-3906`; antidote, scrolls, ley fruit drop as normal ground items.)

### 5.2 `generateLoot(def, playerLuck, affixLootBonus = 0, difficulty = 'normal')` (`LootSystem.ts:31-80`)
```
mods = normal:    { levelBonus 0, qualityBonus 0,  extraAffixes 0 }
       nightmare: { levelBonus 3, qualityBonus 5,  extraAffixes 1 }          (LootSystem.ts:23-29)
       hell:      { levelBonus 6, qualityBonus 12, extraAffixes 2 }
L   = def.level + mods.levelBonus                 // item level
lb  = playerLuck * 0.5
qb  = affixLootBonus + mods.qualityBonus
out = []
// 1) main equipment drop
if chance((def.elite ? 80 : 40) + lb):            out += generateEquipment(L, rollQuality(L, playerLuck, def.elite ?? false, qb), mods.extraAffixes)
// 2) elite second drop
if def.elite and chance(50 + lb):                 out += generateEquipment(L, rollQuality(L, playerLuck, true, qb), mods.extraAffixes)
// 3) affix-elite third drop (gate uses affixLootBonus WITHOUT the difficulty bonus)
if affixLootBonus >= 10 and chance(30 + lb + affixLootBonus):
                                                  out += generateEquipment(L, rollQuality(L, playerLuck, true, qb), mods.extraAffixes)
// 4) mini-boss floor
if def.isMiniBoss: enforceFloor(out, L, def.isSubDungeonMiniBoss ? 'rare' : 'magic')
// 5) consumable
if chance(30):                                    out += generateConsumable(L)
// 6) gem
if chance(5 + lb * 0.2 + affixLootBonus * 0.3):   out += generateGem(L)
return out                                        // null results are skipped
```
`enforceFloor(out, L, floor)` (`:87-107`): if no item in `out` with an equipment base meets
`qualityMeetsFloor(q, floor)`, append `generateEquipment(L, floor)` (no extra affixes), or if that returned null
`generateEquipmentWide(L, floor)`. Existing lower-quality drops are kept.

### 5.3 Quality roll — `rollQuality(L, luck, isElite, affixBonus)` (`LootSystem.ts:124-135`)
```
r = rand() * 100;  lm = luck * 0.3;  em = isElite ? 15 : 0;  a = affixBonus
if r < 0.5 + lm*0.1 + (L > 20 ? 1 : 0) + a*0.15 → legendary
if r < 2   + lm*0.2 + em*0.5           + a*0.3  → set
if r < 15  + lm     + em               + a      → rare
if r < 45  + lm                        + a*0.5  → magic
else normal
```
Checks are sequential thresholds on one draw (not additive bands). Note the main drop of a non-elite monster passes
`isElite = false`, but the second/third drops always pass `true`.

### 5.4 Base selection
* `generateEquipment(L, q, extra = 0)` (`:137-150`): `q == set` → `generateSetPiece(L)` (§4.4). Otherwise pool =
  `[...Weapons, ...Armors, ...Accessories]` (table order) filtered `levelReq <= L+3 && levelReq >= max(1, L-10)`;
  empty → null; else uniform pick → `createItem(base, L, q, extra)`. Every base has equal weight (a ring is as likely
  as a sword).
* `generateEquipmentWide(L, q)` (`:110-122`): same with window `[max(1, L-20), L+5]`; if empty, any equipment base.
* `generateConsumable(L)` (`:377-393`): pool = Consumables with `levelReq <= L+5` (table order), uniform;
  item `{quality normal, level 1, quantity randomInt(1,3), identified}`.
* `generateGem(L)` (`:395-411`): pool = Gems with `levelReq <= L+5`, uniform; `{quality normal, level 1, quantity 1}`.

### 5.5 Other item sources (all general systems)
| Source | Rule | Cite |
|---|---|---|
| Quest fixed items | `createItem(id, player.level, 'normal')` per listed id (duplicates = 2 units); ids must be item base ids (a set piece id would yield nothing) | `ZoneScene.ts:4119-4122` |
| Quest pick-one gear | §5.6 | `QuestRewards.ts:48-91`, `ZoneScene.ts:4096-4105` |
| Quest turn-in delivery | each granted item: `addItem`, and if the bag is full → `stash.push(item)` (ignores stash capacity) | `ZoneScene.ts:4125-4128` |
| Dialogue choice reward | `createItem(id, player.level, 'normal')` → `addItem` (lost if bag full) | `UIScene.ts:3645-3651` |
| Shop purchase | `createItem(id, player.level, 'normal')` (§12.3) | `UIScene.ts:1254-1262` |
| Random-event treasure cache | fake def `{level: Lc, elite: true, goldReward: [10+5Lc, 20+10Lc]}`; `generateLoot(fake, player.stats.lck, floor(Lc/10))` with **default difficulty** (QUIRK Q20); `gold = randomInt(...)`; items → `dropLootAtPosition` (§6.1, potions stay items). `Lc = floor((zoneMin+zoneMax)/2)` | `ZoneScene.ts:3307-3343`, `RandomEventSystem.ts:394-399` |
| Hidden-area chest | `generateEquipment(zone.levelRange[1], value=='legendary'?legendary : value=='rare'?rare : magic)` → `addItem` directly (lost if full, log still claims success — QUIRK Q20); gold pile `parseInt(value ?? '100')` | `ZoneScene.ts:4974-5005` |
| Ley fruit | §5.1 step 2 | `ZoneScene.ts:3884-3888` |
| Homestead garden / caravan / gem combine | later milestone (`EmberTower.ts:248, 284, 328`) — use `createItem(id, player.level, 'normal')` | — |

Chapter 1 instances: treasure cache `Lc = 4`, quality bonus 0, gold 30–60; hidden chest `rare` at item level 7, gold
pile 200 (`emerald_plains.ts:40, 57-59`).

### 5.6 Quest pick-one gear (`QuestRewards.ts`)
```
itemLevel = max(q.level, min(playerLevel, q.level + 5))                         (:65-67)
quality   = q.rewards.choiceQuality ?? (q.category == 'main' ? 'rare' : 'magic') (:69-71)
for choice in q.rewards.choices:                                                (:74-91)
  pool = candidates(choice, classId):
    weapon  → Weapons with slot 'weapon' and weaponType ∈ CLASS_WEAPON_TYPES[class]
              (warrior: sword, axe, mace · mage: staff, wand · rogue: dagger, bow · unknown class: sword)
    offhand → warrior: Weapons with slot 'offhand' (shields) · other classes: all Accessories
    jewelry → all Accessories
    armor | helmet | gloves | boots | belt → Armors with that slot
  usable = pool with levelReq <= itemLevel + 2
  ranked = (usable nonempty ? usable : [pool's lowest-levelReq base]) stable-sorted by levelReq DESC, first 3
  base   = ranked[floor(rand() * ranked.length)]
  item   = createItem(base, itemLevel, quality), identified
```
Choices are generated once per quest per session and cached (`rewardChoiceCache`, not saved) so re-opening the card
cannot reroll; the chosen index is clamped to `[0, n-1]` (default 0). Chapter 1 gear quests:
`q_find_goblin_chief` (L7, main → rare, weapon+armor), `q_secure_plains` (L8, main → rare, jewelry+boots+gloves),
`q_lost_pendant` (L4, side → magic, jewelry), `q_bandit_trouble` (L7, side → magic, weapon); fixed potions from
`q_collect_slime_gel` (2× `c_hp_potion_s`), `q_herb_gathering` (`c_hp_potion_s`), `q_escort_merchant_plains`
(`c_hp_potion_m`) (`src/data/quests/all_quests.ts:10-147, 803-887`).

### 5.7 Reference probabilities (use as test expectations)
| Case | Value |
|---|---|
| Equipment drop chance, normal monster, luck 5 | 40 + 2.5 = **42.5 %** |
| Same, elite | **82.5 %**; elite second drop **52.5 %** |
| Consumable | **30 %** flat |
| Gem, luck 5, no affixes | 5 + 2.5×0.2 = **5.5 %**; with one affix of bonus 5: **7.0 %** |
| Quality thresholds, L3, luck 5, non-elite, bonus 0 | legendary r < **0.65**, set < **2.3**, rare < **16.5**, magic < **46.5** |
| `goblin_chief` (L5, elite) main drop, luck 5, affix bonus 5 | legendary < **1.4**, set < **11.3**, rare < **36.5**, magic < **49.0** |
| L21, luck 0, non-elite | legendary < **1.5**, set < **2**, rare < **15**, magic < **45** |

### 5.8 RNG draw order (for seeded tests; parity with JS not required)
`chance(main)` → [`rollQuality` → (`generateSetPiece`: `randomInt` piece → `addRandomAffixes`) or
(`randomInt` base → affix rolls)] → elite `chance` → … → third-drop `chance` (only if gate) → floor item (no draw unless
needed) → `chance(30)` → [`randomInt` consumable, `randomInt` qty] → `chance(gem)` → [`randomInt` gem].
`addRandomAffixes` = `randomInt(count)` then per affix `randomInt(pick)`, `randomInt(value)` (no draws for empty pools).

### 5.9 Chapter 1 pools (item levels 1–13; generated from the data)
In Chapter 1, random drops use item level 1–7 (monsters 1–6 — `goblin_chief` 5, shaman 6, quest hunts their base
monster's level; treasure cache 4; hidden chest 7). Quest gear reaches item level 13 (`q_secure_plains` 8+5) but picks
bases with §5.6, not the drop window; shop goods use the hero level (≤ 10). Rows 8–13 are listed for reference (the
general rule, and where Chapter 2 starts).

| Item level | Equipment drop window `[max(1,L-10), L+3]` → bases | Affix tier band (allowed) | Set-piece candidates | Named legendaries possible |
|---|---|---|---|---|
| 1–2 | L1: 13 bases (all `levelReq 1` + `w_short_sword`); L2 adds `a_leather_helm`, `a_leather_armor` (15) | [1,2] (1–3) | hunter helm/armor/boots | `leg_shadowstep` |
| 3–4 | 16 bases (+ `w_stiletto`) | [1,2] (1–3) | + `set_archmage_staff` | `leg_shadowstep` |
| 5–6 | 19 bases (+ `w_broad_sword`, `w_arcane_staff`, `w_iron_shield`) | [1,2] (1–3) | + shadow armor/gloves/boots (7) | + `leg_grief`, `leg_frostburn`, `leg_aegis` |
| 7–11 | 27 bases (+ all `levelReq 10`) | L7 [1,2]; L8+ [1,3] (1–4) | L10+: 11 (+ iron shield/belt, shadow helm, archmage robe) | same 4 |
| 12–13 | 26 bases (window drops `levelReq 1`, adds `levelReq 15`) | [1,3] (1–4) | 11 | `leg_grief`, `leg_frostburn`, `leg_aegis`, `leg_maras` |

Affixes reachable at item level ≤ 4 (`levelReq ≤ L+5` → tier 1 only): prefixes `pre_sharp, pre_quick, pre_sturdy,
pre_strong, pre_nimble, pre_wise, pre_hardy, pre_spiritual`; suffixes `suf_life, suf_mana, suf_leech, suf_regen,
suf_flame, suf_frost, suf_spark, suf_fire_res, suf_ice_res, suf_lightning_res, suf_speed, suf_luck`. From item level 5
the tier-2 rows (`levelReq 10`) join: 17 prefixes, 23 suffixes (counts before the per-slot `allowedSlots` filter); nothing above tier 2 is reachable below item level 15.
Consumable pool: L1–4 `c_hp_potion_s, c_mp_potion_s, c_antidote, c_tp_scroll, c_ley_fruit, c_id_scroll`; L5+ adds
`c_hp_potion_m, c_mp_potion_m`. Gem pool: L1–4 `g_ruby_1, g_sapphire_1, g_emerald_1, g_topaz_1`; L5+ adds `g_diamond_1`;
L10+ adds the `_2` ruby/sapphire/emerald/topaz; L13+ adds `g_diamond_2`.

Items that can reach a Chapter 1 hero (drives the icon/mesh list for the art pipeline): the 27 equipment bases with
`levelReq ≤ 10` (drop windows up to item level 7; shop wares and set-piece bases are a subset) plus `j_jade_amulet`,
`a_greaves`, `a_gauntlets` from quest gear at item level 12–13 → **30 equipment bases**; all 9 consumables/scrolls;
gems `g_ruby_1, g_sapphire_1, g_emerald_1, g_topaz_1, g_diamond_1`; the 3 materials. Named legendaries: Shadowstep,
Grief, Frostburn, Aegis. Set pieces: Wilds Hunter helm/armor/boots, Shadow Assassin armor/gloves/boots, Archmage staff.

---

## 6. World drops and pickup

### 6.1 Ground items
* `dropLoot(item, col, row)` (`ZoneScene.ts:3931-3994`) — monster drops: logical position = the monster's tile
  `(col,row)`; the visual is offset by `(+U[0,0.5), +U[0,0.5))` tiles. Pushed to `lootDrops`; emits
  `ITEM_DROPPED {item}`; despawns **60 000 ms** later if still on the ground (matched by uid).
* `dropLootAtPosition(item, col, row)` (`:3684-3717`) — treasure caches: falls in from 30 px above with a 400 ms
  bounce, visual jitter ±10 px x / ±5 px y, **no despawn timer** and no `ITEM_DROPPED` event.
* All ground items and potion pickups are destroyed on zone exit (`:7249-7252`); they are not saved.

### 6.2 Potion pickups (`dropPotion`, `:3996-4028`; collection `:1496-1518`)
`{type: hp|mp, amount, col, row}`; despawn after **30 000 ms**. Every frame, each potion with
`distSq(hero, potion) <= 4` (2 tiles) is consumed immediately: `hp = min(maxHp, hp + amount)` (log
`zone.combat.restoreHp`, type `combat`) or `mana = min(maxMana, mana + amount)` (log `zone.combat.restoreMana`,
`info`). It is collected even at full HP/MP (wasted). Not an inventory item.

### 6.3 Click pickup (`:774-786`, `findLootAt :5674-5679`, `pickupLoot :4030-4065`)
Left pointer-down (not during a cinematic, hero alive) → tile under pointer → first ground item in `lootDrops` order
with `|col - c| < 1.5 && |row - r| < 1.5` (loot is tested **before** NPCs, exits, monsters). Then:
* `distSq(hero, drop) > 4` → path to `(round(col), round(row))` and stop (**no pickup on arrival**; QUIRK Q22).
* else `inventory.addItem(item)`; on success emit `ITEM_PICKED {item}`, achievement `collect` if legendary, remove
  the drop, fly-to-hero animation (300 ms). On failure the item stays (`addItem` logged "bag full").

### 6.4 Auto-loot (`:1520-1524`, `handleAutoLoot :3719-3745`)
`player.autoLootMode ∈ {off, all, magic, rare, legendary}` (`Player.ts:56`), cycled by the HUD button in that order
(`UIScene.ts:619-624`), saved in `settings.autoLootMode`, carried across zones. When not `off`, at most every
**300 ms**: iterate `lootDrops` from last to first; skip `rank < minRank` (`all 0, magic 1, rare 2, legendary 3`;
item ranks normal 0, magic 1, rare 2, legendary/set 3); skip `distSq > 4`; `addItem` → success: same as click pickup
(fly 250 ms); failure: **stop scanning** this tick. Potions are always auto-collected regardless of mode.

### 6.5 Feedback on drop/pickup (render-only details in §18)
`ITEM_DROPPED` → legendary/set: camera flash 220 ms α 0.35 in the quality colour + shake 160 ms intensity 0.005
(`VFXManager.ts:78-83`). `ITEM_PICKED` → SFX `loot_common | loot_magic | loot_rare | loot_legendary` (set uses
legendary) (`AudioManager.ts:298-312`) and a HUD loot notice (§15.4).

---

## 7. The bag (`InventorySystem`)

### 7.1 Model
`inventory: ItemInstance[]`, capacity `MAX_INVENTORY = 100` **entries** (a stack is one entry; there is no item
footprint/2-D packing) (`InventorySystem.ts:10`). Order = insertion order (new entries are appended); the UI lays it out
row-major, 10 columns × 5 rows = 50 per page (`UIScene.ts:981, 1093-1117`).

### 7.2 `addItem(item)` (`:27-54`)
```
if base(item).stackable:
  ex = first entry with ex.baseId == item.baseId and ex.quantity < maxStack
  if ex: add = min(item.quantity, maxStack - ex.quantity); ex.quantity += add; item.quantity -= add
         if item.quantity <= 0: log 'sys.inventory.obtainedQty' {name, qty: add}; return true
if inventory.length >= 100: log 'sys.inventory.bagFull'; return false   // item.quantity may already be reduced!
inventory.push(item); log 'sys.inventory.obtained' {name: qualityPrefix + item.name}; return true
```
Only one partial stack is topped up; the remainder becomes a new entry (not split by `maxStack`). Quality prefix:
`[魔法] / [稀有] / [传奇] / [套装]` (`sys.inventory.qualityPrefix.*`). Stack matching is by `baseId` only.

### 7.3 Removal, sort, discard, bulk destroy
* `removeItem(uid, qty = 1)` (`:56-68`): if `entry.quantity > qty` → decrement, return a shallow copy with
  `quantity = qty` and the **same uid** (unused by shipped callers; port: give the split a new uid); else splice and
  return the entry. Unknown uid → null.
* `sortInventory` / `sortStash` (`:434-440, 492-504`): stable sort by quality order, then type order (§2.1), then
  `name.localeCompare` (stored zh name). Port: compare localised display names with the current culture's collation.
* `discardItem(uid)` (`:442-450`): removes (destroys — nothing is dropped on the ground), logs `sys.inventory.discarded`,
  emits `ITEM_DISCARDED {item}` (no listener). UI asks for confirmation for rare/legendary/set (`UIScene.ts:4307-4316,
  4347-4370`).
* `destroyNormalItems()` (`:452-469`): removes every entry with quality `normal` whose base type is weapon/armor/
  accessory (keeps consumable, scroll, gem, material, unknown bases); logs `sys.inventory.bulkDestroy {count}` if > 0.
  UI "destroy" button, **no confirmation**, resets to page 0 (`UIScene.ts:1133-1137`).

### 7.4 Using consumables (`useConsumable`, `:407-432`; UI `UIScene.ts:4296-4305`)
Only bases of type `consumable` or `scroll`:

| id | effect | value | applied by caller |
|---|---|---|---|
| c_hp_potion_s / m / l | heal | 50 / 150 / 400 | `hp = min(maxHp, hp + v)` |
| c_mp_potion_s / m | mana | 30 / 80 | `mana = min(maxMana, mana + v)` |
| c_antidote | antidote | 1 | **nothing** (QUIRK Q8) |
| c_tp_scroll | teleport | 1 | **nothing** (town portal is free on `R` / right-click, `ZoneScene.ts:2191-2193, 5801`) |
| any other (c_ley_fruit, c_id_scroll) | — | — | returns null, **not consumed** |

On a recognised id: `quantity -= 1`, remove the entry at 0. No cooldown, usable in combat, no potion hotkeys or belt
(QUIRK Q9). Ley fruit is consumed by the pet panel (pets spec).

### 7.5 Bag item actions (UI contract)
Context popup on click/tap (`UIScene.ts:4280-4337`): equipment → **Equip**, **Discard**; consumable/scroll → **Use**,
**Discard**; gem/material → **Discard**. Equipped slot click (desktop): socket panel if the item has socket capacity,
else unequip at once; touch: popup **Sockets** (if capacity > 0) + **Unequip** (`:1045-1073`).
Panel toolbar: page prev/next, **Sort**, **Destroy normals**; title `ui.inventory.title` = `背包 ({count}/{max})` with max 100; an "equipment bonus" summary lists
every non-zero key of `getEquipmentStats()` as `label +v[%]` (`:1139-1153`).

### 7.6 Identification (vestigial)
`identifyItem(uid)` (`:256-270`) consumes one `c_id_scroll` to set `identified = true`, but **no UI calls it**, every
generator sets `identified = true`, and loading a save forces `identified = true` on every item
(`ZoneScene.ts:4334-4343`). The `unidentified` branches in tooltip/compare/forge never trigger. Port: keep the field and
the function (cheap), generate everything identified (QUIRK Q10, open question O4).

---

## 8. Equipment (10 slots) and stat aggregation

### 8.1 `equip(uid)` (`InventorySystem.ts:70-104`)
```
item = bag entry with uid, else false;  slot = base.slot, else false
if slot == ring1 and eq.ring1 and not eq.ring2: slot = ring2
elif slot == ring2 and eq.ring2 and not eq.ring1: slot = ring1
cur = eq[slot]
if cur: if inventory.length >= 100: log 'sys.inventory.swapBagFull'; return false
        inventory.push(cur)                          // old item goes to the END of the bag
remove item from bag; eq[slot] = item; log 'sys.inventory.equipped'; return true
```
* No level, class or weapon-type requirement (QUIRK Q1). Any class can wield any weapon and shield; there is no
  two-handed rule (weapon + offhand always allowed).
* Both rings worn → a new ring always replaces **ring1** (compare shows the weaker ring, QUIRK Q18).
* A swap is refused when the bag is exactly full even though the count would not change (QUIRK Q13).

`unequip(slot)` (`:106-118`): refuse with `bagFull` if the bag has 100 entries; else append to the bag.
After any equip/unequip/socket change the UI calls `zone.invalidateEquipStats()`; the port should invalidate inside the
core on every equipment mutation.

### 8.2 `getEquipmentStats()` (`:149-188`)
```
S = {}
for item in equipment values:
  base = getItemBase(item.baseId)
  if base has baseDamage:  S.weaponDamageMin += baseDamage[0]; S.weaponDamageMax += baseDamage[1]   // shields add 0
  if base has baseDefense: S.defense += baseDefense                                                 // armour only
  for (k, v) in item.stats: S[k] += v                     // affixes + gems; identification ignored
for (k, v) in setBonusStats(): S[k] += v
if S.allStats: for k in [str, dex, int, vit, spi, lck]: S[k] += S.allStats; delete S.allStats
```
`setBonusStats()` (`:203-223`): `count[setId]` = number of equipped items with that `setId`; for each set definition
(overworld then dungeon), for each bonus with `count >= bonus.count`, add its stats. Counting is by `setId`, not by
distinct piece (two `set_archmage_ring` in ring1+ring2 count as 2 — QUIRK Q14).
`getEquippedSetPieceCount(setId)` (`:226-232`) is the same count for the tooltip.

`getTypedEquipStats()` (`:191-200`): zero-filled `EquipStats`, copying only keys that exist in `EquipStats`
(drops `weaponDamageMin/Max` and any unknown key).

### 8.3 Runtime merge (`ZoneScene.getEquipStats`, `ZoneScene.ts:3143-3171`)
`typed gear stats` + achievement bonuses + active pet passive (except `expBonus`, `magicFind`, which are applied at the
kill) + altar blessing + labyrinth boons, each only for keys present in `EquipStats`; cached until invalidated.
Consumers: hero derived stats (`Player.recalcDerived`, `Player.ts:114-146`; `classes-stats-skills.md`) and combat
(`combat-feel.md` §1.1).

### 8.4 What gear does and does not do (important for balance parity)
* Live: every affix/gem/set stat whose key is an `EquipStats` field; armour `baseDefense` → `defense`.
* Inert (QUIRK Q2/Q3/Q5): weapon `baseDamage` and `attackSpeed` (hero damage/attack speed come from stats/class),
  shields have no base defense, `knockback` (Windforce), `magicFind` and gear `lck` for loot, and every legendary
  `specialEffect` (only shown as text). Of the legendary effects, `killHealPercent, elementalDamagePercent,
  doubleShot, ignoreDefense, dodgeCounter, damageReduction, cooldownReduction` already have combat consumers in
  `EquipStats`; `deathDefiance, allStatsBonus, dungeonDamageBonus, voidStrike` have none.

---

## 9. Gems and sockets

### 9.1 Capacity (`CraftingSystem.ts:95-106`)
`baseSocketCount(item)` = base `sockets` for weapon/armour bases (0 for accessories, consumables, unknown bases).
`itemSocketCapacity(item) = baseSocketCount + max(0, bonusSockets ?? 0)`. Hard caps for punching: total ≤ 3,
punched ≤ 1 (§13). `InventorySystem.getMaxSockets(slot)` = capacity of the equipped item (0 if none).

### 9.2 `socketGem(equipSlot, gemUid)` (`InventorySystem.ts:278-328`) — **equipped items only**
Fail (false) if: slot empty; base unknown; `sockets.length >= capacity` (log `sys.inventory.gem.noSlots`); gem uid not
in bag; base type ≠ gem; no `GEM_STAT_MAP` row. Else: `GemInstance {gemId: baseId, name: base.name, stat, value, tier}`
from the map; gem stack `quantity -= 1` (entry removed at 0); `sockets.push(gem)`; recompute `item.stats`; log
`sys.inventory.gem.socketed`. Free of charge; any gem fits any socketed item (no per-slot gem effects).

### 9.3 `unsocketGem(equipSlot, index)` (`:336-381`)
Fail if slot empty or index out of range; refuse if the bag has 100 entries (log `gem.bagFullRemove`) even when the gem
would stack (QUIRK Q19). Else remove the gem at `index` (others shift left), return it to the first bag stack of the
same `gemId` with `quantity < maxStack(10)` or as a new entry (`quantity 1, level 1, quality normal`); recompute stats;
log `gem.removed`. Free; the gem is never destroyed.

Gem stat `allStats` stays as `allStats` inside `item.stats` and is expanded only in §8.2.
Gem combining (3 → 1 next tier) is a homestead feature (later milestone, `EmberTower.ts:284-298`).

### 9.4 Socket panel (UI contract, `UIScene.ts:4373-4559`)
Opened from an equipped item with capacity > 0. Shows each socket (filled: gem icon, `T<tier>`, `name (+v label)`,
**Remove** button; empty: ◇), lists every gem in the bag (7 per row; click to socket when a socket is free; dimmed
otherwise), and an **Unequip** button.

---

## 10. Stash

* `stash: ItemInstance[]`; capacity = `BASE_STASH_SLOTS (80)` + homestead `stashSlots` (warehouse: +10 per level,
  `src/data/homestead.ts:67`) (`InventorySystem.ts:12`, `UIScene.ts:1810-1812`). Saved per hero (`SaveData.stash`).
* `moveToStash(uid, cap)` (`:234-243`): refuse when `stash.length >= cap` (log `sys.inventory.stashFull`); else
  remove the **whole** bag entry and append it. **No stacking inside the stash.**
* `moveFromStash(uid)` (`:245-254`): `addItem(item)` (stacks into the bag); on success remove from the stash; on
  failure the UI logs `ui.stash.bagFull`.
* `sortStash()` uses the bag comparator.
* Overflow: quest turn-in delivers into the stash when the bag is full, ignoring capacity (`ZoneScene.ts:4126`), so
  `stash.length` can exceed capacity; the UI draws `max(cap, length)` slots, slots beyond capacity look locked.
* Access: NPC type `stash` → `UI_TOGGLE_PANEL {panel: 'stash', npcId}` (`ZoneScene.ts:4174-4175`). UI: stash grid left,
  bag grid right, both 8×5 per page with paging and **Sort** buttons; desktop click moves the item across, touch shows a
  **存入仓库 / 取回背包** popup first (`UIScene.ts:1832-1933`). No stash keeper exists in Chapter 1 (open question O9).

---

## 11. Gold

`player.gold` integer ≥ 0, starts at **0**; no starting items (`Player.ts:22`). No cap. Never a ground item.

| Sources | Sinks |
|---|---|
| Kill: `randomInt(goldReward)` (§5.1) | Shop buy (§12.3), buyback (§12.5) |
| Quest turn-in `rewards.gold` (`ZoneScene.ts:4117`) | Blacksmith crafting (§13) |
| Dialogue choice `reward.gold` (`UIScene.ts:3636-3640`) | Mercenary hire / revive (companion spec, `UIScene.ts:4725, 4920`) |
| Treasure cache, hidden-area gold pile, puzzle/rescue events (`ZoneScene.ts:3330, 3531, 3650, 4995`) | Death penalty: soul echo takes `floor(gold × 10/15/20 %)` (normal/nightmare/hell) at hero level ≥ 5 (`SoulEcho.ts:15-34`, applied `ZoneScene.ts:1221-1226`; death spec) |
| Selling (§12.4); reclaiming a soul echo (`ZoneScene.ts:1277`) | Homestead upgrades, gem combining (later milestones) |

---

## 12. NPC shops

### 12.1 Opening
Click an NPC within 3 tiles (`ZoneScene.ts:788-791`) → `interactNPC` (`:4136-4151`): log its first dialogue line,
progress `talk` objectives, advance craft-quest phases, then for type `blacksmith` or `merchant` emit
`SHOP_OPEN {npcId, shopItems, type}`. The UI opens the shop panel (`UIScene.openShop`, `:1163-1423`); closing (X or
backdrop) emits `SHOP_CLOSE {npcId}`. NPCs use these two events to play a talking pose (render-only).

### 12.2 Stock
Static `shopItems` list per NPC, shown in list order, infinite stock, no restock, no level scaling, no random gear.
Chapter 1 shops:
* `blacksmith`: `w_short_sword, w_broad_sword, w_dagger, w_stiletto, w_short_bow, w_oak_staff, w_wooden_shield,
  a_leather_helm, a_leather_armor, a_leather_gloves, a_leather_boots, a_leather_belt` (+ forge tab).
* `merchant`: `c_hp_potion_s, c_hp_potion_m, c_mp_potion_s, c_mp_potion_m, c_antidote, c_tp_scroll, c_id_scroll,
  g_ruby_1, g_sapphire_1, g_emerald_1, g_topaz_1`.
* `plains_herbalist`: `c_hp_potion_s, c_hp_potion_m, c_mp_potion_s, c_antidote`.

### 12.3 Buy
`buyPrice = base.sellPrice × 3` (`UIScene.ts:1238`). The button is enabled iff `gold >= buyPrice`. On click:
`gold -= buyPrice`; `item = createItem(id, player.level, 'normal')` (identified, quantity 1); `addItem(item)` — if the
bag is full the item is **lost** and the gold stays spent (QUIRK Q6). One unit per click. Bought gear is normal quality
at item level = hero level.

### 12.4 Sell (`InventorySystem.sellItem`, `:120-135`)
```
price = base ? base.sellPrice * item.quantity : 1        // quality, affixes, sockets, level ignored
buyback.push({ item: copy(item) /*sockets, affixes, stats copied*/, buybackPrice: price * 5 })
if buyback.length > 5: buyback.shift()                    // FIFO, keeps the 5 most recent
remove the whole entry from the bag; return price         // caller adds it to gold
```
Interaction (`UIScene.ts:1350-1389`): desktop left- or right-click on a bag item sells it at once if its quality is
normal/magic/rare; legendary/set ask for confirmation (`showSellConfirm`, `:1425-1447`). Touch: tap → popup
`ui.shop.sellAction` ("出售 {price}G") → legendary/set confirmation. Only bag items can be sold (equipped items are not listed); gems socketed in a sold item go with it. The tooltip
shows the per-unit `sellPrice` (`:4185-4192`).

### 12.5 Buyback (`buybackItem(index)`, `:137-147`)
Refuse if index invalid or the bag is full (`bagFull`); else remove the entry from the list and append the item to the
bag **without stacking** (uid preserved), return `{item, cost}`; the UI subtracts `cost` (button disabled if
`gold < cost`) and logs `ui.shop.buybackLog`. The buyback list lives in the session (survives zone changes, cleared on
reload; not saved) and is shared by every shop.

### 12.6 Layout constraint (QUIRK Q7)
The web panel draws wares only while `rowTop + i×40 + 40 ≤ 426` (8 rows) and places the buyback list *below all wares*
(`:1233-1237, 1266-1272`). Result: the merchant's last 3 wares (`g_sapphire_1, g_emerald_1, g_topaz_1`), the
blacksmith's last 4 (`a_leather_armor, a_leather_gloves, a_leather_boots, a_leather_belt`) and the whole buyback list
at both NPCs are never visible; only `plains_herbalist` shows buyback. Port: scrollable lists — this is a layout bug,
not design.

### 12.7 Other shop data
* Blacksmith panel has two tabs, **wares** and **forge** (§13); merchant has wares only (`:1206-1226`).
* Wandering-merchant random event sends `merchantItems` like `iron_sword`, `hp_potion` that are not item bases
  (`RandomEventSystem.ts:109-161`), so its shop is empty; `priceMultiplier 1.2` is never read (QUIRK Q12).
* Homestead `potionDiscount` (`homestead.ts:32`) is never applied.

---

## 13. Blacksmith crafting (`CraftingSystem.ts`, pure)

Item level `L`, gold unit `U = craftGoldUnit(L) = 6 × (max(1, floor(L)) + 5)` (`:114-116`): L1 → 36, L5 → 60,
L40 → 270. Materials (`m_scrap` 铁屑, `m_dust` 魔尘, `m_essence` 稀有精华) are ordinary stackable bag items (maxStack 50).

### 13.1 Costs (`craftCost`, `:140-160`) — `null` = action does not apply
| Action | Applies to | Gold | Materials |
|---|---|---|---|
| salvage | any equipment | 0 | — |
| reforge | magic | U | 2 scrap + 1 dust |
| reforge | rare | 2U | 2 dust + 1 essence |
| upgrade | normal → magic | 2U | 3 scrap + 1 dust |
| upgrade | magic → rare | 5U | 4 dust + 1 essence |
| socket | weapon or armour base (not accessories) | 3U | 4 scrap + 1 essence |

Legendary/set items can be salvaged and (weapon/armour bases) socket-punched, but never reforged or upgraded; socket
punching ignores quality entirely.

### 13.2 Salvage yield (`salvageYield`, `:119-137`), `L = max(1, item.level || 1)`
`scrap = 1 + floor(L/12) + (quality == normal ? 1 : 0)`; magic: `dust = 1 + floor(L/20)`; rare: `dust = 1 + floor(L/20)`,
`essence = 1`; legendary/set: `dust = 2 + floor(L/20)`, `essence = 2 + floor(L/25)`. Socketed gems are returned.

### 13.3 `check(action, item, bag, wallet)` (`:264-293`) — first failing reason wins
1. base unknown → `unknownBase`; 2. no slot → `notEquipment`; 3. cost null → `notEquipment` for socket, else
`quality`; 4. item not in the bag array (e.g. equipped) → `notInBag`; 5. socket: `bonusSockets >= 1` or
`capacity >= 3` → `maxSockets`; 6. `gold < cost.gold` → `gold`; 7. any material short (summed over all stacks) →
`materials`; 8. salvage: `bag.length - 1 + slotsNeeded(yields + one per socketed gem) > 100` → `bagFull`, where
`slotsNeeded` counts overflow beyond the free room in existing stacks, `ceil(overflow / maxStack)` per material/gem.
Result `{ok, reason?, cost}` (cost present from step 4 on).

### 13.4 `perform(action, item, bag, wallet)` (`:295-343`)
On `check` failure return `{ok: false, reason, action, item}`. Else `gold -= cost.gold`; spend materials draining the
**smallest stacks first**, then remove emptied stacks; then:
* **salvage**: remove the item; add each material via `addStacked` (top up existing stacks in bag order, then new
  stacks of ≤ maxStack); return each socketed gem the same way (new gem entries are `quantity 1, level 1`). Result
  carries `yields` and `gems`.
* **reforge**: `range = magic [1,2] | rare [3,4]`; clear affixes; `rollAffixes(item, item.level, range)`;
  `identified = true`; `finalize` (= `refreshItem`: rebuild name + stats; sockets/gems kept).
* **upgrade**: `next = normal→magic | magic→rare`; `quality = next`;
  `target = range[0] + floor(rng() × (range[1]-range[0]+1))`; `need = max(1, target - affixes.length)`;
  `rollAffixes(item, item.level, need, need)` (keeps existing affixes; the new ones again start with a prefix);
  `identified = true`; `finalize`.
* **socket**: `bonusSockets += 1`.
Crafting acts only on bag items, never on equipped gear. The roller (`lootCraftRoller`, `:353-358`) uses the
loot generator's RNG; `rng` (constructor) is only used for the upgrade target.

### 13.5 Forge UI contract (`UIScene.ts:1451-1746`)
Forge tab: click a bag item to put it on the anvil (non-equipment → error SFX + log `ui.forge.notEquipment`; clicking
it again takes it off). The pane shows the anvil card (name, quality · level, sockets `filled/max`, affix list), owned
material counts, and one row per action with a preview (salvage: yields list + "gems returned"; reforge: `min–max`
affixes; upgrade: `from → to` quality; socket: `cap → cap+1`), the cost chips (red when short) or the block reason,
and a button enabled iff `check().ok`. Salvage of a rare/legendary/set item or of any item with filled sockets asks
for confirmation (`:1645-1651`). After success: SFX `anvil` (+ `loot_rare`/`loot_magic` 140 ms later on upgrade), log
`ui.forge.log.<action>`, hammer-strike FX on the anvil slot. Desktop right-click in the forge tab still sells.

---

## 14. Item comparison (`ItemCompare.ts`, pure)

* `itemStatTotals(item)` (`:16-26`): `__avgDamage = (min+max)/2` if the base has `baseDamage` (shields contribute 0),
  `__baseDefense = baseDefense` if non-zero; affix values only if `identified || quality == normal`; socketed gem
  values always.
* `compareTarget(item, equipment)` (`:45-58`): null if the base has no slot or the item (by uid) is worn. Rings: ring1
  empty → `{ring1}`; else ring2 empty → `{ring2}`; else the ring with the lower `itemScore = Σ|totals| + level×0.1`
  (tie → ring1). Other slots: `{slot, equipped: eq[slot]}`.
* `statDeltas(candidate, equipped?)` (`:69-80`): for every key in either total, `d = Math.round((a-b)×10)/10`, keep
  `d ≠ 0`; stable sort by rank (`__avgDamage` 0, `__baseDefense` 1, others 2) then `|d|` descending.
* Tooltip (`UIScene.ts:4206-4232`): header `ui.compare.header {slot}`; empty slot → `ui.compare.emptySlot`; no deltas
  → `ui.compare.same`; each delta `▲ +x` (green `#7ee07e`) / `▼ −x` (red `#ff6b5a`) with `%` for percent stats.
  Desktop shows the worn item's tooltip beside the candidate's (tag `ui.compare.equippedTag`); touch shows only the
  candidate.

---

## 15. Names, tooltips, localisation

### 15.1 Display name (`getItemDisplayName`, `src/i18n/gameAccessors.ts:42-105`)
```
base missing → item.name
baseName = i18n 'data.item.<baseId>.name' ?? (en ? base.nameEn : base.name)
if quality == normal or no affixes → baseName
split affixes: suffix if AllAffixes[affixId].type == 'suffix'; EVERYTHING else (incl. legendary/set fixed affixes) → "prefix"
en: [prefixes' nameEn ?? stored name].join(' ') + ' ' + baseName  + (suffixes ? ' ' + [suffix nameEn].join(' ') : '')
zh: [data.affix.<id> ?? stored name].join('')    + baseName        + (suffixes ? '·' + [suffix names].join('·') : '')
    (the prefix part and its trailing space are omitted when there are no prefixes)
```
So a rare reads e.g. `锋利的强壮的短剑·生命` / `Sharp Strong Short Sword of Life`. QUIRK Q4: legendary and set items
display as the concatenation of their fixed affix names + base (`灵魂收割噬灵亡灵之力恶魔之刃`), never their real name;
the `data.legendary.*`, `data.legAffix.*`, `data.setAffix.*` i18n keys exist but are unused. Recommend FIX: legendary
→ `data.legendary.<id>.name`; set → `data.set.<setId>.name` + ' ' + baseName.

### 15.2 Tooltip content (`buildItemTooltip`, `UIScene.ts:4052-4203`), top to bottom
Icon + display name (quality colour) + `<quality label> · Lv.<level>`; type line `ui.tooltip.type.<type>
(ui.tooltip.slot.<slot>)`; weapon `damage min-max`; armour `defense v`; base description; then (divider) affix lines
`+v[%] <label>` (blue `#7fb0ff`), legendary effect text (orange), gem effect (when the item is a gem), socketed gems
`◆ name: +v label`, socket count `filled/max`; set block: `<set name> (equipped/total)` and each bonus
`✓|○ (count) <desc>` (green when active); compare block (§14); sell price. QUIRK Q15: the set bonus text is looked up
as `data.set.<id>.bonus.<index>` with index 0..n-1, but the locale keys are `bonus.<count>` (2/3/4), so English shows
the zh fallback. FIX: use `bonus.count`.

### 15.3 Stored strings
`ItemInstance.name`, affix/gem `name`, `legendaryEffect` are zh strings baked at creation (generic legendary text uses
the locale active at that moment). Log messages (`sys.inventory.obtained`, `ui.dialogue.gotItem`) and the world drop
label use the stored `name` (QUIRK Q16). Port: core logs carry ids; UE localises.

### 15.4 Loot notices (`handleItemPicked`, `UIScene.ts:721-763`)
On `ITEM_PICKED`: a 236×34 notice (icon + display name ×qty, quality colour) slides in from +24 px over 220 ms at the
loot anchor (bottom-right), newest at the bottom, max 4 (oldest destroyed), each 40 px apart; after 3200 ms fades out
over 400 ms.

---

## 16. Save data
`SaveData` (`src/data/types.ts:609-670`, written `ZoneScene.ts:4237-4283`, read `:4302-4376`):
`player.gold`, `inventory: ItemInstance[]`, `equipment: Partial<Record<EquipSlot, ItemInstance>>`,
`stash: ItemInstance[]`, `settings.autoLootMode`. Item JSON = §2.4 field names verbatim. Load fix-ups: missing
`sockets` → `[]` (`SaveSystem.ts:26-32, 95-97`), every item `identified = true`. Not saved: ground loot, buyback, reward
choice cache, forge selection, UI pages. Zone transitions keep the `GameSession` (inventory/stash/buyback live there,
`src/game/GameSession.ts:26-28`) and pass gold + `autoLootMode` in `playerStats` (`ZoneScene.ts:440-456`).
Port: keep these names in the core's save JSON so a web-save importer is trivial (open question O10).

---

## 17. Events

| Web event (`src/utils/EventBus.ts`) | Payload | Emitted by | Port (core → UE) |
|---|---|---|---|
| `ITEM_DROPPED` | `{item}` | `dropLoot` | `LootDropSpawned {dropId, item, col, row, expiresAtMs?}` (+ `PotionDropSpawned {dropId, type, amount, col, row}`) |
| — | — | despawn / pickup / zone exit | `LootDropRemoved {dropId, reason: picked\|expired\|zoneExit}` |
| `ITEM_PICKED` | `{item}` | click/auto pickup | `ItemPicked {item}` (HUD notice, SFX, achievement) |
| — | — | potion pickup | `PotionPicked {type, amount}` |
| `ITEM_DISCARDED` | `{item}` | `discardItem` | `ItemDiscarded {item}` |
| `INVENTORY_CHANGED` | `{}` | hidden chest, pets, Ember Tower | `InventoryChanged`, `EquipmentChanged {slot}` (also invalidates `EquipStats`), `StashChanged` |
| `SHOP_OPEN` / `SHOP_CLOSE` | `{npcId, shopItems, type}` / `{npcId}` | `interactNPC` / UI | `ShopOpened {npcId, type, wares}` / `ShopClosed {npcId}` |
| `UI_TOGGLE_PANEL` | `{panel:'inventory'\|'stash', npcId?}` | keys `I`, stash NPC | UI-only |
| `LOG_MESSAGE` | `{text, type: loot\|system\|info\|combat}` | everywhere | `LogMessage {key, params, type}` |
| — | — | gold changes (web mutates `player.gold` directly) | `GoldChanged {gold, delta, reason}` |
| — | — | crafting | `CraftPerformed {result}` |

i18n log keys used by this area: `sys.inventory.{bagFull, stashFull, equipped, swapBagFull, obtained, obtainedQty,
qualityPrefix.*, identify.needScroll, identify.success, gem.socketed, gem.noSlots, gem.removed, gem.bagFullRemove,
discarded, bulkDestroy}`, `sys.loot.genericLegendaryEffect`, `zone.combat.restoreHp|restoreMana`,
`zone.quest.rewardItem`, `zone.hiddenArea.gotItem|gotGold`, `zone.event.treasureChest.goldReward`, `ui.shop.*`,
`ui.forge.*`, `ui.stash.*`, `ui.socket.*`, `ui.tooltip.*`, `ui.compare.*`, `ui.context.*`, `ui.inventory.*`.

---

## 18. Render-only parts and their 3D / UE equivalents

| Web (render-only) | Source | 3D / UE equivalent |
|---|---|---|
| Ground drop = tinted "loot bag" sprite (non-normal tinted by quality), floating name label (zh name, 12–13 px Cinzel), bob ±5 px over 800 ms yoyo | `ZoneScene.ts:3931-3985` | `ALootDrop` actor: the item's own 3D mesh lying on the ground (D2 style) or a small bag mesh, quality-tinted rim/emissive; world-space nameplate widget in the quality colour with the **localised** name; gentle bob ≈ 0.1 tile, 1.6 s period. Optional "show all labels" key (Alt). |
| Magic+ ground glow; rare+ light pillar (70 px, legendary/set 110 px) + flash + ring; legendary/set sparkle motes; pulse α×0.55 | `VFXManager.ts:158-209` | Decal glow for magic+; Niagara light beam for rare+ (≈1.5 tiles, legendary/set ≈2.4 tiles) shooting up over 260 ms; sparkle burst for legendary/set. |
| Legendary/set drop: camera flash 220 ms α 0.35 + shake 160 ms 0.005 | `VFXManager.ts:78-83` | Post-process colour flash + camera shake asset, same timings. |
| Potion pickup = flask sprite, bob 600 ms | `ZoneScene.ts:3996-4017` | Flask mesh (hp red / mp blue, size by tier), same bob. |
| Treasure-cache drops fall in with 400 ms bounce | `ZoneScene.ts:3708-3714` | Short physics-lite arc from the chest. |
| Pickup: sprite flies to hero (300 ms click / 250 ms auto), scale → 0.3, fade | `ZoneScene.ts:4053-4063, 3733-3739` | Lerp the mesh to the hero's chest socket, scale down + fade, same durations. |
| Procedural 96 px item icons with per-base variants | `src/graphics/icons/ItemIcons.ts` | Icons rendered from the Blender item meshes (one per base id; variant tables list which bases need distinct looks); quality frame drawn by UMG. |
| Hero appearance ignores equipment | — | Open question O8 (attach weapon/offhand meshes by `weaponType`). |
| Inventory / shop / stash / socket / forge panels, tooltips, context popups, loot notices | `UIScene.ts` (cited per section) | UMG widgets driven by core state; mouse: hover tooltip + click/right-click; touch: tap → tooltip + action popup (thumb-sized), as the web touch path does. Use scrolling lists for wares/buyback (Q7). |
| Audio cues (`loot_*`, `anvil`, `equip`, `error`, `click`) | `AudioManager.ts:298-312`, `UIScene.ts` | Same cue names in the UE sound table. |

---

## 19. Quirk list (keep unless resolved otherwise)
| # | Quirk | Recommendation |
|---|---|---|
| Q1 | No level/class requirement to equip | Open (O5); keep for Chapter 1 parity |
| Q2 | Weapon `baseDamage`/`attackSpeed` never reach combat; shields have no base defense | Open (O1) — biggest balance question |
| Q3 | Gear `magicFind`/`lck` (and altar MF) don't affect loot; `luck` mixes lck points with MF % | Open (O2); FIX recommended (add gear MF) |
| Q4 | Legendary/set display names are concatenated affix names | FIX |
| Q5 | Legendary `specialEffect` is text only; effect text baked in zh | FIX (apply EquipStats-keyed effects; store `legendaryId`) |
| Q6 | Shop buy / dialogue reward / hidden chest with a full bag loses the item | FIX (check space first; or overflow to stash like quests) |
| Q7 | Shop wares beyond 8 rows and the buyback list are not visible | FIX (UI) |
| Q8 | Antidote and TP scroll are consumed with no effect; ID scroll/ley fruit "Use" does nothing | FIX antidote (cleanse poison); TP scroll → open (O6) |
| Q9 | No potion hotkeys / quick slots | Open (O6) |
| Q10 | Identification is vestigial | Keep (O4) |
| Q11 | `MonsterDefinition.lootTable` never read | Keep data, ignore (or design it in later) |
| Q12 | Wandering merchant uses invalid ids; `priceMultiplier`, `potionDiscount` unused | FIX ids when that event is ported |
| Q13 | Equip swap refused when the bag is exactly full | FIX (swap in place) |
| Q14 | Set pieces counted by `setId` (duplicate ring counts twice) | Keep |
| Q15 | Set bonus i18n uses index instead of count | FIX |
| Q16 | World label shows stored zh name in an old palette | FIX (localised name, art-direction palette) |
| Q17 | `leg_void_edge` unreachable; dungeon-exclusive legendary/set bases drop in the overworld; `makeSetItem` dead | Keep for now; revisit with the labyrinth spec |
| Q18 | New ring always replaces ring1, compare shows the weaker ring | FIX (equip into `compareTarget` slot) |
| Q19 | Unsocket refused at 100 entries even if the gem would stack | FIX |
| Q20 | Treasure cache ignores difficulty, its drops never despawn; hidden chest logs success on full bag | Keep / FIX log |
| Q21 | `addItem` tops up only the first partial stack | Keep |
| Q22 | Clicking distant loot only walks there (no pickup on arrival) | FIX (pick up on arrival — essential for touch) |

---

## 20. Worked examples (C++ unit-test vectors)

1. **Affix pools.** Item level 1, base `a_leather_boots`: prefix pool = `pre_sturdy, pre_strong, pre_nimble, pre_wise,
   pre_hardy, pre_spiritual` (6, all tier 1, weight 3 each → uniform); suffix pool = `suf_life, suf_mana, suf_fire_res,
   suf_ice_res, suf_lightning_res, suf_speed, suf_luck` (7). A 2-affix magic boots = 1 prefix + 1 suffix.
2. **Tier bands.** L7 → band [1,2], allowed 1–3; L8 → [1,3], 1–4; L18 → [2,4], 1–5; L28 → [3,5], 2–5; L38 → [4,5], 3–5.
3. **Legendary scaling** (`round(v × clamp(L/35, 0.6, 1.5))`): Grief at L6 → damage 18, attackSpeed 12, lifeSteal 3;
   Shadowstep at L1 → moveSpeed 15, dex 9; Soulreaver at L52 (×1.4857) → damage 67, lifeSteal 12, critDamage 30;
   Soulreaver at L60 (×1.5) → damage 68 (67.5 rounds up).
4. **Craft gold unit:** L0 → 36, L1 → 36, L5 → 60, L40 → 270. At L5: reforge magic 60 g + 2 scrap + 1 dust; reforge rare
   120 g + 2 dust + 1 essence; upgrade normal 120 g + 3 scrap + 1 dust; magic→rare 300 g + 4 dust + 1 essence; socket
   180 g + 4 scrap + 1 essence.
5. **Salvage:** L1 normal → scrap 2; L12 magic → scrap 2, dust 1; L25 rare → scrap 3, dust 2, essence 1; L50 legendary →
   scrap 5, dust 4, essence 4.
6. **Upgrade counts:** normal (0 affixes) with `rng = 0` → target 1 → need 1; magic with 2 affixes, `rng = 0.99` →
   target 4 → need 2; magic with 2 affixes, `rng = 0` → target 3 → need 1.
7. **Prices:** `c_hp_potion_s` buy 15, sell 5, buyback 25; a stack of 3 sells for 15, buyback 75; `w_short_sword` buy 45,
   sell 15; `a_leather_belt` buy 6, sell 2; a legendary `w_broad_sword` sells for 40 like a normal one.
8. **Stacking:** bag holds `c_hp_potion_s ×19`; adding ×3 → existing 20, new entry ×2 appended, returns true. Bag
   at 100 entries with `c_hp_potion_s ×19`; adding ×3 → existing 20, returns false, the dropped item now has quantity 2.
9. **Aggregation:** equipped `a_leather_armor` {pre_sturdy defense 2, suf_life maxHp 10} + `w_short_sword` with
   `g_diamond_1` socketed → `{weaponDamageMin 5, weaponDamageMax 10, defense 10, maxHp 10, str 3, dex 3, int 3,
   vit 3, spi 3, lck 3}`; typed `EquipStats` drops the two weapon keys.
10. **Set bonus:** equipped hunter helm + armor + boots (fixed affixes only) → piece stats critRate 5, dex 18, defense 15,
    hpRegen 4, moveSpeed 20; 2pc attackSpeed 15, moveSpeed 20; 3pc magicFind 30, killHealPercent 3 →
    moveSpeed total 40; plus base defense 5 + 8 + 1 = 14 → defense 29.
11. **Compare:** candidate normal `w_short_sword` vs worn normal `w_rusty_sword` → `[{__avgDamage, +2.5}]`; a shield vs
    an empty offhand → `[]` deltas → shows "empty slot" line; rings worn with scores 5.1 (ring1) and 3.1 (ring2) → target
    ring2.
12. **Mini-boss floor:** shaman (L6, magic floor); loot rolled `[normal sword, potion]` → a magic item from the L6
    window is appended; loot `[rare ring]` → nothing appended.

---

## 21. Open questions
1. **O1 Weapon/shield base stats** (Q2): keep inert for parity, or feed weapon average damage / attack speed and shield
   defense into combat? Affects every damage number in `combat-feel.md`.
2. **O2 Magic find** (Q3): include gear `magicFind` (and gear `lck`) in the loot `luck` input? If yes, keep the
   `×0.5 / ×0.3` coefficients or convert MF to a D2-style multiplier?
3. **O3 Legendary effects** (Q5): apply `specialEffect` as an item stat for the 7 keys that combat already reads; design
   `deathDefiance`, `allStatsBonus`, `dungeonDamageBonus`, `voidStrike` (later milestones)?
4. **O4 Identification** (Q10): keep everything identified (web reality) or implement D2-style unidentified magic+
   drops with ID scrolls?
5. **O5 Requirements** (Q1): enforce `levelReq` and/or class weapon types on equip?
6. **O6 Consumables UX** (Q8/Q9): add potion quick slots (needed for touch), antidote cleanses poison, and decide the TP
   scroll (consume on portal, or remove since the portal is free).
7. **O7 Inventory model**: keep the 100-entry list (recommended; matches the touch UI) rather than a D2 2-D grid?
8. **O8 Hero visuals**: should equipped weapon/offhand (and armour tiers) change the 3D hero mesh?
9. **O9 Stash in Chapter 1**: add a stash keeper to the Chapter 1 camp, or leave the stash unreachable until Chapter 3?
10. **O10 Web save import**: should the UE build import web saves (then keep the JSON field names and uid strings)?
11. **O11 Sell price**: keep quality-independent sell prices (a legendary sells like a normal item) or scale by quality?
