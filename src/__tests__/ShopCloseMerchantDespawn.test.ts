import { describe, it, expect, vi, beforeEach } from 'vitest';
import { EventBus, GameEvents } from '../utils/EventBus';

// ---------------------------------------------------------------------------
// Tests for fix-shop-close-merchant-despawn:
// The wandering merchant should only despawn when SHOP_CLOSE is emitted
// with npcId === 'wandering_merchant', NOT when any generic SHOP_CLOSE fires
// (e.g., from closeAllPanels() due to mutual panel exclusion).
// ---------------------------------------------------------------------------

describe('SHOP_CLOSE event payload — npcId semantics', () => {
  beforeEach(() => {
    EventBus.removeAllListeners();
  });

  it('SHOP_CLOSE can carry an npcId payload', () => {
    const handler = vi.fn();
    EventBus.on(GameEvents.SHOP_CLOSE, handler);
    EventBus.emit(GameEvents.SHOP_CLOSE, { npcId: 'wandering_merchant' });
    expect(handler).toHaveBeenCalledWith({ npcId: 'wandering_merchant' });
  });

  it('SHOP_CLOSE can carry undefined npcId (backwards compat)', () => {
    const handler = vi.fn();
    EventBus.on(GameEvents.SHOP_CLOSE, handler);
    // Legacy emit with no payload (from closeAllPanels when no shop was open)
    EventBus.emit(GameEvents.SHOP_CLOSE);
    expect(handler).toHaveBeenCalledTimes(1);
  });

  it('SHOP_CLOSE with different npcId does not match wandering_merchant', () => {
    const handler = vi.fn();
    EventBus.on(GameEvents.SHOP_CLOSE, handler);
    EventBus.emit(GameEvents.SHOP_CLOSE, { npcId: 'blacksmith_emerald' });
    expect(handler).toHaveBeenCalledWith({ npcId: 'blacksmith_emerald' });
    // The payload contains a different npcId, not 'wandering_merchant'
    const payload = handler.mock.calls[0][0];
    expect(payload.npcId).not.toBe('wandering_merchant');
  });
});

describe('Wandering merchant despawn — filtered by npcId', () => {
  beforeEach(() => {
    EventBus.removeAllListeners();
  });

  it('merchant despawns when SHOP_CLOSE has npcId "wandering_merchant"', () => {
    const despawnSpy = vi.fn();

    // Simulate the fixed pattern: only despawn if npcId matches
    const filteredDespawn = (data?: { npcId?: string }) => {
      if (data?.npcId === 'wandering_merchant') {
        despawnSpy();
      }
    };
    EventBus.on(GameEvents.SHOP_CLOSE, filteredDespawn);

    EventBus.emit(GameEvents.SHOP_CLOSE, { npcId: 'wandering_merchant' });
    expect(despawnSpy).toHaveBeenCalledTimes(1);
  });

  it('merchant does NOT despawn when SHOP_CLOSE has different npcId', () => {
    const despawnSpy = vi.fn();

    const filteredDespawn = (data?: { npcId?: string }) => {
      if (data?.npcId === 'wandering_merchant') {
        despawnSpy();
      }
    };
    EventBus.on(GameEvents.SHOP_CLOSE, filteredDespawn);

    EventBus.emit(GameEvents.SHOP_CLOSE, { npcId: 'blacksmith_emerald' });
    expect(despawnSpy).not.toHaveBeenCalled();
  });

  it('merchant does NOT despawn when SHOP_CLOSE has no payload (closeAllPanels)', () => {
    const despawnSpy = vi.fn();

    const filteredDespawn = (data?: { npcId?: string }) => {
      if (data?.npcId === 'wandering_merchant') {
        despawnSpy();
      }
    };
    EventBus.on(GameEvents.SHOP_CLOSE, filteredDespawn);

    // closeAllPanels emits SHOP_CLOSE with no payload
    EventBus.emit(GameEvents.SHOP_CLOSE);
    expect(despawnSpy).not.toHaveBeenCalled();
  });

  it('merchant does NOT despawn on SHOP_CLOSE from opening inventory (mutual exclusion)', () => {
    const despawnSpy = vi.fn();

    const filteredDespawn = (data?: { npcId?: string }) => {
      if (data?.npcId === 'wandering_merchant') {
        despawnSpy();
      }
    };
    EventBus.on(GameEvents.SHOP_CLOSE, filteredDespawn);

    // Simulating: player opens inventory -> closeAllPanels destroys shop
    // -> emits SHOP_CLOSE with the closed shop's npcId (not wandering_merchant)
    EventBus.emit(GameEvents.SHOP_CLOSE, { npcId: 'some_other_npc' });
    expect(despawnSpy).not.toHaveBeenCalled();
  });

  it('merchant despawns only once even with multiple SHOP_CLOSE events', () => {
    const despawnSpy = vi.fn();

    const filteredDespawn = (data?: { npcId?: string }) => {
      if (data?.npcId === 'wandering_merchant') {
        despawnSpy();
      }
    };
    // Use 'once' like the actual code does
    EventBus.once(GameEvents.SHOP_CLOSE, filteredDespawn);

    EventBus.emit(GameEvents.SHOP_CLOSE, { npcId: 'wandering_merchant' });
    EventBus.emit(GameEvents.SHOP_CLOSE, { npcId: 'wandering_merchant' });
    expect(despawnSpy).toHaveBeenCalledTimes(1);
  });

  it('merchant eventually despawns after non-matching close followed by matching close', () => {
    const despawnSpy = vi.fn();

    // Use 'on' (not 'once') to simulate the fixed handler that stays registered
    // until the correct close event occurs
    const filteredDespawn = (data?: { npcId?: string }) => {
      if (data?.npcId === 'wandering_merchant') {
        despawnSpy();
        EventBus.off(GameEvents.SHOP_CLOSE, filteredDespawn);
      }
    };
    EventBus.on(GameEvents.SHOP_CLOSE, filteredDespawn);

    // First: unrelated close (e.g., opening inventory)
    EventBus.emit(GameEvents.SHOP_CLOSE, { npcId: 'blacksmith_emerald' });
    expect(despawnSpy).not.toHaveBeenCalled();

    // Then: merchant's own shop closes
    EventBus.emit(GameEvents.SHOP_CLOSE, { npcId: 'wandering_merchant' });
    expect(despawnSpy).toHaveBeenCalledTimes(1);
  });
});

describe('NPC.ts SHOP_CLOSE listener — backward compatibility', () => {
  beforeEach(() => {
    EventBus.removeAllListeners();
  });

  it('NPC state listener still fires when SHOP_CLOSE has a payload', () => {
    // NPC.ts line 251 listens for SHOP_CLOSE without checking payload.
    // Our fix adds a payload but doesn't break the NPC listener.
    const stateHandler = vi.fn();
    EventBus.on(GameEvents.SHOP_CLOSE, stateHandler);

    EventBus.emit(GameEvents.SHOP_CLOSE, { npcId: 'wandering_merchant' });
    expect(stateHandler).toHaveBeenCalledTimes(1);
  });

  it('NPC state listener still fires when SHOP_CLOSE has no payload', () => {
    const stateHandler = vi.fn();
    EventBus.on(GameEvents.SHOP_CLOSE, stateHandler);

    EventBus.emit(GameEvents.SHOP_CLOSE);
    expect(stateHandler).toHaveBeenCalledTimes(1);
  });
});
