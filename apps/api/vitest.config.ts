import { defineConfig } from "vitest/config";
import { cloudflareTest, readD1Migrations } from "@cloudflare/vitest-plugin";
// Unit tests must never load local provider credentials.
process.env.CLOUDFLARE_LOAD_DEV_VARS_FROM_DOT_ENV = "false";
export default defineConfig({
  plugins: [
    cloudflareTest({
      wrangler: { configPath: "./wrangler.jsonc" },
      miniflare: {
        bindings: {
          TEST_MIGRATIONS: await readD1Migrations("./migrations"),
          BETTER_AUTH_SECRET: "",
          DEEPSEEK_API_KEY: "",
          STRIPE_SECRET_KEY: "",
          STRIPE_WEBHOOK_SECRET: "",
          SANDBOX_ACCOUNT_IDS: "",
          AI_ENABLED: "false",
        },
      },
    }),
  ],
  test: { setupFiles: ["./test/setup.ts"] },
});
