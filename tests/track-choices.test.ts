import { expect, test } from 'bun:test';
import { readTrackChoices } from '../src/track-choices';

test('saved subtitle choices preserve movie ownership and migrate Off without null values', () => {
  const saved = {
    'file:///first.mp4': { source: 'tr.srt', secondary: 'en.srt' },
    'file:///second.mp4': { source: null, secondary: 0 }
  };

  expect(readTrackChoices(saved)).toEqual({
    'file:///first.mp4': { source: 'tr.srt', secondary: 'en.srt' },
    'file:///second.mp4': { source: 0, secondary: 0 }
  });
  expect(saved['file:///second.mp4'].source).toBeNull();
});

test('malformed subtitle preferences cannot become selection commands or unsupported plist values', () => {
  for (const value of [null, undefined, false, 1, 'tr.srt', []]) {
    expect(readTrackChoices(value)).toEqual({});
  }

  expect(
    readTrackChoices({
      'file:///first.mp4': { source: {}, secondary: ['en.srt'], extra: null },
      'file:///second.mp4': { source: '', secondary: 7 },
      'file:///third.mp4': [],
      'file:///fourth.mp4': null
    })
  ).toEqual({ 'file:///first.mp4': {}, 'file:///second.mp4': {} });
});
