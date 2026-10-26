import { readFileSync, readdirSync, existsSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

/** Preserve the notices of packages whose code reaches the browser bundle. */
export function dependencyNotices() {
  return {
    name: 'neden-dependency-notices',
    apply: 'build',
    applyToEnvironment(environment) { return environment.config.consumer === 'client'; },
    generateBundle(_options, bundle) {
      const packages = new Map();
      function includePackage(modulePath) {
        let directory = dirname(modulePath.split('?')[0]);
        while (directory !== dirname(directory)) {
          const manifestPath = join(directory, 'package.json');
          if (existsSync(manifestPath)) {
            const manifest = JSON.parse(readFileSync(manifestPath, 'utf8'));
            if (manifest.name) {
              packages.set(`${manifest.name}@${manifest.version}`, directory);
              return;
            }
          }
          directory = dirname(directory);
        }
      }
      for (const output of Object.values(bundle)) {
        if (output.type !== 'chunk') continue;
        for (const modulePath of Object.keys(output.modules)) {
          if (modulePath.includes('/node_modules/') && !modulePath.startsWith('\0'))
            includePackage(modulePath);
        }
      }
      // shadcn primitives are copied into our source rather than imported at runtime.
      includePackage(resolve(dirname(fileURLToPath(import.meta.url)), '../node_modules/shadcn/package.json'));
      const notices = [...packages.entries()].sort().map(([name, directory]) => {
        const files = readdirSync(directory).filter(file => /^(licen[cs]e|copying|notice)(\.|-|$)/i.test(file));
        if (!files.length) this.error(`Missing browser dependency notice: ${name}`);
        return `${name}\n${'='.repeat(name.length)}\n${files.map(file => readFileSync(join(directory, file), 'utf8')).join('\n')}`;
      });
      this.emitFile({ type: 'asset', fileName: 'licenses/browser-dependencies.txt', source: notices.join('\n\n') });
    },
  };
}
