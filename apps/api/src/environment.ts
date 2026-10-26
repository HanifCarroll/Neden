import type { AccountUsage } from "./usage";
export interface Env {
  DB: D1Database;
  ACCOUNT_USAGE: DurableObjectNamespace<AccountUsage>;
  WEBSITE_ORIGIN: string;
  API_ORIGIN: string;
  AI_ENABLED: string;
  GLOBAL_DAILY_REQUEST_LIMIT: string;
  BETTER_AUTH_SECRET?: string;
  EMAIL_FROM?: string;
  EMAIL?: {
    send(message: {
      from: { email: string; name: string };
      to: string;
      subject: string;
      text: string;
    }): Promise<unknown>;
  };
  DEEPSEEK_API_KEY?: string;
  STRIPE_SECRET_KEY?: string;
  STRIPE_WEBHOOK_SECRET?: string;
  STRIPE_PRICE_ID?: string;
  STRIPE_PORTAL_CONFIGURATION_ID?: string;
  BILLING_LIVE: string;
  SANDBOX_ACCOUNT_IDS?: string;
  MONTHLY_WORD_LIMIT: string;
  MONTHLY_EXPLANATION_LIMIT: string;
}
