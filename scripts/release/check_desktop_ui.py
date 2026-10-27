"""Run the portable UI acceptance subset with isolated profiles and mock AI responses."""
import os
from pathlib import Path
import subprocess
import sys

CHECKS = [
    "subtitleCatalogUiOffersSourcesAndValidatedLanguages",
    "subtitleToolsRequireExplicitApply",
    "helpDialogsKeepShortcutsIsolated",
    "menusOpenOwnedDialogsAndBlockShortcuts",
    "settingsDraftAndShortcutPolicyUseTheActualUi",
    "trackDropdownsDisplayTheirNamedRolesAndReadableSelection",
    "subtitlePickersExcludeTheOtherSlotsTrack",
    "wordCardSeparatesSensesFormsAndUses",
    "followUpComposerGrowsWithoutMovingSendButton",
    "savingFlashcardShowsResultAndOpensCollection",
    "recentHistoryKeepsFinishedVideosAccessible",
    "streamingRepliesStayAnchoredToTheLatestQuestion",
]

def main():
    build = Path(sys.argv[1]).resolve()
    executable = build / ("NedenNativeChecks.exe" if os.name == "nt" else "NedenNativeChecks")
    environment = dict(os.environ, QT_QPA_PLATFORM="offscreen", QT_QUICK_BACKEND="software",
                       QT_FORCE_STDERR_LOGGING="1", NEDEN_CHECK_HEADLESS="1")
    # Independent invocations prevent state from another scenario hiding regressions.
    for check in CHECKS:
        subprocess.run([str(executable), check], env=environment, check=True, timeout=90)

if __name__ == "__main__":
    main()
