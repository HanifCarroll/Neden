import { createLocalJWKSet, jwtVerify } from "jose";
import type { Env } from "./environment";
import { ServiceError } from "./contracts";
import { identity } from "./identity";

export async function authenticate(request: Request, env: Env) {
  const auth = identity(env);
  const authorization = request.headers.get("Authorization");
  let subject: string;
  let tokenIssuedAt: number | undefined;
  if (authorization) {
    try {
      if (!authorization.startsWith("Bearer ") || authorization.length > 16000)
        throw new Error("Invalid token");
      const keys = await auth.api.getJwks();
      const { payload } = await jwtVerify(
        authorization.slice(7),
        createLocalJWKSet(keys),
        {
          issuer: `${env.API_ORIGIN}/api/auth`,
          audience: env.API_ORIGIN,
          algorithms: ["RS256"],
          requiredClaims: ["exp", "iat", "sub"],
        },
      );
      if (
        !payload.sub ||
        typeof payload.scope !== "string" ||
        !payload.scope.split(" ").includes("study")
      )
        throw new Error("Invalid scope");
      subject = payload.sub;
      tokenIssuedAt = payload.iat;
    } catch {
      throw new ServiceError(
        401,
        "sign_in_required",
        "Your session expired. Sign in again.",
      );
    }
  } else {
    const session = await auth.api.getSession({ headers: request.headers });
    if (!session?.user.emailVerified)
      throw new ServiceError(
        401,
        "sign_in_required",
        "Sign in to your Neden account.",
      );
    if (
      !["GET", "HEAD"].includes(request.method) &&
      request.headers.get("Origin") !== env.WEBSITE_ORIGIN
    )
      throw new ServiceError(
        403,
        "invalid_origin",
        "This request did not originate from Neden.",
      );
    subject = session.user.id;
  }
  const user = await env.DB.prepare(
    "SELECT emailVerified FROM user WHERE id = ?",
  )
    .bind(subject)
    .first<{ emailVerified: number }>();
  if (!user?.emailVerified)
    throw new ServiceError(
      401,
      "sign_in_required",
      "Sign in to your Neden account.",
    );
  if (tokenIssuedAt !== undefined) {
    const revocation = await env.DB.prepare(
      "SELECT revoked_through FROM token_revocations WHERE user_id = ?",
    )
      .bind(subject)
      .first<{ revoked_through: number }>();
    if (revocation && tokenIssuedAt <= revocation.revoked_through)
      throw new ServiceError(
        401,
        "sign_in_required",
        "Your session expired. Sign in again.",
      );
  }
  const account = await env.DB.prepare(
    "SELECT deletion_requested_at FROM billing_accounts WHERE account_id = ?",
  )
    .bind(subject)
    .first<{ deletion_requested_at: number | null }>();
  if (account?.deletion_requested_at)
    throw new ServiceError(
      403,
      "account_deleted",
      "Your account is being deleted.",
    );
  return { id: subject, subject };
}
