/**
 * i18n_zh-CN.json, i18n_en.json, i18n_zh-TW.json — the complete flat string tables.
 *
 * zh-CN.ts / en.ts already spread the sub-tables (story, questStory, abyssRun, homestead, pets), so their
 * default exports are the full tables the game's t() reads. zh-TW is generated exactly like
 * src/i18n/index.ts does at runtime (convertToTraditional over every zh-CN value). Key order = TS order.
 * Runtime fallback chain (t()): zh-TW → zh-CN → en → the key itself; `{name}` placeholders.
 *
 * Remake (implementation-plan.md §2.5), per locale: web table → PORT_STRINGS → unreal/GameData/i18n strings (the remake
 * wins; a replaced key keeps its position, a new key is appended). Then the orphan sweep drops the content-namespace keys
 * whose id no exported table references any more, and the parity checks fail the export when a key the tables reference
 * is missing in zh-CN or en. This module runs last (main.ts) and reads the other tables.
 *
 * zh-TW of a remake key goes through remakeToTraditional (GameData/i18n/zhTWExtra.ts: context phrases, the web
 * converter, then the supplement's characters); every other key keeps convertToTraditional's output byte for byte.
 */
import zhCN from '../../../../../src/i18n/locales/zh-CN';
import en from '../../../../../src/i18n/locales/en';
import { convertToTraditional } from '../../../../../src/i18n/converter';
import remakeZhCN from '../../../../GameData/i18n/zh-CN';
import remakeEn from '../../../../GameData/i18n/en';
import { ZH_TW_CHARS, ZH_TW_PHRASES } from '../../../../GameData/i18n/zhTWExtra';
import { assert, type TableResult } from '../util';
import { noteExtra, remakeRequiredKeys } from '../remake';

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
  // Dark remake U1: the brightness / gamma slider (settings.json video.brightness, an exposure bias on the zone).
  'ui.settings.brightness': { 'zh-CN': '亮度', en: 'Brightness', decision: 'U1' },
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
  // C14 cast target rule: a targeted skill pressed with no monster to resolve, or with its target beyond the skill's reach.
  'zone.combat.noTarget': { 'zh-CN': '没有可攻击的目标!', en: 'No target!', decision: 'C14' },
  'zone.combat.outOfRange': { 'zh-CN': '目标超出范围!', en: 'Target out of range!', decision: 'C14' },
  // Dark remake 2G (U5 W2, DECISIONS DR16): the toast of a K6 permanent quest reward (EvPermanentReward): the quest's
  // `permanent` name (else its name) and one line per field above 0.
  'ui.permanentReward.toast': { 'zh-CN': '{title}：{rewards}', en: '{title}: {rewards}', decision: 'DR16' },
  'ui.permanentReward.maxLife': { 'zh-CN': '最大生命 +{n}', en: 'Max Life +{n}', decision: 'DR16' },
  'ui.permanentReward.statPoints': { 'zh-CN': '属性点 +{n}', en: 'Stat Points +{n}', decision: 'DR16' },
  'ui.permanentReward.skillPoints': { 'zh-CN': '技能点 +{n}', en: 'Skill Points +{n}', decision: 'DR16' },
  'ui.permanentReward.respecTokens': { 'zh-CN': '洗点券 +{n}', en: 'Respec Tokens +{n}', decision: 'DR16' },
};

const REMAKE_SOURCE = { zhCN: 'unreal/GameData/i18n/zh-CN.ts', en: 'unreal/GameData/i18n/en.ts' };
const ZH_TW_SOURCE = 'unreal/GameData/i18n/zhTWExtra.ts';

// ── zh-TW of the remake's strings (final review R4) ─────────────────────────

const PHRASES_LONGEST_FIRST = Object.keys(ZH_TW_PHRASES).sort((a, b) => b.length - a.length || (a < b ? -1 : a > b ? 1 : 0));
const PHRASE_LENGTHS = [...new Set(PHRASES_LONGEST_FIRST.map(p => p.length))];

/** The web converter, then the supplement's characters for what it left Simplified. */
function convertSegment(text: string): string {
  let out = '';
  for (const ch of convertToTraditional(text)) out += ZH_TW_CHARS[ch] ?? ch;
  return out;
}

/**
 * zh-CN -> zh-TW for a GameData string: left to right, a ZH_TW_PHRASES key (longest first) is replaced by its value as
 * is; the text between phrases goes through convertSegment.
 */
function remakeToTraditional(text: string): string {
  let out = '';
  let pending = '';
  for (let i = 0; i < text.length;) {
    let hit: string | undefined;
    for (const len of PHRASE_LENGTHS) {
      const cand = text.slice(i, i + len);
      if (cand.length === len && cand in ZH_TW_PHRASES) { hit = cand; break; }
    }
    if (hit !== undefined) {
      out += convertSegment(pending) + ZH_TW_PHRASES[hit];
      pending = '';
      i += hit.length;
    } else {
      pending += text[i];
      i += 1;
    }
  }
  return out + convertSegment(pending);
}

// ── Orphan sweep ────────────────────────────────────────────────────────────

type TableData = Record<string, unknown>;
type Lookup = (file: string) => TableData;
type Rec = Record<string, unknown>;

const list = (v: unknown): Rec[] => (Array.isArray(v) ? v as Rec[] : []);
const obj = (v: unknown): Record<string, Rec> => (v && typeof v === 'object' && !Array.isArray(v) ? v as Record<string, Rec> : {});
const quests = (t: Lookup) => list(t('quests.json').quests);
const objectiveTargets = (t: Lookup) => quests(t).flatMap(q => list(q.objectives).map(o => String(o.targetId)));
const bareQuestId = (id: string) => (id.startsWith('q_') ? id.slice(2) : id);

/**
 * Content namespaces (implementation-plan.md §2.5): a key `<prefix><id>...` (id = the next `segments` dot segments) is
 * kept while `ids` (read from the exported tables, the same way the game builds the key) contains its id, and swept
 * otherwise. `required` lists keys every referenced record must have in zh-CN and en (parity).
 */
interface ContentNamespace {
  prefix: string;
  segments: number;
  /** Optional filter on the id (story.<cutsceneId> shares `story.` with prologue / chapter / boss / ...). */
  applies?: (id: string) => boolean;
  ids: (t: Lookup) => Iterable<string>;
  required?: (t: Lookup) => Iterable<string>;
}

const CONTENT_NAMESPACES: readonly ContentNamespace[] = [
  { prefix: 'data.quest.', segments: 1, ids: t => quests(t).map(q => String(q.id)),
    required: t => quests(t).flatMap(q => [`data.quest.${q.id}.name`, `data.quest.${q.id}.desc`]) },
  // getQuestTargetName / QuestWorld clue notes read data.questTarget / data.questClue.<objective targetId>.
  { prefix: 'data.questTarget.', segments: 1, ids: objectiveTargets },
  { prefix: 'data.questClue.', segments: 1, ids: objectiveTargets },
  { prefix: 'data.questChoice.', segments: 1, ids: t => quests(t).map(q => String(q.id)) },
  { prefix: 'data.monster.', segments: 1,
    ids: t => [...Object.keys(obj(t('monsters.json').defs)), ...list(t('quest_hunts.json').hunts).map(h => String(h.huntId))],
    required: t => [...Object.keys(obj(t('monsters.json').defs)), ...list(t('quest_hunts.json').hunts).map(h => String(h.huntId))]
      .map(id => `data.monster.${id}`) },
  { prefix: 'data.npc.', segments: 1, ids: t => Object.keys(obj(t('npcs.json').npcs)),
    required: t => Object.keys(obj(t('npcs.json').npcs)).map(id => `data.npc.${id}.name`) },
  { prefix: 'data.dialogue.', segments: 2,
    ids: t => Object.entries(obj(t('dialogue_trees.json').trees)).filter(([, tr]) => tr.kind === 'npc')
      .flatMap(([id, tr]) => Object.keys(obj(tr.nodes)).map(n => `${id}.${n}`)),
    required: t => Object.entries(obj(t('dialogue_trees.json').trees)).filter(([, tr]) => tr.kind === 'npc')
      .flatMap(([id, tr]) => Object.entries(obj(tr.nodes)).flatMap(([n, node]) =>
        [`data.dialogue.${id}.${n}.text`, ...list(node.choices).map((_, i) => `data.dialogue.${id}.${n}.choice.${i}`)])) },
  { prefix: 'data.miniBossDialogue.', segments: 2,
    ids: t => Object.entries(obj(t('minibosses.json').dialogues)).flatMap(([m, d]) => Object.keys(obj(d.nodes)).map(n => `${m}.${n}`)),
    required: t => Object.entries(obj(t('minibosses.json').dialogues))
      .flatMap(([m, d]) => Object.keys(obj(d.nodes)).map(n => `data.miniBossDialogue.${m}.${n}`)) },
  { prefix: 'data.lore.', segments: 1, ids: t => Object.values(obj(t('lore.json').byZone)).flatMap(z => list(z).map(e => String(e.id))),
    required: t => Object.values(obj(t('lore.json').byZone)).flatMap(z => list(z).flatMap(e => [`data.lore.${e.id}.name`, `data.lore.${e.id}.text`])) },
  { prefix: 'data.storyDeco.', segments: 1,
    ids: t => Object.values(obj(t('maps.json').maps)).flatMap(m => list(m.storyDecorations).map(d => String(d.id))),
    required: t => Object.values(obj(t('maps.json').maps)).flatMap(m => list(m.storyDecorations)
      .flatMap(d => [`data.storyDeco.${d.id}.name`, `data.storyDeco.${d.id}.desc`])) },
  { prefix: 'data.hiddenArea.', segments: 1,
    ids: t => Object.values(obj(t('maps.json').maps)).flatMap(m => list(m.hiddenAreas).map(d => String(d.id))),
    required: t => Object.values(obj(t('maps.json').maps)).flatMap(m => list(m.hiddenAreas)
      .flatMap(d => [`data.hiddenArea.${d.id}.name`, `data.hiddenArea.${d.id}.discovery`])) },
  // QuestWorld (Q11): data.escortNpc / data.defendTarget.<quest id without q_> (a quest without one shows the data's name).
  { prefix: 'data.escortNpc.', segments: 1, ids: t => quests(t).filter(q => q.escortNpc).map(q => bareQuestId(String(q.id))) },
  { prefix: 'data.defendTarget.', segments: 1, ids: t => quests(t).filter(q => q.defendTarget).map(q => bareQuestId(String(q.id))) },
  { prefix: 'data.achievement.', segments: 1, ids: t => list(t('achievements.json').achievements).map(a => String(a.id)),
    required: t => list(t('achievements.json').achievements).flatMap(a => [`data.achievement.${a.id}.name`, `data.achievement.${a.id}.desc`]) },
  { prefix: 'story.', segments: 1, applies: id => id.startsWith('cs_'), ids: t => Object.keys(obj(t('story.json').cutscenes)) },
  { prefix: 'story.boss.', segments: 1, ids: t => list(t('story.json').bossIntros).map(b => String(b.monsterId)) },
  // Story sequences: a slide key (`story.<seq>.<n>.<field>`) the exported sequence no longer uses is dropped (the
  // remake's prologue has no heading lines, so the web's `story.prologue.<n>.heading` go).
  ...(['prologue', 'epilogue', 'credits'] as const).map((seq): ContentNamespace => ({
    prefix: `story.${seq}.`, segments: 2, ids: t => slideKeys(t, seq).map(k => k.slice(`story.${seq}.`.length)) })),
];

/** The i18n keys the slides of story.json sequence `seq` reference. */
function slideKeys(t: Lookup, seq: string): string[] {
  const slides = list(obj(t('story.json'))[seq]?.slides);
  return slides.flatMap(s => Object.values(s).filter((v): v is string => typeof v === 'string' && v.startsWith(`story.${seq}.`)));
}

/**
 * NPC lines (`data.npc.<id>.dialogue.<n>`): the game reads lines 0..n-1 of the NPC's exported `dialogue` (QuestWorld logs
 * line 0, the linear panel shows them all), so a line at or past the end of a known NPC's list is an orphan: the web's
 * extra lines of an NPC the remake re-voiced with fewer lines (chapter1.md §3).
 */
function npcLineOrphan(t: Lookup, key: string): boolean {
  const m = /^data\.npc\.([^.]+)\.dialogue\.(\d+)$/.exec(key);
  if (!m) return false;
  const npc = obj(t('npcs.json').npcs)[m[1]];
  return npc !== undefined && Number(m[2]) >= (Array.isArray(npc.dialogue) ? npc.dialogue.length : 0);
}

/**
 * Keys in the content namespaces that no exported table referenced before the remake either (web i18n debt: objective
 * targets the web dropped long ago). The sweep keeps them, so it only removes what the remake orphans and the Phase 0
 * export stays byte-identical. Each entry must still exist and be unreferenced, or the export fails (stale entry).
 */
const WEB_I18N_DEBT: readonly string[] = [
  'data.questTarget.stone_golem', 'data.questTarget.desert_scorpion', 'data.questTarget.succubus',
  'data.questTarget.mat_mushroom', 'data.questTarget.mat_moonlight_herb', 'data.questTarget.mat_ancient_relic',
  'data.questTarget.zone_scout_camp', 'data.questTarget.zone_collapsed_mine', 'data.questTarget.zone_dragon_nest',
  'data.questTarget.zone_buried_ruins', 'data.questTarget.zone_fallen_shrine', 'data.questTarget.zone_hero_memorial',
];

/** The namespace and id of a content key, or null for any other key. */
function contentId(key: string): { ns: ContentNamespace; id: string } | null {
  let best: { ns: ContentNamespace; id: string } | null = null;
  for (const ns of CONTENT_NAMESPACES) {
    if (!key.startsWith(ns.prefix)) continue;
    const parts = key.slice(ns.prefix.length).split('.');
    if (parts.length < ns.segments) continue;
    const id = parts.slice(0, ns.segments).join('.');
    if (ns.applies && !ns.applies(id)) continue;
    if (!best || ns.prefix.length > best.ns.prefix.length) best = { ns, id };
  }
  return best;
}

/** Key-shaped string values anywhere in a table (`root.seg.seg`, root = a namespace root of the string tables). */
function collectKeyRefs(v: unknown, roots: ReadonlySet<string>, out: Map<string, string>, file: string, path: string): void {
  if (typeof v === 'string') {
    if (/^[a-z][A-Za-z0-9]*(\.[A-Za-z0-9_-]+)+$/.test(v) && roots.has(v.slice(0, v.indexOf('.'))) && !out.has(v)) out.set(v, `${file}:${path}`);
  } else if (Array.isArray(v)) {
    v.forEach((x, i) => collectKeyRefs(x, roots, out, file, `${path}[${i}]`));
  } else if (v && typeof v === 'object') {
    for (const [k, x] of Object.entries(v)) collectKeyRefs(x, roots, out, file, path ? `${path}.${k}` : k);
  }
}

const fewKeys = (keys: readonly string[]) => `${keys.slice(0, 12).join(', ')}${keys.length > 12 ? `, … (${keys.length})` : ''}`;

export function exportI18n(_ctx: unknown, tables: readonly TableResult[]): TableResult[] {
  for (const k of Object.keys(ZH_TW_CHARS)) {
    assert(convertToTraditional(k) === k, `${ZH_TW_SOURCE}: '${k}' is in the web converter's map already — remove it from ZH_TW_CHARS`);
  }
  for (const k of Object.keys(PORT_STRINGS)) {
    assert(!(k in zhCN) && !(k in en), `PORT_STRINGS key ${k} now exists in the web tables — remove it from the exporter`);
  }
  const lookup: Lookup = file => {
    const t = tables.find(x => x.file === file);
    assert(t, `i18n sweep: ${file} was not exported before the i18n tables`);
    return t.data;
  };

  // ── merge: web → PORT_STRINGS → GameData (remake wins) ──
  const zh: Record<string, string> = { ...zhCN };
  const enAll: Record<string, string> = { ...en };
  for (const [k, v] of Object.entries(PORT_STRINGS)) {
    zh[k] = v['zh-CN'];
    enAll[k] = v.en;
  }
  const remakeKeys = Object.keys(remakeZhCN);
  const onlyZh = remakeKeys.filter(k => !(k in remakeEn));
  const onlyEn = Object.keys(remakeEn).filter(k => !(k in remakeZhCN));
  assert(!onlyZh.length && !onlyEn.length, 'GameData i18n: zh-CN and en must define the same keys' +
    (onlyZh.length ? `; only in zh-CN: ${fewKeys(onlyZh)}` : '') + (onlyEn.length ? `; only in en: ${fewKeys(onlyEn)}` : ''));
  // Replaced keys, by what they replace: a web string or a port string (PORT_STRINGS keys never collide with web keys).
  const replacedPort = remakeKeys.filter(k => k in PORT_STRINGS);
  const replacedWeb = remakeKeys.filter(k => k in zh && !(k in PORT_STRINGS));
  for (const k of remakeKeys) {
    assert(typeof remakeZhCN[k] === 'string' && typeof remakeEn[k] === 'string', `GameData i18n: ${k} is not a string`);
    zh[k] = remakeZhCN[k];
    enAll[k] = remakeEn[k];
  }
  for (const [name, table] of [['zh-CN', zhCN], ['en', en]] as const) {
    for (const [k, v] of Object.entries(table)) assert(typeof v === 'string', `${name}: ${k} is not a string`);
  }

  // ── orphan sweep (content namespaces only) ──
  for (const id of Object.keys(obj(lookup('story.json').cutscenes))) {
    assert(id.startsWith('cs_'), `story.json cutscene '${id}': cutscene ids start with cs_ (the i18n sweep keys story.<cutsceneId> on it)`);
  }
  const referenced = new Map(CONTENT_NAMESPACES.map(ns => [ns, new Set(ns.ids(lookup))]));
  const orphans: string[] = [];
  for (const k of new Set([...Object.keys(zh), ...Object.keys(enAll)])) {
    const c = contentId(k);
    if ((c && !referenced.get(c.ns)!.has(c.id)) || npcLineOrphan(lookup, k)) orphans.push(k);
  }
  const orphanSet = new Set(orphans);
  for (const k of WEB_I18N_DEBT) {
    assert(k in zh || k in enAll, `WEB_I18N_DEBT: ${k} is no longer in the string tables — remove the entry`);
    assert(orphanSet.has(k), `WEB_I18N_DEBT: ${k} is referenced by an exported table now — remove the entry`);
  }
  const remakeOrphans = orphans.filter(k => k in remakeZhCN);
  assert(!remakeOrphans.length, `GameData i18n: no exported table references ${fewKeys(remakeOrphans)} (typo, or the record was removed)`);
  const swept = orphans.filter(k => !WEB_I18N_DEBT.includes(k));
  for (const k of swept) { delete zh[k]; delete enAll[k]; }

  // ── parity: every key the exported tables reference exists in zh-CN and en ──
  const roots = new Set([...Object.keys(zh), ...Object.keys(enAll)].map(k => k.slice(0, k.indexOf('.'))).filter(Boolean));
  const refs = new Map<string, string>();
  for (const t of tables) collectKeyRefs(t.data, roots, refs, t.file, '');
  for (const ns of CONTENT_NAMESPACES) for (const k of ns.required?.(lookup) ?? []) if (!refs.has(k)) refs.set(k, `convention ${ns.prefix}*`);
  // Keys the core builds from exported data (remake.ts noteRequiredKey; K6: a remake status type's name).
  for (const [k, where] of remakeRequiredKeys()) if (!refs.has(k)) refs.set(k, where);
  const missing = [...refs].filter(([k]) => !(k in zh) || !(k in enAll)).map(([k, where]) =>
    `${k} (${!(k in zh) && !(k in enAll) ? 'zh-CN, en' : !(k in zh) ? 'zh-CN' : 'en'}; ${where})`);
  assert(!missing.length, `i18n parity: referenced keys missing: ${fewKeys(missing)}`);
  const oneSided = Object.keys(zh).filter(k => contentId(k) && !(k in enAll)).concat(Object.keys(enAll).filter(k => contentId(k) && !(k in zh)));
  assert(!oneSided.length, `i18n parity: content keys in only one of zh-CN / en: ${fewKeys(oneSided)}`);

  // remake blocks (only when the remake changed the tables). A port string the remake replaced leaves `portKeys` (it no
  // longer comes from the exporter's PORT_STRINGS) and is listed in `replacedPortKeys` instead.
  const files = ['i18n_zh-CN.json', 'i18n_en.json', 'i18n_zh-TW.json'];
  if (remakeKeys.length) {
    for (const f of files) {
      const src = f === 'i18n_en.json' ? REMAKE_SOURCE.en : REMAKE_SOURCE.zhCN;
      noteExtra(f, src, 'stringsReplacedWeb', replacedWeb.length);
      noteExtra(f, src, 'stringsReplacedPort', replacedPort.length);
      if (replacedPort.length) noteExtra(f, src, 'replacedPortKeys', replacedPort);
      noteExtra(f, src, 'stringsAdded', remakeKeys.length - replacedWeb.length - replacedPort.length);
    }
  }
  // Swept keys: content the remake removed (redirected modules / overrides), listed for review.
  if (swept.length) for (const f of files) noteExtra(f, 'unreal/GameData', 'swept', swept);

  const portKeys = Object.fromEntries(Object.entries(PORT_STRINGS).filter(([k]) => !(k in remakeZhCN) && k in zh)
    .map(([k, v]) => [k, v.decision]));
  const zhTW: Record<string, string> = {};
  let supplemented = 0;
  for (const [k, v] of Object.entries(zh)) {
    zhTW[k] = k in remakeZhCN ? remakeToTraditional(v) : convertToTraditional(v);
    if (zhTW[k] !== convertToTraditional(v)) supplemented += 1;
  }
  if (remakeKeys.length) {
    noteExtra('i18n_zh-TW.json', ZH_TW_SOURCE, 'zhTWSupplement',
      { keys: remakeKeys.length, changed: supplemented, chars: Object.keys(ZH_TW_CHARS).length,
        phrases: PHRASES_LONGEST_FIRST.length });
  }
  const zhKeys = new Set(Object.keys(zh));
  const enKeys = new Set(Object.keys(enAll));
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
