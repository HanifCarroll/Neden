#include "platform_media_session.h"
#include "learning_session.h"
#include <QPointer>
#include <QTimer>
#import <AppKit/AppKit.h>
#import <MediaPlayer/MediaPlayer.h>
#import <IOKit/pwr_mgt/IOPMLib.h>
#import <CoreAudio/CoreAudio.h>

struct PlatformMediaSession::Native {
    IOPMAssertionID assertion = kIOPMNullAssertionID;
    NSMutableArray *commands = [NSMutableArray array];
    NSMutableArray *targets = [NSMutableArray array];
    id sleepObserver = nil;
    AudioObjectPropertyListenerBlock outputListener = nil;
    AudioDeviceID output = kAudioObjectUnknown;
};
namespace {
AudioDeviceID defaultOutput() {
    AudioDeviceID result = kAudioObjectUnknown;
    UInt32 size = sizeof(result);
    AudioObjectPropertyAddress address{kAudioHardwarePropertyDefaultOutputDevice,
                                       kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
    AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, nullptr, &size, &result);
    return result;
}
} // namespace
PlatformMediaSession::PlatformMediaSession(LearningSession *player, QWindow *playerWindow, QObject *parent)
    : QObject(parent), native(std::make_unique<Native>()), session(player), window(playerWindow) {
    const QPointer<LearningSession> guarded(player);
    auto bind = [this, guarded](MPRemoteCommand *command, auto action) {
        id target = [command addTargetWithHandler:^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent *event) {
          if (!guarded || !guarded->hasMedia())
              return MPRemoteCommandHandlerStatusNoSuchContent;
          QMetaObject::invokeMethod(
              guarded,
              [guarded, event, action] {
                  if (guarded)
                      action(guarded.data(), event);
              },
              Qt::QueuedConnection);
          return MPRemoteCommandHandlerStatusSuccess;
        }];
        [native->commands addObject:command];
        [native->targets addObject:target];
    };
    MPRemoteCommandCenter *center = [MPRemoteCommandCenter sharedCommandCenter];
    bind(center.playCommand, [](LearningSession *s, MPRemoteCommandEvent *) { s->setPlaying(); });
    bind(center.pauseCommand, [](LearningSession *s, MPRemoteCommandEvent *) { s->setPaused(true); });
    bind(center.togglePlayPauseCommand, [](LearningSession *s, MPRemoteCommandEvent *) { s->toggle(); });
    bind(center.nextTrackCommand, [](LearningSession *s, MPRemoteCommandEvent *) { s->queue()->next(); });
    bind(center.previousTrackCommand,
         [](LearningSession *s, MPRemoteCommandEvent *) { s->queue()->previous(); });
    bind(center.changePlaybackPositionCommand, [](LearningSession *s, MPRemoteCommandEvent *event) {
        s->seek(static_cast<MPChangePlaybackPositionCommandEvent *>(event).positionTime);
    });
    native->sleepObserver = [[[NSWorkspace sharedWorkspace] notificationCenter]
        addObserverForName:NSWorkspaceWillSleepNotification
                    object:nil
                     queue:[NSOperationQueue mainQueue]
                usingBlock:^(NSNotification *) {
                  if (guarded)
                      guarded->setPaused(true);
                }];
    native->output = defaultOutput();
    const QPointer<PlatformMediaSession> self(this);
    native->outputListener = ^(UInt32, const AudioObjectPropertyAddress *) {
      if (!self)
          return;
      const auto next = defaultOutput();
      // Pause before a changed route can unexpectedly play through speakers.
      if (self->native->output != kAudioObjectUnknown && next != self->native->output && guarded &&
          guarded->hasMedia() && !guarded->paused())
          guarded->setPaused(true);
      self->native->output = next;
    };
    AudioObjectPropertyAddress address{kAudioHardwarePropertyDefaultOutputDevice,
                                       kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
    AudioObjectAddPropertyListenerBlock(kAudioObjectSystemObject, &address, dispatch_get_main_queue(),
                                        native->outputListener);
    connect(session, &LearningSession::pausedChanged, this, &PlatformMediaSession::update);
    connect(session, &LearningSession::mediaChanged, this, &PlatformMediaSession::update);
    connect(session, &LearningSession::playbackStateChanged, this, &PlatformMediaSession::update);
    connect(session->queue(), &MediaQueue::changed, this, &PlatformMediaSession::update);
    auto *timer = new QTimer(this);
    timer->setInterval(1000);
    connect(timer, &QTimer::timeout, this, &PlatformMediaSession::update);
    timer->start();
}
void PlatformMediaSession::update() {
    const bool active = session->hasMedia() && !session->paused() && session->playbackState() != "error";
    if (active && native->assertion == kIOPMNullAssertionID)
        IOPMAssertionCreateWithName(kIOPMAssertionTypePreventUserIdleDisplaySleep, kIOPMAssertionLevelOn,
                                    CFSTR("Neden video playback"), &native->assertion);
    else if (!active && native->assertion != kIOPMNullAssertionID) {
        IOPMAssertionRelease(native->assertion);
        native->assertion = kIOPMNullAssertionID;
    }
    auto *center = [MPRemoteCommandCenter sharedCommandCenter];
    center.playCommand.enabled = session->hasMedia();
    center.pauseCommand.enabled = session->hasMedia();
    center.changePlaybackPositionCommand.enabled = session->seekable();
    center.nextTrackCommand.enabled = session->queue()->hasNext();
    center.previousTrackCommand.enabled = session->queue()->hasPrevious();
    auto *info = [MPNowPlayingInfoCenter defaultCenter];
    if (!session->hasMedia()) {
        info.nowPlayingInfo = nil;
        return;
    }
    info.nowPlayingInfo = @{
        MPMediaItemPropertyTitle : session->mediaTitle().toNSString(),
        MPMediaItemPropertyPlaybackDuration : @(session->duration()),
        MPNowPlayingInfoPropertyElapsedPlaybackTime : @(session->position()),
        MPNowPlayingInfoPropertyPlaybackRate : @(active ? session->playbackSpeed() : 0),
        MPNowPlayingInfoPropertyMediaType : @(MPNowPlayingInfoMediaTypeVideo)
    };
    info.playbackState = active ? MPNowPlayingPlaybackStatePlaying : MPNowPlayingPlaybackStatePaused;
}
PlatformMediaSession::~PlatformMediaSession() {
    AudioObjectPropertyAddress address{kAudioHardwarePropertyDefaultOutputDevice,
                                       kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
    AudioObjectRemovePropertyListenerBlock(kAudioObjectSystemObject, &address, dispatch_get_main_queue(),
                                           native->outputListener);
    [[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:native->sleepObserver];
    for (NSUInteger index = 0; index < native->commands.count; ++index)
        [native->commands[index] removeTarget:native->targets[index]];
    if (native->assertion != kIOPMNullAssertionID)
        IOPMAssertionRelease(native->assertion);
    [MPNowPlayingInfoCenter defaultCenter].nowPlayingInfo = nil;
    [MPNowPlayingInfoCenter defaultCenter].playbackState = MPNowPlayingPlaybackStateStopped;
}
