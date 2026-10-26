CREATE TABLE IF NOT EXISTS billing_accounts (
  account_id TEXT PRIMARY KEY,
  stripe_customer_id TEXT UNIQUE,
  deletion_requested_at INTEGER,
  created_at INTEGER NOT NULL DEFAULT (unixepoch())
);
CREATE TABLE IF NOT EXISTS billing_events (
  event_id TEXT PRIMARY KEY,
  processed_at INTEGER NOT NULL DEFAULT (unixepoch())
);
