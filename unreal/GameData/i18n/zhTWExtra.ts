/**
 * zh-TW supplement for the remake's strings (final review R4). The exporter converts every zh-CN string with the web
 * converter (src/i18n/converter.ts, read-only for the remake); its character map misses many Simplified characters
 * (军 尸 麦 边 没 着 ...), so Chapter 1 came out half Simplified. For the keys of ./zh-CN.ts only, the exporter
 * (unreal/Tools/export-data/src/tables/i18n.ts, remakeToTraditional) then:
 *
 *   1. scans the zh-CN text left to right and, where a ZH_TW_PHRASES key starts (longest first), emits its value as is:
 *      the context-dependent characters (皇后 vs 之后, 一只 vs 只是, 石台, 公里, 干了的, 头发, 关系, ...);
 *   2. converts the text between phrases with the web converter, then maps what it left with ZH_TW_CHARS.
 *
 * Web-sourced keys keep the web converter's output byte for byte. content_ch1_test "Chapter 1 zh-TW" fails when a zh-TW
 * string Chapter 1 shows keeps a Simplified-only character (CoreTests/golden/zh_simplified_only.txt): add the character
 * here (and a phrase when its Traditional form depends on the word). Taiwan forms throughout (著 裡 為 眾).
 *
 * A name that contains 后 里 于 余 采 尸 叶 涂 (a default below that is also a Traditional character) needs a phrase
 * entry keeping it, e.g. '某里': '某里'.
 */

/** 'sT sT ...' -> { s: T } (each token is one Simplified and one Traditional character). */
function pairs(...rows: string[]): Record<string, string> {
  const out: Record<string, string> = {};
  for (const tok of rows.join(' ').split(/\s+/).filter(Boolean)) {
    const [s, t, ...rest] = [...tok];
    if (s === undefined || t === undefined || rest.length) throw new Error(`zhTWExtra: bad pair '${tok}'`);
    if (s in out) throw new Error(`zhTWExtra: '${s}' mapped twice`);
    out[s] = t;
  }
  return out;
}

/**
 * Simplified -> Traditional characters the web map lacks, applied after it (a character the web map already converts
 * never reaches this table).
 */
export const ZH_TW_CHARS: Readonly<Record<string, string>> = pairs(
  // Chapter 1's strings beyond the common list below (final review R4).
  '铛鐺',
  // Also Traditional characters in their own right, but converted in Taiwan usage (final review): 胜 勝, 厂 廠, 苹 蘋.
  '胜勝 厂廠 苹蘋',
  // The context-dependent characters, by their usual Traditional form; ZH_TW_PHRASES keeps the other readings.
  '后後 里裡 于於 余餘 采採 尸屍 复復 叶葉 涂塗 仑崙 凄淒 卤滷 啮嚙 绷繃',
  // The rest of GB2312 level 1 (the 3755 common characters) that is Simplified-only with a single Traditional form and
  // that the web map lacks: OpenCC STCharacters (+ TWVariants), the source of CoreTests/golden/zh_simplified_only.txt.
  '专專 丛叢 丽麗 乌烏 乔喬 争爭 亚亞 亩畝 众眾 伞傘 儿兒 兹茲 内內 册冊 写寫 军軍 农農 冯馮 决決 凯凱 删刪 刽劊 剐剮 剥剝',
  '劳勞 匀勻 厦廈 厨廚 厩廄 县縣 叁叄 号號 吓嚇 吴吳 呕嘔 呛嗆 咙嚨 啸嘯 嘱囑 嚣囂 囱囪 坞塢 坠墜 堑塹 壶壺 娄婁 娇嬌 婶嬸',
  '孪孿 宾賓 寝寢 寻尋 导導 尔爾 屉屜 届屆 岛島 峦巒 崭嶄 带帶 庞龐 怂慫 恒恆 恼惱 户戶 挚摯 挛攣 挝撾 挞撻 挟挾 挠撓 损損',
  '捣搗 搀攙 携攜 摈擯 摊攤 擞擻 攒攢 敛斂 枢樞 枫楓 柠檸 椭橢 槛檻 橱櫥 欧歐 毡氈 没沒 沤漚 沥瀝 沦淪 泞濘 泼潑 洼窪 满滿',
  '滦灤 潍濰 澜瀾 濒瀕 炽熾 烃烴 烬燼 爷爺 狰猙 獭獺 琼瓊 痒癢 着著 矾礬 码碼 砾礫 硷礆 碍礙 祷禱 禄祿 离離 秃禿 秽穢 税稅',
  '笋筍 箩籮 篓簍 类類 粤粵 纶綸 绅紳 绊絆 绎繹 绑綁 绒絨 绚絢 绦絛 绸綢 缄緘 网網 羡羨 耻恥 聋聾 联聯 肮骯 苇葦 茧繭 萤螢',
  '蔼藹 蕴蘊 虱蝨 虾蝦 蚂螞 蚕蠶 蜕蛻 蜗蝸 蝉蟬 衔銜 袜襪 誉譽 订訂 讣訃 讥譏 讨討 讫訖 讶訝 讹訛 讼訟 讽諷 诀訣 诅詛 诌謅',
  '试試 诛誅 询詢 诣詣 该該 诬誣 诽誹 谆諄 谍諜 谗讒 谚諺 谩謾 谰讕 贬貶 赁賃 赡贍 赢贏 赣贛 赵趙 趋趨 践踐 踌躊 踪蹤 蹿躥',
  '躯軀 轿轎 较較 输輸 辞辭 辩辯 辫辮 边邊 邓鄧 邹鄒 郧鄖 郸鄲 钎釺 钒釩 钝鈍 钞鈔 钠鈉 钡鋇 钨鎢 钵缽 钾鉀 铀鈾 铂鉑 铬鉻',
  '铰鉸 铱銥 锑銻 锗鍺 锨鍁 锯鋸 镁鎂 镍鎳 阳陽 际際 陇隴 陈陳 陕陝 陨隕 随隨 隶隸 雇僱 雏雛 霉黴 静靜 韦韋 麦麥 齐齊 龚龔',
);

/**
 * Simplified phrase -> its Traditional (Taiwan) form, for characters whose form depends on the word. Matched on the
 * zh-CN text before any character conversion, longest key first; the value is final (write every character of it in
 * its Traditional form).
 */
export const ZH_TW_PHRASES: Readonly<Record<string, string>> = {
  // 后: 後 (after, behind, rear) by default; a queen or the earth keeps 后.
  '皇后': '皇后', '王后': '王后', '太后': '太后', '天后': '天后', '后土': '后土',
  // 里: 裡 (inside) by default; the unit of distance and a neighbourhood keep 里.
  '公里': '公里', '英里': '英里', '千里': '千里', '万里': '萬里', '里程': '里程', '故里': '故里', '邻里': '鄰里',
  '乡里': '鄉里', '里长': '里長',
  // ... but not across a word boundary (家乡 + 里, 坟 + 里 + 长).
  '家乡里': '家鄉裡', '坟里长': '墳裡長',
  // 采: 採 (pick, gather) by default; bearing and flair keep 采.
  '风采': '風采', '神采': '神采', '文采': '文采', '兴高采烈': '興高采烈', '无精打采': '無精打采',
  // 复: 復 (again, restore) by default; 複 (compound, repeat), 覆 (answer, over and over).
  '复杂': '複雜', '复制': '複製', '重复': '重複', '复合': '複合', '复数': '複數', '复述': '複述', '复习': '複習',
  '反复': '反覆', '答复': '答覆', '回复': '回覆',
  // 只 (only) stays; after a numeral or 这 / 那 it is the measure word 隻, unless an adverb follows (只是, 只能, ...).
  '一只': '一隻', '两只': '兩隻', '三只': '三隻', '四只': '四隻', '五只': '五隻', '六只': '六隻', '七只': '七隻',
  '八只': '八隻', '九只': '九隻', '十只': '十隻', '二只': '二隻', '几只': '幾隻', '每只': '每隻', '这只': '這隻',
  '那只': '那隻',
  '一只是': '一只是', '一只能': '一只能', '一只有': '一只有', '一只要': '一只要', '一只会': '一只會', '一只好': '一只好',
  '这只是': '這只是', '这只能': '這只能', '这只有': '這只有', '这只会': '這只會', '这只要': '這只要', '这只好': '這只好',
  '那只是': '那只是', '那只能': '那只能', '那只有': '那只有', '那只会': '那只會', '那只要': '那只要', '那只好': '那只好',
  // 台 stays; a platform or a stage is 臺, a counter 檯.
  '石台': '石臺', '平台': '平臺', '舞台': '舞臺', '高台': '高臺', '祭台': '祭臺', '看台': '看臺', '讲台': '講臺',
  '台阶': '臺階', '柜台': '櫃檯',
  // 干: the web's 幹 (do, trunk) unless dry (乾) or 干 itself (interfere, shield, a number of).
  '干了的': '乾了的', '干透': '乾透', '干燥': '乾燥', '干涸': '乾涸', '干枯': '乾枯', '干净': '乾淨', '干瘪': '乾癟',
  '干粮': '乾糧', '干草': '乾草', '干裂': '乾裂', '干渴': '乾渴', '干血': '乾血', '干尸': '乾屍', '饼干': '餅乾',
  '晒干': '曬乾', '烤干': '烤乾', '风干': '風乾', '烧干': '燒乾', '流干': '流乾',
  '若干': '若干', '干涉': '干涉', '干扰': '干擾', '干预': '干預', '相干': '相干', '干戈': '干戈',
  // 发: the web's 發 unless hair (髮).
  '头发': '頭髮', '白发': '白髮', '黑发': '黑髮', '灰发': '灰髮', '金发': '金髮', '红发': '紅髮', '长发': '長髮',
  '短发': '短髮', '毛发': '毛髮', '发丝': '髮絲', '发辫': '髮辮', '发髻': '髮髻', '须发': '鬚髮', '理发': '理髮',
  '披头散发': '披頭散髮',
  '苍白发': '蒼白發', '黄金发': '黃金發', '成长发': '成長發',
  '长发展': '長發展', '白发光': '白發光', '黑发光': '黑發光', '灰发光': '灰發光', '金发光': '金發光', '红发光': '紅發光',
  // 面 stays; flour and noodles are 麵.
  '面包': '麵包', '面粉': '麵粉', '面条': '麵條',
  // 系 stays (system, lineage); a relation is 係, a tie 繫.
  '关系': '關係', '联系': '聯繫', '维系': '維繫', '牵系': '牽繫', '系上': '繫上', '系着': '繫著', '系紧': '繫緊',
  '系住': '繫住', '系好': '繫好', '体系': '體系', '派系': '派系', '世系': '世系',
  // 制: the web's 製 (make) unless control or rule (制).
  '控制': '控制', '制度': '制度', '限制': '限制', '压制': '壓制', '克制': '克制', '强制': '強制', '节制': '節制',
  '制止': '制止', '制服': '制服', '制约': '制約', '制定': '制定', '牵制': '牽制', '遏制': '遏制', '抑制': '抑制',
  '专制': '專制', '体制': '體制',
  // 松: the web's 鬆 (loose) unless the pine.
  '松树': '松樹', '松林': '松林', '松针': '松針', '松木': '松木', '松脂': '松脂', '松香': '松香', '青松': '青松',
  // 斗: the web's 鬥 (fight) unless the cloak, the Dipper or a measure.
  '斗篷': '斗篷', '北斗': '北斗', '漏斗': '漏斗', '斗笠': '斗笠', '烟斗': '煙斗',
  // 冲: the web's 沖 (rinse) unless a charge or a clash (衝).
  '冲锋': '衝鋒', '冲击': '衝擊', '冲向': '衝向', '冲出': '衝出', '冲进': '衝進', '冲破': '衝破', '冲突': '衝突',
  '冲动': '衝動', '冲上': '衝上', '冲过': '衝過', '冲撞': '衝撞', '冲杀': '衝殺', '要冲': '要衝',
  // 尽: the web's 盡 unless "as far as possible" (儘).
  '尽管': '儘管', '尽量': '儘量', '尽快': '儘快', '尽早': '儘早',
  // 历: the web's 歷 unless the calendar (曆).
  '日历': '日曆', '历法': '曆法', '农历': '農曆',
  // 钟: the web's 鐘 (bell, clock) unless fondness (鍾).
  '钟情': '鍾情', '钟爱': '鍾愛',
  // 准: the web's 準 (accurate, standard) unless permission (准).
  '批准': '批准', '准许': '准許', '不准': '不准', '获准': '獲准', '不准确': '不準確',
  // Characters the web map leaves alone whose other reading is common in story text.
  '特征': '特徵', '象征': '象徵', '征兆': '徵兆', '征召': '徵召', '征税': '徵稅',
  '防御': '防禦', '抵御': '抵禦', '御敌': '禦敵',
  '注释': '註釋', '注解': '註解', '注册': '註冊',
  '舍弃': '捨棄', '舍不得': '捨不得', '不舍': '不捨', '施舍': '施捨', '舍身': '捨身', '取舍': '取捨',
  '丑陋': '醜陋', '丑恶': '醜惡', '精致': '精緻', '细致': '細緻',
  '卷入': '捲入', '卷起': '捲起', '席卷': '席捲',
  '谷物': '穀物', '稻谷': '稻穀', '五谷': '五穀', '谷仓': '穀倉',
  '胡子': '鬍子', '胡须': '鬍鬚', '欲望': '慾望', '手表': '手錶', '仆人': '僕人', '呼吁': '呼籲',
  '忧郁': '憂鬱', '郁闷': '鬱悶', '抑郁': '抑鬱', '阴郁': '陰鬱',
};
