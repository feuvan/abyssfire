# Phaser 性能调研与 P0 方案

日期：2026-03-21

## 现象

- 浏览器内存占用约 1.1GB
- 运行一段时间后出现卡顿
- 区域切换、面板交互和持续战斗会放大问题

## 高置信度根因

### 1. 启动阶段一次性解码全部 BGM

- `src/scenes/BootScene.ts` 会在启动时预加载 5 个地图 * 3 个状态的全部 BGM。
- Phaser WebAudio 路径会把音频解码成 `AudioBuffer`，内存占用远大于 mp3 文件本身。
- 本地文件总大小约 26.94 MiB，但按时长、采样率和双声道浮点 PCM 换算，解码后约 430.7 MiB。
- 这部分单独就足以解释总内存中的一个大头。

结论：

- BGM 必须改为按 zone 懒加载。
- 当前 zone 之外的 `AudioBuffer` 要及时释放。
- 在音频未就绪时继续使用现有程序音乐作为兜底。

### 2. 启动阶段一次性生成大量程序纹理

- `SpriteGenerator.generateAll()` 会在 `BootScene` 一次性生成玩家、怪物、NPC、装饰物等全部程序纹理。
- 当前配置是 `DPR=2`、`TEXTURE_SCALE=3`。
- 仅角色和 NPC 动画 sheet 的原始 RGBA 体积估算就在 140 MiB 左右。

结论：

- `TEXTURE_SCALE=3` 对当前内存预算偏激进。
- 后续应改成按需生成，或至少把 `TEXTURE_SCALE` 降到 2。

实际修复（2026-03-21）：

- 按当前约束，未改 `DPR`，也未降低 `TEXTURE_SCALE`。
- `BootScene` 现在只生成地形 tile、营地装饰和少量公共 effect/particle 纹理，不再一次性烘焙全部角色类 sheet。
- 玩家、怪物、NPC 和普通地图装饰改成首次实际使用时再生成，并在进入新 zone 后按内容逐步补齐。
- zone 关闭时会额外释放怪物 / NPC / 装饰 / loot / portal 等程序纹理，避免跨 zone 常驻累计。

### 3. ZoneScene 生命周期清理不完整

- `ZoneScene` 定义了 `shutdown()`，但没有接入 Phaser `SHUTDOWN` 生命周期。
- 场景通过 `scene.restart()` 切换区域，容易让自建系统、事件监听和 DOM 监听跨 zone 叠加。

高风险对象：

- `LightingSystem`
- `WeatherSystem`
- `TrailRenderer`
- `MobileControlsSystem`
- `game.canvas` 上的 `contextmenu` 监听
- `EventBus` 上的匿名监听

结论：

- 必须把 `shutdown` 正式挂到 Phaser 场景生命周期。
- 所有 `EventBus` / `input` / DOM 监听都要做精确解绑，不能再用 `removeAllListeners` 这种全局清空。

### 4. 运行时纹理缓存和对象回收不完整

- 地形过渡纹理 `tile_t_*` 会按邻居组合动态生成并缓存进 `TextureManager`。
- 当前没有回收策略，zone 切换后缓存持续增长。
- 传送门 sprite 在 tile 首次进入视野时创建，但未纳入任何可见性/销毁管理。
- 环境尘埃 emitter 创建后未持有引用，也未在 scene shutdown 时清理。

结论：

- 需要在 zone 关闭时清理 `tile_t_*`。
- 传送门 sprite、环境尘埃 emitter 必须纳入可控生命周期。

### 5. 卡顿热点集中在主线程和 GPU 带宽

- `ZoneScene.updateVisibleTiles()` 每 100ms 扫完整张 80x80 地图并创建/销毁对象。
- `LightingSystem` 每帧更新 CanvasTexture 并上传到 GPU。
- 主相机启用了 bloom、vignette 和额外的 color grading pipeline。
- `UIScene.update()` 中存在每帧 `setText()`，会触发 Text 重新绘制和纹理上传。
- `TrailRenderer` 使用两张常驻 RenderTexture，并频繁创建临时 `Graphics`。

结论：

- 当前卡顿不是单点问题，而是 CPU、GC、GPU pass 叠加。
- P0 先止血 leak 和大内存项，P1 再做热路径重构。

实际修复（2026-03-21）：

- `ZoneScene.updateVisibleTiles()` 已改成基于 camera 世界边界反推 tile 范围的 bounded scan，并缓存每个 tile / decor / camp decor 的世界坐标。
- 退出点查询从 tile 循环内的 `Array.find()` 改成了预构建 lookup map，避免热路径重复线性扫描。
- `UIScene.update()` 里的 HP / MP / EXP / 金币 / 自动战斗 / 自动拾取 / zone 名称 / 技能 CD 文本改成仅在值变化时刷新。
- minimap 和 quest tracker 额外加了 `250ms` 节流，避免此前整张 minimap 在半个时间窗口内被每帧重画。
- `LightingSystem` 没有切到新的 DynamicTexture 实现，而是在现有 CanvasTexture 路径上加 `50ms` 节流和脏标记；这样风险更低，且已经能显著减少每秒画布重绘和 GPU 上传次数。
- `TrailRenderer` 没有上完整对象池，而是改成复用离屏 image stamp，替代高频临时 `Graphics` / `Image` 分配。
- 额外修复了一个未在初版规划里写出的热点：campfire glow 使用了错误的可见性 key，导致可见 campfire 会反复创建并销毁 glow/tween。
- 控制台里的 `Canvas2D: Multiple readback operations using getImageData...` 告警也已单独收口：对会频繁 `getImageData()` 的程序纹理 canvas 显式启用 `willReadFrequently: true`，避免浏览器反复提示并让 readback 路径走到更合适的实现。

## P0 实施范围

### 目标

- 先把明显的内存泄漏和大对象常驻问题处理掉。
- 在不改玩法逻辑的前提下，把区域切换后的资源残留压下去。

### 本轮补丁内容

1. `BootScene`

- 移除启动时对全部 BGM 的预加载和解码。

2. `AudioManager` / `AudioLoader` / `MusicEngine`

- 改成按 zone 懒加载 `explore/combat/victory` 三首 BGM。
- 切换 zone 时释放旧 zone 的 BGM buffer。
- 加载完成后刷新当前音乐层。
- 若音频尚未加载成功，继续沿用程序音乐兜底。

3. `ZoneScene`

- 把 `shutdown()` 接到 Phaser `SHUTDOWN` 事件。
- 精确解绑 `pointerdown`、`EventBus` 和 `contextmenu` 监听。
- 清理 `LightingSystem`、`WeatherSystem`、`TrailRenderer`、`MobileControlsSystem`。
- 追踪并销毁传送门 sprite、环境尘埃 emitter。
- 关闭 zone 时清理 `tile_t_*` 过渡纹理缓存。

4. `UIScene`

- 不再使用 `removeAllListeners`，改为保存 handler 后逐个解绑。
- 音频面板 slider 的 `pointermove/pointerup` 监听纳入显式清理。

5. `LightingSystem` / `VFXManager` / `MobileControlsSystem`

- 保存监听函数引用并在 `destroy()` 中正确解绑。

## 后续建议

### P1

- 把 `DPR` 改为可配置或自适应上限，优先尝试 `1` 或 `min(devicePixelRatio, 1.25)`。
- 把 `TEXTURE_SCALE` 从 3 降到 2。
- `UIScene` 文本更新改成仅在值变化时刷新。
- `updateVisibleTiles()` 改成基于相机 tile 边界的增量更新，而不是周期性扫全图。
- `LightingSystem` 降低更新频率，或改成更适合频繁更新的动态纹理路径。

与原规划的差异：

- 由于本轮明确禁止改 `DPR` 和 `TEXTURE_SCALE`，实际采用的是“懒生成 + 显式回收”替代“直接降分辨率”。
- `updateVisibleTiles()` 这轮先落了“相机边界裁剪 + 坐标缓存 + lookup map”的 bounded scan，尚未继续做更复杂的环形增量 diff；当前收益已经足够覆盖主要卡顿来源。
- `LightingSystem` 先保留现有 overlay 架构，只做节流和脏更新；如果后续浏览器 profile 里这块仍然占比高，再考虑迁到 DynamicTexture / shader 路径。

### P2

- 程序纹理按职业 / 地图 / 实际出场内容延迟生成。
- `TrailRenderer` 和高频技能特效改对象池。
- 全屏后处理做质量档位，给浏览器端默认较低档。

## Phaser 文档对应点

- Scene shutdown / 生命周期清理：
  - <https://docs.phaser.io/api-documentation/event/scenes-events>
  - <https://docs.phaser.io/api-documentation/class/scenes-systems>
- EventEmitter 精确解绑：
  - <https://docs.phaser.io/api-documentation/class/events-eventemitter>
- Text 频繁更新的成本：
  - <https://docs.phaser.io/api-documentation/class/gameobjects-text>
- TextureManager 纹理移除：
  - <https://docs.phaser.io/api-documentation/class/textures-texturemanager>
- Bloom / FX 成本：
  - <https://docs.phaser.io/api-documentation/class/fx-bloom>
