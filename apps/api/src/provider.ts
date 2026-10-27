import type { StudyRequest } from "./contracts";
import { ServiceError } from "./contracts";
import { wordPrompt, explanationPrompt, translationPrompt } from "./prompts";
import type { Env } from "./environment";

export function providerBody(request: StudyRequest) {
  const messages: { role: string; content: string }[] = [
    {
      role: "system",
      content:
        request.operation === "translate"
          ? translationPrompt
          : request.operation === "word"
            ? wordPrompt
            : explanationPrompt,
    },
    {
      role: "user",
      content: JSON.stringify(
        request.operation === "translate"
          ? request
          : request.operation === "word"
            ? { ...request.context, word: request.word }
            : request.context,
      ),
    },
  ];
  if (request.operation === "followup") {
    for (const turn of request.history) {
      if (turn.question)
        messages.push({ role: "user", content: turn.question });
      messages.push({ role: "assistant", content: turn.answer });
    }
    messages.push({ role: "user", content: request.question });
  }
  return {
    model: "deepseek-flash",
    messages,
    thinking: { type: "disabled" },
    max_tokens:
      request.operation === "translate"
        ? 4096
        : request.operation === "word"
          ? 900
          : 1600,
    stream: request.operation !== "word" && request.operation !== "translate",
    ...(request.operation === "word" || request.operation === "translate"
      ? { response_format: { type: "json_object" } }
      : { stream_options: { include_usage: true } }),
  };
}
export async function callProvider(
  env: Env,
  request: StudyRequest,
  signal: AbortSignal,
) {
  if (env.AI_ENABLED !== "true" || !env.DEEPSEEK_API_KEY)
    throw new ServiceError(
      503,
      "ai_unavailable",
      "Hosted AI is temporarily unavailable.",
    );
  return fetch("https://api.deepseek.com/chat/completions", {
    method: "POST",
    // Workers supports manual/follow; a 3xx is rejected by the caller like any non-2xx.
    redirect: "manual",
    signal,
    headers: {
      Authorization: `Bearer ${env.DEEPSEEK_API_KEY}`,
      "Content-Type": "application/json",
    },
    body: JSON.stringify(providerBody(request)),
  });
}
export function tokenUsage(value: unknown): {
  input: number | null;
  output: number | null;
} {
  const usage = (
    value as {
      usage?: { prompt_tokens?: unknown; completion_tokens?: unknown };
    }
  )?.usage;
  const number = (v: unknown) =>
    typeof v === "number" && Number.isSafeInteger(v) && v >= 0 ? v : null;
  return {
    input: number(usage?.prompt_tokens),
    output: number(usage?.completion_tokens),
  };
}
