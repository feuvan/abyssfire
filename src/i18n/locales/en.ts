/**
 * English locale.
 * Final fallback in the fallback chain: zh-TW → zh-CN → en → key path.
 */
import type { LocaleData } from '../types';

const en: LocaleData = {
  // ─── Boot Scene ───
  'boot.title': 'Abyssfire',
  'boot.subtitle': 'A B Y S S F I R E',
  'boot.loading': 'Forging Abyssfire...',
  'boot.ready': 'Ready!',

  // ─── Menu Scene — Title ───
  'menu.title': 'ABYSSFIRE',
  'menu.subtitle': 'Abyssfire',

  // ─── Menu Scene — Main Menu Buttons ───
  'menu.newGame': 'New Journey',
  'menu.continue': 'Continue - {class} Lv.{level}',
  'menu.continueSubtitle': 'Continue your adventure',
  'menu.help': 'Controls',
  'menu.ost': 'Soundtrack',
  'menu.credits': 'Credits',
  'menu.language': 'Language',
  'menu.back': '← Back',
  'menu.backShort': 'Back',

  // ─── Menu Scene — Class Selection ───
  'menu.classSelect.title': 'Choose a Class',
  'menu.classSelect.confirm': 'Start Adventure',
  'menu.classSelect.warrior.name': 'Warrior',
  'menu.classSelect.warrior.desc': 'Iron will, peerless with sword and shield',
  'menu.classSelect.mage.name': 'Mage',
  'menu.classSelect.mage.desc': 'Arcane might, devastating destruction',
  'menu.classSelect.rogue.name': 'Rogue',
  'menu.classSelect.rogue.desc': 'Shadow stealth, lethal precision',

  // ─── Menu Scene — Help Panel ───
  'menu.helpPanel.title': 'Keyboard Shortcuts',
  'menu.helpPanel.cat.movement': 'Movement',
  'menu.helpPanel.cat.combat': 'Combat',
  'menu.helpPanel.cat.ui': 'Interface',
  'menu.helpPanel.movement.wasd': 'Move up / left / down / right',
  'menu.helpPanel.movement.mouse': 'Click to move / attack / interact',
  'menu.helpPanel.combat.skills': 'Use skills',
  'menu.helpPanel.combat.autoCombat': 'Toggle auto-combat',
  'menu.helpPanel.combat.teleport': 'Teleport to camp',
  'menu.helpPanel.ui.inventory': 'Inventory',
  'menu.helpPanel.ui.character': 'Character Stats',
  'menu.helpPanel.ui.skillTree': 'Skill Tree',
  'menu.helpPanel.ui.questLog': 'Quest Log',
  'menu.helpPanel.ui.map': 'Map',
  'menu.helpPanel.ui.homestead': 'Homestead',
  'menu.helpPanel.ui.audio': 'Audio Settings',
  'menu.helpPanel.ui.escape': 'Return to Main Menu',

  // ─── Menu Scene — Jukebox Panel ───
  'menu.jukebox.header': 'ABYSSFIRE OST',
  'menu.jukebox.subtitle': 'Soundtrack · {count} Tracks · {duration}',
  'menu.jukebox.track.menu': 'Abyssfire · Prologue',
  'menu.jukebox.track.emerald_plains.explore': 'Emerald Plains · Explore',
  'menu.jukebox.track.emerald_plains.combat': 'Emerald Plains · Combat',
  'menu.jukebox.track.twilight_forest.explore': 'Twilight Forest · Explore',
  'menu.jukebox.track.twilight_forest.combat': 'Twilight Forest · Combat',
  'menu.jukebox.track.anvil_mountains.explore': 'Anvil Mountains · Explore',
  'menu.jukebox.track.anvil_mountains.combat': 'Anvil Mountains · Combat',
  'menu.jukebox.track.scorching_desert.explore': 'Scorching Desert · Explore',
  'menu.jukebox.track.scorching_desert.combat': 'Scorching Desert · Combat',
  'menu.jukebox.track.abyss_rift.explore': 'Abyss Rift · Explore',
  'menu.jukebox.track.abyss_rift.combat': 'Abyss Rift · Combat',

  // ─── Menu Scene — Credits Panel ───
  'menu.creditsPanel.title': 'Credits',
  'menu.creditsPanel.tileArt': 'Tile Art',
  'menu.creditsPanel.tileArt.license': 'License: Creative Commons Zero (CC0)',
  'menu.creditsPanel.bgm': 'Background Music',
  'menu.creditsPanel.bgm.source': 'All from OpenGameArt.org',
  'menu.creditsPanel.engine': 'Game Engine',

  // ─── Menu Scene — Difficulty Selector ───
  'menu.difficulty.title': 'Select Difficulty',
  'menu.difficulty.normal': 'Normal',
  'menu.difficulty.nightmare': 'Nightmare',
  'menu.difficulty.hell': 'Hell',
  'menu.difficulty.desc.normal': 'Standard difficulty',
  'menu.difficulty.desc.nightmare': 'Monster damage ×1.5, EXP ×2',
  'menu.difficulty.desc.hell': 'Monster damage ×2, EXP ×3',
  'menu.difficulty.current': 'Current',
  'menu.difficulty.locked': 'Locked — Complete the previous difficulty',

  // ─── Language Selector ───
  'menu.langSelect.zhCN': '简体中文',
  'menu.langSelect.zhTW': '繁體中文',
  'menu.langSelect.en': 'English',

  // ─── Test-only keys (for fallback testing) ───
  'test.enOnly': 'This key only exists in English',
};

export default en;
