import type { Env } from "./environment";
import { desktopClientId } from "./identity";

// Run during deployment, never from a public request. No desktop client secret exists.
export async function registerDesktopClient(
  env: Pick<Env, "DB" | "API_ORIGIN">,
) {
  for (const statement of desktopRegistrationStatements(env.API_ORIGIN))
    await env.DB.prepare(statement).run();
}

export function desktopRegistrationStatements(apiOrigin: string): string[] {
  const literal = (value: string) => "'" + value.replaceAll("'", "''") + "'";
  const scopes = JSON.stringify([
    "openid",
    "profile",
    "email",
    "offline_access",
    "study",
  ]);
  return [
    `INSERT OR IGNORE INTO oauthClient
      (id, clientId, name, redirectUris, tokenEndpointAuthMethod, applicationType, grantTypes, responseTypes, scopes, requirePKCE, skipConsent, disabled)
      VALUES (${literal(desktopClientId)}, ${literal(desktopClientId)}, 'Neden desktop',
        ${literal(JSON.stringify(["http://127.0.0.1/oauth/callback"]))}, 'none', 'native',
        ${literal(JSON.stringify(["authorization_code", "refresh_token"]))},
        ${literal(JSON.stringify(["code"]))}, ${literal(scopes)}, 1, 1, 0)`,
    `INSERT OR IGNORE INTO oauthResource (id, identifier, name, allowedScopes, disabled)
      VALUES ('neden-api', ${literal(apiOrigin)}, 'Neden AI', ${literal(scopes)}, 0)`,
    `INSERT OR IGNORE INTO oauthClientResource (id, clientId, resourceId)
      VALUES ('neden-desktop-api', ${literal(desktopClientId)}, ${literal(apiOrigin)})`,
  ];
}
