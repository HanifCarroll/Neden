import { copyFileSync, readFileSync, realpathSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';

const pluginDirectory = resolve(import.meta.dir, '../apps/iina-plugin');
const packageDirectory = process.argv[2];
if (!packageDirectory) {
  throw new Error('A plugin output directory is required.');
}

// 1. Resolve dependencies from markdown-it, including Bun's isolated workspace links.
const markdownEntry = Bun.resolveSync('markdown-it', pluginDirectory);
const dependencies = [
  ['markdown-it', 'LICENSE'],
  ['entities', 'LICENSE'],
  ['linkify-it', 'LICENSE'],
  ['mdurl', 'LICENSE'],
  ['uc.micro', 'LICENSE.txt'],
  ['punycode.js', 'LICENSE-MIT.txt'],
  ['argparse', 'LICENSE']
];

// 2. Locate each package's own notice without depending on the installer's layout.
for (const [name, filename] of dependencies) {
  let directory = dirname(realpathSync(Bun.resolveSync(name, dirname(markdownEntry))));
  while (true) {
    try {
      const metadata = JSON.parse(readFileSync(join(directory, 'package.json'), 'utf8'));
      if (metadata.name === name) {
        break;
      }
    } catch (error) {
      if ((error as NodeJS.ErrnoException).code !== 'ENOENT') {
        throw error;
      }
    }

    const parent = dirname(directory);
    if (parent === directory) {
      throw new Error(`Cannot locate the notice for ${name}.`);
    }

    directory = parent;
  }
  copyFileSync(join(directory, filename), join(packageDirectory, 'licenses', `${name}.LICENSE`));
}
