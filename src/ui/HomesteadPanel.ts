/**
 * Homestead panel (H) — the Ember Tower (余烬之塔): embers, the way home, and
 * one page per wing: buildings (upgrades), herb garden, gem workshop, caravan
 * post, altar. UIScene owns the container (frame, title, close button) and
 * calls `buildHomesteadPanel` to fill it; every action goes through the
 * zone's EmberTower, and `reopen` redraws the panel after a change.
 */
import Phaser from 'phaser';
import { t } from '../i18n';
import { getBuildingDesc, getBuildingName, getItemBaseName, getPetName, getQuestName } from '../i18n/gameAccessors';
import {
  BLESSINGS, EXPEDITION_OPTIONS, GEM_COMBINE_COUNT, blessingCost, blessingStats,
  gardenCapacity, gardenInterval, gemCombineGold, maxCombineTier, nextGemId,
} from '../data/homestead';
import { GEM_STAT_MAP } from '../data/items/bases';
import { AllQuests } from '../data/quests/all_quests';
import type { HomesteadSystem } from '../systems/HomesteadSystem';
import type { EmberTower, HomesteadPage } from '../systems/EmberTower';
import { addSectionHeader, drawBarFill, drawCard, drawWell, pipTexture, UI_COLORS, UI_FONT, type ButtonOptions, type UiButton } from './UiKit';

export const HOMESTEAD_PAGES: readonly HomesteadPage[] = ['buildings', 'garden', 'workshop', 'caravan', 'altar'];

/** Wing whose unlock opens each page (buildings is always open). */
const PAGE_WING: Record<HomesteadPage, string | null> = {
  buildings: null, garden: 'herb_garden', workshop: 'gem_workshop', caravan: 'training_ground', altar: 'altar',
};

type IconDrawer = (g: Phaser.GameObjects.Graphics, cx: number, cy: number, s: number, level: number, maxLevel: number) => void;

export interface HomesteadPanelKit {
  scene: Phaser.Scene;
  panel: Phaser.GameObjects.Container;
  pw: number;
  ph: number;
  px: (n: number) => number;
  fs: (n: number) => string;
  button: (x: number, y: number, w: number, h: number, label: string, onClick: () => void, opts?: Omit<ButtonOptions, 'onClick'>) => UiButton;
  buildingIcons: Record<string, IconDrawer>;
  homestead: HomesteadSystem;
  tower: EmberTower | null;
  player: { gold: number; level: number };
  touch: boolean;
  reopen: (page: HomesteadPage) => void;
}

const EMBER = '#ff9a4a';

function fmtTime(ms: number): string {
  const s = Math.max(0, Math.ceil(ms / 1000));
  return `${Math.floor(s / 60)}:${String(s % 60).padStart(2, '0')}`;
}

function statLine(stats: Record<string, number>): string {
  return Object.entries(stats).map(([k, v]) => t(`homestead.stat.${k}`, { v })).join('  ');
}

export function buildHomesteadPanel(kit: HomesteadPanelKit, page: HomesteadPage): void {
  const { scene, panel, pw, px, fs, homestead: hs } = kit;
  const tower = hs.tower;
  const text = (x: number, y: number, s: string, size: number, color: string, opts: Phaser.Types.GameObjects.Text.TextStyle = {}) => {
    const o = scene.add.text(x, y, s, { fontSize: fs(size), color, fontFamily: UI_FONT, ...opts });
    panel.add(o);
    return o;
  };
  const rowX = px(16), rowW = pw - px(32);

  // ── Header strip: embers + the way home ──
  const topY = px(50);
  const chip = scene.add.graphics();
  drawCard(chip, rowX, topY, px(150), px(28), { border: 0x8a4a22, strip: 0xff8a3a });
  panel.add(chip);
  text(rowX + px(12), topY + px(14), t('homestead.panel.embers', { n: tower.embers }), 13, EMBER, { fontStyle: 'bold' }).setOrigin(0, 0.5);

  const rightX = rowX + rowW;
  if (!tower.towerUnlocked) {
    const hint = text(rightX, topY + px(14), t('homestead.panel.lockedHint', { quest: getQuestName('q_explore_goblin_camp', '篝火营地') }), 11, UI_COLORS.muted);
    hint.setOrigin(1, 0.5);
    if (hint.width > rowW - px(170)) hint.setScale((rowW - px(170)) / hint.width, 1);
  } else if (kit.tower?.inTower) {
    text(rightX - px(140), topY + px(14), t('homestead.panel.inTower'), 11, '#9fd8ff').setOrigin(1, 0.5);
    panel.add(kit.button(rightX - px(64), topY + px(14), px(124), px(26), t('homestead.panel.leave'), () => {
      kit.tower?.leaveTower();
      kit.reopen(page);
    }, { variant: 'secondary', fontSize: 11 }));
  } else {
    const block = kit.tower?.enterBlock() ?? 'unsafe';
    if (block) text(rightX - px(148), topY + px(14), t(`homestead.enter.${block}`), 10, UI_COLORS.muted).setOrigin(1, 0.5);
    panel.add(kit.button(rightX - px(68), topY + px(14), px(132), px(26), t('homestead.panel.goHome'), () => {
      if (kit.tower?.enterTower()) kit.reopen(page);
    }, { variant: 'primary', fontSize: 11, disabled: !!block }));
  }

  // ── Tabs ──
  const tabY = topY + px(44);
  const tabW = (rowW - px(4) * (HOMESTEAD_PAGES.length - 1)) / HOMESTEAD_PAGES.length;
  HOMESTEAD_PAGES.forEach((p, i) => {
    const wing = PAGE_WING[p];
    const open = !wing || tower.isBuildingUnlocked(wing);
    panel.add(kit.button(rowX + tabW / 2 + i * (tabW + px(4)), tabY, tabW, px(26), t(`homestead.tab.${p}`), () => kit.reopen(p), {
      variant: p === page ? 'primary' : open ? 'secondary' : 'ghost', fontSize: 11,
    }));
  });

  const bodyY = tabY + px(24);
  const wing = PAGE_WING[page];
  if (wing && !tower.isBuildingUnlocked(wing)) {
    const def = hs.getBuildingDef(wing);
    const q = def?.unlockQuest ? AllQuests.find(x => x.id === def.unlockQuest) : undefined;
    text(pw / 2, bodyY + px(90), t('homestead.panel.wingLocked', {
      name: getBuildingName(wing, def?.name ?? wing),
      quest: q ? getQuestName(q.id, q.name) : '',
    }), 12, UI_COLORS.muted, { align: 'center', wordWrap: { width: rowW - px(60), useAdvancedWrap: true } }).setOrigin(0.5, 0);
  } else {
    switch (page) {
      case 'buildings': buildingsPage(kit, bodyY); break;
      case 'garden': gardenPage(kit, bodyY); break;
      case 'workshop': workshopPage(kit, bodyY); break;
      case 'caravan': caravanPage(kit, bodyY); break;
      case 'altar': altarPage(kit, bodyY); break;
    }
  }

  text(pw / 2, kit.ph - px(16), t(kit.touch ? 'ui.homestead.footerTouch' : 'ui.homestead.footer'), 11, UI_COLORS.muted).setOrigin(0.5);
}

function header(kit: HomesteadPanelKit, y: number, label: string): void {
  kit.panel.add(addSectionHeader(kit.scene, kit.px(16), y, kit.pw - kit.px(32), label));
}

/** "Only in the tower" note for pages whose actions need the wing at hand. */
function towerOnlyNote(kit: HomesteadPanelKit, y: number): boolean {
  if (kit.tower?.inTower) return false;
  const o = kit.scene.add.text(kit.pw / 2, y, t('homestead.panel.towerOnly'), {
    fontSize: kit.fs(11), color: '#9fd8ff', fontFamily: UI_FONT,
  }).setOrigin(0.5);
  kit.panel.add(o);
  return true;
}

// ── Buildings ──────────────────────────────────────────────────────────────

function buildingsPage(kit: HomesteadPanelKit, y0: number): void {
  const { scene, panel, px, fs, homestead: hs } = kit;
  const rowX = px(16), rowW = kit.pw - px(32);
  header(kit, y0 + px(14), t('ui.homestead.buildingsHeader'));
  const cardH = px(54), gap = px(5), icon = px(40);
  hs.getAllBuildings().forEach((b, i) => {
    const sy = y0 + px(28) + i * (cardH + gap);
    const lv = hs.getBuildingLevel(b.id);
    const unlocked = hs.tower.isBuildingUnlocked(b.id);
    const maxed = lv >= b.maxLevel;
    const cost = hs.getUpgradeCost(b.id);
    const g = scene.add.graphics();
    drawCard(g, rowX, sy, rowW, cardH, maxed ? { fill: 0x241d12, border: 0xd4a54a, strip: 0xd4a54a } : { border: unlocked ? 0x3f3845 : 0x2a262c });
    const ix = rowX + px(10), iy = sy + (cardH - icon) / 2;
    drawWell(g, ix, iy, icon, icon, px(4), maxed ? 0xd4a54a : 0x4a4250);
    panel.add(g);
    const draw = kit.buildingIcons[b.id];
    if (draw) {
      const ig = scene.add.graphics();
      draw(ig, ix + icon / 2, iy + icon / 2, icon, lv, b.maxLevel);
      if (!unlocked) ig.setAlpha(0.35);
      panel.add(ig);
    }
    const tx = ix + icon + px(12);
    const textMaxW = rowW - (tx - rowX) - px(132);
    panel.add(scene.add.text(tx, sy + px(6), getBuildingName(b.id, b.name), {
      fontSize: fs(13), color: !unlocked ? UI_COLORS.dim : maxed ? UI_COLORS.goldBright : UI_COLORS.text, fontFamily: UI_FONT, fontStyle: 'bold',
    }));
    const q = b.unlockQuest ? AllQuests.find(x => x.id === b.unlockQuest) : undefined;
    const desc = unlocked ? getBuildingDesc(b.id, b.description) : t('homestead.panel.unlockBy', { quest: q ? getQuestName(q.id, q.name) : '' });
    const d = scene.add.text(tx, sy + px(24), desc, { fontSize: fs(10), color: unlocked ? UI_COLORS.muted : '#b08a60', fontFamily: UI_FONT });
    if (d.width > textMaxW) d.setScale(textMaxW / d.width, 1);
    panel.add(d);
    const pipY = sy + px(43), pipGap = px(11);
    const on = pipTexture(scene, px(8), maxed ? 0xffd98a : 0x6fd35a);
    const off = pipTexture(scene, px(8), null);
    for (let p = 0; p < b.maxLevel; p++) panel.add(scene.add.image(tx + px(4) + p * pipGap, pipY, p < lv ? on : off));
    panel.add(scene.add.text(tx + b.maxLevel * pipGap + px(4), pipY, `Lv.${lv}/${b.maxLevel}`, {
      fontSize: fs(10), color: maxed ? UI_COLORS.goldBright : UI_COLORS.muted, fontFamily: UI_FONT, fontStyle: 'bold',
    }).setOrigin(0, 0.5));
    const bx = rowX + rowW - px(62), by = sy + cardH / 2;
    if (maxed) {
      panel.add(scene.add.text(bx, by, t('ui.homestead.maxLevel'), { fontSize: fs(11), color: UI_COLORS.goldBright, fontFamily: UI_FONT, fontStyle: 'bold' }).setOrigin(0.5));
    } else if (cost) {
      const label = cost.embers > 0
        ? t('homestead.panel.upgradeCost', { gold: cost.gold, embers: cost.embers })
        : t('ui.homestead.upgrade', { cost: String(cost.gold) });
      panel.add(kit.button(bx, by, px(112), px(30), label, () => {
        if (kit.tower?.upgradeBuilding(b.id)) kit.reopen('buildings');
      }, { variant: 'success', fontSize: 10, disabled: !hs.canUpgrade(b.id, kit.player.gold) }));
    }
  });
}

// ── Herb garden ────────────────────────────────────────────────────────────

function gardenPage(kit: HomesteadPanelKit, y0: number): void {
  const { scene, panel, px, fs, homestead: hs } = kit;
  const tower = hs.tower;
  const rowX = px(16), rowW = kit.pw - px(32);
  const lv = hs.getBuildingLevel('herb_garden');
  header(kit, y0 + px(14), t('homestead.garden.header', { level: lv }));
  const info = [
    t('homestead.garden.rate', { n: gardenInterval(lv) }),
    t('homestead.garden.capacity', { n: tower.gardenStockCount(), max: gardenCapacity(lv) }),
  ];
  info.forEach((s, i) => panel.add(scene.add.text(rowX + px(6), y0 + px(34) + i * px(18), s, { fontSize: fs(11), color: UI_COLORS.textSoft, fontFamily: UI_FONT })));
  // Progress toward the next yield.
  const barY = y0 + px(76), barW = rowW - px(12);
  const g = scene.add.graphics();
  drawWell(g, rowX + px(6), barY, barW, px(10), px(3));
  drawBarFill(g, rowX + px(7), barY + 1, Math.round((barW - 2) * Math.min(1, tower.garden.progress / gardenInterval(lv))), px(8), 0x6fd35a);
  panel.add(g);

  header(kit, barY + px(28), t('homestead.garden.stock'));
  const stock = Object.entries(tower.garden.stock);
  if (stock.length === 0) {
    panel.add(scene.add.text(kit.pw / 2, barY + px(58), t('homestead.garden.empty'), { fontSize: fs(11), color: UI_COLORS.dim, fontFamily: UI_FONT }).setOrigin(0.5));
  }
  stock.forEach(([id, n], i) => {
    panel.add(scene.add.text(rowX + px(12) + (i % 2) * (rowW / 2), barY + px(46) + Math.floor(i / 2) * px(20), `${getItemBaseName(id)} ×${n}`, {
      fontSize: fs(12), color: id === 'c_ley_fruit' ? '#9dff8a' : UI_COLORS.text, fontFamily: UI_FONT,
    }));
  });
  const btnY = kit.ph - px(64);
  if (!towerOnlyNote(kit, btnY)) {
    panel.add(kit.button(kit.pw / 2, btnY, px(160), px(32), t('homestead.garden.harvest'), () => {
      kit.tower?.harvestGarden();
      kit.reopen('garden');
    }, { variant: 'success', disabled: stock.length === 0 }));
  }
}

// ── Gem workshop ───────────────────────────────────────────────────────────

function workshopPage(kit: HomesteadPanelKit, y0: number): void {
  const { scene, panel, px, fs, homestead: hs } = kit;
  const rowX = px(16), rowW = kit.pw - px(32);
  const lv = hs.getBuildingLevel('gem_workshop');
  header(kit, y0 + px(14), t('homestead.workshop.header', { tier: maxCombineTier(lv) }));
  panel.add(scene.add.text(rowX + px(6), y0 + px(32), t('homestead.workshop.rule', { n: GEM_COMBINE_COUNT }), { fontSize: fs(11), color: UI_COLORS.textSoft, fontFamily: UI_FONT }));
  if (towerOnlyNote(kit, y0 + px(60))) return;
  const counts = kit.tower?.gemCounts() ?? {};
  const gems = Object.keys(counts).filter(id => nextGemId(id)).sort((a, b) => GEM_STAT_MAP[a].tier - GEM_STAT_MAP[b].tier || a.localeCompare(b));
  if (gems.length === 0) {
    panel.add(scene.add.text(kit.pw / 2, y0 + px(80), t('homestead.workshop.none'), { fontSize: fs(11), color: UI_COLORS.dim, fontFamily: UI_FONT }).setOrigin(0.5));
    return;
  }
  const rowH = px(34);
  gems.slice(0, 9).forEach((id, i) => {
    const sy = y0 + px(52) + i * (rowH + px(4));
    const next = nextGemId(id)!;
    const g = scene.add.graphics();
    drawCard(g, rowX, sy, rowW, rowH, { border: 0x3f3845 });
    panel.add(g);
    panel.add(scene.add.text(rowX + px(10), sy + rowH / 2, `${getItemBaseName(id)} ×${counts[id]}  →  ${getItemBaseName(next)}`, {
      fontSize: fs(11), color: UI_COLORS.text, fontFamily: UI_FONT,
    }).setOrigin(0, 0.5));
    const block = kit.tower?.gemBlock(id) ?? 'workshop';
    if (block && block !== 'gold' && block !== 'count') {
      panel.add(scene.add.text(rowX + rowW - px(10), sy + rowH / 2, t(`homestead.workshop.block.${block}`), { fontSize: fs(10), color: UI_COLORS.muted, fontFamily: UI_FONT }).setOrigin(1, 0.5));
      return;
    }
    panel.add(kit.button(rowX + rowW - px(62), sy + rowH / 2, px(112), px(26), t('homestead.workshop.combine', { gold: gemCombineGold(GEM_STAT_MAP[next].tier) }), () => {
      if (kit.tower?.combineGem(id)) kit.reopen('workshop');
    }, { variant: 'success', fontSize: 10, disabled: !!block }));
  });
}

// ── Caravan post ───────────────────────────────────────────────────────────

function caravanPage(kit: HomesteadPanelKit, y0: number): void {
  const { scene, panel, px, fs, homestead: hs } = kit;
  const tower = hs.tower;
  const rowX = px(16), rowW = kit.pw - px(32);
  header(kit, y0 + px(14), t('homestead.caravan.header'));
  panel.add(scene.add.text(rowX + px(6), y0 + px(32), t('homestead.caravan.rule'), {
    fontSize: fs(11), color: UI_COLORS.textSoft, fontFamily: UI_FONT, wordWrap: { width: rowW - px(12), useAdvancedWrap: true },
  }));
  const e = tower.expedition;
  if (e) {
    const g = scene.add.graphics();
    drawCard(g, rowX, y0 + px(72), rowW, px(66), { border: 0xd4a54a, strip: 0xd4a54a });
    panel.add(g);
    panel.add(scene.add.text(rowX + px(14), y0 + px(82), t('homestead.caravan.away', { name: getPetName(e.petId, e.petId), option: t(`homestead.expedition.${e.optionId}`) }), {
      fontSize: fs(12), color: UI_COLORS.text, fontFamily: UI_FONT, fontStyle: 'bold',
    }));
    const status = tower.expeditionDone
      ? t('homestead.caravan.back')
      : t('homestead.caravan.progress', { kills: e.kills, need: e.killsRequired, time: fmtTime(e.remainingMs) });
    panel.add(scene.add.text(rowX + px(14), y0 + px(108), status, { fontSize: fs(11), color: tower.expeditionDone ? '#9dff8a' : UI_COLORS.muted, fontFamily: UI_FONT }));
    if (tower.expeditionDone && !towerOnlyNote(kit, y0 + px(160))) {
      panel.add(kit.button(kit.pw / 2, y0 + px(166), px(160), px(32), t('homestead.caravan.claim'), () => {
        kit.tower?.claimExpedition();
        kit.reopen('caravan');
      }, { variant: 'success' }));
    }
    return;
  }
  if (towerOnlyNote(kit, y0 + px(84))) return;
  const idle = kit.tower?.idlePets() ?? [];
  if (idle.length === 0) {
    panel.add(scene.add.text(kit.pw / 2, y0 + px(96), t('homestead.caravan.noPets'), {
      fontSize: fs(11), color: UI_COLORS.dim, fontFamily: UI_FONT, align: 'center', wordWrap: { width: rowW - px(40), useAdvancedWrap: true },
    }).setOrigin(0.5, 0));
    return;
  }
  const rowH = px(36);
  idle.slice(0, 8).forEach((petId, i) => {
    const sy = y0 + px(72) + i * (rowH + px(4));
    const g = scene.add.graphics();
    drawCard(g, rowX, sy, rowW, rowH, { border: 0x3f3845 });
    panel.add(g);
    panel.add(scene.add.text(rowX + px(12), sy + rowH / 2, getPetName(petId, petId), { fontSize: fs(12), color: UI_COLORS.text, fontFamily: UI_FONT, fontStyle: 'bold' }).setOrigin(0, 0.5));
    EXPEDITION_OPTIONS.forEach((opt, j) => {
      const label = t('homestead.caravan.send', { option: t(`homestead.expedition.${opt.id}`), kills: opt.killsRequired, min: Math.round(opt.durationMs / 60000) });
      panel.add(kit.button(rowX + rowW - px(76) - (EXPEDITION_OPTIONS.length - 1 - j) * px(150), sy + rowH / 2, px(144), px(26), label, () => {
        if (kit.tower?.sendExpedition(petId, opt.id)) kit.reopen('caravan');
      }, { variant: j === 0 ? 'secondary' : 'primary', fontSize: 10 }));
    });
  });
}

// ── Altar ──────────────────────────────────────────────────────────────────

function altarPage(kit: HomesteadPanelKit, y0: number): void {
  const { scene, panel, px, fs, homestead: hs } = kit;
  const tower = hs.tower;
  const rowX = px(16), rowW = kit.pw - px(32);
  const lv = hs.getBuildingLevel('altar');
  header(kit, y0 + px(14), t('homestead.altar.header', { cost: blessingCost(lv) }));
  panel.add(scene.add.text(rowX + px(6), y0 + px(32), t('homestead.altar.rule'), {
    fontSize: fs(11), color: UI_COLORS.textSoft, fontFamily: UI_FONT, wordWrap: { width: rowW - px(12), useAdvancedWrap: true },
  }));
  const b = tower.blessing;
  panel.add(scene.add.text(rowX + px(6), y0 + px(70), b
    ? t('homestead.altar.active', { name: t(`homestead.blessing.${b.id}.name`), time: fmtTime(b.remainingMs) })
    : t('homestead.altar.none'), { fontSize: fs(12), color: b ? '#ffd98a' : UI_COLORS.dim, fontFamily: UI_FONT, fontStyle: 'bold' }));
  const inTower = !towerOnlyNote(kit, kit.ph - px(56));
  const rowH = px(52);
  BLESSINGS.forEach((def, i) => {
    const sy = y0 + px(96) + i * (rowH + px(5));
    const g = scene.add.graphics();
    const active = b?.id === def.id;
    drawCard(g, rowX, sy, rowW, rowH, active ? { fill: 0x2a1a10, border: 0xff9a4a, strip: 0xff9a4a } : { border: 0x3f3845 });
    panel.add(g);
    panel.add(scene.add.text(rowX + px(12), sy + px(8), t(`homestead.blessing.${def.id}.name`), { fontSize: fs(13), color: EMBER, fontFamily: UI_FONT, fontStyle: 'bold' }));
    panel.add(scene.add.text(rowX + px(12), sy + px(29), statLine(blessingStats(def.id, Math.max(1, lv)) as Record<string, number>), { fontSize: fs(11), color: UI_COLORS.textSoft, fontFamily: UI_FONT }));
    if (inTower) {
      panel.add(kit.button(rowX + rowW - px(62), sy + rowH / 2, px(112), px(28), t('homestead.altar.buy', { n: blessingCost(lv) }), () => {
        if (kit.tower?.buyBlessing(def.id)) kit.reopen('altar');
      }, { variant: 'primary', fontSize: 10, disabled: !!tower.blessingBlock(def.id) }));
    }
  });
}
