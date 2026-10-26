import tailwindcss from '@tailwindcss/vite';
import { defineConfig } from 'astro/config';
import react from '@astrojs/react';
import sitemap from '@astrojs/sitemap';
export default defineConfig({
  site: 'https://neden.hanifcarroll.com',
  devToolbar: { enabled: false },
  vite: { plugins: [tailwindcss()] },
  integrations: [react(), sitemap()],
});
