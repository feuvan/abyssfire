// Minimal typings for the few Node built-ins the exporter uses (the repo has no @types/node and the
// exporter must not add root dependencies). Only used by `tsc -p unreal/Tools/export-data`.
declare module 'node:fs' {
  export function readFileSync(path: string, encoding: 'utf8'): string;
  export function writeFileSync(path: string, data: string, encoding: 'utf8'): void;
  export function mkdirSync(path: string, opts?: { recursive?: boolean }): void;
  export function existsSync(path: string): boolean;
  export function readdirSync(path: string, opts: { withFileTypes: true }): { name: string; isDirectory(): boolean }[];
}
declare module 'node:path' {
  export function resolve(...parts: string[]): string;
  export function relative(from: string, to: string): string;
  export function dirname(p: string): string;
}
declare module 'node:crypto' {
  export function createHash(alg: string): { update(s: string): { digest(enc: 'hex'): string } };
}
