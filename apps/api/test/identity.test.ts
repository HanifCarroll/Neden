import { describe, expect, it } from "vitest";
import { env } from "cloudflare:test";
import { registerDesktopClient } from "../src/desktop-registration";
import { identity } from "../src/identity";
import { authenticate } from "../src/auth";
import type { Env } from "../src/environment";
const emails: { text: string }[] = [];
const settings: Env = {
  ...env,
  BETTER_AUTH_SECRET: "local-tests-only-secret-with-more-than-32-characters",
  EMAIL_FROM: "accounts@example.com",
  EMAIL: {
    async send(message) {
      emails.push(message);
    },
  },
};
function request(
  path: string,
  body?: unknown,
  cookie?: string,
  origin = settings.WEBSITE_ORIGIN,
) {
  return new Request(`${settings.API_ORIGIN}/api/auth/${path}`, {
    method: body ? "POST" : "GET",
    headers: {
      "Content-Type": "application/json",
      Origin: origin,
      ...(cookie ? { Cookie: cookie } : {}),
    },
    ...(body ? { body: JSON.stringify(body) } : {}),
  });
}
describe("account lifecycle", () => {
  it("requires verification, signs in, and rejects cross-site API writes", async () => {
    const auth = identity(settings);
    const email = `person-${crypto.randomUUID()}@example.com`;
    const password = "a-long-test-only-passphrase";
    const signup = await auth.handler(
      request("sign-up/email", { name: "Test", email, password }),
    );
    expect(signup.status).toBe(200);
    const before = await auth.handler(
      request("sign-in/email", { email, password }),
    );
    expect(before.status).toBe(403);
    const link = emails.at(-1)!.text.match(/https?:\/\/[^\s]+/)![0];
    expect((await auth.handler(new Request(link))).status).toBeLessThan(400);
    const login = await auth.handler(
      request("sign-in/email", { email, password }),
    );
    expect(login.status).toBe(200);
    const cookie = login.headers
      .getSetCookie()
      .map((value) => value.split(";")[0])
      .join("; ");
    expect(cookie).toContain("session_token");
    const current = await authenticate(
      new Request(`${settings.API_ORIGIN}/v1/account`, {
        headers: { Cookie: cookie },
      }),
      settings,
    );
    expect(current.id).toBeTruthy();
    await expect(
      authenticate(
        new Request(`${settings.API_ORIGIN}/v1/billing/checkout`, {
          method: "POST",
          headers: { Cookie: cookie, Origin: "https://other.example.com" },
        }),
        settings,
      ),
    ).rejects.toMatchObject({ code: "invalid_origin" });
    await registerDesktopClient(settings);
    const verifier = "a".repeat(64);
    const digest = await crypto.subtle.digest(
      "SHA-256",
      new TextEncoder().encode(verifier),
    );
    const challenge = btoa(String.fromCharCode(...new Uint8Array(digest)))
      .replaceAll("+", "-")
      .replaceAll("/", "_")
      .replaceAll("=", "");
    const parameters = new URLSearchParams({
      client_id: "neden-desktop",
      response_type: "code",
      redirect_uri: "http://127.0.0.1:49234/oauth/callback",
      scope: "openid offline_access study",
      resource: settings.API_ORIGIN,
      state: "random-test-state",
      code_challenge: challenge,
      code_challenge_method: "S256",
    });
    const authorized = await auth.handler(
      request(`oauth2/authorize?${parameters}`, undefined, cookie),
    );
    expect(authorized.status).toBe(302);
    const redirect = new URL(authorized.headers.get("Location")!);
    expect(redirect.origin).toBe("http://127.0.0.1:49234");
    expect(redirect.searchParams.get("state")).toBe("random-test-state");
    const code = redirect.searchParams.get("code")!;
    const exchange = (codeVerifier: string) =>
      auth.handler(
        new Request(`${settings.API_ORIGIN}/api/auth/oauth2/token`, {
          method: "POST",
          headers: { "Content-Type": "application/x-www-form-urlencoded" },
          body: new URLSearchParams({
            grant_type: "authorization_code",
            code,
            client_id: "neden-desktop",
            redirect_uri: "http://127.0.0.1:49234/oauth/callback",
            code_verifier: codeVerifier,
            resource: settings.API_ORIGIN,
          }),
        }),
      );
    expect((await exchange("b".repeat(64))).status).toBe(400);
    // A failed verifier attempt may consume a code, so start a fresh authorization.
    const authorizedAgain = await auth.handler(
      request(`oauth2/authorize?${parameters}`, undefined, cookie),
    );
    const freshCode = new URL(
      authorizedAgain.headers.get("Location")!,
    ).searchParams.get("code")!;
    const tokenRequest = new Request(
      `${settings.API_ORIGIN}/api/auth/oauth2/token`,
      {
        method: "POST",
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body: new URLSearchParams({
          grant_type: "authorization_code",
          code: freshCode,
          client_id: "neden-desktop",
          redirect_uri: "http://127.0.0.1:49234/oauth/callback",
          code_verifier: verifier,
          resource: settings.API_ORIGIN,
        }),
      },
    );
    const tokensResponse = await auth.handler(tokenRequest.clone());
    expect(tokensResponse.status).toBe(200);
    const tokens = (await tokensResponse.json()) as {
      access_token: string;
      refresh_token: string;
    };
    expect(tokens.refresh_token).toBeTruthy();
    expect(
      (
        await authenticate(
          new Request(`${settings.API_ORIGIN}/v1/account`, {
            headers: { Authorization: `Bearer ${tokens.access_token}` },
          }),
          settings,
        )
      ).id,
    ).toBe(current.id);
    expect((await auth.handler(tokenRequest.clone())).status).toBe(400);
    expect((await auth.handler(request("sign-out", {}, cookie))).status).toBe(
      200,
    );
    await expect(
      authenticate(
        new Request(`${settings.API_ORIGIN}/v1/account`, {
          headers: { Cookie: cookie },
        }),
        settings,
      ),
    ).rejects.toMatchObject({ code: "sign_in_required" });
    await auth.handler(
      request("request-password-reset", {
        email,
        redirectTo: `${settings.WEBSITE_ORIGIN}/account/`,
      }),
    );
    const recovery = await auth.handler(
      new Request(emails.at(-1)!.text.match(/https?:\/\/[^\s]+/)![0]),
    );
    const recoveryToken = new URL(
      recovery.headers.get("Location")!,
    ).searchParams.get("token");
    expect(
      (
        await auth.handler(
          request("reset-password", {
            token: recoveryToken,
            newPassword: "replacement-long-test-password",
          }),
        )
      ).status,
    ).toBe(200);
    await expect(
      authenticate(
        new Request(`${settings.API_ORIGIN}/v1/account`, {
          headers: { Authorization: `Bearer ${tokens.access_token}` },
        }),
        settings,
      ),
    ).rejects.toMatchObject({ code: "sign_in_required" });
    const refresh = await auth.handler(
      new Request(`${settings.API_ORIGIN}/api/auth/oauth2/token`, {
        method: "POST",
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body: new URLSearchParams({
          grant_type: "refresh_token",
          client_id: "neden-desktop",
          refresh_token: tokens.refresh_token,
          resource: settings.API_ORIGIN,
        }),
      }),
    );
    expect(refresh.status).toBe(400);
  });
  it("resets passwords, revokes old sessions, and deletes accounts", async () => {
    const auth = identity(settings);
    const email = `reset-${crypto.randomUUID()}@example.com`;
    const password = "first-long-test-passphrase";
    await auth.handler(
      request("sign-up/email", { name: "Test", email, password }),
    );
    const verifyUrl = emails.at(-1)!.text.match(/https?:\/\/[^\s]+/)![0];
    await auth.handler(new Request(verifyUrl));
    const login = await auth.handler(
      request("sign-in/email", { email, password }),
    );
    const cookie = login.headers
      .getSetCookie()
      .map((value) => value.split(";")[0])
      .join("; ");
    const account = await authenticate(
      new Request(`${settings.API_ORIGIN}/v1/account`, {
        headers: { Cookie: cookie },
      }),
      settings,
    );
    const reset = await auth.handler(
      request("request-password-reset", {
        email,
        redirectTo: `${settings.WEBSITE_ORIGIN}/account/`,
      }),
    );
    expect(reset.status).toBe(200);
    const resetUrl = emails.at(-1)!.text.match(/https?:\/\/[^\s]+/)![0];
    const redirected = await auth.handler(new Request(resetUrl));
    const token = new URL(redirected.headers.get("Location")!).searchParams.get(
      "token",
    );
    const newPassword = "second-long-test-passphrase";
    expect(
      (await auth.handler(request("reset-password", { token, newPassword })))
        .status,
    ).toBe(200);
    await expect(
      authenticate(
        new Request(`${settings.API_ORIGIN}/v1/account`, {
          headers: { Cookie: cookie },
        }),
        settings,
      ),
    ).rejects.toMatchObject({ code: "sign_in_required" });
    expect(
      (await auth.handler(request("sign-in/email", { email, password })))
        .status,
    ).toBe(401);
    const newLogin = await auth.handler(
      request("sign-in/email", { email, password: newPassword }),
    );
    const newCookie = newLogin.headers
      .getSetCookie()
      .map((value) => value.split(";")[0])
      .join("; ");
    expect(
      (
        await auth.handler(
          request("delete-user", { password: "wrong-password" }, newCookie),
        )
      ).status,
    ).toBeGreaterThanOrEqual(400);
    expect(
      (
        await auth.handler(
          request("delete-user", { password: newPassword }, newCookie),
        )
      ).status,
    ).toBe(200);
    expect(
      await settings.DB.prepare("SELECT id FROM user WHERE id = ?")
        .bind(account.id)
        .first(),
    ).toBeNull();
    expect(
      (
        await settings.DB.prepare(
          "SELECT deletion_requested_at FROM billing_accounts WHERE account_id = ?",
        )
          .bind(account.id)
          .first()
      )?.deletion_requested_at,
    ).toBeTruthy();
  });
  it("does not allow standalone JWT minting or dynamic client registration", async () => {
    const auth = identity(settings);
    expect((await auth.handler(request("token"))).status).toBe(404);
    expect(
      (
        await auth.handler(
          request("oauth2/register", {
            client_name: "untrusted",
            redirect_uris: ["https://other.example.com"],
          }),
        )
      ).status,
    ).toBeGreaterThanOrEqual(400);
  });
});
