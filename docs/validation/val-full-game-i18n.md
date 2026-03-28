# Validation Contract: Full Game i18n (Milestone 2)

All assertions assume the i18n foundation (Milestone 1) is in place with a working `t()` function, locale switching mechanism, and `zh-CN` / `zh-TW` / `en` locale files loaded. The game is running at `localhost:5173` in a browser.

---

## UIScene Assertions

### VAL-UI-001: Inventory Panel Title and Item Count
Open the inventory panel (press `I`). The panel title "背包" and item count format `背包 (N/100)` must display correctly in zh-CN. Switch locale to `en`: the title must read "Inventory" and count format "Inventory (N/100)". Switch to zh-TW: title must use Traditional Chinese equivalent. No raw Chinese characters may appear when locale is `en`.
Tool: agent-browser
Evidence: Screenshot of inventory panel header in all 3 locales showing title and item count text.

### VAL-UI-002: Equipment Slot Names
Open inventory panel. The 10 equipment slot labels (头盔, 铠甲, 手套, 鞋子, 武器, 副手, 项链, 戒指1, 戒指2, 腰带) must each display in the active locale. Switch to `en`: expect Helmet, Armor, Gloves, Boots, Weapon, Offhand, Necklace, Ring 1, Ring 2, Belt.
Tool: agent-browser
Evidence: Screenshot of equipment slots area in zh-CN and en locales.

### VAL-UI-003: Inventory Action Buttons (Sort, Destroy, Pagination)
Open inventory panel with items present. The `[整理]` (Sort), `[销毁]` (Destroy), and pagination controls (`< 上一页`, `下一页 >`, `第N/M页`) must all switch language on locale change. Verify the `装备加成` (Equipment Bonus) footer also switches (or shows "无" → "None").
Tool: agent-browser
Evidence: Screenshot of inventory bottom area in zh-CN and en showing action buttons and pagination.

### VAL-UI-004: Shop Panel — Titles, Buy/Sell Buttons, Buyback
Open a shop NPC. Verify the title displays "铁匠铺" or "商店" in zh-CN, and "Blacksmith" or "Shop" in en. The column headers "商品列表" and "你的背包" must switch. Buy `[买]`, sell, and buyback `[回购]` buttons must switch. Gold display `金币: NG` must switch. The hint text "右键快速卖出" must switch. Buyback confirmation shows translated item name.
Tool: agent-browser
Evidence: Screenshot of full shop panel in zh-CN and en, including buyback section and gold display.

### VAL-UI-005: Shop Sell Confirmation Dialog
Right-click an item in the shop to trigger the sell confirmation popup. The text `卖出 {name} ({price}G)?` and buttons `[确定]`/`[取消]` must all be translated. In `en`, expect "Sell {name} ({price}G)?" with "Confirm" / "Cancel".
Tool: agent-browser
Evidence: Screenshot of sell confirmation dialog in both locales.

### VAL-UI-006: Character Stats Panel — Title, Stat Names, Descriptions
Open character panel (press `C`). The title `角色属性 - {className}` and subtitle `Lv.N 剩余属性点: M` must switch locales. All 6 stat rows (力量 STR, 敏捷 DEX, 体质 VIT, 智力 INT, 精神 SPI, 幸运 LCK) with their descriptions (e.g., "物理伤害/负重") must be fully translated in en. The computed stats section (攻击, 防御, 暴击率, 暴击伤害, 金币) must also switch.
Tool: agent-browser
Evidence: Screenshot of full character panel in zh-CN and en showing stat labels, descriptions, and computed stats.

### VAL-UI-007: Skill Tree Panel — Title, Class Name, Skill Points
Open skill tree (press `K`). The title "技 能 树", class+skill points line `{className} · 剩余技能点: N`, and footer hint `按 K 关闭 · 悬停查看详情 · 滚轮翻页` must translate. Branch names (战斗大师/守护者/狂战士, 烈焰/冰霜/奥术, 刺杀/箭术/陷阱) must translate. The "协同" (Synergy) label on skill cards must translate.
Tool: agent-browser
Evidence: Screenshot of skill tree panel in zh-CN and en showing title, branch names, and footer text.

### VAL-UI-008: Skill Tooltip — Stats and Labels
Hover over a skill in the skill tree. The tooltip must show translated labels: 伤害→Damage, 消耗→Cost, 冷却→Cooldown, 范围→Range, AOE半径→AOE Radius, 暴击加成→Crit Bonus, 眩晕→Stun, 增益→Buff. The "协同增益" and "下一级" section headers must translate. Damage type names (物理/火焰/冰霜/闪电/毒素/奥术) must translate.
Tool: agent-browser
Evidence: Screenshot of skill tooltip in both locales showing stat labels and damage type.

### VAL-UI-009: Quest Log Panel — Quest Cards and Objectives
Open a quest card from NPC interaction. Verify: category badge `【主线】`/`【支线】` translates to "[Main]"/"[Side]" in en. Section labels `目标:` → "Objectives:", `奖励:` → "Rewards:". Quest name and description text are translated. Objective progress checkmarks (✓/○) with translated objective text display correctly.
Tool: agent-browser
Evidence: Screenshot of quest card in zh-CN and en showing category badge, objectives section, and rewards.

### VAL-UI-010: Quest Tracker HUD
With active quests, verify the quest tracker overlay. The header "任务追踪" must translate to "Quest Tracker" in en. Quest names and objective progress text in the tracker must be translated. The scroll indicator text pattern `▼ 还有 N 个任务` must translate.
Tool: agent-browser
Evidence: Screenshot of quest tracker HUD in both locales with at least 1 active quest.

### VAL-UI-011: HUD Elements — Auto-combat and Auto-loot Labels
Verify the auto-combat toggle message `自动战斗: 开启/关闭` translates in the combat log. The auto-loot button label `拾取\nOFF` / `拾取\nON` translates. The inventory shortcut label `背包\n(I)` translates.
Tool: agent-browser
Evidence: Screenshot of HUD buttons area and combat log after toggling auto-combat in both locales.

### VAL-UI-012: Combat Log Header
Verify the combat log header text `战斗日志` translates to "Combat Log" in en.
Tool: agent-browser
Evidence: Screenshot of combat log panel header in both locales.

### VAL-UI-013: World Map Panel Title and Close Hint
Open the world map (press `M`). The title "渊火" and the close hint "按 M 关闭" must translate. Zone names on the map must be translated.
Tool: agent-browser
Evidence: Screenshot of world map panel in both locales showing title and close hint.

### VAL-UI-014: NPC Dialogue Panel — Name, Text, Actions, Close Hint
Interact with an NPC. The dialogue panel must show translated NPC name, translated dialogue text, translated action button labels (e.g., `接受: {questName}`), and the close hint "点击外部关闭" → "Click outside to close" in en.
Tool: agent-browser
Evidence: Screenshot of NPC dialogue panel in both locales.

### VAL-UI-015: Homestead Panel — Title, Section Headers, Building Names
Open homestead (press `H`). The title "家 园" must translate. Section headers "── 建筑 ──" and "── 宠物 (N 只) ──" must translate. All 6 building names (药草园, 训练场, 宝石工坊, 宠物小屋, 仓库, 祭坛) and their descriptions must translate. The "已满级" badge and "升级 NG" button text must translate. The empty pet placeholder text "暂无宠物\n击杀Boss·完成任务·稀有刷新可获得" must translate.
Tool: agent-browser
Evidence: Screenshot of homestead panel in both locales showing buildings section and pets section.

### VAL-UI-016: Pet Stat Labels in Homestead
If pets are present in homestead, the bonus stat labels (经验, 攻击, 掉宝, 暴击, 回血, 攻速, 防御, 回蓝) must translate (EXP, ATK, MF, Crit, HP Regen, ASPD, DEF, MP Regen or equivalent).
Tool: agent-browser
Evidence: Screenshot of pet card showing stat label in both locales.

### VAL-UI-017: Item Tooltip — Full Translation with Affixes
Hover over a magic/rare item in inventory. The tooltip must show: translated item base name, translated quality prefix ([魔法]/[稀有]/[传奇]/[套装] → [Magic]/[Rare]/[Legendary]/[Set]), translated affix names (e.g., "锋利的" → "Sharp"), translated stat display names from STAT_DISPLAY map, and translated description text. Socket display and gem names must also translate.
Tool: agent-browser
Evidence: Screenshot of item tooltip for a rare item with affixes in both locales.

### VAL-UI-018: Item Tooltip — Set Bonus Display
Hover over a set item. Verify the set name (e.g., "铁壁守护者" → "Iron Guardian"), set bonus descriptions, and piece count progress all translate correctly.
Tool: agent-browser
Evidence: Screenshot of set item tooltip showing set bonus section in both locales.

### VAL-UI-019: Context Menu — Item Actions
Right-click an item in inventory to show context menu. The action labels for equip/use/sell/identify/socket must all be translated.
Tool: agent-browser
Evidence: Screenshot of item context menu in both locales.

### VAL-UI-020: Compass Direction Labels
With the minimap/compass visible, verify compass direction text (东/南/西/北/东南/东北/西南/西北) translates to (E/S/W/N/SE/NE/SW/NW) in en.
Tool: agent-browser
Evidence: Screenshot or text extraction of compass direction labels in both locales.

### VAL-UI-021: Mercenary Panel — Names, Descriptions, Action Buttons
Open the mercenary panel. All 5 mercenary names (守护骑士, 狂战士, 游侠射手, 圣光牧师, 元素法师) and descriptions must translate. Hire/Dismiss/Revive buttons with cost text must translate. Messages like "金币不足! 需要 NG" must translate.
Tool: agent-browser
Evidence: Screenshot of mercenary panel in both locales showing names, descriptions, and action buttons.

### VAL-UI-022: Achievement Panel — Names and Descriptions
Open achievements panel. All achievement names (初出茅庐, 百人斩, 屠戮者, etc.) and descriptions must translate. Title rewards (e.g., "新手冒险者") must translate.
Tool: agent-browser
Evidence: Screenshot of achievement panel in both locales showing at least 3 achievements.

### VAL-UI-023: Audio Settings Labels
If audio settings UI exists, verify labels for BGM, SFX volume, and music track names (e.g., "渊火 · 序章", "翠绿平原 · 探索") translate.
Tool: agent-browser
Evidence: Screenshot of audio settings/jukebox in both locales.

### VAL-UI-024: Gem Socketing UI Labels
Open the gem socketing interface on an item with sockets. Verify labels "没有空余插槽" → "No empty sockets", gem stat descriptions, and socket/unsocket action text all translate.
Tool: agent-browser
Evidence: Screenshot of gem socketing UI in both locales.

### VAL-UI-025: Lore Collectible Display
Open the lore panel (if accessible). Verify lore entry names (e.g., "古老石碑", "褪色日记") and their full narrative text translate to en. The discovery notification `发现传说: {name}` must translate.
Tool: agent-browser
Evidence: Screenshot of a lore entry in both locales.

---

## ZoneScene Assertions

### VAL-ZONE-001: Zone Entry Announcement
Enter a zone. The zone entry message `进入 {zoneName} (Lv.{min}-{max})` must display with a translated zone name and label format. In en: "Entering {zoneName} (Lv.{min}-{max})".
Tool: agent-browser
Evidence: Screenshot of zone entry floating text in both locales.

### VAL-ZONE-002: Death and Respawn Text
Trigger player death. The floating text "你已死亡" must translate to "You Died". The combat log message "你已死亡，返回营地复活..." must translate to en equivalent.
Tool: agent-browser
Evidence: Screenshot of death screen text and combat log death message in both locales.

### VAL-ZONE-003: Combat Log — Damage and Healing Messages
Engage in combat. Verify log messages with template interpolation: `恢复 {amount} 生命` → "Restored {amount} HP", `恢复 {amount} 法力` → "Restored {amount} MP". Entity names in messages like `{skillName} 激活!` must use translated skill names.
Tool: agent-browser
Evidence: Screenshot of combat log with healing and skill activation messages in both locales.

### VAL-ZONE-004: Combat Log — Skill Effect Messages
Use skills in combat. Verify specific messages translate: "法力不足!" → "Not enough mana!", "免费施法！法力未消耗" → "Free cast! No mana consumed", "闪避反击就绪！下次攻击必定暴击" → equivalent en text. Skill names within messages must be translated.
Tool: agent-browser
Evidence: Screenshot of combat log during skill usage in both locales.

### VAL-ZONE-005: Teleport Messages
Use town portal or teleport skill. Verify messages: "正在开启传送门..." → "Opening portal...", "已传送回营地。" → "Teleported to camp.", "你已在营地中，无法使用传送门。" → en equivalent. Dungeon-specific: "已传送至副本入口。" / "已传送至楼层出口。" must translate.
Tool: agent-browser
Evidence: Screenshot of combat log showing teleport messages in both locales.

### VAL-ZONE-006: Level Up Floating Text
Level up the player. The floating text "升级!" must translate to "Level Up!" and `等级 {level}` to "Level {level}" in en.
Tool: agent-browser
Evidence: Screenshot of level-up floating text in both locales.

### VAL-ZONE-007: Quest Completion Floating Text
Complete a quest. The floating text "任务完成!" must translate to "Quest Complete!" in en.
Tool: agent-browser
Evidence: Screenshot of quest completion floating text in both locales.

### VAL-ZONE-008: Monster Kill Log Message
Kill a monster. The log message `击杀 {monsterName}! +{exp}EXP +{gold}G` must use translated monster name and label format. In en: "Killed {monsterName}! +{exp}EXP +{gold}G".
Tool: agent-browser
Evidence: Screenshot of combat log kill message in both locales with different monster names.

### VAL-ZONE-009: Random Event — Treasure Chest
Trigger a treasure chest event. The label "宝箱" on the sprite and log message `从宝箱中获得 {gold} 金币` must translate. The hidden reward type labels (宝箱/金币堆/卷轴) must translate.
Tool: agent-browser
Evidence: Screenshot of treasure chest interaction and log message in both locales.

### VAL-ZONE-010: Random Event — Wandering Merchant
Trigger a wandering merchant event. The NPC label "流浪商人" and announcement "流浪商人出现了! 看看他的商品吧。" must translate.
Tool: agent-browser
Evidence: Screenshot of wandering merchant label and log message in both locales.

### VAL-ZONE-011: Random Event — Rescue and Puzzle
Trigger rescue and puzzle events. Verify: rescue NPC names (迷路的旅人, 受伤的猎人, 被困的矿工, etc.) translate. Puzzle label "谜题装置" and prompts translate. The "离开" button on puzzle UI translates. Reward messages with interpolated gold/exp values translate.
Tool: agent-browser
Evidence: Screenshot of puzzle UI and rescue log message in both locales.

### VAL-ZONE-012: Random Event — Ambush
Trigger an ambush event. The alert message "伏兵出现!" must translate to en.
Tool: agent-browser
Evidence: Screenshot of ambush event message in both locales.

### VAL-ZONE-013: Status Effect Names in Log
Apply status effects in combat. Verify that status effect names (灼烧, 冰冻, 中毒, 流血, 减速, 眩晕) in log messages translate. Messages like `{effectName}效果已施加` and `{effectName}效果已消失` must fully translate with interpolated translated effect names.
Tool: agent-browser
Evidence: Screenshot of combat log showing status effect apply/expire messages in both locales.

### VAL-ZONE-014: Elite Monster Affix Names
Encounter an elite monster. The affix display names (炎魔, 迅捷, 瞬移, 狂暴, 诅咒, 吸血, 冰封) in the monster name format `[affix1·affix2] 原名` must translate. The curse aura message "诅咒光环：属性降低！" must translate.
Tool: agent-browser
Evidence: Screenshot of elite monster with affix names in both locales.

### VAL-ZONE-015: Pet Discovery — Void Butterfly
Discover the void butterfly pet. The floating label "✦ 虚空蝶" and discovery message `发现了稀有宠物: 虚空蝶! 已收入宠物小屋。` must translate.
Tool: agent-browser
Evidence: Screenshot of pet discovery event in both locales.

### VAL-ZONE-016: Dungeon System Messages
Enter and navigate the dungeon. Verify: `进入深渊迷宫... (共{N}层)` → "Entering the Abyss Maze... ({N} floors)", `进入第{N}层...` → "Entering floor {N}...", `离开深渊迷宫，返回深渊裂谷...` → en equivalent. Sub-dungeon messages `进入{name}...` and `离开副本，返回主地图...` must translate. The blocked entrance message "此入口暂时无法进入" must translate.
Tool: agent-browser
Evidence: Screenshot of dungeon entry/exit messages in combat log in both locales.

### VAL-ZONE-017: Hidden Area Discovery
Discover a hidden area. The floating text `发现隐藏区域: {areaName}` must translate with translated area name (e.g., "封印密室" → "Sealed Chamber"). Zone-specific area names (精灵族秘密宝库, 月光祭坛, 矮人秘密金库, 古代绿洲遗址) must all translate.
Tool: agent-browser
Evidence: Screenshot of hidden area discovery message in both locales.

### VAL-ZONE-018: Escort and Defend Quest Messages
During escort quest: `{npcName} 出现了！护送目标已标记。` must translate. Defend quest: `{targetName} 需要保护！准备迎接敌袭。` and wave text `第 {N}/{M} 波敌人来袭!` must translate. Success/failure messages (所有浪潮已击退！防守成功！ / 护送目标已死亡! 任务失败。 / 防守目标被摧毁! 任务失败。) must translate.
Tool: agent-browser
Evidence: Screenshot of escort/defend quest log messages in both locales.

### VAL-ZONE-019: Craft and Deliver Quest Completion Messages
Complete craft/deliver quest objectives. Messages `制作完成: {targetName}` and `交付完成: {targetName}` must translate with translated target names.
Tool: agent-browser
Evidence: Screenshot of craft/deliver completion messages in combat log in both locales.

### VAL-ZONE-020: Save Position Reset Message
Load a save where the saved position is unreachable. The message "存档位置不可达，已重置至营地" must translate.
Tool: agent-browser
Evidence: Screenshot of combat log showing position reset message in both locales.

---

## Data File Assertions

### VAL-DATA-001: Item Base Names — Weapons
Verify that all weapon base items display translated names. Sample: "生锈的剑" → "Rusty Sword", "短弓" → "Short Bow", "奥术法杖" → "Arcane Staff", "深渊之刃" → "Abyssal Dagger". Hover over weapons in inventory to confirm tooltip shows the translated name matching `nameEn` field or equivalent en locale key.
Tool: agent-browser
Evidence: Screenshot of weapon item tooltips in en locale for at least 4 different weapon types.

### VAL-DATA-002: Item Base Names — Armor
Verify armor item translations. Sample: "布帽" → "Cloth Cap", "锁子甲" → "Chain Mail", "板甲" → "Plate Armor", "皮手套" → "Leather Gloves". Confirm descriptions also translate (e.g., "简单的布帽" → en description).
Tool: agent-browser
Evidence: Screenshot of armor item tooltips in en locale for at least 4 different armor types.

### VAL-DATA-003: Item Affix Names — Prefixes and Suffixes
Verify that generated item affixes display translated names. Prefixes: "锋利的" → "Sharp", "致命的" → "Deadly", "深渊的" → "Abyssal". Suffixes: "生命" → "of Life", "吸血" → "of Leech", "不朽" → "of Immortality". The compound item names must assemble correctly in en (e.g., "Sharp Short Sword of Life").
Tool: agent-browser
Evidence: Screenshot of magic/rare item names showing translated affixes in en locale.

### VAL-DATA-004: Item Affix Stat Display Names
Verify that the STAT_DISPLAY map values translate in tooltips. All stat names that appear as affix effects (damage, defense, maxHp, maxMana, critRate, lifeSteal, etc.) must show translated labels in en.
Tool: agent-browser
Evidence: Screenshot of item tooltip stat lines in en locale.

### VAL-DATA-005: Legendary and Set Item Names
Verify legendary items: dungeon legendaries "深渊之冠" → "Crown of the Abyss", "虚空之刃" → "Void's Edge" (or equivalent en). Set names: "铁壁守护者" → "Iron Guardian", "暗影刺客" → "Shadow Assassin". Set bonus descriptions must also translate. Affix lore names (虚空之力, 深渊护佑, etc.) must translate.
Tool: agent-browser
Evidence: Screenshot of legendary/set item tooltip in en locale showing name, affixes, and set bonuses.

### VAL-DATA-006: Consumable Item Names
Verify potion and consumable names translate: HP/MP potions, identify scrolls, and gem items must all show translated names in en.
Tool: agent-browser
Evidence: Screenshot of consumable items in inventory/shop in en locale.

### VAL-DATA-007: Class Names and Descriptions
On the class selection screen (MenuScene) and in character panel, verify class names translate: "战士" → "Warrior", "法师" → "Mage", "盗贼" → "Rogue". Class descriptions also translate.
Tool: agent-browser
Evidence: Screenshot of class selection screen in en locale showing all 3 class names and descriptions.

### VAL-DATA-008: Skill Names and Descriptions
Open skill tree for each class. Verify at least 3 skills per class have translated names and descriptions. Warrior sample: "战吼" → en name, "怒火" → en name. Mage: "火球术" → "Fireball", "陨石" → "Meteor". Rogue: "背刺" → "Backstab", "毒刃" → "Poison Blade", "消失" → "Vanish".
Tool: agent-browser
Evidence: Screenshot of skill tree tooltips in en locale for each class.

### VAL-DATA-009: Quest Names and Descriptions
Open quest log with active quests. Verify quest names translate: "史莱姆之灾" → en name, "哥布林猎杀" → en name, "亡灵净化" → en name. Quest descriptions and objective target names (e.g., "绿色史莱姆" → "Green Slime", "骷髅战士" → "Skeleton") must translate.
Tool: agent-browser
Evidence: Screenshot of quest log/tracker in en locale showing quest names and objective text.

### VAL-DATA-010: NPC Names and Dialogue Lines
Interact with NPCs in en locale. Verify names translate: "铁匠" → "Blacksmith", "商人" → "Merchant", "村长" → "Village Elder", "侦察兵" → "Scout". Their dialogue lines must be fully translated (no zh-CN characters in en mode).
Tool: agent-browser
Evidence: Screenshot of NPC dialogue panel in en locale for at least 2 different NPCs.

### VAL-DATA-011: Dialogue Trees — Full Narrative Translation
Open a dialogue tree (e.g., Village Elder's tree in Emerald Plains). Navigate at least 3 nodes deep. All narrative text must be translated: initial greeting, branching choices, quest trigger labels. Verify no Chinese characters appear in en locale at any node. Choice text like "告诉我发生了什么" must fully translate.
Tool: agent-browser
Evidence: Screenshot of dialogue tree at 3 different nodes in en locale showing translated narrative and choices.

### VAL-DATA-012: Dialogue Trees — Quest Trigger Labels
In a dialogue tree, verify quest trigger choice labels translate: `接受: {questName}` must show "Accept: {translated quest name}" in en. Reward notification dialogue "感谢你完成了任务！" must translate.
Tool: agent-browser
Evidence: Screenshot of quest acceptance dialogue choice in en locale.

### VAL-DATA-013: Monster Names
Encounter monsters in different zones. Verify translated names: Zone 1: "绿色史莱姆" → "Green Slime", "哥布林" → "Goblin". Zone 2: "骷髅兵" → "Skeleton", "狼人" → "Werewolf". Zone 3: "石像鬼" → "Gargoyle". Zone 4: "火元素" → "Fire Elemental". Zone 5: "小恶魔" → "Imp". Monster names in combat log and floating damage text must use translated names.
Tool: agent-browser
Evidence: Screenshot of monster name labels and combat log in en locale across at least 2 zones.

### VAL-DATA-014: Zone and Map Names
Verify all 5 zone names translate: "翡翠平原" → "Emerald Plains", "暮色森林" → "Twilight Forest", "铁砧山脉" → "Anvil Mountains", "灼热荒漠" → "Scorching Desert", "深渊裂谷" → "Abyss Rift". These appear in zone entry messages, world map, and minimap labels.
Tool: agent-browser
Evidence: Screenshot of world map in en locale showing all zone names.

### VAL-DATA-015: Mini-Boss Names and Dialogue
Encounter a mini-boss. Verify names translate: "哥布林萨满" → en name, "暗影织者" → en name, "铁甲守卫" → en name, "沙漠亡灵" → en name, "虚空先驱" → en name. Pre-battle dialogue lines (narrative text) must be fully translated in en mode.
Tool: agent-browser
Evidence: Screenshot of mini-boss encounter dialogue in en locale.

### VAL-DATA-016: Lore Collectible Names and Full Text
Collect lore items in different zones. Verify names translate: "古老石碑" → en name, "树灵遗刻" → en name, "矮人铭文" → en name. The full lore text (multi-sentence narrative) must be translated without truncation. Discovery message in combat log must use translated name.
Tool: agent-browser
Evidence: Screenshot of lore collectible text display in en locale for at least 2 entries from different zones.

### VAL-DATA-017: Sub-Dungeon Names and Boss Names
Enter a sub-dungeon. Verify names translate: "废弃矮人矿道" → en name, "恶魔祭坛洞窟" → en name. Sub-dungeon mini-boss names ("矿道铁卫", "祭坛守卫者") must translate.
Tool: agent-browser
Evidence: Screenshot of sub-dungeon entry message and boss encounter in en locale.

### VAL-DATA-018: Dungeon Exclusive Items and Boss Names
In the dungeon system, verify boss name "深渊之主·卡萨诺尔" translates. Dungeon monster names ("深渊暗影", "深渊恶鬼", "深渊守卫") translate. Dungeon-exclusive set name "深渊行者" and its bonus descriptions translate.
Tool: agent-browser
Evidence: Screenshot of dungeon boss encounter or dungeon item tooltip in en locale.

### VAL-DATA-019: Story Decorations — Names and Descriptions
Examine story decorations in zones (e.g., "倒塌的精灵塔", "被腐蚀的神龛", "半埋的宫殿残垣"). Both the name and full description text (multi-sentence lore) must translate to en.
Tool: agent-browser
Evidence: Screenshot of story decoration interaction showing translated name and description in en locale.

### VAL-DATA-020: Hidden Area Discovery Text
Discover a hidden area (e.g., "精灵族秘密宝库"). The `discoveryText` field (long narrative paragraph like "你在草丛中发现了...") must be fully translated in en.
Tool: agent-browser
Evidence: Screenshot of hidden area discovery text in en locale.

---

## System Message Assertions

### VAL-SYS-001: InventorySystem — Bag Full and Equip Messages
Fill the inventory and attempt to add an item. Verify "背包已满!" → "Bag is full!" in en. Equip an item: "装备了 {name}" → "Equipped {name}". Swap with full bag: "背包已满，无法换装!" → en equivalent. Discard: "丢弃了 {name}" → "Discarded {name}". Bulk destroy: "销毁了 N 件普通装备" → en equivalent.
Tool: agent-browser
Evidence: Screenshot of combat log showing inventory system messages in both locales.

### VAL-SYS-002: InventorySystem — Identify and Gem Messages
Use an identify scroll: "需要鉴定卷轴!" → en equivalent for missing scroll, "鉴定了 {name}" → "Identified {name}". Gem socket: "镶嵌了 {gemName}" → "Socketed {gemName}", "没有空余插槽" → "No empty sockets", "取出了 {gemName}" → "Removed {gemName}". Bag full during gem removal: "背包已满，无法取出宝石!" → en equivalent.
Tool: agent-browser
Evidence: Screenshot of combat log showing identify and gem system messages in both locales.

### VAL-SYS-003: InventorySystem — Quality Prefix Labels
Verify loot pickup messages show translated quality: "[魔法]" → "[Magic]", "[稀有]" → "[Rare]", "[传奇]" → "[Legendary]", "[套装]" → "[Set]" in en locale.
Tool: agent-browser
Evidence: Screenshot of loot pickup messages in combat log in en locale showing quality prefixes.

### VAL-SYS-004: InventorySystem — Stash Messages
Interact with stash NPC. Verify "仓库已满!" → "Stash is full!" in en. Stash NPC dialogue lines must translate.
Tool: agent-browser
Evidence: Screenshot of stash interaction messages in both locales.

### VAL-SYS-005: QuestSystem — Type Labels
Verify quest type labels translate: 猎杀→Kill, 收集→Collect, 探索→Explore, 对话→Talk, 护送→Escort, 防守→Defend, 调查→Investigate, 制作交付→Craft & Deliver. These appear in quest card type badges and quest log filters.
Tool: agent-browser
Evidence: Screenshot of quest card showing type badge in both locales for at least 3 different quest types.

### VAL-SYS-006: QuestSystem — Phase Labels
Verify craft-deliver quest phase labels translate: "采集材料" → "Gather Materials", "制作物品" → "Craft Item", "交付成品" → "Deliver Product" (or en equivalents). Quest log messages: `接受任务: {name}`, `任务完成: {name}! 返回NPC交付。`, `任务失败: {name}`, `交付任务: {name}` all translate with interpolated translated quest names.
Tool: agent-browser
Evidence: Screenshot of craft-deliver quest in log and tracker showing phase labels in both locales.

### VAL-SYS-007: MercenarySystem — Hire, Dismiss, Revive Messages
Hire a mercenary. Verify: "雇佣了 {name}!" → "Hired {name}!". Dismiss: "{name} 已被解雇" → "{name} was dismissed". Revive: "{name} 已复活!" → "{name} has been revived!". Error messages: "金币不足! 需要 {cost}G" → en equivalent, "没有佣兵可复活" → en, "佣兵还活着" → en, "未知的佣兵类型" → en.
Tool: agent-browser
Evidence: Screenshot of mercenary hire/dismiss/revive messages in combat log in both locales.

### VAL-SYS-008: MercenarySystem — Level Up and Death Messages
Level up a mercenary through combat. Verify: `{name} 升级到 Lv.{level}!` → en equivalent. Mercenary death: `{name} 阵亡了!` → en. Healing message: `{name} 治疗了你! +{amount} HP` → en equivalent.
Tool: agent-browser
Evidence: Screenshot of mercenary level-up and death messages in combat log in both locales.

### VAL-SYS-009: HomesteadSystem — Building and Pet Messages
Upgrade a building and feed a pet. Verify: `{name} 升级到 Lv.{level}!` messages for both buildings and pets translate. Pet evolution: `{name} 进化为 {evolvedName}! 属性加成提升!` translates. Pet discovery: `获得宠物: {name}!` translates. Duplicate pet: "你已经拥有这只宠物了!" translates.
Tool: agent-browser
Evidence: Screenshot of homestead system messages in combat log in both locales.

### VAL-SYS-010: AchievementSystem — Unlock Notification
Unlock an achievement. Verify the notification `成就解锁: {name}! (称号: {title})` translates to en with translated achievement name and title.
Tool: agent-browser
Evidence: Screenshot of achievement unlock notification in both locales.

### VAL-SYS-011: StatusEffectSystem — Apply and Expire Messages
Apply a status effect. Verify: `{effectName}效果已施加` and `{effectName}效果已消失` translate with translated effect names. Refresh message `中毒效果已刷新` → en equivalent. All 6 effect names (灼烧/冰冻/中毒/流血/减速/眩晕) must translate.
Tool: agent-browser
Evidence: Screenshot of status effect messages in combat log in both locales.

### VAL-SYS-012: DifficultySystem — Names and Unlock Messages
Unlock a new difficulty. Verify difficulty names translate: "普通" → "Normal", "噩梦" → "Nightmare", "地狱" → "Hell". Unlock messages: "噩梦难度已解锁" → en, "地狱难度已解锁" → en.
Tool: agent-browser
Evidence: Screenshot of difficulty unlock message and difficulty selection in both locales.

### VAL-SYS-013: EliteAffixSystem — Affix Names Display
Encounter an elite monster with affixes. Verify the composite name format translates: Chinese format `[炎魔·迅捷] {monsterName}` must become an equivalent en format. All 7 affix names must have en translations.
Tool: agent-browser
Evidence: Screenshot of elite monster name plate in both locales.

### VAL-SYS-014: RandomEventSystem — Event Names and Messages
Trigger random events. Verify all 5 event type names translate: 伏击→Ambush, 宝箱→Treasure, 流浪商人→Wandering Merchant, 救援→Rescue, 谜题→Puzzle. Event messages must translate. Zone-specific puzzle prompts, solutions, and reward text must translate.
Tool: agent-browser
Evidence: Screenshot of random event trigger messages in combat log in both locales.

### VAL-SYS-015: DungeonSystem — Floor and Boss Messages
Navigate dungeon floors. Verify floor transition messages translate. The dungeon BOSS encounter announcement and mid-boss names translate. Dungeon-exclusive set and legendary item names in loot drops translate.
Tool: agent-browser
Evidence: Screenshot of dungeon floor transition and boss encounter messages in both locales.

---

## zh-TW Specific Assertions

### VAL-TW-001: zh-TW Basic Character Conversion
Switch locale to zh-TW. Verify that Simplified Chinese characters are converted to Traditional: "背包" → "背包" (same), "铁匠" → "鐵匠", "装备" → "裝備", "技能" → "技能", "任务" → "任務", "成就" → "成就", "商店" → "商店". Check at least 10 strings across different panels for correct Traditional conversion.
Tool: agent-browser
Evidence: Screenshot of inventory, shop, and character panels in zh-TW locale.

### VAL-TW-002: zh-TW Item Names
Verify item names in zh-TW use Traditional characters. Sample: "生锈的剑" → "生鏽的劍", "锁子甲" → "鎖子甲". Affix names: "锋利的" → "鋒利的", "致命的" → "致命的". Check that no Simplified-only characters appear in zh-TW mode.
Tool: agent-browser
Evidence: Screenshot of item tooltips in zh-TW locale for at least 3 items with affixes.

### VAL-TW-003: zh-TW Skill and Class Names
Open skill tree in zh-TW. Verify class names: "战士" → "戰士", "法师" → "法師", "盗贼" → "盜賊". Skill names: "火球术" → "火球術", "背刺" → "背刺". Branch names: "烈焰" → "烈焰", "刺杀" → "刺殺".
Tool: agent-browser
Evidence: Screenshot of skill tree in zh-TW locale showing class name and skill names.

### VAL-TW-004: zh-TW Quest Text
Open a quest card in zh-TW. Verify quest names and descriptions use Traditional characters. Category badges: "【主线】" → "【主線】", "【支线】" → "【支線】". Quest type labels must use Traditional.
Tool: agent-browser
Evidence: Screenshot of quest card in zh-TW locale showing category badge, name, and description.

### VAL-TW-005: zh-TW Dialogue Tree Narrative
Open a dialogue tree in zh-TW. Navigate through at least 2 nodes. All narrative text must use Traditional Chinese characters. Verify no Simplified characters appear in choice text or NPC dialogue.
Tool: agent-browser
Evidence: Screenshot of dialogue tree at 2 different nodes in zh-TW showing Traditional character text.

### VAL-TW-006: zh-TW Zone and Monster Names
Enter a zone in zh-TW. Verify zone name uses Traditional: "翡翠平原" → "翡翠平原" (same), "暮色森林" → "暮色森林", "铁砧山脉" → "鐵砧山脈", "灼热荒漠" → "灼熱荒漠", "深渊裂谷" → "深淵裂谷". Monster names: "哥布林" → "哥布林", "骷髅兵" → "骷髏兵", "火元素" → "火元素".
Tool: agent-browser
Evidence: Screenshot of zone entry text and monster names in zh-TW locale.

### VAL-TW-007: zh-TW Lore and Story Decoration Text
View a lore collectible in zh-TW. Verify long narrative text uses Traditional characters throughout. Check at least one story decoration description for Traditional conversion.
Tool: agent-browser
Evidence: Screenshot of lore entry text in zh-TW locale showing Traditional characters.

### VAL-TW-008: zh-TW System Messages with Interpolation
In zh-TW locale, verify template strings with interpolation format correctly: `背包 (5/100)` uses Traditional equivalent, `金幣: 500G` (金币→金幣), `已傳送回營地。` (已传送回营地→已傳送回營地). No mixed Simplified/Traditional in any single string.
Tool: agent-browser
Evidence: Screenshot of HUD and combat log in zh-TW locale showing interpolated messages.

### VAL-TW-009: zh-TW Homestead and Pet Names
Open homestead in zh-TW. Verify building names: "药草园" → "藥草園", "训练场" → "訓練場", "宝石工坊" → "寶石工坊". Pet names: "小精灵" → "小精靈", "小火龙" → "小火龍". Descriptions must all use Traditional characters.
Tool: agent-browser
Evidence: Screenshot of homestead panel in zh-TW locale showing building and pet names.

### VAL-TW-010: zh-TW Complete Locale File Coverage
Programmatically verify that the zh-TW locale file has the same number of keys as the zh-CN locale file. No key present in zh-CN should be missing from zh-TW. Run a script or manual comparison to confirm 100% key parity.
Tool: agent-browser
Evidence: Console output or script result showing key count match between zh-CN and zh-TW locale files.
