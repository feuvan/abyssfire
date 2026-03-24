/**
 * Lore collectibles — interactable objects placed throughout zones.
 * Each zone has ≥3 lore objects with distinct visuals and Chinese lore text (≥50 chars).
 * At least half are placed in non-obvious exploration-rewarding locations.
 */

export interface LoreEntry {
  id: string;
  /** Zone map ID. */
  zone: string;
  /** Display name (Chinese). */
  name: string;
  /** Lore text shown on interaction (Chinese, ≥50 chars). */
  text: string;
  /** Tile position. */
  col: number;
  row: number;
  /** Visual sprite type for procedural generation. */
  spriteType: 'ancient_tablet' | 'old_scroll' | 'crystal_shard' | 'carved_stone' | 'torn_journal' | 'rune_pillar';
  /** Whether this is in a non-obvious / hidden location. */
  hidden: boolean;
}

// ─── Zone 1: Emerald Plains (翡翠平原) ─────────────────────────────────
const emeraldPlainsLore: LoreEntry[] = [
  {
    id: 'lore_ep_01',
    zone: 'emerald_plains',
    name: '古老石碑',
    text: '这块石碑上刻着古老的文字："翡翠平原曾是精灵族的圣地，千年前的大灾变摧毁了他们的文明。灵脉之力散布于大地，吸引了各种生灵前来。"石碑的底部还残留着精灵族的纹章。',
    col: 45,
    row: 35,
    spriteType: 'ancient_tablet',
    hidden: false,
  },
  {
    id: 'lore_ep_02',
    zone: 'emerald_plains',
    name: '褪色日记',
    text: '一本被风雨侵蚀的日记本，字迹模糊但仍可辨认："第七日，哥布林部落的异变越来越严重。它们的萨满似乎在进行某种古老的仪式，试图唤醒沉睡在平原地下的灵脉之力。如果成功，后果不堪设想……"',
    col: 105,
    row: 15,
    spriteType: 'torn_journal',
    hidden: true,
  },
  {
    id: 'lore_ep_03',
    zone: 'emerald_plains',
    name: '水晶碎片',
    text: '一块散发着微弱光芒的水晶碎片。它的内部似乎封印着某种影像——你看到了远古精灵族在平原上建造高塔、与自然和谐共生的画面。水晶的裂痕中渗出淡淡的绿色光芒，那是灵脉残余的能量。',
    col: 15,
    row: 108,
    spriteType: 'crystal_shard',
    hidden: true,
  },
  {
    id: 'lore_ep_04',
    zone: 'emerald_plains',
    name: '雕刻石柱',
    text: '一根半埋在泥土中的石柱，上面雕刻着精灵族的历史。根据这些浮雕，翡翠平原的灵脉曾经是连接五大区域的能量枢纽。大灾变后灵脉断裂，各区域从此走向不同的命运。',
    col: 85,
    row: 80,
    spriteType: 'carved_stone',
    hidden: false,
  },
];

// ─── Zone 2: Twilight Forest (暮色森林) ────────────────────────────────
const twilightForestLore: LoreEntry[] = [
  {
    id: 'lore_tf_01',
    zone: 'twilight_forest',
    name: '树灵遗刻',
    text: '这棵古树的树皮上刻着树灵族的文字："暮色森林的永夜并非自然现象。数百年前，一位强大的巫师为了追求永生，将自己与森林的生命之树融为一体，却意外释放了黑暗能量，让整片森林永坠暮光。"',
    col: 50,
    row: 65,
    spriteType: 'carved_stone',
    hidden: false,
  },
  {
    id: 'lore_tf_02',
    zone: 'twilight_forest',
    name: '符文石柱',
    text: '一根覆满苔藓的符文石柱，散发着幽暗的光芒。石柱上的符文记载："封印暗影的关键在于森林深处的三座祭坛。当三座祭坛同时点亮时，黑暗将被驱散，森林将重现光明。"然而祭坛的位置已被刻意抹去。',
    col: 109,
    row: 24,
    spriteType: 'rune_pillar',
    hidden: true,
  },
  {
    id: 'lore_tf_03',
    zone: 'twilight_forest',
    name: '亡灵手札',
    text: '一本被黑暗能量侵蚀的手札："我是最后一个目睹森林变化的人类。暗影织者并非天生的恶物——她曾是守护森林的精灵，被黑暗力量腐化后变成了如今的模样。也许，净化她才是拯救森林的真正方法。"',
    col: 25,
    row: 105,
    spriteType: 'old_scroll',
    hidden: true,
  },
  {
    id: 'lore_tf_04',
    zone: 'twilight_forest',
    name: '月光碎片',
    text: '一块在暮色中散发着银白光芒的碎片。传说这是远古月亮女神留下的遗物，能够驱散黑暗。碎片表面映射出森林曾经的模样——阳光穿透茂密的树叶，鸟儿在枝头歌唱，一片生机盎然。',
    col: 80,
    row: 89,
    spriteType: 'crystal_shard',
    hidden: false,
  },
];

// ─── Zone 3: Anvil Mountains (铁砧山脉) ────────────────────────────────
const anvilMountainsLore: LoreEntry[] = [
  {
    id: 'lore_am_01',
    zone: 'anvil_mountains',
    name: '矮人铭文',
    text: '这块金属板上刻着精密的矮人铭文："铁砧山脉第七层的大熔炉中，矮人王布鲁恩铸造了传说中的命运之锤。那把锤子据说能够锻造出世间万物，甚至可以重塑灵脉的断裂之处。然而大灾变后，命运之锤就此失踪。"',
    col: 45,
    row: 30,
    spriteType: 'ancient_tablet',
    hidden: false,
  },
  {
    id: 'lore_am_02',
    zone: 'anvil_mountains',
    name: '锻造日志',
    text: '一本矮人锻造大师的日志："第三百二十一日。铁甲守卫的核心已经完成，但我发现一个致命的缺陷——守卫只会执行最后一道命令，无法区分敌友。如果建造者陨落，守卫将永远执行最后的指令……攻击一切。"',
    col: 100,
    row: 40,
    spriteType: 'torn_journal',
    hidden: true,
  },
  {
    id: 'lore_am_03',
    zone: 'anvil_mountains',
    name: '宝石原矿',
    text: '一块蕴含着多种宝石的原矿石。矮人族相信，宝石是大地的眼泪，每一颗都封印着远古的记忆。这块原矿中似乎封存着铁砧山脉建城的画面——数千矮人齐心协力，在山腹中开凿出宏伟的地下都市。',
    col: 20,
    row: 110,
    spriteType: 'crystal_shard',
    hidden: true,
  },
  {
    id: 'lore_am_04',
    zone: 'anvil_mountains',
    name: '石像碑记',
    text: '一尊倒塌的矮人战士石像旁的碑文："献给铁砧山脉最后的守护者们。当石像鬼占据山脉上层时，是他们用生命为平民争取了撤退的时间。愿铁锤与铁砧永远铭记他们的牺牲。——幸存者联盟立"',
    col: 75,
    row: 15,
    spriteType: 'carved_stone',
    hidden: false,
  },
];

// ─── Zone 4: Scorching Desert (灼热沙漠) ──────────────────────────────
const scorchingDesertLore: LoreEntry[] = [
  {
    id: 'lore_sd_01',
    zone: 'scorching_desert',
    name: '沙漠壁画',
    text: '一面被风沙半掩的古老壁画。画面描绘了一个繁荣的沙漠王国——高耸的尖塔、流淌的绿洲、身着华服的贵族。壁画的最后一幅显示大地裂开，火焰从裂缝中喷涌而出，整个王国在一夜之间化为废墟。',
    col: 50,
    row: 25,
    spriteType: 'ancient_tablet',
    hidden: false,
  },
  {
    id: 'lore_sd_02',
    zone: 'scorching_desert',
    name: '祭司卷轴',
    text: '一卷勉强保存完好的羊皮卷轴："火焰裂隙并非天灾，而是人祸。沙漠王国的大祭司为了获得永恒的力量，打破了封印深渊的第四道结界。裂隙从此将地狱之火引入人间，王国灭亡，大祭司则堕落为不死的亡灵。"',
    col: 108,
    row: 60,
    spriteType: 'old_scroll',
    hidden: true,
  },
  {
    id: 'lore_sd_03',
    zone: 'scorching_desert',
    name: '凤凰羽化石',
    text: '一块罕见的凤凰羽化石，至今仍散发着灼热的温度。传说凤凰是火焰裂隙中诞生的神兽，它们既是毁灭的化身，也是重生的象征。沙漠游牧民相信，只要有凤凰存在，这片沙漠就还有希望重现生机。',
    col: 15,
    row: 100,
    spriteType: 'crystal_shard',
    hidden: true,
  },
  {
    id: 'lore_sd_04',
    zone: 'scorching_desert',
    name: '绿洲遗迹碑',
    text: '一座矗立在干涸绿洲边的石碑："此处曾是沙漠王国的皇家花园。清泉从地下涌出，养育了万物。当火焰裂隙撕裂大地后，地下水脉被烧干，花园化为一片焦土。——游牧民历史学者记"',
    col: 90,
    row: 105,
    spriteType: 'carved_stone',
    hidden: false,
  },
];

// ─── Zone 5: Abyss Rift (深渊裂隙) ────────────────────────────────────
const abyssRiftLore: LoreEntry[] = [
  {
    id: 'lore_ar_01',
    zone: 'abyss_rift',
    name: '深渊碑文',
    text: '一块被暗紫色能量包裹的石碑："深渊裂隙是连接人间与虚空的通道。远古时代，五位贤者联手设置了五道封印，将深渊之门紧紧关闭。然而，随着岁月流逝和各地灾变，封印已经接连破碎，如今只剩最后一道。"',
    col: 60,
    row: 30,
    spriteType: 'rune_pillar',
    hidden: false,
  },
  {
    id: 'lore_ar_02',
    zone: 'abyss_rift',
    name: '堕落者遗书',
    text: '一封被虚空能量侵蚀的遗书："我是第五位守护者的后裔。我们世代守护着最后的封印，但虚空的腐蚀太过强大。我的同伴一个个堕落成为恶魔的仆从。如果你看到这封信……请替我完成使命，封印深渊之门。——最后的守护者"',
    col: 100,
    row: 25,
    spriteType: 'old_scroll',
    hidden: true,
  },
  {
    id: 'lore_ar_03',
    zone: 'abyss_rift',
    name: '虚空结晶',
    text: '一块散发着不祥紫光的结晶体。触碰它时，你的脑海中闪过破碎的画面：虚空中的魔王注视着人间，无数恶魔在黑暗中等待召唤。结晶似乎在警告你——深渊裂隙的另一端，有着远超想象的恐怖力量。',
    col: 30,
    row: 100,
    spriteType: 'crystal_shard',
    hidden: true,
  },
  {
    id: 'lore_ar_04',
    zone: 'abyss_rift',
    name: '封印残阵',
    text: '一个残破的魔法阵，仍然闪烁着微弱的光芒。这是远古贤者设置的五道封印之一。阵法的中心刻着铭文："以我之血为锁，以我之魂为钥。唯有五阵齐亮，深渊之门方可永封。"阵法已经破碎大半，修复之路遥遥无期。',
    col: 75,
    row: 80,
    spriteType: 'rune_pillar',
    hidden: false,
  },
  {
    id: 'lore_ar_05',
    zone: 'abyss_rift',
    name: '恶魔契约石板',
    text: '一块刻满恶魔文字的石板。虽然大部分无法辨认，但其中一段被翻译过的文字清晰可见："与深渊签订契约者，将获得无尽的力量，代价是灵魂的永恒沉沦。虚空先驱是第一个接受契约的人间生灵，也是魔王最忠实的仆从。"',
    col: 15,
    row: 65,
    spriteType: 'ancient_tablet',
    hidden: true,
  },
];

/** All lore entries indexed by zone. */
export const LoreByZone: Record<string, LoreEntry[]> = {
  emerald_plains: emeraldPlainsLore,
  twilight_forest: twilightForestLore,
  anvil_mountains: anvilMountainsLore,
  scorching_desert: scorchingDesertLore,
  abyss_rift: abyssRiftLore,
};

/** All lore entries as a flat array. */
export const AllLoreEntries: LoreEntry[] = [
  ...emeraldPlainsLore,
  ...twilightForestLore,
  ...anvilMountainsLore,
  ...scorchingDesertLore,
  ...abyssRiftLore,
];

/** Get total lore count per zone. */
export function getLoreCountByZone(zone: string): number {
  return (LoreByZone[zone] ?? []).length;
}
