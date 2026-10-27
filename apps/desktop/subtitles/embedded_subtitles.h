#pragma once
#include "cues.h"
#include <atomic>
namespace EmbeddedSubtitles {
QVector<Cue> read(const QString &path, int streamIndex, const std::atomic_bool &cancelled);
}
