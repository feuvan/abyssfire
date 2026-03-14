import { EventBus, GameEvents } from '../utils/EventBus';
import { getItemBase } from '../data/items/bases';
import type { ItemInstance, EquipSlot, WeaponBase, ArmorBase } from '../data/types';

const MAX_INVENTORY = 100;
const MAX_STASH = 80;

export class InventorySystem {
  inventory: ItemInstance[] = [];
  equipment: Partial<Record<EquipSlot, ItemInstance>> = {};
  stash: ItemInstance[] = [];

  addItem(item: ItemInstance): boolean {
    // Try to stack with existing
    if (this.isStackable(item)) {
      const existing = this.inventory.find(i => i.baseId === item.baseId && i.quantity < this.getMaxStack(item));
      if (existing) {
        const space = this.getMaxStack(item) - existing.quantity;
        const toAdd = Math.min(item.quantity, space);
        existing.quantity += toAdd;
        item.quantity -= toAdd;
        if (item.quantity <= 0) {
          EventBus.emit(GameEvents.LOG_MESSAGE, { text: `获得 ${item.name} x${toAdd}`, type: 'loot' });
          return true;
        }
      }
    }

    if (this.inventory.length >= MAX_INVENTORY) {
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: '背包已满!', type: 'system' });
      return false;
    }

    this.inventory.push(item);
    EventBus.emit(GameEvents.LOG_MESSAGE, {
      text: `获得 ${this.getQualityPrefix(item.quality)}${item.name}`,
      type: 'loot',
    });
    return true;
  }

  removeItem(uid: string, quantity = 1): ItemInstance | null {
    const idx = this.inventory.findIndex(i => i.uid === uid);
    if (idx === -1) return null;

    const item = this.inventory[idx];
    if (item.quantity > quantity) {
      item.quantity -= quantity;
      return { ...item, quantity };
    }

    this.inventory.splice(idx, 1);
    return item;
  }

  equip(uid: string): boolean {
    const item = this.inventory.find(i => i.uid === uid);
    if (!item) return false;

    const base = getItemBase(item.baseId);
    if (!base || !base.slot) return false;

    let slot = base.slot as EquipSlot;

    // Rings: if target slot is occupied, try the other ring slot
    if (slot === 'ring1' && this.equipment['ring1'] && !this.equipment['ring2']) {
      slot = 'ring2';
    } else if (slot === 'ring2' && this.equipment['ring2'] && !this.equipment['ring1']) {
      slot = 'ring1';
    }

    const current = this.equipment[slot];

    // Unequip current
    if (current) {
      if (this.inventory.length >= MAX_INVENTORY) {
        EventBus.emit(GameEvents.LOG_MESSAGE, { text: '背包已满，无法换装!', type: 'system' });
        return false;
      }
      this.inventory.push(current);
    }

    // Remove from inventory and equip
    const idx = this.inventory.findIndex(i => i.uid === uid);
    if (idx !== -1) this.inventory.splice(idx, 1);
    this.equipment[slot] = item;

    EventBus.emit(GameEvents.LOG_MESSAGE, { text: `装备了 ${item.name}`, type: 'info' });
    return true;
  }

  unequip(slot: EquipSlot): boolean {
    const item = this.equipment[slot];
    if (!item) return false;

    if (this.inventory.length >= MAX_INVENTORY) {
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: '背包已满!', type: 'system' });
      return false;
    }

    delete this.equipment[slot];
    this.inventory.push(item);
    return true;
  }

  sellItem(uid: string): number {
    const item = this.inventory.find(i => i.uid === uid);
    if (!item) return 0;

    const base = getItemBase(item.baseId);
    const price = base ? base.sellPrice * item.quantity : 1;
    this.removeItem(uid, item.quantity);
    return price;
  }

  getEquipmentStats(): Record<string, number> {
    const stats: Record<string, number> = {};
    for (const item of Object.values(this.equipment)) {
      if (!item) continue;
      const base = getItemBase(item.baseId);

      // Base stats
      if (base && 'baseDamage' in base) {
        const wb = base as WeaponBase;
        stats['weaponDamageMin'] = (stats['weaponDamageMin'] ?? 0) + wb.baseDamage[0];
        stats['weaponDamageMax'] = (stats['weaponDamageMax'] ?? 0) + wb.baseDamage[1];
      }
      if (base && 'baseDefense' in base) {
        const ab = base as ArmorBase;
        stats['defense'] = (stats['defense'] ?? 0) + ab.baseDefense;
      }

      // Affix stats
      for (const [stat, value] of Object.entries(item.stats)) {
        stats[stat] = (stats[stat] ?? 0) + (value ?? 0);
      }
    }
    return stats;
  }

  moveToStash(uid: string): boolean {
    if (this.stash.length >= MAX_STASH) {
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: '仓库已满!', type: 'system' });
      return false;
    }
    const item = this.removeItem(uid, Infinity);
    if (!item) return false;
    this.stash.push(item);
    return true;
  }

  moveFromStash(uid: string): boolean {
    const idx = this.stash.findIndex(i => i.uid === uid);
    if (idx === -1) return false;
    const item = this.stash[idx];
    if (this.addItem(item)) {
      this.stash.splice(idx, 1);
      return true;
    }
    return false;
  }

  identifyItem(uid: string): boolean {
    const item = this.inventory.find(i => i.uid === uid);
    if (!item || item.identified) return false;
    // Check for ID scroll
    const scroll = this.inventory.find(i => i.baseId === 'c_id_scroll' && i.quantity > 0);
    if (!scroll) {
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: '需要鉴定卷轴!', type: 'system' });
      return false;
    }
    scroll.quantity--;
    if (scroll.quantity <= 0) this.removeItem(scroll.uid);
    item.identified = true;
    EventBus.emit(GameEvents.LOG_MESSAGE, { text: `鉴定了 ${item.name}`, type: 'info' });
    return true;
  }

  useConsumable(uid: string): { effect: string; value: number } | null {
    const item = this.inventory.find(i => i.uid === uid);
    if (!item) return null;

    const base = getItemBase(item.baseId);
    if (!base || (base.type !== 'consumable' && base.type !== 'scroll')) return null;

    let effect = '';
    let value = 0;

    switch (item.baseId) {
      case 'c_hp_potion_s': effect = 'heal'; value = 50; break;
      case 'c_hp_potion_m': effect = 'heal'; value = 150; break;
      case 'c_hp_potion_l': effect = 'heal'; value = 400; break;
      case 'c_mp_potion_s': effect = 'mana'; value = 30; break;
      case 'c_mp_potion_m': effect = 'mana'; value = 80; break;
      case 'c_antidote': effect = 'antidote'; value = 1; break;
      case 'c_tp_scroll': effect = 'teleport'; value = 1; break;
      default: return null;
    }

    item.quantity--;
    if (item.quantity <= 0) this.removeItem(uid);

    return { effect, value };
  }

  sortInventory(): void {
    const qualityOrder: Record<string, number> = { legendary: 0, set: 1, rare: 2, magic: 3, normal: 4 };
    const typeOrder: Record<string, number> = { weapon: 0, armor: 1, accessory: 2, consumable: 3, gem: 4, material: 5, scroll: 6 };
    this.inventory.sort((a, b) => {
      const qa = qualityOrder[a.quality] ?? 5;
      const qb = qualityOrder[b.quality] ?? 5;
      if (qa !== qb) return qa - qb;
      const baseA = getItemBase(a.baseId);
      const baseB = getItemBase(b.baseId);
      const ta = typeOrder[baseA?.type ?? ''] ?? 7;
      const tb = typeOrder[baseB?.type ?? ''] ?? 7;
      if (ta !== tb) return ta - tb;
      return a.name.localeCompare(b.name);
    });
  }

  discardItem(uid: string): boolean {
    const idx = this.inventory.findIndex(i => i.uid === uid);
    if (idx === -1) return false;
    const item = this.inventory[idx];
    this.inventory.splice(idx, 1);
    EventBus.emit(GameEvents.LOG_MESSAGE, { text: `丢弃了 ${item.name}`, type: 'system' });
    EventBus.emit(GameEvents.ITEM_DISCARDED, { item });
    return true;
  }

  destroyNormalItems(): number {
    const toRemove: string[] = [];
    for (const item of this.inventory) {
      if (item.quality !== 'normal') continue;
      const base = getItemBase(item.baseId);
      if (!base) continue;
      if (base.type === 'consumable' || base.type === 'scroll' || base.type === 'gem' || base.type === 'material') continue;
      toRemove.push(item.uid);
    }
    for (const uid of toRemove) {
      const idx = this.inventory.findIndex(i => i.uid === uid);
      if (idx !== -1) this.inventory.splice(idx, 1);
    }
    if (toRemove.length > 0) {
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: `销毁了 ${toRemove.length} 件普通装备`, type: 'system' });
    }
    return toRemove.length;
  }

  private isStackable(item: ItemInstance): boolean {
    const base = getItemBase(item.baseId);
    return base?.stackable ?? false;
  }

  private getMaxStack(item: ItemInstance): number {
    const base = getItemBase(item.baseId);
    return base?.maxStack ?? 1;
  }

  private getQualityPrefix(quality: string): string {
    switch (quality) {
      case 'magic': return '[魔法] ';
      case 'rare': return '[稀有] ';
      case 'legendary': return '[传奇] ';
      case 'set': return '[套装] ';
      default: return '';
    }
  }
}
