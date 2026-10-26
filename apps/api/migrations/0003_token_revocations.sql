-- Password recovery must invalidate already-issued desktop JWTs as well as sessions.
CREATE TABLE token_revocations (
  user_id TEXT PRIMARY KEY REFERENCES user(id) ON DELETE CASCADE,
  revoked_through INTEGER NOT NULL
);
