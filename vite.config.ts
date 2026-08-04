import { defineConfig } from 'vite';
import { readdirSync } from 'node:fs';
import { resolve } from 'node:path';

function getBgmManifest(): Record<string, string> {
  const directory = resolve(process.cwd(), 'public/assets/audio/bgm');
  return Object.fromEntries(
    readdirSync(directory)
      .filter(file => file.endsWith('.mp3'))
      .map(file => [file.replace(/\.mp3$/, ''), `assets/audio/bgm/${file}`]),
  );
}

export default defineConfig({
  base: process.env.NODE_ENV === 'production' ? './' : '/abyssfire/',
  define: {
    __BGM_MANIFEST__: JSON.stringify(getBgmManifest()),
  },
  build: {
    rollupOptions: {
      output: {
        manualChunks(id) {
          if (id.includes('/node_modules/phaser/')) return 'vendor-phaser';
          if (id.includes('/node_modules/dexie/')) return 'vendor-dexie';
        },
      },
    },
  },
});
