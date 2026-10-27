#include "subtitle_tools.h"
#include "subtitle_document.h"
#include <cmath>
#include <stdexcept>

SubtitleTools::SubtitleTools(QString directory, AppSettings &settings, AiClient::CredentialReader credentials,
                             QObject *parent)
    : QObject(parent), directory(std::move(directory)), translation(settings, std::move(credentials), this) {
    connect(&translation, &SubtitleTranslation::progress, this, [this](int completed, int total) {
        status = QString("Translating %1 of %2 lines…").arg(completed).arg(total);
        emit changed();
    });
    connect(&translation, &SubtitleTranslation::failed, this, [this](const QString &message) {
        translating = false;
        status = message;
        emit changed();
    });
    connect(&translation, &SubtitleTranslation::ready, this, [this](const QVector<Cue> &cues) {
        translating = false;
        publish(cues, targetLanguage, !sourceSecondary);
    });
    connect(&settings, &AppSettings::changed, this, &SubtitleTools::invalidate);
}
SubtitleTools::~SubtitleTools() {
    cancel();
    for (const auto &job : jobs) {
        job.thread->disconnect(this);
        job.thread->wait();
        delete job.thread;
    }
}
bool SubtitleTools::busy() const {
    if (translating)
        return true;
    for (const auto &job : jobs)
        if (!*job.cancelled)
            return true;
    return false;
}
void SubtitleTools::cancel() {
    ++generation;
    translation.cancel();
    translating = false;
    for (const auto &job : jobs)
        *job.cancelled = true;
    proposed.clear();
    status = "Cancelled. Original tracks are unchanged.";
    emit changed();
}
void SubtitleTools::invalidate() {
    cancel();
    source.clear();
    reference.clear();
    status.clear();
    emit changed();
}
void SubtitleTools::prepare(QVector<Cue> cues, QVector<Cue> referenceCues, bool secondary) {
    invalidate();
    source = std::move(cues);
    reference = std::move(referenceCues);
    sourceSecondary = secondary;
    status = source.isEmpty() ? "Choose a text subtitle track first. Image subtitles can be displayed but "
                                "need OCR for text tools."
                              : QString();
    emit changed();
}
int SubtitleTools::translationBatches() const {
    int batches = 0;
    for (int start = 0; start < source.size(); start = SubtitleTranslation::batchEnd(source, start))
        ++batches;
    return batches;
}
QString SubtitleTools::lineText(int index) const {
    if (index < 0 || index >= source.size())
        return {};
    const auto &cue = source[index];
    return QString::number(cue.startMs / 1000.0, 'f', 3) + " s · " + cue.text;
}
void SubtitleTools::launch(std::function<QVector<Cue>(const std::atomic_bool &)> work,
                           std::function<void(QVector<Cue>)> accept) {
    struct Result {
        QVector<Cue> cues;
        QString error;
    };
    auto result = std::make_shared<Result>();
    auto cancelled = std::make_shared<std::atomic_bool>(false);
    const auto requestGeneration = generation;
    auto *thread = QThread::create([result, cancelled, work = std::move(work)] {
        try {
            result->cues = work(*cancelled);
        } catch (const std::exception &error) {
            result->error = QString::fromUtf8(error.what());
        }
    });
    jobs.append({thread, cancelled});
    connect(thread, &QThread::finished, this,
            [this, thread, cancelled, result, requestGeneration, accept = std::move(accept)] {
                for (int i = 0; i < jobs.size(); ++i)
                    if (jobs[i].thread == thread) {
                        jobs.removeAt(i);
                        break;
                    }
                thread->deleteLater();
                if (*cancelled || requestGeneration != generation)
                    return;
                if (result->error.isEmpty()) {
                    accept(std::move(result->cues));
                } else {
                    status = result->error;
                }
                emit changed();
            });
    thread->start();
    emit changed();
}
void SubtitleTools::publish(const QVector<Cue> &cues, const QString &label, bool secondary,
                            bool preserveDelay) {
    try {
        const auto path = SubtitleDocument::save(cues, directory, label);
        status = "New subtitle track saved and loaded.";
        emit trackReady(path, secondary, preserveDelay);
    } catch (const std::exception &error) {
        status = QString::fromUtf8(error.what());
    }
    emit changed();
}
void SubtitleTools::extract(const QString &path, int stream, bool secondary) {
    if (!decoder) {
        status = "Text extraction is unavailable in this build.";
        emit changed();
        return;
    }
    // Two roles may be restored together; both imports share the media generation.
    if (jobs.size() >= 2 || translating) {
        status = "Wait for the current subtitle operation.";
        emit changed();
        return;
    }
    const auto read = decoder;
    status = "Reading embedded subtitles…";
    launch([read, path, stream](const std::atomic_bool &cancelled) { return read(path, stream, cancelled); },
           [this, secondary](QVector<Cue> cues) { publish(cues, "Embedded", secondary, true); });
}
void SubtitleTools::translate(const QString &language) {
    if (busy() || source.isEmpty())
        return;
    targetLanguage = language;
    translating = true;
    proposed.clear();
    translation.start(source, language);
    emit changed();
}
void SubtitleTools::alignToReference() {
    if (busy() || source.isEmpty())
        return;
    proposed.clear();
    const auto input = source, accurate = reference;
    status = "Comparing subtitle timing…";
    launch(
        [input, accurate](const std::atomic_bool &cancelled) {
            return SubtitleDocument::retime(input, SubtitleDocument::align(input, accurate, cancelled));
        },
        [this](QVector<Cue> cues) {
            proposed = std::move(cues);
            status = QString("Match found. First line moves by %1 seconds. Apply to save a separate track.")
                         .arg((proposed.first().startMs - source.first().startMs) / 1000.0, 0, 'f', 2);
        });
}
void SubtitleTools::matchLines(int firstLine, double firstSeconds, int secondLine, double secondSeconds) {
    if (busy())
        return;
    proposed.clear();
    try {
        if (firstLine < 0 || firstLine >= source.size() || secondLine >= source.size() ||
            !std::isfinite(firstSeconds) || !std::isfinite(secondSeconds) || firstSeconds < 0 ||
            firstSeconds > 86400 || (secondLine >= 0 && (secondSeconds < 0 || secondSeconds > 86400)))
            throw std::runtime_error("Choose valid lines and video times in seconds.");
        const auto timing =
            SubtitleDocument::fromAnchors(source[firstLine].startMs, qRound64(firstSeconds * 1000),
                                          secondLine >= 0 ? source[secondLine].startMs : -1,
                                          secondLine >= 0 ? qRound64(secondSeconds * 1000) : -1);
        proposed = SubtitleDocument::retime(source, timing);
        status = QString("Correction ready: offset %1 s, timing rate %2. Apply to save a separate track.")
                     .arg(timing.offsetMs / 1000, 0, 'f', 2)
                     .arg(timing.rate, 0, 'f', 6);
    } catch (const std::exception &error) {
        status = QString::fromUtf8(error.what());
    }
    emit changed();
}
void SubtitleTools::applyRepair() {
    if (busy() || proposed.isEmpty())
        return;
    const auto result = std::exchange(proposed, {});
    publish(result, "Repaired", sourceSecondary);
}
