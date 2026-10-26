import type { Env } from "./environment";
import { ServiceError, studyRequest } from "./contracts";
import { boundedJson, boundedText } from "./http";
import { callProvider, tokenUsage } from "./provider";

export async function study(
  request: Request,
  env: Env,
  accountId: string,
  ctx: { waitUntil(promise: Promise<unknown>): void },
) {
  const parsed = studyRequest.safeParse(await boundedJson(request));
  if (!parsed.success)
    throw new ServiceError(
      400,
      "invalid_request",
      parsed.error.issues[0].message,
    );
  const requestId = request.headers.get("Idempotency-Key");
  if (!requestId || !/^[a-zA-Z0-9-]{16,80}$/.test(requestId))
    throw new ServiceError(
      400,
      "request_id_required",
      "A unique request ID is required.",
    );
  if (env.AI_ENABLED !== "true" || !env.DEEPSEEK_API_KEY)
    throw new ServiceError(
      503,
      "ai_unavailable",
      "Hosted AI is temporarily unavailable.",
    );
  const usage = env.ACCOUNT_USAGE.get(env.ACCOUNT_USAGE.idFromName(accountId));
  const reserved = await usage.reserve(requestId, parsed.data.operation);
  if (!reserved.ok)
    throw new ServiceError(reserved.status, reserved.code, reserved.message);
  const global = env.ACCOUNT_USAGE.get(
    env.ACCOUNT_USAGE.idFromName("__global_budget"),
  );
  if (!(await global.globalBudget())) {
    await usage.finish(requestId, null, null, true);
    throw new ServiceError(
      503,
      "daily_capacity",
      "Hosted AI has reached its daily capacity. Please try tomorrow.",
    );
  }
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), 90000);
  request.signal.addEventListener("abort", () => controller.abort(), {
    once: true,
  });
  let upstream: Response;
  try {
    upstream = await callProvider(env, parsed.data, controller.signal);
  } catch {
    clearTimeout(timeout);
    await usage.finish(requestId, null, null);
    throw new ServiceError(
      502,
      "provider_unavailable",
      "The AI connection was interrupted.",
    );
  }
  if (!upstream.ok || !upstream.body) {
    clearTimeout(timeout);
    await upstream.body?.cancel();
    await usage.finish(requestId, null, null, true);
    throw new ServiceError(
      502,
      "provider_unavailable",
      "AI is unavailable. Your allowance was restored.",
    );
  }
  if (parsed.data.operation === "word") {
    try {
      const text = await boundedText(upstream, 64000);
      const result = JSON.parse(text);
      const tokens = tokenUsage(result);
      await usage.finish(requestId, tokens.input, tokens.output);
      return Response.json(result);
    } catch {
      await usage.finish(requestId, null, null);
      throw new ServiceError(
        502,
        "invalid_answer",
        "The word answer was interrupted. Please try again.",
      );
    } finally {
      clearTimeout(timeout);
    }
  }
  // Read only bounded SSE frames to extract usage; never persist conversation text.
  const reader = upstream.body.getReader();
  const decoder = new TextDecoder();
  let buffer = "";
  let total = 0;
  let tokens: ReturnType<typeof tokenUsage> = { input: null, output: null };
  let finished = false;
  const finish = async () => {
    if (finished) return;
    finished = true;
    clearTimeout(timeout);
    await usage.finish(requestId, tokens.input, tokens.output);
  };
  const stream = new ReadableStream<Uint8Array>({
    async pull(destination) {
      try {
        const { done, value } = await reader.read();
        if (done) {
          await finish();
          destination.close();
          return;
        }
        total += value.length;
        if (total > 256000) throw new Error("Response too large");
        buffer += decoder.decode(value, { stream: true });
        const lines = buffer.split("\n");
        buffer = lines.pop() ?? "";
        if (buffer.length > 64000) throw new Error("SSE frame too large");
        for (const line of lines) {
          if (!line.startsWith("data: ") || line.trim() === "data: [DONE]")
            continue;
          try {
            const next = tokenUsage(JSON.parse(line.slice(6)));
            if (next.input !== null) tokens = next;
          } catch {
            /* Non-JSON provider line. */
          }
        }
        destination.enqueue(value);
      } catch {
        controller.abort();
        await reader.cancel().catch(() => {});
        await finish();
        destination.error(new Error("The answer was interrupted."));
      }
    },
    async cancel() {
      controller.abort();
      await reader.cancel().catch(() => {});
      ctx.waitUntil(finish());
    },
  });
  return new Response(stream, {
    headers: {
      "Content-Type": "text/event-stream",
      "Cache-Control": "no-store",
    },
  });
}
