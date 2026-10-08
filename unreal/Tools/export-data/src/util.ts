/**
 * Output helpers: deterministic JSON formatting, the table writer and TS source assertions.
 */
import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { createHash } from 'node:crypto';
import { dirname, resolve } from 'node:path';

export const SCHEMA_VERSION = 1;

export interface Ctx {
  repoRoot: string;
  /** `unreal/` (or a temp copy of it in --check mode). */
  outRoot: string;
  check: boolean;
  committedRoot: string;
}

export interface TableResult {
  /** Output file name, relative to unreal/Data (e.g. `classes.json`). */
  file: string;
  source: string[];
  data: Record<string, unknown>;
  /** Record counts for the run summary (label → count). */
  counts: Record<string, number>;
}

// ── Deterministic JSON ──────────────────────────────────────────────────────

const INLINE_MAX = 110;

function isPlainObject(v: unknown): v is Record<string, unknown> {
  if (v === null || typeof v !== 'object' || Array.isArray(v)) return false;
  const proto = Object.getPrototypeOf(v);
  return proto === Object.prototype || proto === null;
}

function isPrimitive(v: unknown): boolean {
  return v === null || typeof v === 'number' || typeof v === 'string' || typeof v === 'boolean';
}

function fmtPrimitive(v: unknown, path: string): string {
  if (v === null) return 'null';
  if (typeof v === 'boolean') return v ? 'true' : 'false';
  if (typeof v === 'string') return JSON.stringify(v);
  if (typeof v === 'number') {
    if (!Number.isFinite(v)) throw new Error(`non-finite number at ${path}`);
    if (Object.is(v, -0)) return '0';
    return JSON.stringify(v);
  }
  throw new Error(`unsupported value at ${path}: ${typeof v}`);
}

/**
 * JSON with insertion-ordered keys (the TS source order), 2-space indent, short primitive arrays and
 * flat objects kept on one line. Rejects functions, Maps, Sets, class instances, NaN/Infinity and
 * `undefined` inside arrays so nothing is silently dropped.
 */
export function formatJson(value: unknown, indent = '', path = '$'): string {
  if (isPrimitive(value)) return fmtPrimitive(value, path);
  const next = indent + '  ';
  if (Array.isArray(value)) {
    if (value.length === 0) return '[]';
    value.forEach((v, i) => { if (v === undefined) throw new Error(`undefined in array at ${path}[${i}]`); });
    if (value.every(isPrimitive)) {
      const inline = `[${value.map((v, i) => fmtPrimitive(v, `${path}[${i}]`)).join(', ')}]`;
      if (inline.length + indent.length <= INLINE_MAX) return inline;
    }
    return `[\n${value.map((v, i) => next + formatJson(v, next, `${path}[${i}]`)).join(',\n')}\n${indent}]`;
  }
  if (!isPlainObject(value)) {
    const kind = value instanceof Map ? 'Map' : value instanceof Set ? 'Set' : typeof value;
    throw new Error(`unsupported ${kind} at ${path}`);
  }
  const entries = Object.entries(value).filter(([, v]) => v !== undefined);
  if (entries.length === 0) return '{}';
  if (entries.every(([, v]) => isPrimitive(v))) {
    const inline = `{${entries.map(([k, v]) => `${JSON.stringify(k)}: ${fmtPrimitive(v, `${path}.${k}`)}`).join(', ')}}`;
    if (inline.length + indent.length <= INLINE_MAX) return inline;
  }
  return `{\n${entries.map(([k, v]) => `${next}${JSON.stringify(k)}: ${formatJson(v, next, `${path}.${k}`)}`).join(',\n')}\n${indent}}`;
}

export function sha256(s: string): string {
  return createHash('sha256').update(s).digest('hex');
}

/** Rejects values JSON would silently drop or rewrite (functions, NaN/Infinity, Map/Set, class instances). */
function checkPlain(v: unknown, path: string): void {
  if (v === null || v === undefined || typeof v === 'string' || typeof v === 'boolean') return;
  if (typeof v === 'number') { if (!Number.isFinite(v)) throw new Error(`non-finite number at ${path}`); return; }
  if (typeof v !== 'object') throw new Error(`unsupported ${typeof v} at ${path}`);
  if (Array.isArray(v)) { v.forEach((x, i) => checkPlain(x, `${path}[${i}]`)); return; }
  const proto = Object.getPrototypeOf(v);
  if (proto !== Object.prototype && proto !== null) throw new Error(`non-plain object at ${path}`);
  for (const [k, x] of Object.entries(v)) checkPlain(x, `${path}.${k}`);
}

/** Deep copy through JSON semantics (drops `undefined` object fields, detaches shared references). */
export function plain<T>(v: T, path = '$'): T {
  checkPlain(v, path);
  return JSON.parse(JSON.stringify(v)) as T;
}

export function writeText(path: string, text: string): void {
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, text, 'utf8');
}

/** Writes `{schemaVersion, source, ...data}` and returns the text written. */
export function writeTable(ctx: Ctx, t: TableResult): string {
  const doc = { schemaVersion: SCHEMA_VERSION, source: t.source, ...t.data };
  const text = formatJson(doc) + '\n';
  writeText(resolve(ctx.outRoot, 'Data', t.file), text);
  return text;
}

// ── TS source assertions ────────────────────────────────────────────────────

const sourceCache = new Map<string, string>();
let repoRootForSource = '';

export function setRepoRoot(root: string): void { repoRootForSource = root; }

export function readSource(file: string): string {
  if (!sourceCache.has(file)) sourceCache.set(file, readFileSync(resolve(repoRootForSource, file), 'utf8'));
  return sourceCache.get(file)!;
}

/**
 * The exporter transcribes a few rules that live inside code (not in exported tables). Each such
 * value is pinned to the exact TS text it was read from; if the web source changes the export
 * fails here instead of drifting silently. Whitespace runs are normalised.
 */
export function assertSource(file: string, ...snippets: string[]): void {
  const norm = (s: string) => s.replace(/\s+/g, ' ');
  const src = norm(readSource(file));
  for (const s of snippets) {
    if (!src.includes(norm(s))) {
      throw new Error(`TS source drift: ${file} no longer contains:\n    ${s}\n  Update the exporter table that cites it.`);
    }
  }
}

export function fail(msg: string): never {
  throw new Error(msg);
}

export function assert(cond: unknown, msg: string): asserts cond {
  if (!cond) throw new Error(`export check failed: ${msg}`);
}

/** Deterministic PRNG (mulberry32) used to replace Math.random while the exporter runs. */
export function mulberry32(seed: number): () => number {
  let a = seed >>> 0;
  return () => {
    a = (a + 0x6d2b79f5) >>> 0;
    let t = a;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

/** Runs `fn` with Math.random returning the scripted values (then a fixed 0.5). */
export function withRandom<T>(values: number[], fn: () => T): T {
  const saved = Math.random;
  let i = 0;
  Math.random = () => (i < values.length ? values[i++] : 0.5);
  try { return fn(); } finally { Math.random = saved; }
}

export function countKeys(o: object): number { return Object.keys(o).length; }
