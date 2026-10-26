import { betterAuth } from "better-auth";
import { jwt } from "better-auth/plugins";
import { oauthProvider } from "@better-auth/oauth-provider";
import type { Env } from "./environment";
import { ServiceError } from "./contracts";
import { accountEmail, type AccountEmailPurpose } from "./account-email";

export const desktopClientId = "neden-desktop";

export function identityOptions(env: Env) {
  return {
    appName: "Neden",
    baseURL: env.API_ORIGIN,
    basePath: "/api/auth",
    secret: env.BETTER_AUTH_SECRET,
    database: env.DB,
    trustedOrigins: [env.WEBSITE_ORIGIN],
    logger: { disabled: true },
    emailAndPassword: {
      enabled: true,
      requireEmailVerification: true,
      minPasswordLength: 12,
      maxPasswordLength: 128,
      revokeSessionsOnPasswordReset: true,
      onPasswordReset: async ({ user }: { user: { id: string } }) => {
        await env.DB.batch([
          env.DB.prepare(
            `INSERT INTO token_revocations (user_id, revoked_through) VALUES (?, unixepoch())
            ON CONFLICT(user_id) DO UPDATE SET revoked_through = unixepoch()`,
          ).bind(user.id),
          env.DB.prepare("DELETE FROM oauthAccessToken WHERE userId = ?").bind(
            user.id,
          ),
          env.DB.prepare("DELETE FROM oauthRefreshToken WHERE userId = ?").bind(
            user.id,
          ),
        ]);
      },
      sendResetPassword: async ({
        user,
        url,
      }: {
        user: { email: string };
        url: string;
      }) => sendAccountEmail(env, user.email, "password-reset", url),
    },
    emailVerification: {
      sendOnSignUp: true,
      sendOnSignIn: true,
      autoSignInAfterVerification: true,
      sendVerificationEmail: async ({
        user,
        url,
      }: {
        user: { email: string };
        url: string;
      }) => sendAccountEmail(env, user.email, "verification", url),
    },
    user: {
      deleteUser: {
        enabled: true,
        beforeDelete: async (user: { id: string }) => {
          await env.ACCOUNT_USAGE.get(
            env.ACCOUNT_USAGE.idFromName(user.id),
          ).closeAccount(user.id);
        },
      },
    },
    session: { expiresIn: 30 * 86400, updateAge: 86400, freshAge: 600 },
    rateLimit: {
      enabled: true,
      storage: "database" as const,
      window: 60,
      max: 60,
      customRules: {
        "/sign-up/email": { window: 3600, max: 5 },
        "/sign-in/email": { window: 60, max: 5 },
        "/request-password-reset": { window: 3600, max: 5 },
        "/send-verification-email": { window: 3600, max: 5 },
      },
    },
    advanced: { ipAddress: { ipAddressHeaders: ["cf-connecting-ip"] } },
    disabledPaths: ["/token"],
    plugins: [
      jwt({ jwks: { keyPairConfig: { alg: "RS256" } } }),
      oauthProvider({
        loginPage: `${env.WEBSITE_ORIGIN}/account`,
        consentPage: `${env.WEBSITE_ORIGIN}/account`,
        scopes: ["openid", "profile", "email", "offline_access", "study"],
        grantTypes: ["authorization_code", "refresh_token"],
        accessTokenExpiresIn: 600,
        resources: [
          {
            identifier: env.API_ORIGIN,
            allowedScopes: [
              "openid",
              "profile",
              "email",
              "offline_access",
              "study",
            ],
          },
        ],
        clientRegistrationDefaultResources: [env.API_ORIGIN],
        cachedTrustedClients: new Set([desktopClientId]),
        allowDynamicClientRegistration: false,
        allowUnauthenticatedClientRegistration: false,
        clientPrivileges: async () => false,
        resourcePrivileges: async () => false,
      }),
    ],
  };
}

export function identity(env: Env) {
  if (!env.BETTER_AUTH_SECRET || env.BETTER_AUTH_SECRET.length < 32)
    throw new ServiceError(
      503,
      "login_unavailable",
      "Account sign-in is not configured yet.",
    );
  // Each Worker request owns its initialization; an aborted request cannot poison later ones.
  return betterAuth(identityOptions(env));
}

async function sendAccountEmail(
  env: Env,
  email: string,
  purpose: AccountEmailPurpose,
  url: string,
) {
  if (!env.EMAIL || !env.EMAIL_FROM)
    throw new Error("Account email is unavailable.");
  await env.EMAIL.send({
    from: { email: env.EMAIL_FROM, name: "Neden" },
    to: email,
    ...accountEmail(purpose, url),
  });
}
