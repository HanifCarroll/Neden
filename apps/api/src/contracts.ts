import { z } from "zod";

// Clients supply language evidence, never a provider, system prompt, or model.
const context = z
  .object({
    selection: z.string().max(4000).optional(),
    clickedSubtitle: z.string().max(4000).optional(),
    sourceCue: z.string().max(8000).optional(),
    sourceLanguage: z.string().max(80),
    explanationLanguage: z.string().max(80),
    before: z.unknown().optional(),
    after: z.unknown().optional(),
    secondary: z.unknown().optional(),
  })
  .strip()
  .refine(
    (value) => JSON.stringify(value).length <= 16000,
    "Subtitle context is too long.",
  );
const turn = z.object({
  question: z.string().max(2000),
  answer: z.string().min(1).max(16000),
});
export const studyRequest = z
  .discriminatedUnion("operation", [
    z.object({
      operation: z.literal("word"),
      word: z
        .string()
        .min(1)
        .max(100)
        .regex(/^[\p{L}\p{M}\p{N}]+$/u),
      context,
    }),
    z.object({ operation: z.literal("explain"), context }),
    z.object({
      operation: z.literal("translate"),
      targetLanguage: z
        .string()
        .trim()
        .min(2)
        .max(80)
        .regex(/^[\p{L}\p{M} ()-]+$/u),
      cues: z
        .array(
          z.object({
            id: z.number().int().min(0).max(49999),
            text: z.string().trim().min(1).max(4000),
          }),
        )
        .min(1)
        .max(8),
      contextBefore: z.array(z.string().max(1000)).max(2).default([]),
      contextAfter: z.array(z.string().max(1000)).max(2).default([]),
    }),
    z.object({
      operation: z.literal("followup"),
      context,
      question: z.string().trim().min(1).max(2000),
      history: z.array(turn).min(1).max(10),
    }),
  ])
  .superRefine((value, check) => {
    if (value.operation === "translate") {
      if (
        value.cues.reduce((length, cue) => length + cue.text.length, 0) >
          4000 ||
        value.cues.some((cue, index) => cue.id !== value.cues[0].id + index)
      )
        check.addIssue({
          code: "custom",
          message:
            "Translation needs a bounded, consecutive batch of subtitle lines.",
        });
      return;
    }
    if (
      !(
        value.operation === "word"
          ? value.context.clickedSubtitle
          : value.context.selection
      )?.trim()
    )
      check.addIssue({
        code: "custom",
        message: "Select subtitle text first.",
      });
    if (JSON.stringify(value).length > 32000)
      check.addIssue({
        code: "custom",
        message: "Start a new explanation; this conversation is too long.",
      });
  });
export type StudyRequest = z.infer<typeof studyRequest>;
export type Operation = StudyRequest["operation"];
export interface Entitlement {
  subscriptionId: string;
  periodStart: number;
  periodEnd: number;
  enabled: boolean;
  cancelAtPeriodEnd: boolean;
  wordLimit: number;
  explanationLimit: number;
}
export class ServiceError extends Error {
  constructor(
    public status: number,
    public code: string,
    message: string,
  ) {
    super(message);
  }
}
