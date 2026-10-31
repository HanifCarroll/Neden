import { expect, test } from 'bun:test';
import fc from 'fast-check';
import { Conversation, type ContextSnapshot, type RequestOwner } from '../src/conversation';
import { HelperFrames, NativeStream, type StreamRecord } from '../src/stream';
import { utf8Bytes } from '../src/utf8';

function integerSetting(name: string, fallback: number, maximum: number): number {
  const value = process.env[name];
  if (value === undefined) {
    return fallback;
  }
  const parsed = Number(value);
  if (!/^\d+$/.test(value) || !Number.isSafeInteger(parsed) || parsed > maximum) {
    throw new Error(`${name} must be an integer from 0 to ${maximum}`);
  }
  return parsed;
}

const runs = integerSetting('NEDEN_PROPERTY_RUNS', 200, 10000);
if (!runs) {
  throw new Error('NEDEN_PROPERTY_RUNS must be positive');
}
const options = {
  seed: integerSetting('NEDEN_PROPERTY_SEED', 0x4e454445, 0xffffffff),
  numRuns: runs,
  ...(process.env.NEDEN_PROPERTY_PATH ? { path: process.env.NEDEN_PROPERTY_PATH } : {})
};
const unicode = fc.string({ unit: 'binary', maxLength: 128 });

test('property: UTF-8 lengths agree with the platform encoder for Unicode scalar strings', () => {
  fc.assert(
    fc.property(unicode, (value) => {
      expect(utf8Bytes(value)).toBe(Buffer.byteLength(value, 'utf8'));
    }),
    options
  );
});

test('property: lone UTF-16 surrogates are rejected', () => {
  fc.assert(
    fc.property(fc.integer({ min: 0xd800, max: 0xdfff }), unicode, (unit, text) => {
      expect(() => utf8Bytes(`prefix${String.fromCharCode(unit)}x${text}`)).toThrow();
    }),
    options
  );
});

test('property: helper frames decode identically under arbitrary fragmentation', () => {
  fc.assert(
    fc.property(
      fc.array(fc.string({ unit: 'binary', minLength: 1, maxLength: 128 }), {
        minLength: 1,
        maxLength: 16
      }),
      fc.array(fc.integer({ min: 1, max: 37 }), { minLength: 1, maxLength: 40 }),
      (texts, sizes) => {
        const wire = `READY\n${texts.map((text) => `DELTA ${Buffer.from(text).toString('base64')}\r\n`).join('')}DONE\n`;
        const expected: StreamRecord[] = [
          { kind: 'ready' },
          ...texts.map((value) => ({ kind: 'delta' as const, value })),
          { kind: 'done' }
        ];
        const decoder = new HelperFrames();
        const actual: StreamRecord[] = [];
        for (let offset = 0, index = 0; offset < wire.length; index++) {
          const length = sizes[index % sizes.length];
          decoder.enqueue(wire.slice(offset, offset + length));
          actual.push(...decoder.drain());
          offset += length;
        }
        expect(actual).toEqual(expected);
        expect(decoder.drain()).toEqual([]);
      }
    ),
    options
  );
});

test('property: malformed UTF-8 helper payloads fail without throwing from the decoder', () => {
  fc.assert(
    fc.property(fc.uint8Array({ maxLength: 128 }), (bytes) => {
      const invalid = Buffer.concat([Buffer.from([0xff]), Buffer.from(bytes)]).toString('base64');
      const decoder = new HelperFrames();
      decoder.enqueue(`DELTA ${invalid}\n`);
      expect(decoder.drain()).toEqual([{ kind: 'error', value: 'invalid_helper_frame' }]);
    }),
    options
  );
});

test('helper buffer limit has a deterministic error and discards oversized output', () => {
  const decoder = new HelperFrames();
  decoder.enqueue('x'.repeat(256 * 1024 + 1));
  expect(decoder.drain()).toEqual([{ kind: 'error', value: 'helper_output_too_large' }]);
  expect(decoder.drain()).toEqual([]);
});

test('property: cancelled native streams never publish later deltas or resend credentials', () => {
  fc.assert(
    fc.property(
      fc.array(fc.constantFrom('ready', 'delta', 'cancel', 'done', 'pump'), { maxLength: 60 }),
      (commands) => {
        let output: (chunk: string) => void = () => {};
        const writes: string[] = [];
        const events: StreamRecord[] = [];
        const host = {
          file: {
            handle: (path: string) => ({
              write: (value: string) => writes.push(`${path}:${value}`),
              close: () => {}
            })
          },
          utils: {
            exec: (_path: string, _args: string[], _cwd: null, stdout: (chunk: string) => void) => {
              output = stdout;
              return new Promise<{ status: number }>(() => {});
            }
          }
        };
        const stream = new NativeStream(
          host,
          '/synthetic/helper',
          '/synthetic/request',
          'synthetic-secret',
          (event) => events.push(event)
        );
        stream.start();
        let cancelled = false;
        let deltasAtCancellation = 0;
        for (const command of commands) {
          if (command === 'cancel') {
            if (!cancelled) {
              deltasAtCancellation = events.filter((event) => event.kind === 'delta').length;
            }
            cancelled = true;
            stream.cancel();
          } else if (command === 'pump') {
            stream.pump();
          } else {
            const frames = { ready: 'READY\n', done: 'DONE\n', delta: 'DELTA dGV4dA==\n' };
            output(frames[command]);
          }
          if (cancelled) {
            expect(events.filter((event) => event.kind === 'delta')).toHaveLength(
              deltasAtCancellation
            );
          }
          expect(
            writes.filter((write) => write.endsWith(':synthetic-secret')).length
          ).toBeLessThanOrEqual(1);
          expect(events.filter((event) => event.kind !== 'delta').length).toBeLessThanOrEqual(1);
        }
        stream.cancel();
      }
    ),
    options
  );
});

test('property: conversation action sequences reject every retired request owner', () => {
  fc.assert(
    fc.property(
      fc.array(fc.constantFrom('delta', 'complete', 'stop', 'retry', 'followup', 'stale'), {
        maxLength: 60
      }),
      (commands) => {
        const context: ContextSnapshot = {
          selection: {
            mediaEpoch: 1,
            sourceTrackId: 1,
            cueIndex: 0,
            cueStartMs: 1000,
            cueEndMs: 2000,
            cueText: 'İstanbul',
            exactText: 'İstanbul',
            rangeStartUtf16: 0,
            rangeEndUtf16: 8
          },
          before: [],
          after: [],
          secondary: [],
          sourceLanguage: 'Turkish',
          explanationLanguage: 'English'
        };
        const conversation = new Conversation(1, context);
        let owner: RequestOwner | null = conversation.beginInitial().owner;
        const retired: RequestOwner[] = [];
        let answer = '';
        let status: 'streaming' | 'complete' | 'incomplete' | 'failed' = 'streaming';
        for (const command of commands) {
          if (command === 'delta' && owner) {
            expect(conversation.delta(owner, 'text')).toBe(true);
            answer += 'text';
          } else if ((command === 'complete' || command === 'stop') && owner) {
            if (command === 'complete') {
              conversation.complete(owner);
              status = 'complete';
            } else {
              conversation.stop();
              status = answer ? 'incomplete' : 'failed';
            }
            retired.push(owner);
            owner = null;
          } else if (command === 'retry' && !owner && status !== 'complete') {
            owner = conversation.retry().owner;
            status = 'streaming';
            answer = '';
          } else if (command === 'followup' && !owner && conversation.turns.length < 20) {
            owner = conversation.followUp('Why?').owner;
            status = 'streaming';
            answer = '';
          }
          const before = structuredClone(conversation.turns);
          for (const old of retired) {
            expect(conversation.delta(old, 'STALE')).toBe(false);
            expect(conversation.complete(old)).toBe(false);
            expect(conversation.fail(old, 'STALE')).toBe(false);
          }
          expect(conversation.turns).toEqual(before);
          expect(conversation.turns.at(-1)?.answer).toBe(answer);
          expect(conversation.turns.at(-1)?.status).toBe(status);
          expect(conversation.context.selection.cueStartMs).toBe(1000);
        }
      }
    ),
    options
  );
});
