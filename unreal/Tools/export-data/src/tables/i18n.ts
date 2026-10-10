/**
 * i18n_zh-CN.json, i18n_en.json, i18n_zh-TW.json — the complete flat string tables.
 *
 * zh-CN.ts / en.ts already spread the sub-tables (story, questStory, abyssRun, homestead, pets), so their
 * default exports are the full tables the game's t() reads. zh-TW is generated exactly like
 * src/i18n/index.ts does at runtime (convertToTraditional over every zh-CN value). Key order = TS order.
 * Runtime fallback chain (t()): zh-TW → zh-CN → en → the key itself; `{name}` placeholders.
 */
import zhCN from '../../../../../src/i18n/locales/zh-CN';
import en from '../../../../../src/i18n/locales/en';
import { convertToTraditional } from '../../../../../src/i18n/converter';
import { assert, type TableResult } from '../util';

const SOURCES = ['src/i18n/locales/zh-CN.ts', 'src/i18n/locales/en.ts', 'src/i18n/locales/story.ts', 'src/i18n/locales/questStory.ts',
  'src/i18n/locales/abyssRun.ts', 'src/i18n/locales/homestead.ts', 'src/i18n/locales/pets.ts'];

/**
 * Port-only strings (decisions that add player-facing text the web never had). Merged after the web tables, so a web
 * key with the same name would win; the export fails instead (assert below). zh-TW is converted like every other key.
 */
const PORT_STRINGS: Record<string, { 'zh-CN': string; en: string; decision: string }> = {
  'zone.exit.sealedChapter2': { 'zh-CN': '第二章即将开放', en: 'Chapter 2 is coming soon', decision: 'W7' },
  'sys.inventory.levelTooLow': { 'zh-CN': '等级不足，需要等级 {level}', en: 'Level too low: requires level {level}', decision: 'I3' },
  // quests 3.5: an examined clue floats "<targetName>\n<note>" (the web joined the two translated strings in code).
  'zone.quest.clueNote': { 'zh-CN': '{targetName}\n{note}', en: '{targetName}\n{note}', decision: 'Q11' },
  // world 13.3: a solved puzzle logs "<solution> — <reward>" before the gold / exp line (ZoneScene.ts joined them in code).
  'zone.event.puzzle.solved': { 'zh-CN': '{solution} — {reward}', en: '{solution} — {reward}', decision: 'world 13.3' },
  // UE-layer UI (save-ui-input, DECISIONS U1/U2/U4/U8/U9, P4, P13, Q6, I3/I4, C3, C10, W3): text the port UI shows that the
  // web never had. Every literal key the Abyssfire module passes to Loc*/LocalizeOr must exist here or in the web tables
  // (unreal/Scripts/tests/test_ui_i18n_keys.py).
  'menu.settings': { 'zh-CN': '设置', en: 'Settings', decision: 'U9' },
  'menu.quit': { 'zh-CN': '退出游戏', en: 'Quit', decision: 'U4' },
  'menu.slot.label': { 'zh-CN': '存档 {n}', en: 'Slot {n}', decision: 'U1' },
  'menu.slot.empty': { 'zh-CN': '空存档位', en: 'Empty slot', decision: 'U1' },
  'menu.slot.continue': { 'zh-CN': '继续', en: 'Continue', decision: 'U1' },
  'menu.slot.overwrite': { 'zh-CN': '新建', en: 'New', decision: 'U1' },
  'menu.slot.delete': { 'zh-CN': '删除', en: 'Delete', decision: 'U1' },
  'menu.slot.playTime': { 'zh-CN': '游戏时间 {time}', en: 'Played {time}', decision: 'U1' },
  'menu.slot.overwriteTitle': { 'zh-CN': '在此开始新的旅程？', en: 'Start a new journey here?', decision: 'U1' },
  'menu.slot.overwriteBody': { 'zh-CN': '新的旅程开始后，此存档位中的英雄将被替换。', en: 'The hero saved in this slot will be replaced once the new journey starts.', decision: 'U1' },
  'menu.slot.deleteTitle': { 'zh-CN': '删除此存档？', en: 'Delete this save?', decision: 'U1' },
  'menu.slot.deleteBody': { 'zh-CN': '{class} Lv.{level} 将被永久删除。', en: '{class} Lv.{level} will be lost forever.', decision: 'U1' },
  'menu.slot.loadFailedTitle': { 'zh-CN': '无法读取此存档', en: 'Cannot load this save', decision: 'U2' },
  'menu.slot.versionTooNew': { 'zh-CN': '此存档由更新版本的游戏创建，请更新游戏后继续。', en: 'This save was made by a newer version of the game. Update the game to continue it.', decision: 'U2' },
  'menu.slot.corrupt': { 'zh-CN': '此存档已损坏，无法读取。', en: 'This save file is damaged and cannot be loaded.', decision: 'U2' },
  'menu.slot.corruptBackup': { 'zh-CN': '此存档已损坏。是否读取此存档位的上一份存档？', en: 'This save file is damaged. Load the previous save of this slot instead?', decision: 'U2' },
  'menu.slot.loadBackup': { 'zh-CN': '读取上一份存档', en: 'Load previous save', decision: 'U2' },
  'menu.helpPanel.col.keyboard': { 'zh-CN': '键盘', en: 'Keyboard', decision: 'P13' },
  'menu.helpPanel.col.gamepad': { 'zh-CN': '手柄', en: 'Gamepad', decision: 'P13' },
  'menu.helpPanel.movement.interact': { 'zh-CN': '对话 / 使用', en: 'Talk / use', decision: 'Q6' },
  'menu.helpPanel.combat.autoLoot': { 'zh-CN': '切换自动拾取', en: 'Cycle auto-loot', decision: 'save-ui-input 2.3' },
  'menu.helpPanel.combat.potions': { 'zh-CN': '饮用药水', en: 'Drink a potion', decision: 'I4' },
  'menu.helpPanel.ui.achievements': { 'zh-CN': '成就', en: 'Achievements', decision: 'U9' },
  'menu.helpPanel.ui.settings': { 'zh-CN': '设置', en: 'Settings', decision: 'U9' },
  'menu.helpPanel.ui.menu': { 'zh-CN': '关闭面板 / 系统菜单', en: 'Close panel / system menu', decision: 'U4' },
  'menu.helpPanel.touchNote': { 'zh-CN': '触屏：摇杆移动，技能扇区攻击，顶部按钮打开面板。', en: 'Touch: joystick to move, the skill fan to attack, the top row opens panels.', decision: 'P4' },
  'menu.creditsPanel.design': { 'zh-CN': '设计与开发', en: 'Design & Development', decision: 'credits' },
  'menu.creditsPanel.art': { 'zh-CN': '美术', en: 'Art', decision: 'credits' },
  'menu.creditsPanel.artPipeline': { 'zh-CN': '角色、道具、图标与肖像由 Blender 管线制作', en: 'Characters, props, icons and portraits built with the Blender pipeline', decision: 'R10' },
  'menu.creditsPanel.audio': { 'zh-CN': '音乐与音效', en: 'Music & Sound', decision: 'credits' },
  'menu.creditsPanel.audioPipeline': { 'zh-CN': '程序化配乐与音效由渊火合成器渲染', en: 'Procedural scores and effects rendered by the Abyssfire synthesiser', decision: 'A1' },
  'menu.creditsPanel.fonts': { 'zh-CN': '字体', en: 'Fonts', decision: 'credits' },
  'menu.creditsPanel.engineNotice': { 'zh-CN': 'Unreal® 是 Epic Games, Inc. 在美国及其他地区的商标或注册商标。', en: 'Unreal® is a trademark or registered trademark of Epic Games, Inc. in the United States of America and elsewhere.', decision: 'credits' },
  'ui.settings.title': { 'zh-CN': '设 置', en: 'Settings', decision: 'U9' },
  'ui.settings.audio': { 'zh-CN': '音频', en: 'Audio', decision: 'U9' },
  'ui.settings.master': { 'zh-CN': '总音量', en: 'Master', decision: 'U9' },
  'ui.settings.display': { 'zh-CN': '显示', en: 'Display', decision: 'U9' },
  'ui.settings.quality': { 'zh-CN': '画质', en: 'Graphics', decision: 'U9' },
  'ui.settings.quality.auto': { 'zh-CN': '自动', en: 'Auto', decision: 'U9' },
  'ui.settings.quality.low': { 'zh-CN': '低', en: 'Low', decision: 'U9' },
  'ui.settings.quality.balanced': { 'zh-CN': '中', en: 'Medium', decision: 'U9' },
  'ui.settings.quality.high': { 'zh-CN': '高', en: 'High', decision: 'U9' },
  'ui.settings.controls': { 'zh-CN': '操作方式', en: 'Controls', decision: 'U9' },
  'ui.settings.controls.auto': { 'zh-CN': '自动', en: 'Auto', decision: 'U9' },
  'ui.settings.controls.desktop': { 'zh-CN': '鼠标与键盘', en: 'Mouse & Keys', decision: 'U9' },
  'ui.settings.controls.touch': { 'zh-CN': '触屏', en: 'Touch', decision: 'U9' },
  'ui.settings.touch': { 'zh-CN': '触屏操作', en: 'Touch Controls', decision: 'U9' },
  'ui.settings.touchScale': { 'zh-CN': '大小', en: 'Size', decision: 'U9' },
  'ui.settings.touchOpacity': { 'zh-CN': '透明度', en: 'Opacity', decision: 'U9' },
  'ui.settings.feel': { 'zh-CN': '游戏', en: 'Gameplay', decision: 'U9' },
  'ui.settings.cameraShake': { 'zh-CN': '镜头震动', en: 'Camera shake', decision: 'U9' },
  'ui.settings.damageNumbers': { 'zh-CN': '伤害数字', en: 'Damage numbers', decision: 'U9' },
  'ui.settings.on': { 'zh-CN': '开', en: 'On', decision: 'U9' },
  'ui.settings.off': { 'zh-CN': '关', en: 'Off', decision: 'U9' },
  'ui.settings.defaults': { 'zh-CN': '恢复默认', en: 'Restore defaults', decision: 'U9' },
  'ui.system.title': { 'zh-CN': '菜 单', en: 'Menu', decision: 'U4' },
  'ui.system.resume': { 'zh-CN': '继续游戏', en: 'Resume', decision: 'U4' },
  'ui.system.settings': { 'zh-CN': '设置', en: 'Settings', decision: 'U4' },
  'ui.system.achievements': { 'zh-CN': '成就', en: 'Achievements', decision: 'U9' },
  'ui.system.controls': { 'zh-CN': '操作说明', en: 'Controls', decision: 'U4' },
  'ui.system.saveAndExit': { 'zh-CN': '保存并返回标题', en: 'Save & Return to Title', decision: 'U4' },
  'ui.system.quit': { 'zh-CN': '保存并退出', en: 'Save & Quit', decision: 'U4' },
  'ui.error.dataTitle': { 'zh-CN': '数据错误', en: 'Data error', decision: 'U2' },
  'ui.error.startFailed': { 'zh-CN': '无法开始', en: 'Cannot start', decision: 'U2' },
  'ui.error.coreCreateFailed': { 'zh-CN': '无法创建游戏核心（数据未完成加载）。', en: 'The game core could not be created (data not finalized).', decision: 'U2' },
  'ui.error.newGameFailed': { 'zh-CN': '无法创建新游戏（未知职业或缺少默认地图）。', en: 'The new game could not be created (unknown class or missing default map).', decision: 'U2' },
  'ui.error.loadFailed': { 'zh-CN': '无法读取', en: 'Cannot load', decision: 'U2' },
  'ui.error.saveFailedTitle': { 'zh-CN': '保存失败', en: 'Save failed', decision: 'U2' },
  'ui.error.saveFailed': { 'zh-CN': '无法保存游戏。', en: 'The game could not be saved.', decision: 'U2' },
  'ui.error.ok': { 'zh-CN': '确定', en: 'OK', decision: 'U2' },
  'sys.mobile.portal': { 'zh-CN': '回城', en: 'Portal', decision: 'W3, world-map-nav 7.4' },
  'sys.mobile.potion.hp': { 'zh-CN': '生命', en: 'HP', decision: 'I4' },
  'sys.mobile.potion.mp': { 'zh-CN': '法力', en: 'MP', decision: 'I4' },
  'sys.mobile.interact.talk': { 'zh-CN': '对话', en: 'Talk', decision: 'Q6' },
  'sys.mobile.interact.use': { 'zh-CN': '使用', en: 'Use', decision: 'Q6' },
  'sys.mobile.interact.pickup': { 'zh-CN': '拾取', en: 'Pick up', decision: 'Q6' },
  'sys.mobile.interact.open': { 'zh-CN': '打开', en: 'Open', decision: 'Q6' },
  'sys.mobile.panel.menu': { 'zh-CN': '菜单', en: 'Menu', decision: 'U9' },
  'ui.skillTree.hotbar': { 'zh-CN': '技能栏', en: 'Skill bar', decision: 'C3' },
  'ui.skillTree.hotbarHint': { 'zh-CN': '点击已学技能，再点击栏位。右键点击栏位可清除。', en: 'Click a learned skill, then a slot. Right-click a slot to clear it.', decision: 'C3' },
  'ui.skillTree.hotbarHintTouch': { 'zh-CN': '点击已学技能，再点击栏位。点击栏位可移动或清除。', en: 'Tap a learned skill, then a slot. Tap a slot to move or clear it.', decision: 'C3' },
  'ui.skillTree.hotbarPick': { 'zh-CN': '为 {name} 选择栏位', en: 'Pick a slot for {name}', decision: 'C3' },
  'ui.skillTree.learn': { 'zh-CN': '学习', en: 'Learn', decision: 'C3' },
  'ui.skillTree.move': { 'zh-CN': '移动', en: 'Move', decision: 'C3' },
  'ui.skillTree.clear': { 'zh-CN': '清除', en: 'Clear', decision: 'C3' },
  'ui.skillTree.passive': { 'zh-CN': '被动', en: 'Passive', decision: 'C1' },
  'ui.worldMap.explored': { 'zh-CN': '已探索 {percent}%', en: 'Explored {percent}%', decision: 'U8' },
  'ui.worldMap.legend.hero': { 'zh-CN': '你', en: 'You', decision: 'U8' },
  'ui.worldMap.legend.questGiver': { 'zh-CN': '任务发布者', en: 'Quest giver', decision: 'U8' },
  'ui.worldMap.legend.guide': { 'zh-CN': '任务目标', en: 'Quest target', decision: 'U8' },
  'ui.worldMap.legend.exit': { 'zh-CN': '出口', en: 'Exit', decision: 'U8' },
  'ui.worldMap.legend.monster': { 'zh-CN': '怪物', en: 'Monster', decision: 'U8' },
  'ui.worldMap.legend.questArea': { 'zh-CN': '任务区域', en: 'Quest area', decision: 'U8' },
  'ui.questLog.track': { 'zh-CN': '追踪', en: 'Track', decision: 'save-ui-input 7.6' },
  'ui.questLog.untrack': { 'zh-CN': '取消追踪', en: 'Stop tracking', decision: 'save-ui-input 7.6' },
  'ui.puzzle.title': { 'zh-CN': '古老的谜题', en: 'An Ancient Puzzle', decision: 'world 13.3' },
  'ui.forge.place': { 'zh-CN': '放上铁砧', en: 'Place on anvil', decision: 'loot 13' },
  'ui.shop.buyPrice': { 'zh-CN': '价格: {price}G', en: 'Price: {price}G', decision: 'I5' },
  'ui.hud.potionBest': { 'zh-CN': '最佳可用', en: 'Best available', decision: 'I4' },
  'ui.context.quickSlot': { 'zh-CN': '快捷栏', en: 'Quick slot', decision: 'I4' },
  'ui.character.computed.hit': { 'zh-CN': '普攻伤害', en: 'Basic hit', decision: 'save-ui-input 7.2' },
  'ui.character.computed.dodge': { 'zh-CN': '闪避', en: 'Dodge', decision: 'save-ui-input 7.2' },
  'ui.tooltip.inert': { 'zh-CN': '(未生效)', en: '(inactive)', decision: 'C10' },
  'ui.tooltip.levelReq': { 'zh-CN': '需要等级 {level}', en: 'Requires level {level}', decision: 'I3' },
};

export function exportI18n(): TableResult[] {
  for (const k of Object.keys(PORT_STRINGS)) {
    assert(!(k in zhCN) && !(k in en), `PORT_STRINGS key ${k} now exists in the web tables — remove it from the exporter`);
  }
  const zh: Record<string, string> = { ...zhCN };
  const enAll: Record<string, string> = { ...en };
  for (const [k, v] of Object.entries(PORT_STRINGS)) {
    zh[k] = v['zh-CN'];
    enAll[k] = v.en;
  }
  const portKeys = Object.fromEntries(Object.entries(PORT_STRINGS).map(([k, v]) => [k, v.decision]));
  const zhTW: Record<string, string> = {};
  for (const [k, v] of Object.entries(zh)) zhTW[k] = convertToTraditional(v);
  for (const [name, table] of [['zh-CN', zhCN], ['en', en]] as const) {
    for (const [k, v] of Object.entries(table)) assert(typeof v === 'string', `${name}: ${k} is not a string`);
  }
  const zhKeys = new Set(Object.keys(zhCN));
  const enKeys = new Set(Object.keys(en));
  const missingInEn = [...zhKeys].filter(k => !enKeys.has(k));
  const missingInZh = [...enKeys].filter(k => !zhKeys.has(k));
  const table = (locale: string, strings: Record<string, string>, source: string[], extra: Record<string, unknown>): TableResult => ({
    file: `i18n_${locale}.json`,
    source,
    data: { locale, fallback: locale === 'zh-TW' ? ['zh-CN', 'en'] : locale === 'zh-CN' ? ['en'] : [], placeholder: '{name}', ...extra,
      keyCount: Object.keys(strings).length, strings },
    counts: { keys: Object.keys(strings).length },
  });
  return [
    table('zh-CN', zh, SOURCES.filter(s => !s.endsWith('/en.ts')),
      { missingInOtherLocale: missingInZh.length ? { keysOnlyInEn: missingInZh } : {}, portKeys }),
    table('en', enAll, SOURCES.filter(s => !s.endsWith('/zh-CN.ts')),
      { missingInOtherLocale: missingInEn.length ? { keysOnlyInZhCN: missingInEn } : {}, portKeys }),
    table('zh-TW', zhTW, [...SOURCES.filter(s => !s.endsWith('/en.ts')), 'src/i18n/converter.ts', 'src/i18n/index.ts'],
      { generatedFrom: 'zh-CN via convertToTraditional', portKeys }),
  ];
}
