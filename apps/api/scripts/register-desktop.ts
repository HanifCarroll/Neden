import { desktopRegistrationStatements } from "../src/desktop-registration";
import { readFile, mkdtemp, writeFile, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { spawnSync } from "node:child_process";

const environment = process.argv[2];
if (!environment)
  throw new Error("Usage: bun scripts/register-desktop.ts <environment>");
const configuration = JSON.parse(
  await readFile(new URL("../wrangler.jsonc", import.meta.url), "utf8"),
);
const selected = configuration.env?.[environment];
const origin = selected?.vars?.API_ORIGIN;
const database = selected?.d1_databases?.find(
  (binding: { binding: string }) => binding.binding === "DB",
);
if (!origin || !database || new URL(origin).protocol !== "https:")
  throw new Error(
    "Select an environment with an HTTPS API origin and D1 database.",
  );
const temporaryDirectory = await mkdtemp(join(tmpdir(), "neden-registration-"));
try {
  const path = join(temporaryDirectory, "registration.sql");
  await writeFile(
    path,
    desktopRegistrationStatements(origin).join(";\n") + ";\n",
  );
  const result = spawnSync(
    "bunx",
    [
      "wrangler",
      "d1",
      "execute",
      database.database_name,
      "--remote",
      "--env",
      environment,
      "--file",
      path,
    ],
    { cwd: new URL("..", import.meta.url), stdio: "inherit" },
  );
  if (result.status !== 0) throw new Error("Desktop registration failed.");
} finally {
  await rm(temporaryDirectory, { recursive: true });
}
