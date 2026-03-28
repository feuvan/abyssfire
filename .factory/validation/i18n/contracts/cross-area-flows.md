### VAL-CROSS-001: Full flow language consistency — MenuScene to gameplay
Set language to `en` in the MenuScene language selector. Start a new game (select class, enter ZoneScene). Verify that ZoneScene map labels, NPC names, monster names, and the parallel UIScene HUD (HP/MP labels, gold display, skill bar tooltips, combat log messages) all render in English. No zh-CN strings should appear in any visible text element.
Tool: agent-browser
Evidence: Screenshot of MenuScene with English selected; screenshot of ZoneScene + UIScene overlay after entering gameplay; DOM/canvas text snapshot confirming English strings in HUD, combat log, and entity labels.

### VAL-CROSS-002: Full flow language consistency — zh-TW variant
Set language to `zh-TW` in the MenuScene language selector. Start a new game. Verify that ZoneScene and UIScene render Traditional Chinese characters (e.g., 「背包」not「背包」but checking for Traditional variants like「裝備」vs「装备」,「戰士」vs「战士」,「技能」vs「技能」). No Simplified Chinese (zh-CN) exclusive characters should appear where Traditional Chinese differs.
Tool: agent-browser
Evidence: Screenshot of gameplay in zh-TW; text snapshot listing all visible UI strings; diff against zh-CN showing Traditional character substitutions (e.g., 戰/战, 裝/装, 點/点, 聯/联).

### VAL-CROSS-003: Language persistence across scene transitions
Set language to `en` in MenuScene. Click "New Game", select a class (entering ZoneScene + UIScene). Press ESC or navigate back to MenuScene. Verify MenuScene still displays in English (button labels, title subtitle, version text). Re-enter gameplay — verify ZoneScene + UIScene remain in English.
Tool: agent-browser
Evidence: Screenshot of MenuScene after returning from gameplay showing English UI; screenshot of re-entered gameplay showing English UI; console log confirming no language-reset events fired during scene transitions.

### VAL-CROSS-004: Language persistence across page reloads
Set language to `en` via the language selector. Reload the browser page (F5 / Ctrl+R). Verify that BootScene loading text (if any loading indicators exist) renders in English. Verify MenuScene renders in English after boot completes — title subtitle, menu buttons ("Continue Game", "New Journey", "Game Controls"), and version text should all be in English.
Tool: agent-browser
Evidence: Screenshot of BootScene during load showing English text (or absence of zh-CN); screenshot of MenuScene post-reload showing English labels; `localStorage` dump confirming the `abyssfire_lang` (or equivalent) key persists with value `en`.

### VAL-CROSS-005: zh-TW end-to-end — boot through gameplay
Select `zh-TW` as the language. Reload the page to trigger a full BootScene → MenuScene flow. Verify BootScene loading indicators use Traditional Chinese. On MenuScene, verify all button labels use Traditional Chinese (e.g., 「新的旅程」→「新的旅程」or its zh-TW equivalent, 「遊戲控制」vs「游戏控制」). Start a new game and verify gameplay UI (inventory panel title, skill panel, character panel, quest log, gold display) all use Traditional Chinese.
Tool: agent-browser
Evidence: Sequential screenshots of BootScene, MenuScene, and gameplay (inventory panel open, character panel open); text comparison table showing zh-TW strings vs zh-CN for at least 10 key UI elements.

### VAL-CROSS-006: In-game language switch — immediate UI update
During active gameplay (ZoneScene + UIScene running), open a settings or language panel (if accessible via keyboard shortcut or UI button). Change language from `zh-CN` to `en`. Without leaving the scene or reloading, verify: (a) HUD labels (HP, MP, gold, level) update to English immediately, (b) combat log messages emitted after the switch are in English, (c) any open panel (inventory, skills, character) refreshes to English, (d) NPC dialogue triggered after the switch uses English strings.
Tool: agent-browser
Evidence: Before/after screenshots of HUD with language change; combat log screenshot showing English messages post-switch; inventory panel screenshot in English; NPC dialogue screenshot in English.

### VAL-CROSS-007: Default language on fresh install
Clear all `localStorage` and `IndexedDB` data for the application (simulating a fresh install). Load the game. Verify the game defaults to `zh-CN` — BootScene and MenuScene should display Simplified Chinese. Specifically check: title subtitle shows「渊 火」, main menu buttons show「新的旅程」「游戏控制」「原声音乐」「制作名单」. No English or Traditional Chinese should appear in menu button labels.
Tool: agent-browser
Evidence: Screenshot of `localStorage` showing no `abyssfire_lang` key; screenshot of MenuScene showing zh-CN default labels; DOM/canvas text snapshot confirming Simplified Chinese.

### VAL-CROSS-008: Mixed content consistency — English mode, no zh-CN leakage
Set language to `en`. Start a new game as Warrior. Enter combat (auto-attack or skill usage against a monster). Verify: (a) combat log messages are fully English (e.g., "Player attacks Slime for 25 damage", not "玩家 attacks 史莱姆 for 25 damage"), (b) monster names above health bars are in English, (c) quest objectives in the quest tracker are in English, (d) item names in loot drops and inventory tooltips are in English, (e) NPC shop item names and prices use English formatting. No zh-CN characters should appear anywhere in the visible UI.
Tool: agent-browser
Evidence: Screenshot of combat with combat log visible; screenshot of quest tracker showing English objectives; screenshot of inventory with item tooltip in English; screenshot of NPC shop in English; text audit searching for any CJK Unicode character (U+4E00–U+9FFF) in rendered text — should find zero occurrences outside decorative/logo elements.

### VAL-CROSS-009: Mixed content consistency — zh-TW mode, no zh-CN leakage
Set language to `zh-TW`. Play through combat and open inventory. Verify that strings which differ between Simplified and Traditional Chinese use Traditional forms. Specifically check these high-frequency UI terms: 裝備(not 装备), 戰士/法師/盜賊(not 战士/法师/盗贼), 經驗(not 经验), 傷害(not 伤害), 技能點(not 技能点), 任務(not 任务), 金幣(not 金币). Combat log entity names (monster names, skill names) must also use Traditional Chinese.
Tool: agent-browser
Evidence: Screenshot of inventory panel with zh-TW strings; screenshot of combat log with Traditional Chinese; character-level audit of at least 15 key terms confirming Traditional variants are used.

### VAL-CROSS-010: Template literal formatting — inventory count
Open inventory panel in each language (zh-CN, zh-TW, en). Verify the inventory count display formats correctly: zh-CN should show「背包 (N/100)」, zh-TW should show the Traditional equivalent, en should show something like "Inventory (N/100)" or "Backpack (N/100)". The numeric values (N and 100) must be correctly interpolated in all three languages. No placeholder tokens like `{count}` or `${count}` should be visible.
Tool: agent-browser
Evidence: Screenshot of inventory panel header in all 3 languages side-by-side; text content extraction confirming correct numeric interpolation.

### VAL-CROSS-011: Template literal formatting — gold display
Verify gold amount display in all 3 languages during gameplay. zh-CN:「金币: 1234G」, zh-TW: Traditional equivalent, en: "Gold: 1234G" or "Gold: 1,234". The numeric value must match the player's actual gold count. Test with at least 3 different gold amounts (0, small number, large number like 99999).
Tool: agent-browser
Evidence: Screenshots of gold display in each language at different amounts; verification that displayed number matches `player.gold` state.

### VAL-CROSS-012: Template literal formatting — level and experience display
Verify level display in all 3 languages. zh-CN:「Lv.5 剩余属性点: 3」, zh-TW: Traditional equivalent, en: "Lv.5 Stat Points: 3" (or equivalent). Verify experience bar label if present. Open character panel and verify stat labels (strength, agility, intelligence, vitality) are translated and numeric values are correctly interpolated.
Tool: agent-browser
Evidence: Screenshot of character panel in all 3 languages; verification of stat label translations and numeric accuracy.

### VAL-CROSS-013: Template literal formatting — combat log damage messages
Trigger combat in each language. Verify combat log messages format correctly with entity names and damage numbers interpolated: zh-CN:「战士 对 史莱姆 造成 50 点伤害」, en: "Warrior deals 50 damage to Slime", zh-TW: Traditional Chinese equivalent. Ensure no format string artifacts (`%d`, `{0}`, `${damage}`) appear in rendered text.
Tool: agent-browser
Evidence: Screenshot of combat log in each language showing at least 3 combat messages; text extraction confirming correct formatting.

### VAL-CROSS-014: Template literal formatting — skill tooltips and cooldowns
Hover over or click skill slots in each language. Verify skill name, description, mana cost, cooldown, and damage multiplier are all translated and numerically accurate. Example: zh-CN:「猛击 - 消耗30法力 - 冷却3秒 - 150%武器伤害」, en: "Smash - 30 Mana - 3s CD - 150% Weapon Damage". Numeric values must be consistent across languages (same skill = same numbers).
Tool: agent-browser
Evidence: Screenshot of skill tooltip in all 3 languages; numeric comparison table showing mana cost, cooldown, and damage multiplier are identical across languages.

### VAL-CROSS-015: Template literal formatting — quest objective progress
Accept a kill quest and partially complete it. Verify quest tracker displays progress correctly in all 3 languages: zh-CN:「消灭史莱姆 3/5」, en: "Kill Slime 3/5", zh-TW: Traditional equivalent. The progress numerator and denominator must update correctly and match actual kill count regardless of language.
Tool: agent-browser
Evidence: Screenshot of quest tracker in each language showing in-progress quest; verification that progress numbers match actual game state.

### VAL-CROSS-016: Language switch does not reset game state
During active gameplay with progress (gold, inventory items, quest progress, explored map), switch language from zh-CN to en. Verify: (a) player position unchanged, (b) inventory contents identical, (c) gold amount unchanged, (d) quest progress preserved, (e) explored fog-of-war state preserved. The language change should be purely cosmetic with zero gameplay side effects.
Tool: agent-browser
Evidence: Before/after comparison of player stats, inventory, gold, quest progress, and map exploration state; screenshot pairs proving game state continuity across language switch.
