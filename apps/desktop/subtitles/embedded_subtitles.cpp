#include "embedded_subtitles.h"
#include <QElapsedTimer>
#include <QFileInfo>
#include <QFile>
#include <QRegularExpression>
#include <algorithm>
#include <memory>
#include <stdexcept>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}
namespace {
struct FormatDeleter {
    void operator()(AVFormatContext *value) const { avformat_close_input(&value); }
};
struct CodecDeleter {
    void operator()(AVCodecContext *value) const { avcodec_free_context(&value); }
};
struct PacketDeleter {
    void operator()(AVPacket *value) const { av_packet_free(&value); }
};
struct Subtitle {
    AVSubtitle value{};
    ~Subtitle() { avsubtitle_free(&value); }
};
struct Interruption {
    const std::atomic_bool &cancelled;
    QElapsedTimer timer;
    static int check(void *opaque) {
        const auto &state = *static_cast<Interruption *>(opaque);
        return state.cancelled || state.timer.elapsed() > 120000;
    }
};
QString plainAss(const char *ass) {
    QString text = QString::fromUtf8(ass);
    int position = 0;
    for (int field = 0; field < 8; ++field) {
        position = text.indexOf(',', position);
        if (position < 0)
            return {};
        ++position;
    }
    text = text.mid(position);
    static const QRegularExpression drawing("\\\\p[1-9]");
    if (drawing.match(text).hasMatch())
        return {};
    static const QRegularExpression overrides("\\{[^}]*\\}");
    text.remove(overrides);
    text.replace("\\N", "\n").replace("\\n", "\n").replace("\\h", " ");
    return subtitlePlainText(text).trimmed();
}
} // namespace
QVector<Cue> EmbeddedSubtitles::read(const QString &path, int streamIndex,
                                     const std::atomic_bool &cancelled) {
    if (!QFileInfo(path).isFile())
        throw std::runtime_error("Embedded text extraction needs a local video file.");
    Interruption interruption{cancelled, {}};
    interruption.timer.start();
    AVFormatContext *rawFormat = avformat_alloc_context();
    if (!rawFormat)
        throw std::runtime_error("Cannot allocate the subtitle reader.");
    rawFormat->interrupt_callback = {Interruption::check, &interruption};
    AVDictionary *options = nullptr;
    av_dict_set(&options, "protocol_whitelist", "file", 0);
    const auto encodedPath = QFile::encodeName(path);
    const int opened = avformat_open_input(&rawFormat, encodedPath.constData(), nullptr, &options);
    av_dict_free(&options);
    std::unique_ptr<AVFormatContext, FormatDeleter> format(rawFormat);
    if (opened < 0)
        throw std::runtime_error("Cannot read embedded subtitles from this video.");
    if (avformat_find_stream_info(format.get(), nullptr) < 0 || streamIndex < 0 ||
        static_cast<unsigned>(streamIndex) >= format->nb_streams)
        throw std::runtime_error("The embedded subtitle stream is unavailable.");
    const auto *stream = format->streams[streamIndex];
    const auto *codec = avcodec_find_decoder(stream->codecpar->codec_id);
    const auto *description = avcodec_descriptor_get(stream->codecpar->codec_id);
    if (stream->codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE || !codec || !description ||
        !(description->props & AV_CODEC_PROP_TEXT_SUB))
        throw std::runtime_error("This is an image subtitle track. It can be displayed, but text tools "
                                 "require OCR or a text track.");
    std::unique_ptr<AVCodecContext, CodecDeleter> decoder(avcodec_alloc_context3(codec));
    if (!decoder || avcodec_parameters_to_context(decoder.get(), stream->codecpar) < 0)
        throw std::runtime_error("Cannot initialize the subtitle decoder.");
    decoder->pkt_timebase = stream->time_base;
    if (avcodec_open2(decoder.get(), codec, nullptr) < 0)
        throw std::runtime_error("Cannot open the subtitle decoder.");
    std::unique_ptr<AVPacket, PacketDeleter> packet(av_packet_alloc());
    if (!packet)
        throw std::runtime_error("Cannot allocate a subtitle packet.");
    QVector<Cue> cues;
    qint64 textBytes = 0;
    const qint64 origin = format->start_time == AV_NOPTS_VALUE ? 0 : format->start_time / 1000;
    int readStatus = 0;
    while ((readStatus = av_read_frame(format.get(), packet.get())) >= 0) {
        if (Interruption::check(&interruption))
            throw std::runtime_error("Subtitle extraction was cancelled or timed out.");
        if (packet->stream_index == streamIndex) {
            Subtitle subtitle;
            int gotSubtitle = 0;
            if (avcodec_decode_subtitle2(decoder.get(), &subtitle.value, &gotSubtitle, packet.get()) < 0)
                throw std::runtime_error("The embedded subtitle track is damaged.");
            if (gotSubtitle) {
                QStringList lines;
                for (unsigned i = 0; i < subtitle.value.num_rects; ++i) {
                    const auto *rectangle = subtitle.value.rects[i];
                    QString text;
                    if (rectangle->text)
                        text = subtitlePlainText(QString::fromUtf8(rectangle->text));
                    else if (rectangle->ass)
                        text = plainAss(rectangle->ass);
                    if (!text.trimmed().isEmpty())
                        lines.append(text.trimmed());
                }
                if (!lines.isEmpty()) {
                    const qint64 timestamp =
                        subtitle.value.pts != AV_NOPTS_VALUE
                            ? subtitle.value.pts / 1000
                            : (packet->pts != AV_NOPTS_VALUE
                                   ? av_rescale_q(packet->pts, stream->time_base, AVRational{1, 1000})
                                   : -1);
                    if (timestamp < 0)
                        throw std::runtime_error("An embedded subtitle is missing its timestamp.");
                    const qint64 start = timestamp - origin + subtitle.value.start_display_time;
                    qint64 end = timestamp - origin + subtitle.value.end_display_time;
                    if (subtitle.value.end_display_time == UINT32_MAX || end <= start)
                        end = start + av_rescale_q(packet->duration, stream->time_base, AVRational{1, 1000});
                    const QString text = lines.join('\n');
                    textBytes += text.toUtf8().size();
                    if (textBytes > 8 * 1024 * 1024 || text.size() > 4000 || cues.size() >= 50000 ||
                        end <= start || end > 86400000)
                        throw std::runtime_error(
                            "Embedded subtitles exceed supported text or timing limits.");
                    if (end > 0)
                        cues.append({qMax(qint64(0), start), end, text, {}});
                }
            }
        }
        av_packet_unref(packet.get());
    }
    if (readStatus != AVERROR_EOF || Interruption::check(&interruption))
        throw std::runtime_error(
            "Subtitle extraction was interrupted. Try again with a complete local video.");
    if (cues.isEmpty())
        throw std::runtime_error("This embedded track contains no readable text.");
    std::stable_sort(cues.begin(), cues.end(),
                     [](const Cue &left, const Cue &right) { return left.startMs < right.startMs; });
    return cues;
}
