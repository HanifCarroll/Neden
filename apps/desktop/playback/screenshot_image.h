#pragma once
#include <QImage>
#include <mpv/client.h>
#include <cstring>

// Copy mpv's borrowed screenshot before the next event invalidates its pixel buffer.
// mpv 0.36 returns bgr0; Qt owns encoding so the runtime needs no image encoder.
inline QImage screenshotImage(const mpv_node &result) {
    if (result.format != MPV_FORMAT_NODE_MAP || !result.u.list)
        return {};
    auto field = [&result](const char *name) -> const mpv_node * {
        for (int index = 0; index < result.u.list->num; ++index)
            if (std::strcmp(result.u.list->keys[index], name) == 0)
                return &result.u.list->values[index];
        return nullptr;
    };
    const auto *widthField = field("w");
    const auto *heightField = field("h");
    const auto *strideField = field("stride");
    const auto *formatField = field("format");
    const auto *dataField = field("data");
    if (!widthField || !heightField || !strideField || !formatField || !dataField ||
        widthField->format != MPV_FORMAT_INT64 || heightField->format != MPV_FORMAT_INT64 ||
        strideField->format != MPV_FORMAT_INT64 || formatField->format != MPV_FORMAT_STRING ||
        !formatField->u.string || std::strcmp(formatField->u.string, "bgr0") != 0 ||
        dataField->format != MPV_FORMAT_BYTE_ARRAY || !dataField->u.ba || !dataField->u.ba->data)
        return {};

    const auto width = widthField->u.int64;
    const auto height = heightField->u.int64;
    const auto stride = strideField->u.int64;
    if (width <= 0 || height <= 0 || width > 16384 || height > 16384 || width * height > 64000000 ||
        stride < width * 4)
        return {};
    const auto available = dataField->u.ba->size;
    const auto rowBytes = static_cast<size_t>(width * 4);
    if (available < rowBytes ||
        static_cast<quint64>(height - 1) > (available - rowBytes) / static_cast<quint64>(stride))
        return {};

    QImage image(static_cast<int>(width), static_cast<int>(height), QImage::Format_RGB32);
    if (image.isNull())
        return {};
    const auto *pixels = static_cast<const unsigned char *>(dataField->u.ba->data);
    for (int row = 0; row < image.height(); ++row) {
        const auto *source = pixels + row * stride;
        auto *destination = reinterpret_cast<QRgb *>(image.scanLine(row));
        for (int column = 0; column < image.width(); ++column) {
            const auto *pixel = source + column * 4;
            destination[column] = qRgb(pixel[2], pixel[1], pixel[0]);
        }
    }
    return image;
}
