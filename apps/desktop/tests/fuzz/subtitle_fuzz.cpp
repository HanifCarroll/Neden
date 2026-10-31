#include "cues.h"
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size > 8 * 1024 * 1024)
        return 0;
    try {
        const auto cues = parseSrt(QString::fromUtf8(reinterpret_cast<const char *>(data), qsizetype(size)));
        qint64 previousStart = -1;
        for (const auto &cue : cues) {
            if (cue.startMs < previousStart || cue.startMs < 0 || cue.endMs <= cue.startMs ||
                cue.text.isEmpty() || cue.text.size() > 4000)
                std::abort();
            previousStart = cue.startMs;
        }
        // Exercise binary lookup at all generated boundaries without assuming overlap policy.
        for (const auto &cue : cues) {
            for (qint64 time : {cue.startMs, cue.endMs}) {
                const int index = cueAt(cues, time);
                if (index < -1 || index >= cues.size())
                    std::abort();
                if (index >= 0 && !(cues[index].startMs <= time && time < cues[index].endMs))
                    std::abort();
            }
        }
    } catch (const std::runtime_error &) {
        // Invalid input is an expected parser result; sanitizer failures must still terminate.
    }
    return 0;
}
