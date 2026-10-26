import type { Env } from "./environment";
import { desktopClientId } from "./identity";

// Run during deployment, never from a public request. No desktop client secret exists.
export async function registerDesktopClient(
  env: Pick<Env, "DB" | "API_ORIGIN">,
) {
  const scopes = JSON.stringify([
    "openid",
    "profile",
    "email",
    "offline_access",
    "study",
  ]);
  await env.DB.batch([
    env.DB.prepare(
      `INSERT OR IGNORE INTO oauthClient
      (id, clientId, name, redirectUris, tokenEndpointAuthMethod, applicationType, grantTypes, responseTypes, scopes, requirePKCE, skipConsent, disabled)
      VALUES (?, ?, 'Neden desktop', ?, 'none', 'native', ?, ?, ?, 1, 1, 0)`,
    ).bind(
      desktopClientId,
      desktopClientId,
      JSON.stringify(["http://127.0.0.1/oauth/callback"]),
      JSON.stringify(["authorization_code", "refresh_token"]),
      JSON.stringify(["code"]),
      scopes,
    ),
    env.DB.prepare(
      `INSERT OR IGNORE INTO oauthResource (id, identifier, name, allowedScopes, disabled)
      VALUES ('neden-api', ?, 'Neden AI', ?, 0)`,
    ).bind(env.API_ORIGIN, scopes),
    env.DB.prepare(
      `INSERT OR IGNORE INTO oauthClientResource (id, clientId, resourceId)
      VALUES ('neden-desktop-api', ?, ?)`,
    ).bind(desktopClientId, env.API_ORIGIN),
  ]);
}
