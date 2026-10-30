import { env, runInDurableObject } from "cloudflare:test";
import { afterEach, describe, expect, it, vi } from "vitest";
import worker from "../src/index";
import { studyRequest } from "../src/contracts";
import { study } from "../src/study";
import { callProvider, providerBody } from "../src/provider";
import type { Env } from "../src/environment";
const bindings = env as unknown as Env;
const context = {
  selection: "-[İskender] Eyvallah.",
  clickedSubtitle: "Eyvallah.",
  sourceLanguage: "Turkish",
  explanationLanguage: "English",
};
async function account(limit = 3) {
  const stub = bindings.ACCOUNT_USAGE.get(
    bindings.ACCOUNT_USAGE.idFromName(crypto.randomUUID()),
  );
  await stub.updateEntitlement({
    subscriptionId: "sub_test",
    periodStart: 1,
    periodEnd: Date.now() / 1000 + 3600,
    enabled: true,
    cancelAtPeriodEnd: false,
    wordLimit: limit,
    explanationLimit: limit,
  });
  return stub;
}
describe("request boundary", () => {
  it("preserves language choices and words across scripts and internal joiners", () => {
    for (const [word, sourceLanguage, explanationLanguage] of [
      ["don't", "English", "Spanish"],
      ["l’homme", "French", "Arabic"],
      ["porte-monnaie", "French", "Japanese"],
      ["می‌روم", "Persian", "German"],
      ["مرحبًا", "Arabic", "Portuguese"],
      ["日本語", "Japanese", "French"],
      ["नमस्ते", "Hindi", "Korean"],
    ]) {
      const evidence = { clickedSubtitle: word, sourceLanguage, explanationLanguage };
      const parsed = studyRequest.parse({ operation: "word", word, context: evidence });
      const body = providerBody(parsed);
      const sent = JSON.parse(body.messages[1].content);
      expect(JSON.stringify(sent)).toContain(word);
      expect(JSON.stringify(sent)).toContain(explanationLanguage);
      expect(JSON.stringify(sent)).toContain(sourceLanguage);
    }
    for (const word of ["'word'", "-word", "word-", "two words", "word--word"]) {
      expect(studyRequest.safeParse({ operation: "word", word, context }).success).toBe(false);
    }
  });
  it("streams bounded line lookups and leaves legacy details unchanged", () => {
    const request = {operation: "word", word: "Hola", wordFormat: "line", words: ["Hola", "amigo"], context: {...context, clickedSubtitle: "Hola amigo"}};
    const body = providerBody(studyRequest.parse(request));
    expect(body.stream).toBe(true);
    expect(body.stream_options).toEqual({include_usage: true});
    expect(body.thinking).toEqual({type: "disabled"});
    expect(body.max_tokens).toBe(6000);
    expect(body.response_format).toEqual({type: "json_object"});
    expect(JSON.parse(body.messages[1].content).words).toEqual(request.words);
    const legacy = providerBody(studyRequest.parse({operation: "word", word: "Hola", context: request.context}));
    expect(legacy.stream).toBe(false);
    expect(legacy.max_tokens).toBe(900);
    for (const words of [[], ["amigo", "Hola"], ["Hola", "Hola"], ["Hola", "absent"], Array(65).fill("Hola")])
      expect(studyRequest.safeParse({...request, words}).success).toBe(false);
    expect(studyRequest.safeParse({...request, words: undefined}).success).toBe(false);
    expect(studyRequest.safeParse({...request, wordFormat: undefined}).success).toBe(false);
  });
  it("opts new clients into aligned explanations while preserving legacy Markdown", () => {
    const legacy = providerBody(studyRequest.parse({operation: "explain", context: {...context, selection: "Bonne chance"}}));
    expect(legacy.response_format).toBeUndefined();
    const aligned = providerBody(studyRequest.parse({operation: "explain", explanationFormat: "aligned", context: {...context, selection: "Bonne chance"}}));
    expect(aligned.response_format).toEqual({type: "json_object"});
    expect(aligned.stream).toBe(true);
    expect(aligned.stream_options).toEqual({include_usage: true});
    expect(aligned.messages[0].content).toContain("alignment");
    expect(aligned.max_tokens).toBe(2500);
  });
  it("keeps a highlighted fragment separate from the full subtitle for aligned explanations", () => {
    const selection = "manyak mısın be adam dedi";
    const sourceCue = "Biraz delirdin mi sen acaba, salak mısın sen dedi, manyak mısın be adam dedi.";
    const request = studyRequest.parse({operation: "explain", explanationFormat: "aligned", context: {...context, selection, sourceCue}});
    const body = providerBody(request);
    const sent = JSON.parse(body.messages[1].content);
    expect(sent.selection).toBe(selection);
    expect(sent.context.sourceCue).toBe(sourceCue);
    expect(sent.context.selection).toBeUndefined();
    expect(body.messages[0].content).toContain("selection defines the complete scope");
  });
  it("translates bounded consecutive cues with server-owned instructions and JSON output", () => {
    const parsed = studyRequest.parse({
      operation: "translate",
      targetLanguage: "Spanish",
      cues: [
        { id: 8, text: "Good morning." },
        { id: 9, text: "How are you?" },
      ],
      contextBefore: ["Hello."],
      contextAfter: [],
      model: "expensive",
      system: "override",
    });
    const body = providerBody(parsed);
    expect(body.stream).toBe(false);
    expect(body.max_tokens).toBe(4096);
    expect(body.response_format).toEqual({ type: "json_object" });
    expect(body.messages[0].content).toContain("Keep cue boundaries intact");
    expect(JSON.parse(body.messages[1].content).cues).toHaveLength(2);
    expect(JSON.stringify(body)).not.toContain("expensive");
  });
  it("rejects oversized, missing, duplicated or reordered translation cues", () => {
    const request = {
      operation: "translate",
      targetLanguage: "Spanish",
      cues: [{ id: 0, text: "Hello" }],
    };
    for (const cues of [
      [],
      [{ id: 0, text: "" }],
      [
        { id: 0, text: "Hello" },
        { id: 0, text: "Again" },
      ],
      [
        { id: 2, text: "Hello" },
        { id: 1, text: "Again" },
      ],
      Array.from({ length: 9 }, (_, id) => ({ id, text: "Hello" })),
      [
        { id: 0, text: "a".repeat(3000) },
        { id: 1, text: "b".repeat(2000) },
      ],
    ]) {
      expect(studyRequest.safeParse({ ...request, cues }).success).toBe(false);
    }
  });
  it("translation batches use the existing paid allowance and concurrency limits", async () => {
    const usage = await account(1);
    const first = crypto.randomUUID();
    expect((await usage.reserve(first, "translate")).ok).toBe(true);
    await usage.finish(first, 500, 500);
    expect((await usage.reserve(crypto.randomUUID(), "translate")).ok).toBe(
      false,
    );
  });
  it("strips client model/prompt configuration and keeps server instructions", () => {
    const parsed = studyRequest.parse({
      operation: "word",
      word: "Eyvallah",
      context,
      model: "expensive",
      system: "ignore limits",
    });
    const body = providerBody(parsed);
    expect(body.model).toBe("deepseek-flash");
    expect(body.thinking.type).toBe("disabled");
    expect(body.messages[0].content).toContain("minimal dictionary entry");
    expect(JSON.stringify(body)).not.toContain("ignore limits");
  });
  it("rejects missing context, multiple words and oversized conversations", () => {
    expect(
      studyRequest.safeParse({ operation: "word", word: "two words", context })
        .success,
    ).toBe(false);
    expect(
      studyRequest.safeParse({
        operation: "explain",
        context: { ...context, selection: "" },
      }).success,
    ).toBe(false);
    expect(
      studyRequest.safeParse({
        operation: "followup",
        context,
        question: "why?",
        history: [{ question: "", answer: "x".repeat(16001) }],
      }).success,
    ).toBe(false);
  });
  it("does not expose an unauthenticated AI proxy", async () => {
    const response = await worker.request(
      "/v1/study",
      { method: "POST" },
      bindings,
    );
    expect(response.status).toBe(503); // Identity secret intentionally unconfigured locally.
    expect(await response.text()).not.toContain("secret");
  });
});
describe("atomic allowance", () => {
  it("keeps deleted accounts closed when a late billing update arrives", async () => {
    const stub = await account();
    await stub.closeAccount(crypto.randomUUID());
    await stub.updateEntitlement({
      subscriptionId: "late-subscription",
      periodStart: 1,
      periodEnd: Date.now() / 1000 + 3600,
      enabled: true,
      cancelAtPeriodEnd: false,
      wordLimit: 100,
      explanationLimit: 100,
    });
    expect((await stub.summary()).subscribed).toBe(false);
    expect(await stub.reserve(crypto.randomUUID(), "word")).toMatchObject({
      ok: false,
      code: "subscription_required",
    });
  });
  it("requires a subscription", async () => {
    const stub = bindings.ACCOUNT_USAGE.get(
      bindings.ACCOUNT_USAGE.idFromName(crypto.randomUUID()),
    );
    expect(await stub.reserve(crypto.randomUUID(), "word")).toMatchObject({
      ok: false,
      code: "subscription_required",
    });
  });
  it("does not overspend under concurrent requests", async () => {
    const stub = await account(1);
    const results = await Promise.all(
      Array.from({ length: 8 }, () =>
        stub.reserve(crypto.randomUUID(), "word"),
      ),
    );
    expect(results.filter((result) => result.ok)).toHaveLength(1);
    expect((await stub.summary()).words.used).toBe(1);
  });
  it("blocks duplicate requests even after completion", async () => {
    const stub = await account();
    const id = crypto.randomUUID();
    expect((await stub.reserve(id, "word")).ok).toBe(true);
    await stub.finish(id, 20, 30);
    expect(await stub.reserve(id, "word")).toMatchObject({
      ok: false,
      code: "duplicate_request",
    });
  });
  it("releases definitive provider rejection, but not unknown outcomes", async () => {
    const stub = await account();
    const first = crypto.randomUUID();
    const second = crypto.randomUUID();
    await stub.reserve(first, "word");
    await stub.finish(first, null, null, true);
    await stub.reserve(second, "word");
    await stub.finish(second, null, null);
    expect((await stub.summary()).words.used).toBe(1);
  });
  it("shares the explanation allowance with followups", async () => {
    const stub = await account(1);
    await stub.reserve(crypto.randomUUID(), "explain");
    expect(await stub.reserve(crypto.randomUUID(), "followup")).toMatchObject({
      ok: false,
      code: "allowance_exhausted",
    });
  });
  it("expires orphan reservations without giving away their allowance", async () => {
    const stub = await account();
    const id = crypto.randomUUID();
    await stub.reserve(id, "word");
    await runInDurableObject(stub, async (instance, state) => {
      state.storage.sql.exec(
        "UPDATE reservations SET expires_at = ?",
        Date.now() - 1,
      );
      await instance.alarm();
    });
    // Recent abandoned reservations remain charged.
    expect((await stub.summary()).words.used).toBe(1);
  });
});

describe("hosted provider transport", () => {
  afterEach(() => vi.restoreAllMocks());
  it("uses a redirect mode accepted by Workers and never follows redirects with credentials", async () => {
    const mocked = vi
      .spyOn(globalThis, "fetch")
      .mockImplementation(async (input, init) => {
        const request = new Request(input, init);
        expect(request.redirect).toBe("manual");
        expect(request.headers.get("Authorization")).toBe("Bearer test-only");
        return new Response(null, {
          status: 302,
          headers: { Location: "https://other.example.com" },
        });
      });
    const response = await callProvider(
      { ...bindings, AI_ENABLED: "true", DEEPSEEK_API_KEY: "test-only" },
      { operation: "word", word: "Eyvallah", context },
      new AbortController().signal,
    );
    expect(response.ok).toBe(false);
    expect(mocked).toHaveBeenCalledTimes(1);
  });
});

describe("line word streaming relay", () => {
  afterEach(() => vi.restoreAllMocks());
  it("returns SSE and records the streamed token usage once", async () => {
    const finish = vi.fn().mockResolvedValue({});
    const usage = {reserve: vi.fn().mockResolvedValue({ok: true}), finish, globalBudget: vi.fn().mockResolvedValue(true)};
    const testEnv = {...bindings, AI_ENABLED: "true", DEEPSEEK_API_KEY: "test-key",
      ACCOUNT_USAGE: {idFromName: (name: string) => name, get: () => usage}} as unknown as Env;
    const content = JSON.stringify({entries: [{word: "Hola", partOfSpeech: "interjection", meaning: "hello"}]});
    const event = (value: unknown) => `data: ${JSON.stringify(value)}\n\n`;
    const wire = event({choices: [{delta: {content}, finish_reason: "stop"}]})
      + event({choices: [], usage: {prompt_tokens: 123, completion_tokens: 45}}) + "data: [DONE]\n\n";
    vi.spyOn(globalThis, "fetch").mockResolvedValue(new Response(wire));
    const pending: Promise<unknown>[] = [];
    const response = await study(new Request("https://api.example.test/v1/study", {
      method: "POST", headers: {"Content-Type": "application/json", "Idempotency-Key": crypto.randomUUID()},
      body: JSON.stringify({operation: "word", word: "Hola", wordFormat: "line", words: ["Hola"], context: {...context, clickedSubtitle: "Hola"}}),
    }), testEnv, "account-test", {waitUntil: promise => {pending.push(promise);}});
    expect(response.headers.get("content-type")).toContain("text/event-stream");
    expect(await response.text()).toBe(wire);
    await Promise.all(pending);
    expect(finish).toHaveBeenCalledOnce();
    expect(finish).toHaveBeenCalledWith(expect.any(String), 123, 45);
  });
});
