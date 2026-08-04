export class SimulationScheduler {
  private deadlines = new Map<string, number>();

  due(phase: string, now: number, intervalMs: number): boolean {
    const deadline = this.deadlines.get(phase);
    if (deadline !== undefined && now < deadline) return false;

    this.deadlines.set(phase, now + intervalMs);
    return true;
  }

  reset(): void {
    this.deadlines.clear();
  }
}
