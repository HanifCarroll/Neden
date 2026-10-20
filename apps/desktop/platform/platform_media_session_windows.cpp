#include "platform_media_session.h"
#include "learning_session.h"
#include <QPointer>
#include <QTimer>
#include <windows.h>
#include <systemmediatransportcontrolsinterop.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.h>
#include <winrt/Windows.Media.Playback.h>

using winrt::Windows::Media::MediaPlaybackStatus;
using winrt::Windows::Media::MediaPlaybackType;
using winrt::Windows::Media::SystemMediaTransportControls;
using winrt::Windows::Media::SystemMediaTransportControlsButton;
struct PlatformMediaSession::Native {
    SystemMediaTransportControls controls{nullptr};
    winrt::event_token buttonToken{};
    winrt::event_token positionToken{};
    bool inhibited = false;
    bool ownsApartment = false;
};
PlatformMediaSession::PlatformMediaSession(LearningSession *player, QWindow *playerWindow, QObject *parent)
    : QObject(parent), native(std::make_unique<Native>()), session(player), window(playerWindow) {
    const HRESULT initialization = RoInitialize(RO_INIT_SINGLETHREADED);
    native->ownsApartment = SUCCEEDED(initialization);
    try {
        const auto interop = winrt::get_activation_factory<SystemMediaTransportControls,
                                                           ISystemMediaTransportControlsInterop>();
        winrt::check_hresult(interop->GetForWindow(reinterpret_cast<HWND>(window->winId()),
                                                   winrt::guid_of<SystemMediaTransportControls>(),
                                                   winrt::put_abi(native->controls)));
        native->controls.IsEnabled(true);
        native->controls.IsPlayEnabled(true);
        native->controls.IsPauseEnabled(true);
        const QPointer<LearningSession> guarded(session);
        native->buttonToken = native->controls.ButtonPressed([guarded](const auto &, const auto &args) {
            const auto button = args.Button();
            if (!guarded)
                return;
            QMetaObject::invokeMethod(
                guarded,
                [guarded, button] {
                    if (!guarded)
                        return;
                    switch (button) {
                    case SystemMediaTransportControlsButton::Play:
                        guarded->setPlaying();
                        break;
                    case SystemMediaTransportControlsButton::Pause:
                        guarded->setPaused(true);
                        break;
                    case SystemMediaTransportControlsButton::Next:
                        guarded->queue()->next();
                        break;
                    case SystemMediaTransportControlsButton::Previous:
                        guarded->queue()->previous();
                        break;
                    default:
                        break;
                    }
                },
                Qt::QueuedConnection);
        });
        native->positionToken =
            native->controls.PlaybackPositionChangeRequested([guarded](const auto &, const auto &args) {
                const double seconds = args.RequestedPlaybackPosition().count() / 10000000.0;
                if (guarded)
                    QMetaObject::invokeMethod(
                        guarded,
                        [guarded, seconds] {
                            if (guarded)
                                guarded->seek(seconds);
                        },
                        Qt::QueuedConnection);
            });
    } catch (const winrt::hresult_error &) {
        // Playback remains usable if the Windows session has no transport-control service.
        native->controls = nullptr;
    }
    connect(session, &LearningSession::pausedChanged, this, &PlatformMediaSession::update);
    connect(session, &LearningSession::mediaChanged, this, &PlatformMediaSession::update);
    connect(session, &LearningSession::playbackStateChanged, this, &PlatformMediaSession::update);
    auto *timer = new QTimer(this);
    timer->setInterval(1000);
    connect(timer, &QTimer::timeout, this, &PlatformMediaSession::update);
    timer->start();
}
void PlatformMediaSession::update() {
    const bool active = session->hasMedia() && !session->paused() && session->playbackState() != "error";
    if (native->inhibited != active) {
        SetThreadExecutionState(active ? ES_CONTINUOUS | ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED
                                       : ES_CONTINUOUS);
        native->inhibited = active;
    }
    if (!native->controls)
        return;
    try {
        native->controls.PlaybackStatus(active ? MediaPlaybackStatus::Playing : MediaPlaybackStatus::Paused);
        native->controls.IsNextEnabled(session->queue()->hasNext());
        native->controls.IsPreviousEnabled(session->queue()->hasPrevious());
        auto updater = native->controls.DisplayUpdater();
        updater.Type(MediaPlaybackType::Video);
        updater.VideoProperties().Title(winrt::hstring(session->mediaTitle().toStdWString()));
        updater.Update();
        winrt::Windows::Media::SystemMediaTransportControlsTimelineProperties timeline;
        timeline.StartTime(std::chrono::seconds(0));
        timeline.EndTime(std::chrono::milliseconds(qRound64(session->duration() * 1000)));
        timeline.MinSeekTime(timeline.StartTime());
        timeline.MaxSeekTime(timeline.EndTime());
        timeline.Position(std::chrono::milliseconds(qRound64(session->position() * 1000)));
        native->controls.UpdateTimelineProperties(timeline);
    } catch (const winrt::hresult_error &) {
    }
}
PlatformMediaSession::~PlatformMediaSession() {
    SetThreadExecutionState(ES_CONTINUOUS);
    if (native->controls) {
        native->controls.ButtonPressed(native->buttonToken);
        native->controls.PlaybackPositionChangeRequested(native->positionToken);
        native->controls.IsEnabled(false);
        native->controls = nullptr;
    }
    if (native->ownsApartment)
        RoUninitialize();
}
