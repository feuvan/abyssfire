/**
 * Chapter 1 NPC strings of the dark remake (package 1B part 2, narrative): the re-cast names and lines of the kept role
 * keys, the quest-gated lines for Wave 2, the Captain's dialogue tree and the Lanternwisp. Source:
 * Docs/dark-remake/chapter1.md §2-4, §14; voice: story-bible.md §11 (zh dialogue <= 60 code points).
 *
 * Each key maps to [zh-CN, en]; ./zh-CN.ts and ./en.ts take one column each (see ./ch1World.ts).
 */
export const CH1_NPC_STRINGS: Readonly<Record<string, readonly [zhCN: string, en: string]>> = {
  // ── Knellwatch and the field, re-cast on the kept role keys (chapter1.md §3; knell_chaplain and crow_trader are
  // in ch1World.ts). Wave 1 shows each NPC's first line only; tower_herbalist is Mother Marl in the Ember Tower ──
  'data.npc.quest_elder.name': ['瑟琳·哈洛队长', 'Captain Serin Harrow'],
  'data.npc.quest_elder.dialogue.0': ['坑里爬出来的。能站着，就能拿刀。',
    'Out of the pit and on your feet. Then you can hold a blade.'],
  'data.npc.blacksmith.name': ['哈尔德·熔剑', 'Hald Swordmelter'],
  'data.npc.blacksmith.dialogue.0': ['每把剑的柄脚都刻着名字。剑我熔了，名字留给队长。',
    'Every tang has a name on it. I melt the sword and give the name to the Captain.'],
  'data.npc.merchant.name': ['军需官薇拉', 'Quartermaster Vira'],
  'data.npc.merchant.dialogue.0': ['一个名字，省下一份口粮。今晚又省了三份。', 'One name, one ration saved. Three saved tonight.'],
  'data.npc.stash.name': ['守冢人塔姆欣', 'Tamsin the Gravewarden'],
  'data.npc.stash.dialogue.0': ['死人的东西我替他们收着。活人的，也行。', "I keep the dead's things for them. The living's too."],
  'data.npc.plains_herbalist.name': ['军医玛勒', 'Mother Marl'],
  'data.npc.plains_herbalist.dialogue.0': ['别碰那黑血。碰过的人都不喊疼了——那才最疼。',
    "Don't touch the black blood. The ones who did stopped hurting. That's the worst of it."],
  'data.npc.plains_wanderer.name': ['逃兵柯兰', 'Corran the Deserter'],
  'data.npc.plains_wanderer.dialogue.0': ['我跑了，我活下来了。这两件事，哪件更丢人？', 'I ran and I lived. Which is the greater shame?'],
  'data.npc.tower_herbalist.name': ['军医玛勒', 'Mother Marl'],
  'data.npc.tower_herbalist.dialogue.0': ['这里的火，不要人偿。药圃我守着。', 'The fire here asks no one to pay. I keep the garden.'],
  'data.npc.tower_herbalist.dialogue.1': ['你在外头每放倒一个，园子就多长一寸。别问我为什么。',
    "Every one you put down out there, the garden grows an inch. Don't ask me why."],

  // ── Quest-gated lines (chapter1.md §3), ready for the Wave 2 NPC `lines` (K6): shown once the quest is turned in.
  // No Wave 1 record references them; they are kept because their NPC ids are ──
  'data.npc.blacksmith.after.q_rf_tag_thief': ['那孩子的名牌，我钉在炉边了。打铁的时候，看得见。',
    "I nailed the boy's tag by the forge. I can see it while I work."],
  // S1's discovery is Marl's (chapter1.md §6); the Captain's card turns the quest in, so Marl says it once S1 is done.
  'data.npc.plains_herbalist.after.q_rf_black_blood': ['血里有声音……是死人的声音。它记得他们。',
    "There are voices in the blood... the dead's. It remembers them."],
  'data.npc.quest_elder.after.q_rf_oathless': ['逃兵就是逃兵。……我也是。', 'A deserter is a deserter. ...So am I.'],
  'data.npc.knell_chaplain.after.q_rf_last_toll': ['愿钟声护你。……我会一直听着它。',
    'May the bell keep you. ...I will always be listening to it.'],

  // ── The Captain's dialogue tree (chapter1.md §4; GameData/dialogueTrees.ts), node by node ──
  'data.dialogue.quest_elder.root.text': ['坑里爬出来的。能站着，就能拿刀。',
    'Out of the pit and on your feet. Then you can hold a blade.'],
  'data.dialogue.quest_elder.root.choice.0': ['你是谁？', 'Who are you?'],
  'data.dialogue.quest_elder.root.choice.1': ['那口钟……', 'That bell...'],
  'data.dialogue.quest_elder.root.choice.2': ['有什么要我做的？', 'What needs doing?'],
  'data.dialogue.quest_elder.root.choice.3': ['有饷吗？', 'Is there pay?'],
  'data.dialogue.quest_elder.root.choice.4': ['告辞。', 'Farewell.'],
  'data.dialogue.quest_elder.who.text': ['瑟琳·哈洛，第三营队长。现在是这座堡里军衔最高的活人。',
    'Serin Harrow, Third Company. Now the highest-ranking living soldier in this fort.'],
  'data.dialogue.quest_elder.who.choice.0': ['元帅呢？', 'And the Marshal?'],
  'data.dialogue.quest_elder.who.choice.1': ['回去', 'Back'],
  'data.dialogue.quest_elder.bell.text': ['钟比教会还老，是守火人留下的。黄昏时，一个名字敲一下。',
    'The bell is older than the church; the keepers left it. At dusk, one toll for each name.'],
  'data.dialogue.quest_elder.bell.choice.0': ['回去', 'Back'],
  'data.dialogue.quest_elder.marshal.text': ['元帅下令死守，我下令撤退。我们俩，总有一个是叛徒。他的弩手死了都不放下弩。',
    "The Marshal ordered us to hold. I ordered the retreat. One of us is a traitor. His crossbowmen are dead and still won't put their bows down."],
  'data.dialogue.quest_elder.marshal.choice.0': ['回去', 'Back'],
  'data.dialogue.quest_elder.help.text': ['北边埋骨壕。收尸队没回来。',
    "The burial trenches, north. The burial detail hasn't come back."],
  'data.dialogue.quest_elder.help.choice.0': ['我去。', "I'll go."],
  'data.dialogue.quest_elder.help.choice.1': ['城下的麦田呢？', 'And the wheat below the walls?'],
  'data.dialogue.quest_elder.help.choice.2': ['还有别的吗？', 'Anything else?'],
  'data.dialogue.quest_elder.accept_burial.text': ['带上灯。别带希望。', 'Take a lantern. Leave hope.'],
  'data.dialogue.quest_elder.gnawers_first.text': ['城下的麦田里，有东西在吃我们的人。',
    'In the wheat below the walls, something is eating our people.'],
  'data.dialogue.quest_elder.gnawers_first.choice.0': ['交给我。', 'Leave it to me.'],
  'data.dialogue.quest_elder.accept_gnawers.text': ['十二只。还有那个敲钟的。', 'Twelve of them. And the one ringing the bell.'],
  'data.dialogue.quest_elder.reward_ask.text': ['饷？这是死人的饷。拿去吧，他用不上了。',
    "Pay? This is a dead man's pay. Take it; he won't need it."],
  'data.dialogue.quest_elder.reward_ask.choice.0': ['收下。', 'Take it.'],
  'data.dialogue.quest_elder.reward_ask.choice.1': ['回去', 'Back'],
  'data.dialogue.quest_elder.other_work.text': ['玛勒要血样。神父那边也缺人手。南边沟里有个逃兵，在等人听他说话。',
    "Marl wants blood samples. The chaplain needs hands too. There's a deserter in the southern culvert waiting for someone to listen."],
  'data.dialogue.quest_elder.other_work.choice.0': ['血样？', 'Blood samples?'],
  'data.dialogue.quest_elder.other_work.choice.1': ['神父？', 'The chaplain?'],
  'data.dialogue.quest_elder.accept_ichor.text': ['别用手碰。玛勒会教你。', "Don't touch it bare-handed. Marl will show you."],
  'data.dialogue.quest_elder.chaplain_pointer.text': ['奥德温还在祈祷。我不知道，还有谁在听。',
    "Aldwin still prays. I don't know who's still listening."],
  'data.dialogue.quest_elder.chaplain_pointer.choice.0': ['回去', 'Back'],
  'data.dialogue.quest_elder.farewell.text': ['天黑前回来。天黑以后，城门不开。',
    'Be back before dark. After dark the gate stays shut.'],

  // ── The Lanternwisp: pet_sprite re-dressed as Pell's lantern (chapter1.md S5, §14; story-bible.md §8) ──
  'data.pet.pet_sprite.name': ['灯魂', 'Lanternwisp'],
  'data.pet.pet_sprite.desc': ['佩尔的灯。收尸队里最小的那个走了，灯还在一座坟一座坟地走。',
    "Pell's lantern. The youngest of the burial detail is gone; his light still walks from grave to grave."],
  'data.pet.pet_sprite.origin': ['第一章 · 支线「灯魂」', 'Chapter 1 · Side quest "The Lanternwisp"'],
};
