export type TrackChoices = {
  source?: string | 0;
  secondary?: string | 0;
};

export function readTrackChoices(value: unknown): Record<string, TrackChoices> {
  // 1. Reject malformed preferences before reading movie-specific choices.
  if (!value || typeof value !== 'object' || Array.isArray(value)) {
    return {};
  }

  const movies: Array<[string, TrackChoices]> = [];

  // 2. Keep only supported roles and migrate legacy Off values for IINA's plist storage.
  for (const [url, saved] of Object.entries(value)) {
    if (!saved || typeof saved !== 'object' || Array.isArray(saved)) {
      continue;
    }

    const choices: TrackChoices = {};
    for (const role of ['source', 'secondary'] as const) {
      const choice = (saved as Record<string, unknown>)[role];
      if (choice === null || choice === 0) {
        choices[role] = 0;
      } else if (typeof choice === 'string' && choice.length > 0) {
        choices[role] = choice;
      }
    }
    movies.push([url, choices]);
  }

  return Object.fromEntries(movies);
}
