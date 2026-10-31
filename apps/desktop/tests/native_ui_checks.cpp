#include "embedded_subtitles.h"
#include "shortcut_map.h"
#include "playback_environment.h"
#include "subtitle_appearance.h"
#include "playback_pointer.h"
#include <QEnterEvent>
#include "fullscreen_controller.h"
#include <QScopeGuard>
#include "learning_session.h"
#include "mpv_playback.h"
#include "recent_videos.h"
#include <QColorSpace>
#include <QClipboard>
#include <QFile>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QScreen>
#include <QSurfaceFormat>
#include <QTemporaryDir>
#include <QNetworkReply>
#include <QSslSocket>
#include <QTcpServer>
#include <QTcpSocket>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>
#include <QtWebView/QtWebView>
#include <cmath>
#include <atomic>
#include <memory>

#ifdef Q_OS_MACOS
bool nativeMenusAreAvailable();
bool nativeVideoExtendsUnderTitlebar(QQuickWindow *window);
#endif

// Local deterministic responses shared by independent word, composer, and save checks.
class StudyResponseServer : public QTcpServer {
public:
    int requests = 0;
    int wordRequests = 0;
    QJsonObject sentContext;
    QString wordCardResponse = R"({"lemma":"dalmak","partOfSpeech":"verb","meaning":"I became absorbed","explanation":"Watching TV absorbed his attention.","formNote":"Dalmışım is an inflected form of dalmak.","otherSenses":[{"meaning":"to dive into water","example":"suya dalmak","translation":"to dive into the water","formNote":"Uses the infinitive dalmak."}],"otherUses":[]})";
    QString explanationResponse = "### Natural meaning\nI became absorbed.";
    StudyResponseServer() {
        connect(this, &QTcpServer::newConnection, this, [this] {
            auto *socket = nextPendingConnection();
            auto bytes = std::make_shared<QByteArray>();
            connect(socket, &QTcpSocket::readyRead, socket, [this, socket, bytes] {
                *bytes += socket->readAll();
                const int boundary = bytes->indexOf("\r\n\r\n");
                if (boundary < 0 || socket->property("sent").toBool())
                    return;
                int length = 0;
                for (const auto &line : bytes->left(boundary).split('\n'))
                    if (line.toLower().startsWith("content-length:"))
                        length = line.mid(line.indexOf(':') + 1).trimmed().toInt();
                if (bytes->size() < boundary + 4 + length)
                    return;
                socket->setProperty("sent", true);
                ++requests;
                const auto request = QJsonDocument::fromJson(bytes->mid(boundary + 4, length)).object();
                QByteArray answer;
                const auto data = QJsonDocument::fromJson(request.value("messages").toArray().last().toObject().value("content").toString().toUtf8()).object();
                if (data.value("words").isArray()) {
                    ++wordRequests;
                    sentContext = data;
                    const auto details = QJsonDocument::fromJson(wordCardResponse.toUtf8()).object();
                    QJsonArray entries;
                    for (const auto word : data.value("words").toArray())
                        entries.append(QJsonObject{{"word", word}, {"meaning", details.value("meaning")}, {"partOfSpeech", details.value("partOfSpeech")}});
                    const auto content = QString::fromUtf8(QJsonDocument(QJsonObject{{"entries", entries}}).toJson(QJsonDocument::Compact));
                    answer = "data: " + QJsonDocument(QJsonObject{{"choices", QJsonArray{QJsonObject{
                        {"delta", QJsonObject{{"content", content}}}, {"finish_reason", "stop"}}}}}).toJson(QJsonDocument::Compact)
                        + "\n\ndata: [DONE]\n\n";
                } else if (!request.value("stream").toBool()) {
                    ++wordRequests;
                    sentContext = QJsonDocument::fromJson(request.value("messages")
                                                              .toArray()
                                                              .last()
                                                              .toObject()
                                                              .value("content")
                                                              .toString()
                                                              .toUtf8())
                                      .object();
                    answer = QJsonDocument(
                                 QJsonObject{{"choices", QJsonArray{QJsonObject{
                                                             {"finish_reason", "stop"},
                                                             {"message", QJsonObject{{"content", wordCardResponse}}}}}}})
                                 .toJson();
                } else {
                    answer = "data: " + QJsonDocument(QJsonObject{{"choices", QJsonArray{
                        QJsonObject{{"delta", QJsonObject{{"content", explanationResponse}}}}}}})
                        .toJson(QJsonDocument::Compact) + "\n\ndata: [DONE]\n\n";
                }
                socket->write("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: " +
                              QByteArray::number(answer.size()) + "\r\n\r\n" + answer);
                socket->disconnectFromHost();
            });
        });
    }
};

class NativeUiChecks : public QObject {
    Q_OBJECT
private:
    QTemporaryDir profile;
    std::unique_ptr<SubtitleAppearance> appearance;
    std::unique_ptr<MpvPlayback> playback;
    std::unique_ptr<RecentVideos> recent;
    std::unique_ptr<StudyServices> study;
    std::unique_ptr<LearningSession> session;
    std::unique_ptr<FullscreenController> fullscreen;
    std::unique_ptr<QQmlApplicationEngine> engine;
    QQuickWindow *window = nullptr;
    double stoppedAt = 0;

    void createWindow() {
        appearance = std::make_unique<SubtitleAppearance>(profile.path());
        playback = std::make_unique<MpvPlayback>();
        recent = std::make_unique<RecentVideos>(profile.path());
        study = std::make_unique<StudyServices>(profile.path(), [](const QString &endpoint) {
            return QUrl(endpoint).host() == "127.0.0.1" ? QByteArray("synthetic-test-key") : QByteArray();
        });
        session = std::make_unique<LearningSession>(playback.get());
        study->subtitleTools()->setDecoder(EmbeddedSubtitles::read);
        session->setStudyServices(study.get());
        session->setRecentVideos(recent.get());
        fullscreen = std::make_unique<FullscreenController>();
        engine = std::make_unique<QQmlApplicationEngine>();
        engine->setInitialProperties({{"appearance", QVariant::fromValue(appearance.get())},
                                      {"session", QVariant::fromValue(session.get())},
                                      {"playback", QVariant::fromValue(playback.get())},
                                      {"recentVideos", QVariant::fromValue(recent.get())},
                                      {"fullscreenController", QVariant::fromValue(fullscreen.get())}});
        engine->loadFromModule("Neden", "Main");
        QVERIFY(!engine->rootObjects().isEmpty());
        window = qobject_cast<QQuickWindow *>(engine->rootObjects().first());
        QVERIFY(window);
        if (!qEnvironmentVariableIsSet("NEDEN_CHECK_HEADLESS")) {
            fullscreen->attach(window);
            window->raise();
        }
        window->requestActivate();
        QVERIFY(QTest::qWaitForWindowExposed(window));
    }

    void destroyWindow() {
        if (window)
            window->close();
        engine.reset();
        fullscreen.reset();
        session.reset();
        study.reset();
        recent.reset();
        playback.reset();
        window = nullptr;
    }

    QQuickItem *visualItem(QQuickItem *parent, const QString &name) {
        if (parent->objectName() == name)
            return parent;

        for (auto *child : parent->childItems()) {
            if (auto *found = visualItem(child, name))
                return found;
        }
        return nullptr;
    }

    QPoint wordPoint(QQuickItem *subtitle, int offset = 0) {
        auto *editor = subtitle->property("editor").value<QQuickItem *>();
        QRectF first, second;
        if (!editor ||
            !QMetaObject::invokeMethod(editor, "positionToRectangle", Q_RETURN_ARG(QRectF, first),
                                       Q_ARG(int, offset)) ||
            !QMetaObject::invokeMethod(editor, "positionToRectangle", Q_RETURN_ARG(QRectF, second),
                                       Q_ARG(int, offset + 1)))
            return {};
        return editor->mapToScene(QPointF((first.x() + second.x()) / 2, first.center().y())).toPoint();
    }

    void click(const char *name) {
        QTRY_VERIFY(visualItem(window->contentItem(), QString::fromUtf8(name)));
        auto *item = visualItem(window->contentItem(), QString::fromUtf8(name));
        QVERIFY2(item, name);
        QVERIFY(item->isVisible());
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                          item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint());
        QCoreApplication::processEvents();
    }

    void chooseFile(const char *pickerName, const QString &path, const char *buttonName = nullptr) {
        auto *picker = window->findChild<QObject *>(pickerName);
        QVERIFY(picker);
        // Qt's own picker is controllable without granting OS Accessibility access.
        constexpr int dontUseNativeDialog = 0x8;
        QVERIFY(picker->setProperty("options", dontUseNativeDialog));
        bool accepted = false;
        QTimer::singleShot(500, picker, [picker, path, &accepted] {
            QVERIFY(picker->property("visible").toBool());
            QVERIFY(picker->setProperty("selectedFile", QUrl::fromLocalFile(path)));
            QVERIFY(QMetaObject::invokeMethod(picker, "accepted"));
            QVERIFY(QMetaObject::invokeMethod(picker, "close"));
            accepted = true;
        });
        if (buttonName)
            click(buttonName);
        else
            QVERIFY(QMetaObject::invokeMethod(picker, "open"));
        QTRY_VERIFY_WITH_TIMEOUT(accepted, 10000);
        QTRY_VERIFY(!picker->property("visible").toBool());
        qInfo("Completed file picker: %s", pickerName);
    }

    void chooseSubtitleFile(bool secondary, const QString &path) {
        const QString choiceName = secondary ? "secondaryTrackChoice" : "primaryTrackChoice";
        click(choiceName.toUtf8().constData());
        auto *choice = visualItem(window->contentItem(), choiceName);
        QVERIFY(choice);
        const QString loadOption = choiceName + "Option" +
                                   QString::number(choice->property("count").toInt() - 1);
        chooseFile(secondary ? "secondaryPicker" : "primaryPicker", path,
                   loadOption.toUtf8().constData());
    }

    void configureStudyResponses(StudyResponseServer &server) {
        QVERIFY(server.listen(QHostAddress::LocalHost));
        QVERIFY(study->settings()->configure(QString("http://127.0.0.1:%1").arg(server.serverPort()), "mock",
                                             "https://example.org/%s", false));
        QVERIFY(study->settings()->configureLearning({{"wordProvider", "ai"},
                                                      {"sourceLanguage", "Turkish"},
                                                      {"explanationLanguage", "English"},
                                                      {"includeSecondary", true},
                                                      {"primaryProvider", "configured"}}));
    }

    void prepareCompletedExplanation(StudyResponseServer &server) {
        configureStudyResponses(server);
        window->setProperty("showRecent", false);
        session->closeDefinition();
        session->setPaused(true);
        session->replaceCues({{0, 7200000, "Ben televizyona dalmışım ya.", {}}});
        session->explainCue();
        QTRY_COMPARE(study->explanation()->state(), QString("complete"));
        auto *sidebar = visualItem(window->contentItem(), "learningInspector");
        QVERIFY(sidebar);
        QTRY_COMPARE(sidebar->property("revealProgress").toDouble(), 1.0);
    }

private slots:
    void captureWebsiteScreenshots() {
        if (!qEnvironmentVariableIsSet("NEDEN_CAPTURE_WEBSITE"))
            QSKIP("Opt-in website capture uses original sample media and deterministic study responses.");
        StudyResponseServer server;
        configureStudyResponses(server);
        server.wordCardResponse = R"({"lemma":"kalmak","partOfSpeech":"verb","meaning":"let’s stay","explanation":"","formNote":"kal- (stay) + -alım (let’s).","otherSenses":[],"otherUses":[]})";
        server.explanationResponse = "### Translation\nLet’s stay a little longer.\n\n### Breakdown\n"
            "- **Biraz** — a little; a small amount. Here it refers to time.\n"
            "- **daha** — more. Together, **biraz daha** means ‘a little longer’.\n"
            "- **kalalım** — let’s stay. **kal-** means ‘stay’; **-alım** makes a suggestion that includes the speaker.";
        double captureSeconds = 14;
        QString cue = "Biraz daha kalalım.";
        QString word = "kalalım";
        QString expectedTranslation;
        const auto fixturePath = qEnvironmentVariable("NEDEN_CAPTURE_FIXTURE");
        if (!fixturePath.isEmpty()) {
            QFile fixtureFile(fixturePath);
            QVERIFY(fixtureFile.open(QIODevice::ReadOnly));
            QJsonParseError error;
            const auto document = QJsonDocument::fromJson(fixtureFile.readAll(), &error);
            QVERIFY2(error.error == QJsonParseError::NoError, qPrintable(error.errorString()));
            QVERIFY(document.isObject());
            const auto fixture = document.object();
            captureSeconds = fixture.value("seconds").toDouble(-1);
            cue = fixture.value("cue").toString();
            word = fixture.value("word").toString();
            expectedTranslation = fixture.value("translation").toString();
            QVERIFY(std::isfinite(captureSeconds) && captureSeconds >= 0);
            QVERIFY(!cue.isEmpty() && !word.isEmpty() && cue.contains(word));
            QVERIFY(fixture.value("wordCard").isObject());
            server.wordCardResponse = QString::fromUtf8(
                QJsonDocument(fixture.value("wordCard").toObject()).toJson(QJsonDocument::Compact));
            server.explanationResponse = fixture.value("explanation").toString();
            QVERIFY(!server.explanationResponse.isEmpty());
        }
        window->resize(1440, 900);
        session->openVideo(QUrl::fromLocalFile(qEnvironmentVariable("NEDEN_CHECK_VIDEO")));
        QTRY_VERIFY_WITH_TIMEOUT(session->duration() > 0, 15000);
        session->setPaused(true);
        chooseSubtitleFile(false, qEnvironmentVariable("NEDEN_CHECK_PRIMARY"));
        chooseSubtitleFile(true, qEnvironmentVariable("NEDEN_CHECK_SECONDARY"));
        session->seek(captureSeconds);
        QTRY_VERIFY(std::abs(session->position() - captureSeconds) < 0.25);
        QTRY_COMPARE(session->subtitle(), cue);
        if (!expectedTranslation.isEmpty())
            QTRY_COMPARE(session->translation(), expectedTranslation);
        window->setProperty("showRecent", false);
        window->setProperty("inspectorMode", 1);
        QTRY_COMPARE(window->property("inspectorMode").toInt(), 1);
        QTest::mouseMove(window, QPoint(12, 200));
        QTest::qWait(700);
        const auto output = qEnvironmentVariable("NEDEN_CHECK_ARTIFACTS");
        QVERIFY(window->grabWindow().save(output + "/website-player.png"));
        session->lookupWordAt(cue, cue.indexOf(word));
        QTRY_COMPARE(study->words()->state(), QString("complete"));
        QTest::qWait(300);
        QVERIFY(window->grabWindow().save(output + "/website-word.png"));
        session->closeDefinition();
        session->explainCue();
        QTRY_COMPARE(study->explanation()->state(), QString("complete"));
        QTest::qWait(700);
        QVERIFY(window->grabWindow().save(output + "/website-explanation.png"));
    }

    void init() {
        const QStringList headlessChecks{
            "seekShortcutsSurviveSubtitleAndMouseControlFocus",
            "subtitleCatalogUiOffersSourcesAndValidatedLanguages",
            "subtitleToolsRequireExplicitApply",
                                         "helpDialogsKeepShortcutsIsolated",
                                         "wordCardSeparatesSensesFormsAndUses",
                                         "menusOpenOwnedDialogsAndBlockShortcuts",
                                         "followUpComposerGrowsWithoutMovingSendButton",
                                         "followUpSupportsMacDeleteToLineStart",
                                         "savingFlashcardShowsResultAndOpensCollection",
                                         "recentHistoryKeepsFinishedVideosAccessible",
                                         "sidebarSlidesWithoutResizingVideoPerFrame",
                                         "translatedWordsRevealTheirSourceBreakdown",
                                         "explainHoverRejectsBlankMarginsEvenWithSelectionOrFocus",
                                         "explainHoverCoversBothTracksAndThePathToTheButton",
                                         "explainHoverSurvivesActiveSelections",
                                         "explainButtonFootprintAndStationaryCueChanges",
                                         "pausedVideoHidesControlsAndCursorUntilPointerMoves",
                                         "cursorRestoresAcrossOverlaysAndWindowBoundaries",
                                         "explanationPhraseUsesThePluginCardStyle",
                                         "hoveringAndExplainingTheFullLineUseOneExplicitRequest",
                                         "singleClickShowsContextualWordCardWithoutChangingTheSidebar",
                                         "videoAndSidebarReachTheTopWithoutATitleStrip",
                                         "trackDropdownsDisplayTheirNamedRolesAndReadableSelection",
                                         "subtitlePickersExcludeTheOtherSlotsTrack",
                                         "settingsDraftAndShortcutPolicyUseTheActualUi",
                                         "settingsSeparateCloudFromProviderDrafts",
                                         "streamingRepliesStayAnchoredToTheLatestQuestion"};
        if (qEnvironmentVariableIsSet("NEDEN_CHECK_HEADLESS") &&
            !headlessChecks.contains(QString::fromUtf8(QTest::currentTestFunction()))) {
            QSKIP("This check needs the real player and a visible native display.");
        }
    }

    void initTestCase() {
        if (qEnvironmentVariableIsSet("NEDEN_CHECK_HEADLESS")) {
            createWindow();
            QFile videoFile(profile.filePath("headless-placeholder.webm"));
            QVERIFY(videoFile.open(QIODevice::WriteOnly));
            videoFile.close();
            session->openVideo(QUrl::fromLocalFile(videoFile.fileName()));
            session->setPaused(true);
            return;
        }

        for (const auto &name : {"NEDEN_CHECK_VIDEO", "NEDEN_CHECK_PRIMARY", "NEDEN_CHECK_SECONDARY"}) {
            QVERIFY2(QFile::exists(qEnvironmentVariable(name)), name);
        }
        QDir().mkpath(qEnvironmentVariable("NEDEN_CHECK_ARTIFACTS"));
        createWindow();
    }

    void openAndDisplayTwoIndependentlySelectedTracks() {

        // 1. Exercise the production pickers and real native playback on a visible window.
        chooseFile("videoPicker", qEnvironmentVariable("NEDEN_CHECK_VIDEO"));
        QTRY_VERIFY_WITH_TIMEOUT(session->duration() > 0, 15000);
        session->setPaused(true);
        chooseSubtitleFile(false, qEnvironmentVariable("NEDEN_CHECK_PRIMARY"));
        chooseSubtitleFile(true, qEnvironmentVariable("NEDEN_CHECK_SECONDARY"));
        QVERIFY(!session->cueModel()->entries().isEmpty());
        QVERIFY(session->translationCueCount() > 0);

        // 2. Find an overlap without printing or copying commercial subtitle text.
        QFile secondaryFile(qEnvironmentVariable("NEDEN_CHECK_SECONDARY"));
        QVERIFY(secondaryFile.open(QIODevice::ReadOnly));
        const auto secondary = parseSrt(QString::fromUtf8(secondaryFile.readAll()));
        double target = -1;
        const qint64 videoEndMs = qint64(session->duration() * 1000);
        for (const auto &primaryCue : session->cueModel()->entries()) {
            for (const auto &secondaryCue : secondary) {
                const auto start = qMax(primaryCue.startMs, secondaryCue.startMs);
                const auto end = qMin(qMin(primaryCue.endMs, secondaryCue.endMs), videoEndMs);
                if (end - start > 500) {
                    target = (start + 200) / 1000.0;
                    break;
                }
            }
            if (target >= 0)
                break;
        }
        QVERIFY(target >= 0);
        session->seek(target);
        QTRY_VERIFY_WITH_TIMEOUT(std::abs(session->position() - target) < 0.1, 10000);
        QVERIFY(!session->subtitle().isEmpty());
        QVERIFY(!session->translation().isEmpty());
        QTest::qWait(300);
        const auto earlyArtifacts = qEnvironmentVariable("NEDEN_CHECK_ARTIFACTS");
        if (!earlyArtifacts.isEmpty())
            window->grabWindow().save(earlyArtifacts + "/player-layout.png");
        for (const auto &name : {"primarySubtitleText", "secondarySubtitleText"}) {
            auto *text = visualItem(window->contentItem(), name);
            QVERIFY(text);
            qInfo("Subtitle geometry: %s visible=%d width=%.1f height=%.1f", name, text->isVisible(),
                  text->width(), text->height());
            QTRY_VERIFY(text->isVisible() && text->width() > 0 && text->height() > 0);
            QVERIFY(!text->property("text").toString().isEmpty());
        }
        QVERIFY(!window->grabWindow().isNull());
        const auto artifacts = qEnvironmentVariable("NEDEN_CHECK_ARTIFACTS");
        if (!artifacts.isEmpty()) {
            QDir().mkpath(artifacts);
            QVERIFY(window->grabWindow().save(artifacts + "/player.png"));
        }
        stoppedAt = session->position();
        session->checkpoint();

        // 3. Settings and the recent list use real QML controls and preserve the paused position.
#ifdef Q_OS_MACOS
        QVERIFY(nativeMenusAreAvailable());
        QVERIFY(nativeVideoExtendsUnderTitlebar(window));
        click("explanationTab");
        QTRY_COMPARE(window->property("inspectorTab").toInt(), 1);
        click("transcriptTab");
        QTRY_COMPARE(window->property("inspectorTab").toInt(), 0);
#endif
        QTest::keyClick(window, Qt::Key_Comma, Qt::ControlModifier);
        auto *settings = window->findChild<QObject *>("settingsDialog");
        QVERIFY(settings);
        QTRY_VERIFY(settings->property("opened").toBool());
        if (!artifacts.isEmpty()) {
            QTest::qWait(300);
            QVERIFY(window->grabWindow().save(artifacts + "/settings.png"));
        }
        QVERIFY(QMetaObject::invokeMethod(settings, "close"));
        QTRY_VERIFY(!settings->property("opened").toBool());
        QTest::keyClick(window, Qt::Key_L, Qt::ControlModifier);
        QVERIFY(window->property("showRecent").toBool());
    }

    void seekShortcutsSurviveSubtitleAndMouseControlFocus() {
        window->setProperty("showRecent", false);
        session->replaceCues({{0, 7200000, "A simple subtitle.", {}}});
        auto *forward = window->findChild<QObject *>("seekForwardShortcut");
        auto *backward = window->findChild<QObject *>("seekBackwardShortcut");
        QVERIFY(forward && backward);
        QSignalSpy forwardActivated(forward, SIGNAL(activated()));
        QSignalSpy backwardActivated(backward, SIGNAL(activated()));
        auto *subtitle = visualItem(window->contentItem(), "primarySubtitleText");
        QVERIFY(subtitle);
        auto *editor = subtitle->property("editor").value<QQuickItem *>();
        QVERIFY(editor);
        editor->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_Right);
        QCOMPARE(forwardActivated.count(), 1);
        QTest::keyClick(window, Qt::Key_Left);
        QCOMPARE(backwardActivated.count(), 1);
        QVERIFY(QMetaObject::invokeMethod(editor, "selectAll"));
        QTest::keyClick(window, Qt::Key_Right);
        QCOMPARE(forwardActivated.count(), 2);

        // Mouse adjustments leave playback shortcuts available; Tab still accesses controls.
        window->contentItem()->forceActiveFocus();
        click("volumeSlider");
        auto *volume = visualItem(window->contentItem(), "volumeSlider");
        QVERIFY(volume && !volume->hasActiveFocus());
        QTest::keyClick(window, Qt::Key_Right);
        QCOMPARE(forwardActivated.count(), 3);
        volume->forceActiveFocus(Qt::TabFocusReason);
        const double previousVolume = session->volumeLevel();
        QTest::keyClick(window, Qt::Key_Left);
        QCOMPARE(backwardActivated.count(), 1);
        QVERIFY(session->volumeLevel() < previousVolume);

        click("transcriptSearchButton");
        auto *search = visualItem(window->contentItem(), "transcriptSearchField");
        QVERIFY(search && search->hasActiveFocus());
        search->setProperty("text", "search");
        search->setProperty("cursorPosition", 3);
        QTest::keyClick(window, Qt::Key_Left);
        QCOMPARE(search->property("cursorPosition").toInt(), 2);
        QTest::keyClick(window, Qt::Key_Right);
        QCOMPARE(search->property("cursorPosition").toInt(), 3);
        QCOMPARE(forwardActivated.count(), 3);
        QCOMPARE(backwardActivated.count(), 1);
        click("transcriptSearchButton");

        // Custom bindings use the same routing through read-only text.
        editor->forceActiveFocus();
        auto *keymap = window->property("keymap").value<ShortcutMap *>();
        QVERIFY(keymap);
        QVERIFY(!keymap->matches("seekForward", Qt::Key_Right, Qt::ShiftModifier));
        QVERIFY(keymap->assign("seekForward", "J"));
        QTest::keyClick(window, Qt::Key_J);
        QCOMPARE(forwardActivated.count(), 4);
        QTest::keyClick(window, Qt::Key_Right);
        QCOMPARE(forwardActivated.count(), 4);
        QVERIFY(keymap->reset("seekForward"));

        StudyResponseServer server;
        prepareCompletedExplanation(server);
        auto *answer = visualItem(window->contentItem(), "explanationAnswerText");
        QVERIFY(answer);
        answer->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_Left);
        QCOMPARE(backwardActivated.count(), 2);
    }

    void speedSelectionPreservesPlayerShortcutsAndTextInput() {
        window->setProperty("showRecent", false);
        window->setProperty("inspectorMode", 1);
        session->setPaused(true);

        // 1. Select a speed with the mouse, then use the ordinary player shortcuts.
        click("speedChoice");
        click("speedChoiceOption2");
        auto *speed = visualItem(window->contentItem(), "speedChoice");
        QVERIFY(speed);
        auto *popup = speed->property("popup").value<QObject *>();
        QVERIFY(popup);
        QTRY_VERIFY(!popup->property("visible").toBool());
        QCOMPARE(speed->property("currentIndex").toInt(), 2);
        QTest::keyClick(window, Qt::Key_Space);
        QTRY_VERIFY(!session->paused());
        QVERIFY(!popup->property("visible").toBool());
        QTest::keyClick(window, Qt::Key_Space);
        QTRY_VERIFY(session->paused());
        const double beforeSeek = session->position();
        QTest::keyClick(window, Qt::Key_Right);
        QTRY_VERIFY(std::abs(session->position() - beforeSeek - 5) < 0.15);
        QTest::keyClick(window, Qt::Key_Left);
        QTRY_VERIFY(std::abs(session->position() - beforeSeek) < 0.15);
        const double beforeMute = session->volumeLevel();
        QTest::keyClick(window, Qt::Key_M);
        QCOMPARE(session->volumeLevel(), 0);
        QTest::keyClick(window, Qt::Key_M);
        QCOMPARE(session->volumeLevel(), beforeMute);

        // 2. A closed, keyboard-focused speed control also treats Space as play/pause.
        speed->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_Space);
        QTRY_VERIFY(!session->paused());
        QVERIFY(!popup->property("visible").toBool());
        QTest::keyClick(window, Qt::Key_Space);
        QTRY_VERIFY(session->paused());

        // 3. Single-letter and Space shortcuts must not consume text in search fields.
        click("transcriptSearchButton");
        auto *search = visualItem(window->contentItem(), "transcriptSearchField");
        QVERIFY(search && search->hasActiveFocus());
        QTest::keyClick(window, Qt::Key_M);
        QTest::keyClick(window, Qt::Key_Space);
        QTest::keyClick(window, Qt::Key_M);
        QCOMPARE(search->property("text").toString(), QString("m m"));
        QCOMPARE(session->volumeLevel(), beforeMute);
        QVERIFY(session->paused());
        click("transcriptSearchButton");
        session->rate(1);
        QTRY_COMPARE(speed->property("currentText").toString(), QString("1.0×"));
    }

    void subtitleControlsPlayOneCueThenPauseOnTheRealAdapter() {
        window->setProperty("showRecent", false);
        session->seek(stoppedAt);
        QTRY_VERIFY(std::abs(session->position() - stoppedAt) < 0.1);
        const int initial = session->activeCue();
        QVERIFY(initial > 0 && initial + 1 < session->cueModel()->entries().size());
        session->setPaused(false);
        click("previousSubtitleButton");
        QTRY_VERIFY(std::abs(session->position() -
                             session->cueModel()->entries()[initial - 1].startMs / 1000.0) < 0.1);
        QVERIFY(!session->paused());
        QTRY_VERIFY_WITH_TIMEOUT(!session->replaying(), 15000);
        QVERIFY(session->paused());
        QTest::keyClick(window, Qt::Key_Right, Qt::ControlModifier);
        QTRY_VERIFY(std::abs(session->position() - session->cueModel()->entries()[initial].startMs / 1000.0) <
                    0.1);
        QVERIFY(!session->paused());
        QTRY_VERIFY_WITH_TIMEOUT(!session->replaying(), 15000);
        QVERIFY(session->paused());
        QTest::keyClick(window, Qt::Key_Left, Qt::ControlModifier);
        QTRY_VERIFY(std::abs(session->position() -
                             session->cueModel()->entries()[initial - 1].startMs / 1000.0) < 0.1);
        QVERIFY(!session->paused());
        QTRY_VERIFY_WITH_TIMEOUT(!session->replaying(), 15000);
        QVERIFY(session->paused());
        click("nextSubtitleButton");
        QTRY_VERIFY_WITH_TIMEOUT(!session->replaying(), 15000);
        QVERIFY(session->paused());
        click("replaySubtitleButton");
        QVERIFY(session->replaying());
        QVERIFY(!session->paused());
        const auto cueDuration =
            session->cueModel()->entries()[initial].endMs - session->cueModel()->entries()[initial].startMs;
        QTRY_VERIFY_WITH_TIMEOUT(!session->replaying(), int(cueDuration) + 10000);
        QVERIFY(session->paused());
        QTRY_VERIFY(std::abs(session->position() - session->cueModel()->entries()[initial].endMs / 1000.0) <
                    0.15);
        session->seek(stoppedAt);
        QTRY_VERIFY(std::abs(session->position() - stoppedAt) < 0.1);
        session->setPaused(true);
        session->checkpoint();
    }

    void helpDialogsKeepShortcutsIsolated() {
        session->closeDefinition();
        window->contentItem()->forceActiveFocus();
        auto *menu = window->findChild<QObject *>("playerMenuBar");
        QVERIFY(menu);
        QVERIFY(QMetaObject::invokeMethod(menu, "gettingStartedRequested"));
        auto *guide = window->findChild<QObject *>("gettingStartedDialog");
        QVERIFY(guide);
        QTRY_VERIFY(guide->property("opened").toBool());
        QVERIFY(!window->property("commandShortcutsEnabled").toBool());
        QVERIFY(QMetaObject::invokeMethod(guide, "close"));
        QTRY_VERIFY(!guide->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(menu, "supportRequested", Q_ARG(bool, false)));
        auto *support = window->findChild<QObject *>("supportDialog");
        QVERIFY(support);
        QTRY_VERIFY(support->property("opened").toBool());
        QVERIFY(!window->property("commandShortcutsEnabled").toBool());
        auto *copy = visualItem(window->contentItem(), "copyDiagnosticsButton");
        QVERIFY(copy && copy->isVisible() && copy->isEnabled());
        copy->forceActiveFocus(Qt::TabFocusReason);
        QCOMPARE(window->activeFocusItem(), copy);
        QTest::keyClick(window, Qt::Key_Space);
        QVERIFY(QGuiApplication::clipboard()->text().contains("Neden:"));
        QVERIFY(QMetaObject::invokeMethod(support, "close"));
        QTRY_VERIFY(!support->property("visible").toBool());
        QTRY_VERIFY(window->property("commandShortcutsEnabled").toBool());
    }

    void menusOpenOwnedDialogsAndBlockShortcuts() {
        session->closeDefinition();
        window->contentItem()->forceActiveFocus();
        auto *menu = window->findChild<QObject *>("playerMenuBar");
        QVERIFY(menu);
        QTRY_VERIFY(window->property("commandShortcutsEnabled").toBool());
        const auto checkDialog = [&](const char *request, const char *name) {
            QVERIFY(QMetaObject::invokeMethod(menu, request));
            auto *dialog = window->findChild<QObject *>(name);
            QVERIFY(dialog);
            QTRY_VERIFY(dialog->property("opened").toBool());
            QVERIFY(!window->property("commandShortcutsEnabled").toBool());
            QVERIFY(QMetaObject::invokeMethod(dialog, "close"));
            QTRY_VERIFY(!dialog->property("visible").toBool());
            QTRY_VERIFY(window->property("commandShortcutsEnabled").toBool());
        };
        checkDialog("openUrlRequested", "openUrlDialog");
        checkDialog("jumpToTimeRequested", "jumpToTimeDialog");
        QVERIFY(QMetaObject::invokeMethod(menu, "playerOptionsRequested", Q_ARG(int, 1)));
        auto *optionsDialog = window->findChild<QObject *>("playerOptionsDialog");
        QVERIFY(optionsDialog);
        QTRY_VERIFY(optionsDialog->property("opened").toBool());
        QVERIFY(!window->property("commandShortcutsEnabled").toBool());
        auto *options = window->findChild<QObject *>("playerOptions");
        QVERIFY(options);
        QCOMPARE(options->property("page").toInt(), 1);
        QVERIFY(QMetaObject::invokeMethod(optionsDialog, "close"));
        QTRY_VERIFY(!optionsDialog->property("visible").toBool());
        QTRY_VERIFY(window->property("commandShortcutsEnabled").toBool());
    }

    void settingsDraftAndShortcutPolicyUseTheActualUi() {
        if (!qEnvironmentVariableIsSet("NEDEN_CHECK_HEADLESS")) {
            chooseFile("videoPicker", qEnvironmentVariable("NEDEN_CHECK_VIDEO"));
            QTRY_VERIFY_WITH_TIMEOUT(session->duration() > 0, 15000);
        }
        session->setPaused(true);
        window->setProperty("showRecent", false);
        window->setProperty("inspectorMode", 1);
        session->volume(60);
        window->contentItem()->forceActiveFocus();
        auto *shortcuts = window->findChild<QObject *>("playerShortcuts");
        QVERIFY(shortcuts);
        QTRY_VERIFY(shortcuts->property("playbackKeysEnabled").toBool());
        QTest::keyClick(window, Qt::Key_M);
        QCOMPARE(session->volumeLevel(), 0.0);
        QTest::keyClick(window, Qt::Key_M);
        QCOMPARE(session->volumeLevel(), 60.0);
        click("transcriptSearchButton");
        auto *search = visualItem(window->contentItem(), "transcriptSearchField");
        QVERIFY(search && search->isVisible());
        search->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_M);
        QVERIFY(!search->property("text").toString().isEmpty());
        QCOMPARE(session->volumeLevel(), 60.0);
        QVERIFY(search->setProperty("text", QString()));
        QVERIFY(QMetaObject::invokeMethod(window, "openSettings", Q_ARG(QVariant, QVariant())));
        auto *dialog = window->findChild<QObject *>("settingsDialog");
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("opened").toBool());
        auto *endpoint = visualItem(window->contentItem(), "aiEndpointField");
        auto *model = visualItem(window->contentItem(), "aiModelField");
        auto *provider = visualItem(window->contentItem(), "wordProviderChoice");
        auto *save = visualItem(window->contentItem(), "saveSettingsButton");
        QVERIFY(endpoint && model && provider && save);
        QCOMPARE(provider->property("currentValue").toString(), QString("ai"));
        QCOMPARE(provider->property("count").toInt(), 2);
        QCOMPARE(study->settings()->credentialServices().size(), 4);
        const auto original = study->settings()->endpoint();
        QVERIFY(endpoint->setProperty("text", QString("https://example.org/v1")));
        QVERIFY(model->setProperty("text", QString("draft-model")));
        QCOMPARE(study->settings()->endpoint(), original);
        QVERIFY(QMetaObject::invokeMethod(save, "clicked"));
        QCOMPARE(study->settings()->endpoint(), QString("https://example.org/v1"));
        QCOMPARE(study->settings()->model(), QString("draft-model"));
        QVERIFY(QMetaObject::invokeMethod(dialog, "close"));
        QTRY_VERIFY(!dialog->property("opened").toBool());
        window->contentItem()->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_M);
        QCOMPARE(session->volumeLevel(), 0.0);
        session->volume(60);
    }

    void settingsSeparateCloudFromProviderDrafts() {
        const auto originalLearning = study->settings()->learningSettings();
        const auto restoreLearning = qScopeGuard([&] { study->settings()->configureLearning(originalLearning); });
        const auto originalMode = study->account()->enabled();
        const auto originalSize = window->size();
        const auto restore = qScopeGuard([&] {
            study->account()->setEnabled(originalMode);
            window->resize(originalSize);
        });
        study->account()->setEnabled(false);
        window->findChild<QObject *>("playerDialogs")->setProperty("settingsPage", 2);
        QVERIFY(QMetaObject::invokeMethod(window, "openSettings", Q_ARG(QVariant, QVariant())));
        auto *dialog = window->findChild<QObject *>("settingsDialog");
        QTRY_VERIFY(dialog->property("opened").toBool());
        auto *mode = visualItem(window->contentItem(), "aiModeChoice");
        auto *provider = visualItem(window->contentItem(), "ownProviderSettings");
        auto *model = visualItem(window->contentItem(), "aiModelField");
        auto *advanced = visualItem(window->contentItem(), "advancedAiSettings");
        auto *wordProvider = visualItem(window->contentItem(), "wordProviderChoice");
        auto *downloads = visualItem(window->contentItem(), "subtitleDownloadSettings");
        QVERIFY(mode && provider && model && advanced && wordProvider && downloads);
        QVERIFY(provider->isVisible());
        QVERIFY(!wordProvider->isVisible());
        QVERIFY(!downloads->property("expanded").toBool());
        const auto originalModel = model->property("text");
        QVERIFY(model->setProperty("text", QString("unsaved-test-model")));
        QVERIFY(mode->setProperty("currentIndex", 0));
        QVERIFY(QMetaObject::invokeMethod(mode, "activated", Q_ARG(int, 0)));
        QVERIFY(study->account()->enabled());
        QVERIFY(!provider->isVisible());
        QVERIFY(downloads->isVisible());
        for (const QSize size : {QSize(1280, 820), QSize(900, 540)}) {
            window->resize(size);
            QTest::qWait(100);
            QVERIFY(mode->width() > 200);
            const auto artifacts = qEnvironmentVariable("NEDEN_CHECK_ARTIFACTS");
            if (!artifacts.isEmpty())
                QVERIFY(window->grabWindow().save(artifacts + QString("/settings-cloud-%1.png").arg(size.width())));
        }
        QVERIFY(mode->setProperty("currentIndex", 1));
        QVERIFY(QMetaObject::invokeMethod(mode, "activated", Q_ARG(int, 1)));
        QVERIFY(!study->account()->enabled());
        QVERIFY(provider->isVisible());
        QCOMPARE(model->property("text").toString(), QString("unsaved-test-model"));
        QTest::qWait(100);
        const auto artifacts = qEnvironmentVariable("NEDEN_CHECK_ARTIFACTS");
        if (!artifacts.isEmpty())
            QVERIFY(window->grabWindow().save(artifacts + "/settings-own-key-900.png"));
        auto *toggle = visualItem(window->contentItem(), "advancedAiSettingsToggle");
        QVERIFY(toggle);
        QVERIFY(QMetaObject::invokeMethod(toggle, "clicked"));
        QVERIFY(wordProvider->isVisible());
        QVERIFY(QMetaObject::invokeMethod(toggle, "clicked"));
        QVERIFY(!wordProvider->isVisible());
        QVERIFY(model->setProperty("text", originalModel));
        auto *languageToggle = visualItem(window->contentItem(), "languageSettingsToggle");
        QVERIFY(languageToggle);
        QVERIFY(QMetaObject::invokeMethod(languageToggle, "clicked"));
        auto *source = visualItem(window->contentItem(), "sourceLanguageChoice");
        auto *response = visualItem(window->contentItem(), "explanationLanguageChoice");
        auto *automatic = visualItem(window->contentItem(), "automaticSubtitlesCheck");
        QVERIFY(source && response && automatic);
        QVERIFY(source->isVisible() && response->isVisible());
        QVERIFY(!source->property("editable").toBool());
        QVERIFY(!response->property("editable").toBool());
        const auto choiceIndex = [](const QVariantList &choices, const QString &code) {
            for (int index = 0; index < choices.size(); ++index) {
                if (choices[index].toMap().value("id").toString() == code)
                    return index;
            }
            return -1;
        };
        QVERIFY(source->setProperty("currentIndex", choiceIndex(study->settings()->sourceLanguages(), "es")));
        QVERIFY(response->setProperty("currentIndex", choiceIndex(study->settings()->explanationLanguages(), "fr")));
        QVERIFY(automatic->isEnabled());
        auto *save = visualItem(window->contentItem(), "saveSettingsButton");
        QVERIFY(save);
        QVERIFY(QMetaObject::invokeMethod(save, "clicked"));
        QCOMPARE(study->settings()->sourceLanguage(), QString("Spanish"));
        QCOMPARE(study->settings()->explanationLanguage(), QString("French"));
        QVERIFY(source->setProperty("currentIndex", 0));
        QVERIFY(!automatic->isEnabled());
        QVERIFY(QMetaObject::invokeMethod(dialog, "close"));
        QTRY_VERIFY(!dialog->property("opened").toBool());
    }

    void subtitleCatalogUiOffersSourcesAndValidatedLanguages() {
        QVERIFY(QMetaObject::invokeMethod(window, "openStudy", Q_ARG(QVariant, 2)));
        auto *drawer = window->findChild<QObject *>("studyDrawer");
        QVERIFY(drawer);
        const auto closeDrawer = qScopeGuard([&] { QMetaObject::invokeMethod(drawer, "close"); });
        auto *provider = visualItem(window->contentItem(), "subtitleSearchProviderChoice");
        QTRY_VERIFY(provider && provider->isVisible());
        auto *language = visualItem(window->contentItem(), "subtitleSearchLanguageChoice");
        auto *title = visualItem(window->contentItem(), "subtitleSearchTitle");
        auto *search = visualItem(window->contentItem(), "subtitleSearchButton");
        QVERIFY(language && title && search);
        QCOMPARE(provider->property("count").toInt(), 3);
        QCOMPARE(provider->property("currentValue").toString(), QString("subvault"));
        QVERIFY(!language->property("editable").toBool());
        QVERIFY(language->property("count").toInt() > 50);
        QVERIFY(provider->setProperty("currentIndex", 1));
        QCOMPARE(provider->property("currentValue").toString(), QString("subdl"));
        QVERIFY(title->property("placeholderText").toString().contains("title"));
        QVERIFY(provider->setProperty("currentIndex", 2));
        QCOMPARE(provider->property("currentValue").toString(), QString("opensubtitles"));
        QVERIFY(title->property("placeholderText").toString().contains("fingerprint"));
        QVERIFY(provider->setProperty("currentIndex", 0));
        QVERIFY(search->height() >= 24);
        QVERIFY(QMetaObject::invokeMethod(drawer, "close"));
        QTRY_VERIFY(!drawer->property("visible").toBool());
    }

    void playbackOptionsPresentGroupedControlsAndReadableDetails() {
        chooseFile("videoPicker", qEnvironmentVariable("NEDEN_CHECK_VIDEO"));
        QTRY_VERIFY_WITH_TIMEOUT(session->duration() > 0, 15000);
        session->setPaused(true);
        chooseFile("primaryPicker", qEnvironmentVariable("NEDEN_CHECK_PRIMARY"));
        chooseFile("secondaryPicker", qEnvironmentVariable("NEDEN_CHECK_SECONDARY"));
        window->setProperty("showRecent", false);
        QVERIFY(QMetaObject::invokeMethod(window->findChild<QObject *>("playerDialogs"), "openPlayerOptions", Q_ARG(QVariant, 1)));
        auto *dialog = window->findChild<QObject *>("playerOptionsDialog");
        auto *options = window->findChild<QObject *>("playerOptions");
        QVERIFY(dialog && options);
        QTRY_VERIFY(dialog->property("opened").toBool());
        // The popup is open before its nested layouts have finished positioning controls.
        QTest::qWait(300);
        session->preferences()->resetPicture();
        click("rotateRightButton");
        QCOMPARE(session->preferences()->rotation(), 90);
        QTest::qWait(300);
        click("resetPictureButton");
        QCOMPARE(session->preferences()->rotation(), 0);
        const auto artifacts = qEnvironmentVariable("NEDEN_CHECK_ARTIFACTS");
        const auto originalSize = window->size();
        for (const QSize size : {QSize(1280, 800), QSize(900, 540)}) {
            window->resize(size);
            QVERIFY(QMetaObject::invokeMethod(dialog, "open"));
            for (int page = 0; page < 5; ++page) {
                QVERIFY(options->setProperty("page", page));
                QTest::qWait(200);
                const auto dialogWidth = dialog->property("width").toReal();
                QVERIFY(dialogWidth <= window->width());
                auto *categories = visualItem(window->contentItem(), "playbackCategories");
                QVERIFY(categories && categories->isVisible());
                QVERIFY(categories->width() > 0 && categories->width() < dialogWidth);
                if (!artifacts.isEmpty())
                    QVERIFY(window->grabWindow().save(
                        artifacts + QString("/options-%1-%2.png").arg(window->width()).arg(page)));
            }
            QVERIFY(QMetaObject::invokeMethod(dialog, "close"));
            QTRY_VERIFY(!dialog->property("visible").toBool());
            window->findChild<QObject *>("playerDialogs")->setProperty("settingsPage", 0);
            QVERIFY(QMetaObject::invokeMethod(window, "openSettings", Q_ARG(QVariant, QVariant())));
            auto *settings = window->findChild<QObject *>("settingsDialog");
            QTRY_VERIFY(settings->property("opened").toBool());
            for (int settingsPage = 0; settingsPage < 3; ++settingsPage) {
                window->findChild<QObject *>("playerDialogs")->setProperty("settingsPage", settingsPage);
                QTest::qWait(200);
                if (!artifacts.isEmpty())
                    QVERIFY(window->grabWindow().save(
                        artifacts + QString("/settings-%1-%2.png").arg(window->width()).arg(settingsPage)));
            }
            QVERIFY(QMetaObject::invokeMethod(settings, "close"));
            QTRY_VERIFY(!settings->property("visible").toBool());
        }
        window->resize(originalSize);
        QVERIFY(QMetaObject::invokeMethod(window, "openStudy", Q_ARG(QVariant, 1)));
        auto *studyDrawer = window->findChild<QObject *>("studyDrawer");
        QVERIFY(studyDrawer);
        QTRY_VERIFY(studyDrawer->property("opened").toBool());
        if (!artifacts.isEmpty())
            QVERIFY(window->grabWindow().save(artifacts + "/study-tabs.png"));
        QVERIFY(QMetaObject::invokeMethod(studyDrawer, "close"));
        QTRY_VERIFY(!studyDrawer->property("visible").toBool());
    }

    void trackDropdownsDisplayTheirNamedRolesAndReadableSelection() {
        window->setProperty("showRecent", false);
        window->setProperty("inspectorMode", 1);
        const QStringList names{"046 - Leyla ile Mecnun 46. Bölüm.tr.srt",
                                "046 - Leyla ile Mecnun 46. Bölüm.en.srt"};
        for (int i = 0; i < names.size(); ++i) {
            QFile file(profile.filePath(names[i]));
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("1\n00:00:00,000 --> 02:00:00,000\nSubtitle\n");
            file.close();
            if (i == 0)
                session->openSubtitles(QUrl::fromLocalFile(file.fileName()));
            else
                session->openTranslation(QUrl::fromLocalFile(file.fileName()));
        }
        const auto originalSize = window->size();
        for (const QSize size : {QSize(900, 540), QSize(1280, 756)}) {
            window->resize(size);
            QTest::qWait(80);
            for (const auto &name : {"primaryTrackChoice", "secondaryTrackChoice"}) {
                auto *choice = visualItem(window->contentItem(), name);
                QVERIFY(choice);
                QTRY_COMPARE(choice->property("currentIndex").toInt(),
                             choice->property("selectedTrackIndex").toInt());
                click(name);
                auto *popup = choice->property("popup").value<QObject *>();
                QVERIFY(popup);
                QTRY_VERIFY(popup->property("visible").toBool());
                for (int index = 0; index < choice->property("count").toInt(); ++index) {
                    const auto rowName = QString::fromUtf8(name) + "Option" + QString::number(index);
                    QTRY_VERIFY(visualItem(window->contentItem(), rowName));
                    auto *row = visualItem(window->contentItem(), rowName);
                    auto *label = row->property("contentItem").value<QQuickItem *>();
                    QVERIFY(label);
                    QString expected;
                    QVERIFY(QMetaObject::invokeMethod(choice, "textAt", Q_RETURN_ARG(QString, expected),
                                                      Q_ARG(int, index)));
                    QVERIFY(!expected.isEmpty());
                    QCOMPARE(label->property("text").toString(), expected);
                    QVERIFY(label->height() >= label->property("implicitHeight").toDouble() - 1);
                    QCOMPARE(label->property("color").value<QColor>(),
                             row->property("highlighted").toBool() ? QColor("#10151b") : QColor("#e6eaf2"));
                    if (index > 0)
                        QCOMPARE(label->property("elide").toInt(), int(Qt::ElideNone));
                }
                auto *view = popup->property("contentItem").value<QQuickItem *>();
                QVERIFY(view);
                const auto bounds = view->mapRectToScene(view->boundingRect());
                QVERIFY(bounds.left() >= 0 && bounds.right() <= window->width());
                const auto artifacts = qEnvironmentVariable("NEDEN_CHECK_ARTIFACTS");
                if (!artifacts.isEmpty())
                    QVERIFY(window->grabWindow().save(artifacts + "/" + name + "-" +
                                                      QString::number(size.width()) + ".png"));
                QVERIFY(QMetaObject::invokeMethod(popup, "close"));
            }
        }
        // Ordinary string models (including playback speed) still show their labels.
        click("speedChoice");
        auto *speed = visualItem(window->contentItem(), "speedChoice");
        QTRY_VERIFY(visualItem(window->contentItem(), "speedChoiceOption1"));
        auto *option = visualItem(window->contentItem(), "speedChoiceOption1");
        QVERIFY(option);
        auto *label = option->property("contentItem").value<QQuickItem *>();
        QVERIFY(label);
        QCOMPARE(label->property("text").toString(), QString("0.75×"));
        auto *popup = speed->property("popup").value<QObject *>();
        QVERIFY(popup && QMetaObject::invokeMethod(popup, "close"));
        window->resize(originalSize);
    }

    void subtitlePickersExcludeTheOtherSlotsTrack() {
        window->setProperty("showRecent", false);
        window->setProperty("inspectorMode", 1);
        const QString primaryPath = profile.filePath("exclusive-primary.srt");
        const QString secondaryPath = profile.filePath("exclusive-secondary.srt");
        for (const auto &path : {primaryPath, secondaryPath}) {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("1\n00:00:00,000 --> 02:00:00,000\nSubtitle\n");
        }
        session->openSubtitles(QUrl::fromLocalFile(primaryPath));
        session->openTranslation(QUrl::fromLocalFile(secondaryPath));
        auto *primary = visualItem(window->contentItem(), "primaryTrackChoice");
        auto *secondary = visualItem(window->contentItem(), "secondaryTrackChoice");
        QVERIFY(primary && secondary);
        auto containsTrack = [](QQuickItem *choice, const QString &name) {
            for (int index = 0; index < choice->property("count").toInt(); ++index) {
                QString text;
                QMetaObject::invokeMethod(choice, "textAt", Q_RETURN_ARG(QString, text), Q_ARG(int, index));
                if (text == name)
                    return true;
            }
            return false;
        };
        QTRY_VERIFY(containsTrack(primary, "exclusive-primary.srt"));
        QVERIFY(!containsTrack(primary, "exclusive-secondary.srt"));
        QVERIFY(containsTrack(secondary, "exclusive-secondary.srt"));
        QVERIFY(!containsTrack(secondary, "exclusive-primary.srt"));
        QCOMPARE(primary->property("displayText").toString(), QString("exclusive-primary.srt"));
        QCOMPARE(secondary->property("displayText").toString(), QString("exclusive-secondary.srt"));
        QVERIFY(QMetaObject::invokeMethod(window->findChild<QObject *>("playerDialogs"), "openPlayerOptions", Q_ARG(QVariant, QVariant(2))));
        auto *optionsPrimary = visualItem(window->contentItem(), "primaryOptionsTrackChoice");
        auto *optionsSecondary = visualItem(window->contentItem(), "secondaryOptionsTrackChoice");
        QVERIFY(optionsPrimary && optionsSecondary);
        QVERIFY(containsTrack(optionsPrimary, "exclusive-primary.srt"));
        QVERIFY(!containsTrack(optionsPrimary, "exclusive-secondary.srt"));
        QVERIFY(!containsTrack(optionsSecondary, "exclusive-primary.srt"));
        QVERIFY(containsTrack(optionsSecondary, "exclusive-secondary.srt"));
        auto *options = window->findChild<QObject *>("playerOptionsDialog");
        QVERIFY(options && QMetaObject::invokeMethod(options, "close"));
        session->clearTrack(false);
        QTRY_VERIFY(containsTrack(secondary, "exclusive-primary.srt"));
        QVERIFY(containsTrack(optionsSecondary, "exclusive-primary.srt"));
        session->chooseTrack(primaryPath, true);
        QTRY_VERIFY(!containsTrack(primary, "exclusive-primary.srt"));
        QVERIFY(containsTrack(primary, "exclusive-secondary.srt"));
        QCOMPARE(secondary->property("displayText").toString(), QString("exclusive-primary.srt"));
        session->clearTrack(true);
        QTRY_VERIFY(containsTrack(primary, "exclusive-primary.srt"));
        QVERIFY(containsTrack(primary, "exclusive-secondary.srt"));
    }

    void trackSelectorsAndAppearancePreviewUseActualControls() {
        window->setProperty("showRecent", false);
        window->setProperty("inspectorMode", 1);
        auto *primary = visualItem(window->contentItem(), "primaryTrackChoice");
        auto *secondary = visualItem(window->contentItem(), "secondaryTrackChoice");
        QVERIFY(primary && secondary);
        // Each picker includes Off and Load file, and hides the other slot's track.
        int primaryChoices = 2;
        int secondaryChoices = 2;
        for (const auto &entry : session->subtitleTracks()) {
            const auto track = entry.toMap();
            primaryChoices += !track.value("secondary").toBool();
            secondaryChoices += !track.value("primary").toBool();
        }
        QCOMPARE(primary->property("count").toInt(), primaryChoices);
        QCOMPARE(secondary->property("count").toInt(), secondaryChoices);
        QCOMPARE(primary->property("currentText").toString(), session->sourceSubtitleName().isEmpty() ? QString("Off") : session->sourceSubtitleName());
        QCOMPARE(secondary->property("currentText").toString(), session->translationSubtitleName().isEmpty() ? QString("Off") : session->translationSubtitleName());

        QTest::keyClick(window, Qt::Key_Comma, Qt::ControlModifier);
        auto *settings = window->findChild<QObject *>("settingsDialog");
        QVERIFY(settings);
        QTRY_VERIFY(settings->property("opened").toBool());
        auto *size = visualItem(window->contentItem(), "primarySizeSlider");
        QVERIFY(size);
        const auto savedSize = appearance->primarySize();
        QVERIFY(size->setProperty("value", savedSize + 4));
        QVERIFY(QMetaObject::invokeMethod(size, "moved"));
        QCOMPARE(appearance->primarySize(), savedSize + 4);
        QVERIFY(QMetaObject::invokeMethod(settings, "close"));
        QTRY_VERIFY(!settings->property("opened").toBool());
        QCOMPARE(appearance->primarySize(), savedSize);
    }

    void fullscreenShortcutsChooseTheirModeWithoutChangingThePreference() {
        chooseFile("videoPicker", qEnvironmentVariable("NEDEN_CHECK_VIDEO"));
        QTRY_VERIFY(session->duration() > 0);
        session->setPaused(true);
        window->setProperty("showRecent", false);
        window->setProperty("inspectorMode", 1);
        const bool originalPreference = fullscreen->nativeMode();
        const auto restorePreference = qScopeGuard([&] {
            fullscreen->setActive(false);
            fullscreen->setNativeMode(originalPreference);
        });
        fullscreen->setNativeMode(true);
        window->contentItem()->forceActiveFocus();
        auto *shortcuts = window->findChild<QObject *>("playerShortcuts");
        QVERIFY(shortcuts);
        QTRY_VERIFY(shortcuts->property("fullscreenKeyEnabled").toBool());
        QTest::keyClick(window, Qt::Key_F);
        QTRY_VERIFY(fullscreen->active());
#ifdef Q_OS_MACOS
        QVERIFY(window->visibility() != QWindow::FullScreen);
        QVERIFY(fullscreen->nativeMode());
#endif
        QTest::keyClick(window, Qt::Key_F);
        QTRY_VERIFY(!fullscreen->active());

        fullscreen->setNativeMode(false);
        fullscreen->toggleNative();
        QTRY_VERIFY_WITH_TIMEOUT(fullscreen->active(), 10000);
        QTRY_COMPARE(window->visibility(), QWindow::FullScreen);
        QVERIFY(!fullscreen->nativeMode());
        fullscreen->toggleNative();
        QTRY_VERIFY_WITH_TIMEOUT(!fullscreen->active(), 10000);
        QVERIFY(session->paused());

        click("transcriptSearchButton");
        auto *search = visualItem(window->contentItem(), "transcriptSearchField");
        QVERIFY(search && search->hasActiveFocus());
        QTest::keyClick(window, Qt::Key_F);
        QCOMPARE(search->property("text").toString(), QString("f"));
        QVERIFY(!fullscreen->active());
        click("transcriptSearchButton");
    }

    void videoRenderingIgnoresInheritedFaceCulling() {
        const auto fixture = QFINDTESTDATA("fixtures/white-smoke.mp4");
        QVERIFY(!fixture.isEmpty());
        session->openVideo(QUrl::fromLocalFile(fixture));
        window->setProperty("showRecent", false);
        window->setProperty("inspectorMode", 0);
        QTRY_VERIFY_WITH_TIMEOUT(session->duration() > 0, 10000);
        session->setPaused(false);

        // Reproduce graphics state left by another scene-graph renderer. The
        // video must survive it regardless of triangle winding or render loop.
        const auto injectedFrames = std::make_shared<std::atomic<int>>(0);
        const auto injection = connect(window, &QQuickWindow::beforeSynchronizing, window,
            [injectedFrames] {
                auto *context = QOpenGLContext::currentContext();
                if (!context)
                    return;
                context->functions()->glEnable(GL_CULL_FACE);
                context->functions()->glCullFace(GL_FRONT_AND_BACK);
                ++*injectedFrames;
            }, Qt::DirectConnection);
        const auto cleanup = qScopeGuard([&] {
            disconnect(injection);
            session->setPaused(true);
        });
        auto *video = visualItem(window->contentItem(), "videoSurface");
        QVERIFY(video);
        QTRY_VERIFY_WITH_TIMEOUT(injectedFrames->load() >= 3, 5000);
        for (int sample = 0; sample < 6; ++sample) {
            QTest::qWait(250);
            const auto image = window->screen()->grabWindow(window->winId()).toImage();
            QVERIFY(!image.isNull());
            const auto center = video->mapToScene(QPointF(video->width() / 2, video->height() / 2));
            const QPoint pixel(qRound(center.x() * image.width() / window->width()),
                               qRound(center.y() * image.height() / window->height()));
            QVERIFY(image.rect().contains(pixel));
            const auto color = image.pixelColor(pixel);
            QVERIFY2(color.red() >= 220 && color.green() >= 220 && color.blue() >= 220,
                     "Presented video became black under inherited face culling");
        }
        QVERIFY(injectedFrames->load() >= 6);
    }

    void singleClickVideoTogglesPlaybackAndClosesPausedSidebar() {
        chooseFile("videoPicker", qEnvironmentVariable("NEDEN_CHECK_VIDEO"));
        QTRY_VERIFY(session->duration() > 0);
        window->setProperty("showRecent", false);
        window->setProperty("inspectorMode", 0);
        session->setPaused(true);
        auto *video = visualItem(window->contentItem(), "videoSurface");
        QVERIFY(video && video->isVisible());
        const auto clickPicture = [&] {
            QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                video->mapToScene(QPointF(video->width() / 2, video->height() / 3)).toPoint());
        };
        clickPicture();
        QTRY_VERIFY(!session->paused());
        clickPicture();
        QTRY_VERIFY(session->paused());
        window->setProperty("inspectorMode", 1);
        clickPicture();
        QTRY_VERIFY(!session->paused());
        QCOMPARE(window->property("inspectorMode").toInt(), 0);
        session->setPaused(true);
    }

    void doubleClickVideoTogglesFullscreenWithoutChangingPlayback() {
        window->setProperty("showRecent", false);
        session->setPaused(true);
        const double originalPosition = session->position();

        // 1. Enter fullscreen by double-clicking the picture, away from selectable subtitles.
        auto *video = visualItem(window->contentItem(), "videoSurface");
        QVERIFY(video && video->isVisible());
        QTest::mouseDClick(window, Qt::LeftButton, Qt::NoModifier,
                           video->mapToScene(QPointF(video->width() / 2, video->height() / 3)).toPoint());
        QTRY_VERIFY(fullscreen->active());
        QVERIFY(session->paused());
        QVERIFY(std::abs(session->position() - originalPosition) < 0.1);

        // 2. Use the same gesture on the resized picture to return to the normal window.
        QTest::qWait(200);
        QTest::mouseDClick(window, Qt::LeftButton, Qt::NoModifier,
                           video->mapToScene(QPointF(video->width() / 2, video->height() / 3)).toPoint());
        QTRY_VERIFY(!fullscreen->active());
        QVERIFY(session->paused());
        QVERIFY(std::abs(session->position() - originalPosition) < 0.1);
    }

    void fullscreenControlsOverlayVideoWithoutResizingIt() {
        window->setProperty("showRecent", false);
        session->closeDefinition();
        session->setPaused(true);
        const auto originalInspector = window->property("inspectorMode");
        const auto restoreWindow = qScopeGuard([&] {
            fullscreen->setActive(false);
            window->setProperty("inspectorMode", originalInspector);
            window->setProperty("controlsIdle", false);
        });
        auto *video = visualItem(window->contentItem(), "videoSurface");
        auto *controls = visualItem(window->contentItem(), "playbackControls");
        auto *sidebar = visualItem(window->contentItem(), "learningInspector");
        QVERIFY(video && controls && sidebar);
        const auto bounds = [](QQuickItem *item) { return item->mapRectToScene(item->boundingRect()); };

        for (const bool nativeMode : {false, true}) {
            if (nativeMode && !fullscreen->supportsNativeMode())
                continue;
            window->setProperty("inspectorMode", 0);
            QTRY_COMPARE(sidebar->property("reservedWidth").toDouble(), 0.0);
            if (nativeMode)
                fullscreen->toggleNative();
            else
                fullscreen->toggleInstant();
            QTRY_VERIFY_WITH_TIMEOUT(fullscreen->active(), 10000);
            window->setProperty("controlsIdle", false);
            QTRY_VERIFY(std::abs(bounds(video).bottom() - bounds(controls).bottom()) < 1);
            QVERIFY(bounds(video).intersects(bounds(controls)));
            QVERIFY(std::abs(bounds(video).right() - bounds(controls).right()) < 1);

            const auto videoBounds = bounds(video);
            window->setProperty("controlsIdle", true);
            QTRY_COMPARE(controls->opacity(), 0.0);
            QCOMPARE(bounds(video), videoBounds);
            window->setProperty("controlsIdle", false);
            QTRY_COMPARE(controls->opacity(), 1.0);
            QCOMPARE(bounds(video), videoBounds);

            // Controls end at the video edge, leaving the full-height sidebar unobstructed.
            window->setProperty("inspectorMode", 1);
            QTRY_COMPARE(sidebar->property("revealProgress").toDouble(), 1.0);
            QTRY_VERIFY(std::abs(bounds(video).right() - bounds(controls).right()) < 1);
            QVERIFY(std::abs(bounds(sidebar).bottom() - bounds(video).bottom()) < 1);
            const auto artifacts = qEnvironmentVariable("NEDEN_CHECK_ARTIFACTS");
            if (!artifacts.isEmpty())
                QVERIFY(window->grabWindow().save(artifacts + (nativeMode ? "/native-fullscreen-overlay.png" : "/instant-fullscreen-overlay.png")));

            fullscreen->setActive(false);
            QTRY_VERIFY_WITH_TIMEOUT(!fullscreen->active(), 10000);
            QTRY_VERIFY(std::abs(bounds(video).bottom() - bounds(controls).top()) < 1);
        }
    }

    void fullscreenAndCompactWindowKeepControlsAvailable() {
        window->setProperty("showRecent", false);
        if (!fullscreen->nativeMode()) {
            fullscreen->toggle();
            QTRY_VERIFY(fullscreen->active());
            QTest::qWait(200);
            QVERIFY(visualItem(window->contentItem(), "volumeSlider")->isVisible());
            auto *video = visualItem(window->contentItem(), "videoSurface");
            auto *sidebar = visualItem(window->contentItem(), "learningInspector");
            QVERIFY(video && sidebar);
            QVERIFY(std::abs(video->mapToScene(QPointF()).y()) < 1);
            QVERIFY(std::abs(sidebar->mapToScene(QPointF()).y()) < 1);
            const auto artifacts = qEnvironmentVariable("NEDEN_CHECK_ARTIFACTS");
            if (!artifacts.isEmpty())
                QVERIFY(window->grabWindow().save(artifacts + "/fullscreen-controls.png"));
            fullscreen->toggle();
            QTRY_VERIFY(!fullscreen->active());
#ifdef Q_OS_MACOS
            QTRY_VERIFY(nativeVideoExtendsUnderTitlebar(window));
#endif
            QTest::qWait(200);
        }
        const auto originalSize = window->size();
        window->resize(900, 540);
        QTest::qWait(200);
        for (const auto &name : {"previousSubtitleButton", "nextSubtitleButton", "replaySubtitleButton",
                                 "volumeSlider", "speedChoice"}) {
            auto *item = visualItem(window->contentItem(), name);
            QVERIFY(item && item->isVisible());
            const auto bounds = item->mapRectToScene(item->boundingRect());
            QVERIFY(bounds.left() >= 0 && bounds.right() <= window->width());
            QVERIFY(bounds.top() >= 0 && bounds.bottom() <= window->height());
        }
        const auto artifacts = qEnvironmentVariable("NEDEN_CHECK_ARTIFACTS");
        if (!artifacts.isEmpty())
            QVERIFY(window->grabWindow().save(artifacts + "/compact.png"));
        window->resize(originalSize);
    }

    void packagedHttpsBackendConnectsWithoutCredentials() {
        QVERIFY(QSslSocket::supportsSsl());
        QVERIFY(QSslSocket::availableBackends().contains("securetransport"));
        QNetworkAccessManager network;
        QNetworkRequest request(QUrl("https://api.deepseek.com/models"));
        request.setTransferTimeout(15000);
        auto *reply = network.get(request);
        QSignalSpy finished(reply, &QNetworkReply::finished);
        QVERIFY(finished.wait(20000));
        // An unauthenticated API response proves TLS works; no paid request is sent.
        QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 401);
        QVERIFY(reply->error() != QNetworkReply::SslHandshakeFailedError);
        reply->deleteLater();
    }

    void volumeSelectionAndExplanationUseTheActualControls() {
        // 1. Move the real slider and verify the session, then restore the paused video.
        window->setProperty("showRecent", false);
        auto *volume = visualItem(window->contentItem(), "volumeSlider");
        QVERIFY(volume);
        QVERIFY(volume->isVisible());
        QTest::mouseClick(
            window, Qt::LeftButton, Qt::NoModifier,
            volume->mapToScene(QPointF(volume->width() * 0.35, volume->height() / 2)).toPoint());
        QVERIFY(session->volumeLevel() > 20 && session->volumeLevel() < 50);
        session->volume(0);

        // 2. Open the actual speed popup and verify the highlighted row remains readable.
        click("speedChoice");
        auto *speed = visualItem(window->contentItem(), "speedChoice");
        QVERIFY(speed);
        const auto selectedRow = QString("speedChoiceOption%1").arg(speed->property("currentIndex").toInt());
        QTRY_VERIFY(visualItem(window->contentItem(), selectedRow));
        auto *option = visualItem(window->contentItem(), selectedRow);
        QVERIFY(option->property("highlighted").toBool());
        auto *label = option->property("contentItem").value<QQuickItem *>();
        QVERIFY(label);
        QCOMPARE(label->property("color").value<QColor>(), QColor("#10151b"));
        const auto artifacts = qEnvironmentVariable("NEDEN_CHECK_ARTIFACTS");
        if (!artifacts.isEmpty())
            QVERIFY(window->grabWindow().save(artifacts + "/highlight.png"));
        auto *popup = speed->property("popup").value<QObject *>();
        QVERIFY(popup && QMetaObject::invokeMethod(popup, "close"));

        // 3. Double-click a synthetic Turkish word and serve an incremental answer locally.
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        int requests = 0;
        connect(&server, &QTcpServer::newConnection, this, [&] {
            auto *socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
                socket->readAll();
                if (socket->property("sent").toBool())
                    return;

                socket->setProperty("sent", true);
                ++requests;
                if (requests == 1) {
                    socket->write(
                        "HTTP/1.1 429 Too Many Requests\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
                    socket->disconnectFromHost();
                    return;
                }

                socket->write(
                    "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nConnection: close\r\n\r\n");
                socket->write(
                    "data: {\"choices\":[{\"delta\":{\"content\":\"### Natural meaning\\nLight\"}}]}\n\n");
                QTimer::singleShot(250, socket, [socket] {
                    socket->write("data: [DONE]\n\n");
                    socket->disconnectFromHost();
                });
            });
        });
        QVERIFY(study->settings()->configure(QString("http://127.0.0.1:%1").arg(server.serverPort()), "mock",
                                             "https://example.org/%s", false));
        const auto start = qRound64(session->position() * 1000) - 500;
        session->replaceCues({{start, start + 10000, "ışık", {}}});
        QTest::qWait(250);
        auto *subtitle = visualItem(window->contentItem(), "primarySubtitleText");
        QVERIFY(subtitle && subtitle->isVisible());
        QTest::mouseDClick(
            window, Qt::LeftButton, Qt::NoModifier,
            subtitle->mapToScene(QPointF(subtitle->width() / 2, subtitle->height() / 2)).toPoint());
        QTRY_COMPARE(session->selection(), QString("ışık"));
        QVERIFY(!fullscreen->active());
        QTRY_COMPARE_WITH_TIMEOUT(study->explanation()->state(), QString("failed"), 5000);
        QVERIFY(study->explanation()->message().contains("allowance"));
        QTest::qWait(300);
        QCOMPARE(requests, 1);
        if (!artifacts.isEmpty())
            QVERIFY(window->grabWindow().save(artifacts + "/network-error.png"));
        click("explainButton");
        QTRY_COMPARE_WITH_TIMEOUT(study->explanation()->state(), QString("complete"), 5000);
        QCOMPARE(requests, 2);
        QCOMPARE(study->explanation()->answer(), QString("### Natural meaning\nLight"));
        QCOMPARE(window->property("inspectorTab").toInt(), 1);
        auto *meaning = window->findChild<QObject *>("explanationLoader");
        QVERIFY(meaning && meaning->property("active").toBool());
        if (!artifacts.isEmpty())
            QVERIFY(window->grabWindow().save(artifacts + "/explanation.png"));

        // 4. Draft highlight changes leave the completed phrase and answer stable.
        auto *subject = visualItem(window->contentItem(), "explanationSubject");
        QVERIFY(subject);
        QCOMPARE(subject->property("text").toString(), QString("ışık"));
        session->select("ış");
        QTest::qWait(100);
        QCOMPARE(subject->property("text").toString(), QString("ışık"));
        QCOMPARE(study->explanation()->answer(), QString("### Natural meaning\nLight"));
        QCOMPARE(requests, 2);

        // 5. Read-only subtitle and answer focus must not disable Space or require a click first.
        auto *editor = subtitle->property("editor").value<QQuickItem *>();
        QVERIFY(editor);
        editor->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_Space);
        QTRY_VERIFY(!session->paused());
        QTest::keyClick(window, Qt::Key_Space);
        QTRY_VERIFY(session->paused());
        auto *answer = visualItem(window->contentItem(), "explanationAnswerText");
        QVERIFY(answer);
        answer->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_Space);
        QTRY_VERIFY(!session->paused());
        QTest::keyClick(window, Qt::Key_Space);
        QTRY_VERIFY(session->paused());

        // 6. Clicking the picture returns focus to playback and resumes the paused video.
        editor->forceActiveFocus();
        auto *video = visualItem(window->contentItem(), "videoSurface");
        QVERIFY(video);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                          video->mapToScene(QPointF(video->width() / 2, video->height() / 3)).toPoint());
        QVERIFY(!editor->hasActiveFocus());
        QTRY_VERIFY(!session->paused());
        QTest::keyClick(window, Qt::Key_Space);
        QTRY_VERIFY(session->paused());
        click("transcriptTab");
        QTRY_VERIFY(session->selection().isEmpty());
    }

    void videoAndSidebarReachTheTopWithoutATitleStrip() {
        if (!qEnvironmentVariableIsSet("NEDEN_CHECK_HEADLESS")) {
            session->openVideo(QUrl::fromLocalFile(qEnvironmentVariable("NEDEN_CHECK_VIDEO")));
            QTRY_VERIFY_WITH_TIMEOUT(session->duration() > 0, 15000);
            session->setPaused(true);
        }
        window->setProperty("showRecent", false);
        window->setProperty("inspectorMode", 1);
        const auto savedSize = window->size();
        for (const QSize size : {QSize(900, 540), QSize(1280, 756)}) {
            window->resize(size);
            QTest::qWait(100);
            auto *video = visualItem(window->contentItem(), "videoSurface");
            auto *sidebar = visualItem(window->contentItem(), "learningInspector");
            auto *tabs = visualItem(window->contentItem(), "transcriptTab");
            QVERIFY(video && sidebar && tabs);
            QTRY_COMPARE(sidebar->property("revealProgress").toDouble(), 1.0);
            QVERIFY(!visualItem(window->contentItem(), "playerToolbar"));
            const auto content = window->property("contentItem").value<QQuickItem *>();
            QVERIFY(content);
            const auto top = content->mapToScene(QPointF()).y();
            QVERIFY(std::abs(video->mapToScene(QPointF()).y() - top) < 1);
            QVERIFY(std::abs(sidebar->mapToScene(QPointF()).y() - top) < 1);
            const auto tabBounds = tabs->mapRectToScene(tabs->boundingRect());
            QVERIFY(tabBounds.top() >= top && tabBounds.bottom() < top + 100);
            for (const auto &name : {"volumeSlider", "speedChoice", "toggleSidebarButton"}) {
                auto *control = visualItem(window->contentItem(), name);
                QVERIFY(control && control->isVisible());
                const auto bounds = control->mapRectToScene(control->boundingRect());
                QVERIFY(bounds.left() >= 0 && bounds.right() <= window->width());
            }
            click("closeSidebarButton");
            QTRY_VERIFY(!sidebar->isVisible());
            click("toggleSidebarButton");
            QTRY_VERIFY(sidebar->isVisible());
        }
        window->resize(savedSize);
    }

    void singleClickShowsContextualWordCardWithoutChangingTheSidebar() {
        if (!qEnvironmentVariableIsSet("NEDEN_CHECK_HEADLESS")) {
            session->openVideo(QUrl::fromLocalFile(qEnvironmentVariable("NEDEN_CHECK_VIDEO")));
            QTRY_VERIFY_WITH_TIMEOUT(session->duration() > 0, 15000);
        }
        window->setProperty("showRecent", false);
        session->closeDefinition();
        session->setPaused(true);
        StudyResponseServer server;
        configureStudyResponses(server);
        auto &requests = server.requests;
        auto &wordRequests = server.wordRequests;
        auto &sentContext = server.sentContext;
        const QString cue = "Ben televizyona dalmışım ya.";
        session->replaceCues({{0, 7200000, cue, {}}});
        auto *subtitle = visualItem(window->contentItem(), "primarySubtitleText");
        QVERIFY(subtitle);
        auto *editor = subtitle->property("editor").value<QQuickItem *>();
        QVERIFY(editor);
        QTest::qWait(100);
        QRectF glyph;
        QVERIFY(QMetaObject::invokeMethod(editor, "positionToRectangle", Q_RETURN_ARG(QRectF, glyph),
                                          Q_ARG(int, cue.indexOf("dalmışım") + 2)));
        auto point = editor->mapToScene(glyph.center() + QPointF(3, 0)).toPoint();
        const auto originalSubject = study->explanation()->subject();
        QSignalSpy wordRequested(subtitle, SIGNAL(wordRequested(int)));
        QTest::mouseMove(window, point);
        QTRY_VERIFY(visualItem(subtitle, "subtitleWordHover"));
        QCOMPARE(editor->cursor().shape(), Qt::IBeamCursor);
        QCOMPARE(editor->property("selectedText").toString(), QString());
        QCOMPARE(requests, 0); // Hover must never spend an API request.
        QVERIFY(!session->lookupCachedWordAt(cue, cue.indexOf("dalmışım")));
        QCOMPARE(study->words()->state(), QString("idle"));
        QCOMPARE(requests, 0); // A cache miss must leave the normal click path untouched.
        QTest::mouseMove(window, subtitle->mapToScene(QPointF(2, subtitle->height() / 2)).toPoint());
        QTRY_VERIFY(!visualItem(subtitle, "subtitleWordHover"));
        QTest::mouseMove(window, point);
        QTRY_VERIFY(visualItem(subtitle, "subtitleWordHover"));
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, point);
        QTRY_COMPARE(study->words()->state(), QString("complete"));
        QCOMPARE(wordRequested.count(), 1);
        QCOMPARE(study->words()->word(), QString("dalmışım"));
        QCOMPARE(study->explanation()->subject(), originalSubject);
        QCOMPARE(requests, 1);
        QCOMPARE(wordRequests, 1);
        QCOMPARE(sentContext.value("clickedSubtitle").toString(), cue);
        QCOMPARE(sentContext.value("sourceCue").toString(), cue);
        auto *card = visualItem(window->contentItem(), "wordMeaningCard");
        QVERIFY(card && card->isVisible());
        QVERIFY(!visualItem(card, "wordFullExplanationButton"));
        auto *grammar = visualItem(card, "wordGrammar");
        QVERIFY(grammar);
        QCOMPARE(grammar->property("text").toString(), QString("verb"));
        QVERIFY(card->height() < window->height());
        QVERIFY(!visualItem(card, "wordOtherSenses")->isVisible());
        click("wordDetailsButton");
        QTRY_VERIFY(visualItem(card, "wordOtherSenses")->isVisible());
        QVERIFY(!visualItem(card, "wordOtherUses")->isVisible());
        auto *explainButton = visualItem(window->contentItem(), "primarySubtitleTextExplainButton");
        QVERIFY(explainButton);
        QTest::mouseMove(window, point);
        QTRY_VERIFY(explainButton->isVisible());
        click("primarySubtitleTextExplainButton");
        QTRY_COMPARE(study->explanation()->state(), QString("complete"));
        QCOMPARE(study->explanation()->subject(), cue);
        QCOMPARE(study->words()->state(), QString("idle"));
        QCOMPARE(requests, 3);
        QCOMPARE(wordRequests, 2);
        session->closeDefinition();
        // Opening an explanation changes the picture width. Wait for the
        // sidebar transition, then locate the word in its current layout.
        auto *sidebar = visualItem(window->contentItem(), "learningInspector");
        QVERIFY(sidebar);
        QTRY_COMPARE(sidebar->property("revealProgress").toDouble(), 1.0);
        point = wordPoint(subtitle, cue.indexOf("dalmışım") + 2);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, point);
        QTRY_COMPARE(study->words()->state(), QString("complete"));
        QCOMPARE(requests, 3);
        QCOMPARE(wordRequests, 2);
        // Cached switches must finish before the 350ms minimum gesture delay.
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                          wordPoint(subtitle, cue.indexOf("televizyona") + 2));
        QTRY_COMPARE_WITH_TIMEOUT(study->words()->word(), QString("televizyona"), 150);
        QCOMPARE(study->words()->state(), QString("complete"));
        QVERIFY(!session->lookupCachedWordAt("Another scene.", 2));
        QCOMPARE(study->words()->word(), QString("televizyona"));
        QCOMPARE(wordRequests, 2);
        const auto artifacts = qEnvironmentVariable("NEDEN_CHECK_ARTIFACTS");
        if (!artifacts.isEmpty())
            QVERIFY(window->grabWindow().save(artifacts + "/contextual-word-card.png"));
        click("closeWordCardButton");
        QTRY_COMPARE(study->words()->state(), QString("idle"));
        QVERIFY(session->paused());
        // Even after the first click immediately opens a cached card, double-click
        // selection must replace it with an explanation without another word charge.
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, point);
        QTRY_COMPARE_WITH_TIMEOUT(study->words()->state(), QString("complete"), 150);
        QTest::mouseDClick(window, Qt::LeftButton, Qt::NoModifier, point);
        QTRY_COMPARE(study->explanation()->state(), QString("complete"));
        QTest::qWait(450);
        QCOMPARE(requests, 4);
        QCOMPARE(wordRequests, 2);
        QCOMPARE(study->explanation()->subject(), QString("dalmışım"));
        // Repeating that gesture on an already-explained word dismisses the card too.
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, point);
        QTRY_COMPARE_WITH_TIMEOUT(study->words()->state(), QString("complete"), 150);
        QTest::mouseDClick(window, Qt::LeftButton, Qt::NoModifier, point);
        QTRY_COMPARE_WITH_TIMEOUT(study->words()->state(), QString("idle"), 150);
        QCOMPARE(study->explanation()->subject(), QString("dalmışım"));
        // Dragging after a cached lookup must still select a phrase and dismiss the card.
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                          wordPoint(subtitle, cue.indexOf("televizyona") + 2));
        QTRY_COMPARE_WITH_TIMEOUT(study->words()->state(), QString("complete"), 150);
        QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier,
                          wordPoint(subtitle, cue.indexOf("televizyona") + 1));
        const auto dragEnd = wordPoint(subtitle, cue.indexOf("dalmışım") + 5);
        QTest::mouseMove(window, dragEnd, 30);
        QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, dragEnd);
        QTRY_COMPARE(study->explanation()->state(), QString("complete"));
        QVERIFY(study->explanation()->subject().contains("levizyona"));
        QVERIFY(study->explanation()->subject().contains("dalm"));
        QCOMPARE(study->words()->state(), QString("idle"));
        QCOMPARE(wordRequests, 2);
        // A changed cue cancels a card, so it cannot explain a previous scene.
        session->lookupWordAt(cue, cue.indexOf("dalmışım"));
        session->replaceCues({{0, 7200000, "Başka bir sahne.", {}}});
        QTest::qWait(450);
        QCOMPARE(study->words()->state(), QString("idle"));
        QVERIFY(!card->isVisible());
    }

    void wordCardSeparatesSensesFormsAndUses() {
        StudyResponseServer server;
        configureStudyResponses(server);
        window->setProperty("showRecent", false);
        session->closeDefinition();
        session->setPaused(true);
        const QString cue = "Onları uzaktan izlersin.";
        session->replaceCues({{0, 7200000, cue, {}}});
        server.wordCardResponse = R"({"lemma":"uzak","partOfSpeech":"adverb","meaning":"from a distance","explanation":"","formNote":"Uzak means far; -tan adds from.","otherSenses":[],"otherUses":[{"meaning":"remotely","example":"uzaktan çalışmak","translation":"to work remotely","formNote":""}]})";
        session->lookupWordAt(cue, cue.indexOf("uzaktan"));
        QTRY_COMPARE(study->words()->state(), QString("complete"));
        auto *card = visualItem(window->contentItem(), "wordMeaningCard");
        QVERIFY(card && card->isVisible());
        auto *senses = visualItem(card, "wordOtherSenses");
        auto *uses = visualItem(card, "wordOtherUses");
        auto *form = visualItem(card, "wordFormNote");
        QVERIFY(senses && uses && form);
        QVERIFY(!senses->isVisible());
        QVERIFY(!uses->isVisible());
        QVERIFY(!form->isVisible());
        auto *details = visualItem(card, "wordDetailsButton");
        QVERIFY(details && details->isVisible());
        const auto compactHeight = card->height();
        auto *headword = visualItem(card, "wordHeadword");
        auto *grammar = visualItem(card, "wordGrammar");
        auto *meaning = visualItem(card, "wordContextMeaning");
        QVERIFY(headword && grammar && meaning);
        QCOMPARE(grammar->property("text").toString(), QString("adverb"));
        QTRY_VERIFY(grammar->mapToItem(card, QPointF()).y() >=
                    headword->mapToItem(card, QPointF()).y() + headword->height());
        QTRY_VERIFY(meaning->mapToItem(card, QPointF()).y() >=
                    grammar->mapToItem(card, QPointF()).y() + grammar->height());
        QVERIFY(QMetaObject::invokeMethod(details, "clicked"));
        QTRY_VERIFY(uses->isVisible());
        QVERIFY(form->isVisible());
        QTRY_VERIFY(card->height() > compactHeight);
        QCOMPARE(study->words()->otherUses().first().toMap().value("translation").toString(),
                 QString("to work remotely"));
        const auto artifacts = qEnvironmentVariable("NEDEN_CHECK_ARTIFACTS");
        if (!artifacts.isEmpty()) {
            QTest::qWait(100);
            QVERIFY(window->grabWindow().save(artifacts + "/word-related-use.png"));
        }
        // A different scene has its own cache; details remain opt-in even when empty.
        server.wordCardResponse = R"({"lemma":"uzak","partOfSpeech":"adverb","meaning":"from a distance","explanation":"","formNote":"","otherSenses":[],"otherUses":[]})";
        session->closeWordCard(false);
        const QString nextCue = "Ben uzaktan baktım.";
        session->replaceCues({{0, 7200000, nextCue, {}}});
        session->lookupWordAt(nextCue, nextCue.indexOf("uzaktan"));
        QTRY_COMPARE(study->words()->state(), QString("complete"));
        QVERIFY(!senses->isVisible());
        QVERIFY(!uses->isVisible());
        QVERIFY(!form->isVisible());
        QVERIFY(details->isVisible());
        QVERIFY(!card->property("detailsExpanded").toBool());
        QVERIFY(QMetaObject::invokeMethod(details, "clicked"));
        QTRY_COMPARE(study->words()->detailsState(), QString("complete"));
        QVERIFY(!uses->isVisible());
        QVERIFY(!form->isVisible());
        session->closeWordCard(false);
    }

    void followUpComposerGrowsWithoutMovingSendButton() {
        StudyResponseServer server;
        prepareCompletedExplanation(server);
        auto *question = visualItem(window->contentItem(), "followUpInput");
        auto *composer = visualItem(window->contentItem(), "followUpComposer");
        auto *inputViewport = visualItem(window->contentItem(), "followUpScrollView");
        auto *sendButton = visualItem(window->contentItem(), "sendFollowUpButton");
        auto *saveButton = visualItem(window->contentItem(), "saveFlashcardButton");
        QVERIFY(question && composer && inputViewport && sendButton && saveButton);
        question->setProperty("text", "Why?");
        QTRY_COMPARE(inputViewport->height(), sendButton->height());
        const auto singleLineHeight = sendButton->height();
        question->setProperty("text", QString("How does this expression work in a different sentence? ").repeated(10));
        QTRY_VERIFY(inputViewport->height() > singleLineHeight);
        QVERIFY(inputViewport->height() <= singleLineHeight * 2);
        QCOMPARE(sendButton->height(), singleLineHeight);
        QVERIFY(std::abs(composer->y() + composer->height() - composer->parentItem()->height()) < 1);
        QVERIFY(saveButton->mapToScene(QPointF()).y() < composer->mapToScene(QPointF()).y());
        const auto composerArtifacts = qEnvironmentVariable("NEDEN_CHECK_ARTIFACTS");
        if (!composerArtifacts.isEmpty())
            QVERIFY(window->grabWindow().save(composerArtifacts + "/follow-up-composer.png"));
        question->setProperty("text", "");
        QTRY_COMPARE(inputViewport->height(), singleLineHeight);
    }

    void followUpSupportsMacDeleteToLineStart() {
#ifndef Q_OS_MACOS
        QSKIP("Command-Backspace is a macOS editing shortcut.");
#endif
        StudyResponseServer server;
        prepareCompletedExplanation(server);
        auto *question = visualItem(window->contentItem(), "followUpInput");
        QVERIFY(question);
        question->forceActiveFocus();
        const QString draft = "What does this mean?";
        question->setProperty("text", draft);
        question->setProperty("cursorPosition", draft.size());
        QTest::keyClick(window, Qt::Key_Backspace, Qt::ControlModifier);
        QCOMPARE(question->property("text").toString(), QString());
        QTest::keyClick(window, Qt::Key_Z, Qt::ControlModifier);
        QCOMPARE(question->property("text").toString(), draft);

        question->setProperty("text", "First line\nSecond line");
        question->setProperty("cursorPosition", 17);
        QTest::keyClick(window, Qt::Key_Backspace, Qt::ControlModifier);
        QCOMPARE(question->property("text").toString(), QString("First line\n line"));

        const QString wrappedDraft = QString("How does this expression work? ").repeated(5);
        question->setProperty("text", wrappedDraft);
        question->setProperty("cursorPosition", wrappedDraft.size());
        QTRY_VERIFY(question->property("lineCount").toInt() > 1);
        const auto lastLineY = question->property("cursorRectangle").toRectF().y();
        int lastLineStart = 0;
        for (int position = 0; position <= wrappedDraft.size(); ++position) {
            QRectF characterRectangle;
            QVERIFY(QMetaObject::invokeMethod(question, "positionToRectangle", Q_RETURN_ARG(QRectF, characterRectangle), Q_ARG(int, position)));
            if (characterRectangle.y() == lastLineY) {
                lastLineStart = position;
                break;
            }
        }
        QVERIFY(lastLineStart > 0 && lastLineStart < wrappedDraft.size());
        QTest::keyClick(window, Qt::Key_Backspace, Qt::ControlModifier);
        QCOMPARE(question->property("text").toString(), wrappedDraft.left(lastLineStart));
        QTest::keyClick(window, Qt::Key_Z, Qt::ControlModifier);
        QCOMPARE(question->property("text").toString(), wrappedDraft);

        question->setProperty("text", "Keep this word");
        QVERIFY(QMetaObject::invokeMethod(question, "select", Q_ARG(int, 5), Q_ARG(int, 9)));
        QTest::keyClick(window, Qt::Key_Backspace, Qt::ControlModifier);
        QCOMPARE(question->property("text").toString(), QString("Keep  word"));

        question->setProperty("text", "öyle");
        question->setProperty("cursorPosition", 4);
        QTest::keyClick(window, Qt::Key_Backspace);
        QCOMPARE(question->property("text").toString(), QString("öyl"));
        question->setProperty("cursorPosition", 0);
        QTest::keyClick(window, Qt::Key_Backspace, Qt::ControlModifier);
        QCOMPARE(question->property("text").toString(), QString("öyl"));
        question->setProperty("text", "");
    }

    void savingFlashcardShowsResultAndOpensCollection() {
        StudyResponseServer server;
        prepareCompletedExplanation(server);
        const auto cardsBeforeSave = study->deck()->cards().size();
        auto *sidebar = visualItem(window->contentItem(), "learningInspector");
        QVERIFY(sidebar);
        QTRY_COMPARE(sidebar->property("revealProgress").toDouble(), 1.0);
        click("saveFlashcardButton");
        QCOMPARE(study->deck()->cards().size(), cardsBeforeSave + 1);
        QCOMPARE(study->deck()->cards().last().toMap().value("front").toString(), QString("Ben televizyona dalmışım ya."));
        auto *saveFeedback = visualItem(window->contentItem(), "flashcardSaveFeedback");
        QVERIFY(saveFeedback);
        QTRY_VERIFY(saveFeedback->isVisible());
        QCOMPARE(saveFeedback->property("text").toString(), QString("Flashcard saved on this device."));
        QTest::qWait(100); // Let the source actions wrap after the feedback link appears.
        click("viewFlashcardsButton");
        auto *studyDrawer = window->findChild<QObject *>("studyDrawer");
        auto *studyLoader = window->findChild<QObject *>("studyLoader");
        QVERIFY(studyDrawer && studyLoader);
        QTRY_VERIFY(studyDrawer->property("opened").toBool());
        auto *studyPane = studyLoader->property("item").value<QObject *>();
        QVERIFY(studyPane);
        QCOMPARE(studyPane->property("currentTab").toInt(), 1);
        QVERIFY(QMetaObject::invokeMethod(studyDrawer, "close"));
        QTRY_VERIFY(!studyDrawer->property("visible").toBool());
        session->closeDefinition();
        study->explanation()->invalidateContext();
        QVERIFY(!saveFeedback->isVisible());
    }

    void recentHistoryKeepsFinishedVideosAccessible() {
        session->setPaused(true);
        recent->clear();
        QTemporaryDir media;
        const auto older = media.filePath("older.webm");
        const auto latest = media.filePath("latest.webm");
        for (const auto &path : {older, latest}) {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.close();
            QVERIFY(recent->recordOpened({path, 20, 100}));
        }
        window->setProperty("showRecent", true);
        auto *list = visualItem(window->contentItem(), "recentVideosList");
        QVERIFY(list);
        const auto firstRow = [list]() {
            QQuickItem *entry = nullptr;
            QMetaObject::invokeMethod(list, "forceLayout");
            QMetaObject::invokeMethod(list, "itemAtIndex", Q_RETURN_ARG(QQuickItem *, entry), Q_ARG(int, 0));
            return entry;
        };
        const auto clickRowAction = [this, &firstRow](const QString &name) {
            QTRY_VERIFY(firstRow());
            auto *entry = firstRow();
            QVERIFY(entry);
            auto *button = visualItem(entry, name);
            QVERIFY(button);
            QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                              button->mapToScene(QPointF(button->width() / 2, button->height() / 2)).toPoint());
            QCoreApplication::processEvents();
        };
        QTRY_COMPARE(list->property("count").toInt(), 2);
        QTRY_VERIFY(firstRow());
        const auto historyArtifacts = qEnvironmentVariable("NEDEN_CHECK_ARTIFACTS");
        if (!historyArtifacts.isEmpty())
            QVERIFY(window->grabWindow().save(historyArtifacts + "/continue-watching.png"));
        clickRowAction("markFinishedButton");
        QTRY_COMPARE(list->property("count").toInt(), 1);
        QVERIFY(recent->find(latest)->finished);
        QCOMPARE(recent->count(), 2);
        click("historyTab");
        QTRY_COMPARE(list->property("count").toInt(), 2);
        QTRY_VERIFY(firstRow() && firstRow()->property("finished").toBool());
        clickRowAction("markFinishedButton");
        QVERIFY(!recent->find(latest)->finished);
        click("continueWatchingTab");
        QTRY_COMPARE(list->property("count").toInt(), 2);
        clickRowAction("removeRecentButton");
        QTRY_COMPARE(list->property("count").toInt(), 1);
        QVERIFY(QFile::exists(latest));
        recent->clear();
        window->setProperty("showRecent", false);
    }

    void translatedWordsRevealTheirSourceBreakdown() {
        QQmlComponent component(engine.get());
        component.setData(R"(
            import QtQuick
            import Neden
            ExplanationText { width: 340; height: 420; x: 100; y: 100; z: 100 }
        )", QUrl());
        std::unique_ptr<QObject> fixture(component.create());
        QVERIFY2(fixture, qPrintable(component.errorString()));
        auto *answer = qobject_cast<QQuickItem *>(fixture.get());
        QVERIFY(answer);
        answer->setParentItem(window->contentItem());
        answer->setProperty("markdown", "### Translation\n[Good](neden-breakdown:Bonne,chance) [luck](neden-breakdown:Bonne,chance)!\n\n"
            "### Breakdown\n- **Bonne** — good; feminine adjective.\n- **chance** — luck. Together: good luck.");
        QTest::qWait(80);
        QPoint wordPosition;
        for (int y = 0; y < 120 && wordPosition.isNull(); y += 3) {
            for (int x = 0; x < 220; x += 3) {
                QString link;
                QVERIFY(QMetaObject::invokeMethod(answer, "linkAt", Q_RETURN_ARG(QString, link),
                    Q_ARG(qreal, x), Q_ARG(qreal, y)));
                if (!link.isEmpty()) { wordPosition = QPoint(x + 15, y + 8); break; }
            }
        }
        QVERIFY(!wordPosition.isNull());
        auto *popup = fixture->findChild<QObject *>("translationBreakdownPopup");
        auto *label = fixture->findChild<QObject *>("translationBreakdownText");
        QVERIFY(popup && label);
        QTest::mouseMove(window, answer->mapToScene(wordPosition).toPoint());
        QTRY_VERIFY(popup->property("visible").toBool());
        QVERIFY(label->property("text").toString().contains("Bonne"));
        QVERIFY(label->property("text").toString().contains("chance"));
        QVERIFY(popup->property("x").toDouble() >= 0);
        QVERIFY(popup->property("y").toDouble() >= 0);
        const auto artifacts = qEnvironmentVariable("NEDEN_CHECK_ARTIFACTS");
        if (!artifacts.isEmpty()) {
            QDir().mkpath(artifacts);
            QVERIFY(window->grabWindow().save(artifacts + "/translation-hover.png"));
        }
        QTest::mouseMove(window, QPoint(20, 20));
        QTRY_VERIFY(!popup->property("visible").toBool());
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, answer->mapToScene(wordPosition).toPoint());
        QTRY_VERIFY(popup->property("visible").toBool());
        answer->setProperty("scrolling", true);
        QTRY_VERIFY(!popup->property("visible").toBool());
        answer->setProperty("scrolling", false);
        answer->setProperty("markdown", "### Translation\nOld saved answer\n### Breakdown\n- **Old** — readable.");
        QVERIFY(!answer->property("text").toString().contains("href="));
    }

    void sidebarSlidesWithoutResizingVideoPerFrame() {
        QQmlComponent component(engine.get());
        component.setData(R"(
            import QtQuick
            import Neden
            Item {
                width: 1000; height: 500
                Rectangle { objectName: "video"; width: parent.width - sidebar.reservedWidth }
                SlidingSidebar { id: sidebar; objectName: "sidebar"; width: 320 }
            }
        )", QUrl());
        std::unique_ptr<QObject> fixture(component.create());
        QVERIFY2(fixture, qPrintable(component.errorString()));
        auto *sidebar = fixture->findChild<QQuickItem *>("sidebar");
        auto *video = fixture->findChild<QQuickItem *>("video");
        QVERIFY(sidebar && video);
        QSignalSpy widthChanges(video, &QQuickItem::widthChanged);
        QCOMPARE(video->width(), 1000.0);
        sidebar->setProperty("expanded", true);
        QTRY_COMPARE(video->width(), 680.0);
        QTest::qWait(60);
        const double partial = sidebar->property("revealProgress").toDouble();
        QVERIFY(partial > 0 && partial < 1);
        QTRY_COMPARE(sidebar->x(), 680.0);
        QCOMPARE(widthChanges.count(), 1);
        sidebar->setProperty("expanded", false);
        QTest::qWait(60);
        QCOMPARE(video->width(), 680.0);
        sidebar->setProperty("expanded", true); // Reverse a closing slide immediately.
        QTRY_COMPARE(sidebar->x(), 680.0);
        QCOMPARE(widthChanges.count(), 1);
        sidebar->setProperty("expanded", false);
        QTRY_COMPARE(video->width(), 1000.0);
        QCOMPARE(widthChanges.count(), 2);
        QVERIFY(!sidebar->isVisible());
        sidebar->setProperty("reduceMotion", true);
        sidebar->setProperty("expanded", true);
        QCOMPARE(sidebar->x(), 680.0);
        sidebar->setProperty("expanded", false);
        QCOMPARE(sidebar->x(), 1000.0);
        QCOMPARE(video->width(), 1000.0);
        sidebar->setProperty("reduceMotion", false);
        sidebar->setProperty("expanded", true);
        QTest::qWait(40);
        sidebar->setProperty("reduceMotion", true); // A live preference change settles immediately.
        QCOMPARE(sidebar->x(), 680.0);
        auto *tab = visualItem(window->contentItem(), "explanationTab");
        QVERIFY(tab);
        QCOMPARE(tab->property("text").toString(), QString("Explanation"));
    }

    void pausedVideoHidesControlsAndCursorUntilPointerMoves() {
        window->setProperty("showRecent", false);
        window->setProperty("inspectorMode", 0);
        session->setPaused(true);
        auto *video = visualItem(window->contentItem(), "videoSurface");
        auto *playButton = visualItem(window->contentItem(), "playPauseButton");
        QVERIFY(video && playButton);
        const auto point = video->mapToScene(QPointF(40, 40)).toPoint();
        QTest::mouseMove(window, point);
        QTRY_VERIFY(!window->property("hidePlaybackControls").toBool());
        QTRY_VERIFY_WITH_TIMEOUT(window->property("hidePlaybackControls").toBool(), 4000);
        QTRY_VERIFY(QGuiApplication::overrideCursor());
        QCOMPARE(QGuiApplication::overrideCursor()->shape(), Qt::BlankCursor);
        QTest::mouseMove(window, point + QPoint(10, 10));
        QTRY_VERIFY(!window->property("hidePlaybackControls").toBool());
        QVERIFY(!QGuiApplication::overrideCursor());
        QTest::mouseMove(window, playButton->mapToScene(QPointF(playButton->width() / 2, playButton->height() / 2)).toPoint());
        QTest::qWait(2800);
        QVERIFY(!window->property("hidePlaybackControls").toBool());
        QVERIFY(session->paused());
    }

    void cursorRestoresAcrossOverlaysAndWindowBoundaries() {
        window->setProperty("showRecent", false);
        window->setProperty("inspectorMode", 0);
        session->closeWordCard(false);
        session->setPaused(true);
        auto *video = visualItem(window->contentItem(), "videoSurface");
        auto *pointer = window->findChild<PlaybackPointer *>("playbackPointer");
        QVERIFY(video && pointer);
        window->requestActivate();
        QTRY_VERIFY(window->isActive());

        // An overlapping control deliberately consumes hover. Window activity
        // must still wake the cursor on a single-pixel move inside that control.
        QQmlComponent component(engine.get());
        component.setData(R"(
            import QtQuick
            MouseArea {
                width: 150; height: 150; z: 100
                hoverEnabled: true
                onPositionChanged: mouse => mouse.accepted = true
            }
        )", QUrl());
        std::unique_ptr<QObject> overlay(component.create());
        auto *hoverBlocker = qobject_cast<QQuickItem *>(overlay.get());
        QVERIFY2(hoverBlocker, qPrintable(component.errorString()));
        hoverBlocker->setParentItem(video->parentItem());
        const auto point = hoverBlocker->mapToScene(QPointF(40, 40)).toPoint();
        QTest::mouseMove(window, point);
        QTRY_VERIFY_WITH_TIMEOUT(pointer->hidden(), 4000);
        QTest::mouseMove(window, point + QPoint(1, 0));
        QVERIFY(!pointer->hidden());
        QVERIFY(!window->property("hidePlaybackControls").toBool());
        QVERIFY(!QGuiApplication::overrideCursor());

        // Repeated render/hover updates at one position are not activity.
        window->setProperty("controlsIdle", true);
        QVERIFY(pointer->hidden());
        QHoverEvent stationaryHover(QEvent::HoverMove, point, point, point);
        QCoreApplication::sendEvent(window, &stationaryHover);
        QVERIFY(pointer->hidden());
        QEvent leave(QEvent::Leave);
        QCoreApplication::sendEvent(window, &leave);
        QVERIFY(!pointer->hidden());
        QVERIFY(!QGuiApplication::overrideCursor());
        QEnterEvent enter(point, point, window->mapToGlobal(point));
        QCoreApplication::sendEvent(window, &enter);
        QVERIFY(!window->property("hidePlaybackControls").toBool());

        window->setProperty("controlsIdle", true);
        QVERIFY(pointer->hidden());
        QEvent deactivate(QEvent::WindowDeactivate);
        QCoreApplication::sendEvent(window, &deactivate);
        QVERIFY(!pointer->hidden());
        QVERIFY(!QGuiApplication::overrideCursor());

        overlay.reset();
        session->replaceCues({{0, 7200000, "Merhaba dünya", {}}});
        auto *caption = visualItem(window->contentItem(), "primarySubtitleText");
        QVERIFY(caption);
        const auto captionPoint = wordPoint(caption);
        QTest::mouseMove(window, captionPoint);
        window->setProperty("controlsIdle", true);
        QVERIFY(pointer->hidden());
        QTest::mouseMove(window, captionPoint + QPoint(1, 0));
        QVERIFY(!pointer->hidden());
        QVERIFY(!QGuiApplication::overrideCursor());

        // Detaching the controller must leave no application-wide blank cursor.
        window->setProperty("controlsIdle", true);
        QVERIFY(pointer->hidden());
        pointer->setWindow(nullptr);
        QVERIFY(!QGuiApplication::overrideCursor());
        pointer->setWindow(window);
        QCoreApplication::sendEvent(window, &enter);

        // Entering the sidebar after idle restores the ordinary control cursor.
        window->setProperty("inspectorMode", 1);
        auto *sidebar = visualItem(window->contentItem(), "learningInspector");
        QVERIFY(sidebar);
        QTRY_COMPARE(sidebar->property("revealProgress").toDouble(), 1.0);
        QTest::mouseMove(window, point);
        window->setProperty("controlsIdle", true);
        QVERIFY(pointer->hidden());
        QTest::mouseMove(window, sidebar->mapToScene(QPointF(40, 90)).toPoint());
        QVERIFY(!pointer->hidden());
        window->setProperty("controlsIdle", true);
        QVERIFY(!pointer->hidden());
        window->setProperty("controlsIdle", false);
    }

    void fullscreenPreservesNativeMouseTracking() {
#ifndef Q_OS_MACOS
        QSKIP("This regression checks AppKit tracking during the macOS window-style transition.");
#else
        session->openVideo(QUrl::fromLocalFile(qEnvironmentVariable("NEDEN_CHECK_VIDEO")));
        QTRY_VERIFY_WITH_TIMEOUT(session->duration() > 0, 15000);
        window->setProperty("showRecent", false);
        window->setProperty("inspectorMode", 0);
        session->setPaused(true);
        session->closeWordCard(false);
        session->replaceCues({{0, 7200000, "Merhaba dünya", {}}});
        auto *video = visualItem(window->contentItem(), "videoSurface");
        auto *pointer = window->findChild<PlaybackPointer *>("playbackPointer");
        auto *caption = visualItem(window->contentItem(), "primarySubtitleText");
        auto *explain = visualItem(window->contentItem(), "primarySubtitleTextExplainButton");
        QVERIFY(video && pointer && caption && explain);
        window->requestActivate();
        QTRY_VERIFY(window->isActive());
        window->contentItem()->forceActiveFocus();

        class MouseMovementObserver : public QObject {
        public:
            int movements = 0;
            QPointF lastPosition;
        protected:
            bool eventFilter(QObject *, QEvent *event) override {
                if (event->type() == QEvent::MouseMove) {
                    ++movements;
                    lastPosition = static_cast<QMouseEvent *>(event)->position();
                }
                return false;
            }
        } observer;
        window->installEventFilter(&observer);
        const auto restoreWindow = qScopeGuard([&] {
            window->removeEventFilter(&observer);
            fullscreen->setActive(false);
        });
        const auto moveNativePointer = [&](const QPoint &globalPosition) {
            // On Cocoa, QCursor::setPos posts a real CGEvent through AppKit.
            // QTest::mouseMove bypasses that path and would mask this regression.
            const int previousMovements = observer.movements;
            QCursor::setPos(globalPosition);
            QTRY_VERIFY_WITH_TIMEOUT(observer.movements > previousMovements, 1000);
            QTRY_COMPARE(observer.lastPosition.toPoint(), window->mapFromGlobal(globalPosition));
            QVERIFY(!pointer->hidden());
            QVERIFY(!window->property("hidePlaybackControls").toBool());
        };

        for (const bool nativeMode : {false, false, true}) {
            moveNativePointer(window->mapToGlobal(video->mapToScene(
                QPointF(video->width() / 2, video->height() / 3)).toPoint()));
            QTRY_VERIFY_WITH_TIMEOUT(pointer->hidden(), 4000);
            const QPoint originalGlobalPosition = QCursor::pos();
            if (nativeMode)
                fullscreen->toggleNative();
            else
                QTest::keyClick(window, Qt::Key_F);
            QTRY_VERIFY_WITH_TIMEOUT(fullscreen->active(), 10000);
            QTRY_VERIFY_WITH_TIMEOUT(pointer->hidden(), 4000);

            // A single interior pixel must deliver a real Qt mouse event before
            // visiting either system edge, even when entry began with a hidden cursor.
            moveNativePointer(originalGlobalPosition + QPoint(1, 0));
            for (int offset = 2; offset <= 31; ++offset) {
                moveNativePointer(originalGlobalPosition + QPoint(offset, 0));
                QTest::qWait(100);
                QVERIFY(!pointer->hidden());
            }
            QTRY_VERIFY_WITH_TIMEOUT(pointer->hidden(), 4000);
            moveNativePointer(originalGlobalPosition + QPoint(32, 0));

            // Hover must reach QML too, not merely restore the cursor/controller.
            moveNativePointer(window->mapToGlobal(wordPoint(caption)));
            QTRY_VERIFY(explain->isVisible());
            fullscreen->setActive(false);
            QTRY_VERIFY_WITH_TIMEOUT(!fullscreen->active(), 10000);
            moveNativePointer(originalGlobalPosition + QPoint(1, 0));
        }
#endif
    }

    void explainButtonFootprintAndStationaryCueChanges() {
        window->setProperty("showRecent", false);
        session->setPaused(true);
        session->clearTrack(true);
        session->replaceCues({{0, 7200000, "Merhaba dünya", {}}});
        auto *caption = visualItem(window->contentItem(), "primarySubtitleText");
        auto *button = visualItem(window->contentItem(), "primarySubtitleTextExplainButton");
        auto *video = visualItem(window->contentItem(), "videoSurface");
        QVERIFY(caption && button && video);
        QTest::qWait(100);
        QTest::mouseMove(window, video->mapToScene(QPointF(20, 20)).toPoint());
        QTRY_VERIFY(!button->isVisible());
        // Enter the hidden action directly, without passing over any caption text.
        QTest::mouseMove(window, button->mapToScene(QPointF(button->width() / 2, button->height() / 2)).toPoint());
        QTRY_VERIFY(button->isVisible());
        QTest::qWait(250);
        QVERIFY(button->isVisible());
        auto *background = button->property("background").value<QQuickItem *>();
        QVERIFY(background);
        QVERIFY(background->property("color").value<QColor>().alpha() > 200);
        QTest::mouseMove(window, video->mapToScene(QPointF(20, 20)).toPoint());
        QTRY_VERIFY(!button->isVisible());
        session->replaceCues({{0, 7200000, "MMMMMMMMMMMMMMMM", {}}});
        QTest::qWait(100);
        QTest::mouseMove(window, wordPoint(caption, 8));
        QTRY_VERIFY(button->isVisible());
        // No synthetic mouse move after the cue changes: geometry must refresh itself.
        session->replaceCues({{0, 7200000, "WWWWWWWWWWWWWWWW", {}}});
        QTest::qWait(300);
        QVERIFY(button->isVisible());
        session->replaceCues({});
        QTRY_VERIFY(!button->isVisible());
        session->replaceCues({{0, 7200000, "MMMMMMMMMMMMMMMM", {}}});
        QTRY_VERIFY(button->isVisible());
    }

    void explainHoverRejectsBlankMarginsEvenWithSelectionOrFocus() {
        window->setProperty("showRecent", false);
        session->setPaused(true);
        session->replaceCues({{0, 7200000, "Bir ışık gördüm.", {}}});
        session->clearTrack(true);
        auto *caption = visualItem(window->contentItem(), "primarySubtitleText");
        auto *button = visualItem(window->contentItem(), "primarySubtitleTextExplainButton");
        QVERIFY(caption && button);
        auto *editor = caption->property("editor").value<QQuickItem *>();
        QVERIFY(editor);
        QTest::qWait(80);
        QVERIFY(QMetaObject::invokeMethod(editor, "selectAll"));
        QTest::mouseMove(window, wordPoint(caption));
        QTRY_VERIFY(button->isVisible());
        button->forceActiveFocus();
        for (double horizontal : {8.0, caption->width() - 8}) {
            QTest::mouseMove(window,
                             caption->mapToScene(QPointF(horizontal, caption->height() / 2)).toPoint());
            QTRY_VERIFY_WITH_TIMEOUT(!button->isVisible(), 1000);
            QTest::mouseMove(window, wordPoint(caption));
            QTRY_VERIFY(button->isVisible());
        }
    }

    void explainHoverSurvivesActiveSelections() {
        window->setProperty("showRecent", false);
        window->setProperty("inspectorMode", 1);
        session->setPaused(true);
        QVERIFY(study->settings()->configure("http://127.0.0.1:1", "mock", "https://example.org/%s", false));
        QFile secondaryFile(profile.filePath("selection-hover-secondary.srt"));
        QVERIFY(secondaryFile.open(QIODevice::WriteOnly));
        secondaryFile.write("1\n00:00:00,000 --> 02:00:00,000\nI saw a light.\n");
        secondaryFile.close();
        session->openTranslation(QUrl::fromLocalFile(secondaryFile.fileName()));
        session->replaceCues({{0, 7200000, "Bir ışık gördüm.", {}}});
        auto *primary = visualItem(window->contentItem(), "primarySubtitleText");
        auto *secondary = visualItem(window->contentItem(), "secondarySubtitleText");
        auto *video = visualItem(window->contentItem(), "videoSurface");
        auto *button = visualItem(window->contentItem(), "primarySubtitleTextExplainButton");
        QVERIFY(primary && secondary && video && button);
        auto *sidebar = visualItem(window->contentItem(), "learningInspector");
        QVERIFY(sidebar);
        QTRY_COMPARE(sidebar->property("revealProgress").toDouble(), 1.0);
        auto *primaryEditor = primary->property("editor").value<QQuickItem *>();
        auto *secondaryEditor = secondary->property("editor").value<QQuickItem *>();
        QVERIFY(primaryEditor && secondaryEditor);

        // Real mouse grabs are different from merely retaining a completed selection.
        const auto start = wordPoint(primary);
        QTest::mouseMove(window, start);
        QTRY_VERIFY(button->isVisible());
        QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, start);
        QTRY_VERIFY(primary->property("selecting").toBool());
        const bool visibleWhilePressed = button->isVisible();
        QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, start);
        QVERIFY2(visibleWhilePressed, "Explain disappeared while the subtitle editor held the mouse grab");
        session->closeWordCard(false);

        for (auto *track : {primary, secondary}) {
            auto *editor = track->property("editor").value<QQuickItem *>();
            QVERIFY(editor);
            const auto from = wordPoint(track, 1);
            const auto to = wordPoint(track, 6);
            QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, from);
            QTest::mouseMove(window, to);
            QTest::qWait(230);
            const bool visibleDuringDrag = button->isVisible();
            QTest::mouseMove(window, video->mapToScene(QPointF(20, 20)).toPoint());
            QTest::mouseMove(window, to);
            QTest::qWait(230);
            const bool visibleAfterReentry = button->isVisible();
            QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, to);
            QVERIFY(visibleDuringDrag);
            QVERIFY(visibleAfterReentry);
        }

        for (const int mode : {0, 1, 2}) {
            window->setProperty("inspectorMode", mode);
            QTRY_COMPARE(sidebar->property("revealProgress").toDouble(), mode == 0 ? 0.0 : 1.0);
            for (const int selectedTracks : {1, 2, 3}) {
                session->closeDefinition();
                if (selectedTracks & 1)
                    QVERIFY(QMetaObject::invokeMethod(primaryEditor, "selectAll"));
                if (selectedTracks & 2)
                    QVERIFY(QMetaObject::invokeMethod(secondaryEditor, "selectAll"));
                secondaryEditor->forceActiveFocus();
                const auto selectedPrimary = primaryEditor->property("selectedText").toString();
                const auto selectedSecondary = secondaryEditor->property("selectedText").toString();
                const auto subject = study->explanation()->subject();
                for (auto *track : {primary, secondary}) {
                    QTest::mouseMove(window, video->mapToScene(QPointF(20, 20)).toPoint());
                    QTRY_VERIFY(!button->isVisible());
                    QTest::mouseMove(window, wordPoint(track));
                    QTRY_VERIFY(button->isVisible());
                    QTest::qWait(220); // Stay beyond the hide timer; retention cannot mask a lost hover.
                    QVERIFY(button->isVisible());
                    QCOMPARE(primaryEditor->property("selectedText").toString(), selectedPrimary);
                    QCOMPARE(secondaryEditor->property("selectedText").toString(), selectedSecondary);
                    QCOMPARE(study->explanation()->subject(), subject);
                    QVERIFY(secondaryEditor->hasActiveFocus());
                }
            }
        }
        session->closeDefinition();
        // Disabling text selection must not remove the independent full-line command.
        const auto savedAppearance = appearance->snapshot();
        auto unselectable = savedAppearance;
        unselectable.insert("selectable", false);
        QVERIFY(appearance->preview(unselectable));
        QTest::mouseMove(window, wordPoint(primary));
        QTRY_VERIFY(button->isVisible());
        QVERIFY(appearance->preview(savedAppearance));
        window->setProperty("inspectorMode", 1);
    }

    void explainHoverCoversBothTracksAndThePathToTheButton() {
        window->setProperty("showRecent", false);
        window->setProperty("inspectorMode", 1);
        session->setPaused(true);

        QFile secondaryFile(profile.filePath("hover-secondary.srt"));
        QVERIFY(secondaryFile.open(QIODevice::WriteOnly));
        secondaryFile.write("1\n00:00:00,000 --> 02:00:00,000\nI saw a light.\n");
        secondaryFile.close();
        session->openTranslation(QUrl::fromLocalFile(secondaryFile.fileName()));
        session->setPaused(true);
        auto *primary = visualItem(window->contentItem(), "primarySubtitleText");
        auto *secondary = visualItem(window->contentItem(), "secondarySubtitleText");
        auto *video = visualItem(window->contentItem(), "videoSurface");
        auto *button = visualItem(window->contentItem(), "primarySubtitleTextExplainButton");
        QVERIFY(primary && secondary && video && button);
        const auto savedSize = window->size();
        const auto savedAppearance = appearance->snapshot();

        for (const bool reversed : {false, true}) {
            auto preview = savedAppearance;
            preview.insert("reversed", reversed);
            QVERIFY(appearance->preview(preview));
            for (const QSize size : {QSize(900, 540), QSize(1280, 756)}) {
                window->resize(size);
                for (const auto &phrase : QStringList{
                         "Bir ışık gördüm.", "Bir ışık\ngördüm.", "هذا ضوء جميل.", "日本語の字幕です。",
                         "Kafam olmuş bir milyon, dışarıda kar yağıyor, içime yağmur yağıyor."}) {
                    session->replaceCues({{0, 7200000, phrase, {}}});
                    QTest::qWait(80);
                    QVERIFY(secondary->isVisible());
                    for (auto *track : {secondary, primary}) {
                        QTest::mouseMove(window, video->mapToScene(QPointF(20, 20)).toPoint());
                        QTRY_VERIFY_WITH_TIMEOUT(!button->isVisible(), 1000);
                        const QPoint start = wordPoint(track);
                        QTest::mouseMove(window, start);
                        QTRY_VERIFY_WITH_TIMEOUT(button->isVisible(), 1000);
                        const QPoint target =
                            button->mapToScene(QPointF(button->width() / 2, button->height() / 2)).toPoint();
                        for (int step = 1; step <= 8; ++step) {
                            const QPoint next = start + (target - start) * step / 8;
                            QTest::mouseMove(window, next);
                            QTest::qWait(30);
                            QVERIFY(button->isVisible());
                        }
                        QVERIFY(button->property("hovered").toBool());
                    }
                    // A new cue must retain the action beneath a stationary pointer.
                    QTest::mouseMove(window, wordPoint(primary));
                    session->replaceCues({{0, 7200000, phrase + " Şimdi.", {}}});
                    QTest::qWait(80);
                    QTest::mouseMove(window, wordPoint(primary));
                    QVERIFY(button->isVisible());
                    QVERIFY(session->paused());
                    QVERIFY(study->explanation()->subject().isEmpty());
                }
            }
        }
        session->replaceCues({});
        // A primary-track gap must not hide the action on an active secondary cue.
        QTest::mouseMove(window, wordPoint(secondary));
        QTRY_VERIFY(button->isVisible());
        session->clearTrack(true);
        QTRY_VERIFY(!button->isVisible());
        QVERIFY(appearance->preview(savedAppearance));
        window->resize(savedSize);
    }

    void explanationPhraseUsesThePluginCardStyle() {
        const QString phrase = "Ya abi, bilmem mi? Ben yani ben, motorların üstünde geçti benim hayatım.";
        window->setProperty("showRecent", false);
        window->setProperty("inspectorMode", 1);
        window->setProperty("inspectorMode", 2);
        study->explanation()->explain(phrase, phrase);
        auto *subject = visualItem(window->contentItem(), "explanationSubject");
        auto *card = visualItem(window->contentItem(), "explanationSourceCard");
        auto *disclosure = visualItem(window->contentItem(), "fullSubtitleButton");
        QVERIFY(subject && card && disclosure);
        QTest::qWait(100);
        QCOMPARE(subject->property("text").toString(), phrase);
        QCOMPARE(subject->property("color").value<QColor>(), QColor("#e6eaf2"));
        QCOMPARE(subject->property("font").value<QFont>().pixelSize(), 15);
        QCOMPARE(subject->property("font").value<QFont>().weight(), QFont::DemiBold);
        QVERIFY(card->isVisible());
        QVERIFY(!disclosure->isVisible());
        QVERIFY(card->height() < 200);
        session->select("Ben");
        QCOMPARE(subject->property("text").toString(), phrase);
        const auto artifacts = qEnvironmentVariable("NEDEN_CHECK_ARTIFACTS");
        if (!artifacts.isEmpty())
            QVERIFY(window->grabWindow().save(artifacts + "/source-phrase-card.png"));
        study->explanation()->explain("bilmem mi?", phrase);
        QTRY_VERIFY(disclosure->isVisible());
        QCOMPARE(subject->property("text").toString(), QString("bilmem mi?"));
        session->closeDefinition();
    }

    void hoveringAndExplainingTheFullLineUseOneExplicitRequest() {

        // 1. Serve only a synthetic local explanation and retain the submitted phrase for assertions.
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        int requests = 0;
        QString submitted;
        QJsonArray submittedMessages;
        connect(&server, &QTcpServer::newConnection, this, [&] {
            auto *socket = server.nextPendingConnection();
            auto bytes = std::make_shared<QByteArray>();
            connect(socket, &QTcpSocket::readyRead, socket, [&, socket, bytes] {
                *bytes += socket->readAll();
                const int boundary = bytes->indexOf("\r\n\r\n");
                if (boundary < 0 || socket->property("sent").toBool())
                    return;

                int length = 0;
                for (const auto &line : bytes->left(boundary).split('\n')) {
                    if (line.toLower().startsWith("content-length:"))
                        length = line.mid(line.indexOf(':') + 1).trimmed().toInt();
                }
                if (bytes->size() < boundary + 4 + length)
                    return;

                socket->setProperty("sent", true);
                ++requests;
                const auto body = QJsonDocument::fromJson(bytes->mid(boundary + 4, length)).object();
                submittedMessages = body.value("messages").toArray();
                const auto context =
                    body.value("messages").toArray().at(1).toObject().value("content").toString();
                submitted = QJsonDocument::fromJson(context.toUtf8()).object().value("selection").toString();
                const QByteArray answer = "data: {\"choices\":[{\"delta\":{\"content\":\"### Natural "
                                          "meaning\\nI saw a light.\"}}]}\n\ndata: [DONE]\n\n";
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nConnection: "
                              "close\r\nContent-Length: " +
                              QByteArray::number(answer.size()) + "\r\n\r\n" + answer);
                socket->disconnectFromHost();
            });
        });
        QVERIFY(study->settings()->configure(QString("http://127.0.0.1:%1").arg(server.serverPort()), "mock",
                                             "https://example.org/%s", false));
        session->setPaused(true);
        const auto start = qRound64(session->position() * 1000) - 500;
        const QString phrase = "Bir ışık\ngördüm.";
        session->replaceCues({{start, start + 10000, phrase, {}}});
        auto *subtitle = visualItem(window->contentItem(), "primarySubtitleText");
        auto *video = visualItem(window->contentItem(), "videoSurface");
        QVERIFY(subtitle && video);
        QTest::qWait(150);

        // 2. Hover reveals the button, but sends nothing and does not steal selection gestures.
        QTest::mouseMove(window, video->mapToScene(QPointF(20, 20)).toPoint());
        auto *button = visualItem(window->contentItem(), "primarySubtitleTextExplainButton");
        QVERIFY(button);
        QTRY_VERIFY(!button->isVisible());
        QTest::mouseMove(
            window, subtitle->mapToScene(QPointF(subtitle->width() / 2, subtitle->height() / 2)).toPoint());
        QTRY_VERIFY(button->isVisible());
        QCOMPARE(requests, 0);
        const auto artifacts = qEnvironmentVariable("NEDEN_CHECK_ARTIFACTS");
        if (!artifacts.isEmpty())
            QVERIFY(window->grabWindow().save(artifacts + "/explain-line-hover.png"));

        // 3. Clicking sends the complete multiline cue exactly once and keeps playback paused.
        click("primarySubtitleTextExplainButton");
        QTRY_COMPARE(study->explanation()->state(), QString("complete"));
        QCOMPARE(requests, 1);
        QCOMPARE(submitted, phrase);
        QCOMPARE(study->explanation()->subject(), phrase);
        QVERIFY(session->paused());
        QVERIFY(!fullscreen->active());
        QTest::mouseMove(window, video->mapToScene(QPointF(20, 20)).toPoint());
        QTRY_VERIFY(!button->isVisible());
        // 4. Follow-up editing keeps Space and M as text, while preserving the frozen phrase and first
        // answer.
        auto *input = visualItem(window->contentItem(), "followUpInput");
        QVERIFY(input);
        input->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_W);
        QTest::keyClick(window, Qt::Key_H);
        QTest::keyClick(window, Qt::Key_Y);
        QTest::keyClick(window, Qt::Key_Space);
        QTest::keyClick(window, Qt::Key_M);
        QCOMPARE(input->property("text").toString(), QString("why m"));
        QVERIFY(session->paused());
        click("sendFollowUpButton");
        QTRY_COMPARE(study->explanation()->state(), QString("complete"));
        QCOMPARE(requests, 2);
        QCOMPARE(study->explanation()->turns().size(), 2);
        QCOMPARE(submittedMessages.size(), 4);
        QCOMPARE(submittedMessages.last().toObject().value("content").toString(), QString("why m"));
        QCOMPARE(study->explanation()->subject(), phrase);
        QTest::qWait(150);
        QList<QQuickItem *> answers;
        std::function<void(QQuickItem *)> collectAnswers = [&](QQuickItem *item) {
            if (item->objectName() == "explanationAnswerText")
                answers.append(item);
            for (auto *child : item->childItems())
                collectAnswers(child);
        };
        collectAnswers(window->contentItem());
        QCOMPARE(answers.size(), 2);
        auto *chatView = visualItem(window->contentItem(), "conversationList");
        QVERIFY(chatView->height() > 300);
        for (auto *answer : answers) {
            QVERIFY(answer->height() >= answer->property("contentHeight").toReal() - 1);
            QVERIFY(answer->property("text").toString().contains("I saw a light."));
        }
        if (!artifacts.isEmpty())
            QVERIFY(window->grabWindow().save(artifacts + "/follow-up-chat.png"));
        // Selecting an answer or editing a question cannot suppress the independent caption action.
        QVERIFY(QMetaObject::invokeMethod(answers.last(), "selectAll"));
        answers.last()->forceActiveFocus();
        QTest::mouseMove(
            window, subtitle->mapToScene(QPointF(subtitle->width() / 2, subtitle->height() / 2)).toPoint());
        QTRY_VERIFY(button->isVisible());
        QTest::qWait(230);
        QVERIFY(button->isVisible());
        input->forceActiveFocus();
        QVERIFY(button->isVisible());
        QCOMPARE(requests, 2);

        QFile secondaryFile(profile.filePath("secondary-only-explain.srt"));
        QVERIFY(secondaryFile.open(QIODevice::WriteOnly));
        secondaryFile.write("1\n00:00:00,000 --> 02:00:00,000\nI saw a light.\n");
        secondaryFile.close();
        session->openTranslation(QUrl::fromLocalFile(secondaryFile.fileName()));
        session->replaceCues({});
        auto *secondary = visualItem(window->contentItem(), "secondarySubtitleText");
        QVERIFY(secondary && secondary->isVisible());
        QTest::qWait(80);
        QTest::mouseMove(
            window,
            secondary->mapToScene(QPointF(secondary->width() / 2, secondary->height() / 2)).toPoint());
        QTRY_VERIFY(button->isVisible());
        click("primarySubtitleTextExplainButton");
        QTRY_COMPARE(study->explanation()->state(), QString("complete"));
        QCOMPARE(requests, 3);
        QCOMPARE(submitted, QString("I saw a light."));
        const auto secondaryContext =
            QJsonDocument::fromJson(submittedMessages[1].toObject().value("content").toString().toUtf8())
                .object();
        QCOMPARE(secondaryContext.value("selectedTrack").toString(), QString("secondary"));
        click("transcriptTab");
        QCOMPARE(study->explanation()->turns().size(), 1);
    }

    void streamingRepliesStayAnchoredToTheLatestQuestion() {
        window->setProperty("showRecent", false);
        session->setPaused(true);
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        QPointer<QTcpSocket> activeSocket;
        int requests = 0;
        connect(&server, &QTcpServer::newConnection, this, [&] {
            auto *socket = server.nextPendingConnection();
            auto bytes = std::make_shared<QByteArray>();
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QTcpSocket::readyRead, socket, [&, socket, bytes] {
                *bytes += socket->readAll();
                const int boundary = bytes->indexOf("\r\n\r\n");
                if (boundary < 0 || socket->property("sent").toBool())
                    return;
                int length = 0;
                for (const auto &line : bytes->left(boundary).split('\n'))
                    if (line.toLower().startsWith("content-length:"))
                        length = line.mid(line.indexOf(':') + 1).trimmed().toInt();
                if (bytes->size() < boundary + 4 + length)
                    return;
                socket->setProperty("sent", true);
                activeSocket = socket;
                ++requests;
                socket->write(
                    "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nConnection: close\r\n\r\n");
                socket->flush();
            });
        });
        const QString hiddenModel = "model-name-must-not-be-displayed";
        QVERIFY(study->settings()->configure(QString("http://127.0.0.1:%1").arg(server.serverPort()),
                                             hiddenModel, "https://example.org/%s", false));
        session->replaceCues({{0, 7200000, "Merak etmeyin, alacaksınız.", {}}});
        session->explainCue();
        QTRY_COMPARE(requests, 1);
        auto *chat = visualItem(window->contentItem(), "conversationList");
        QVERIFY(chat);
        QTRY_COMPARE(chat->property("count").toInt(), 1);
        QTest::qWait(80);
        const auto initialY = chat->property("contentY").toReal();
        QVERIFY(std::abs(initialY) < 1);
        const QString paragraph = "This is a synthetic explanation used to check the reading position.\n\n";
        const auto append = [&](const QString &text) {
            QVERIFY(activeSocket);
            const auto data =
                QJsonDocument(QJsonObject{{"choices", QJsonArray{QJsonObject{
                                                          {"delta", QJsonObject{{"content", text}}}}}}})
                    .toJson(QJsonDocument::Compact);
            activeSocket->write("data: " + data + "\n\n");
            activeSocket->flush();
        };
        const auto finish = [&] {
            QVERIFY(activeSocket);
            activeSocket->write("data: [DONE]\n\n");
            activeSocket->disconnectFromHost();
            QTRY_COMPARE(study->explanation()->state(), QString("complete"));
        };
        append(paragraph.repeated(40));
        QTRY_VERIFY(chat->property("contentHeight").toReal() > chat->height() * 2);
        QTest::qWait(80);
        QVERIFY(std::abs(chat->property("contentY").toReal() - initialY) < 1);
        for (int chunk = 0; chunk < 3; ++chunk) {
            const auto size = study->explanation()->answer().size();
            append(paragraph.repeated(8));
            QTRY_VERIFY(study->explanation()->answer().size() > size);
            QTest::qWait(40);
            QVERIFY(std::abs(chat->property("contentY").toReal() - initialY) < 1);
        }
        QVERIFY(chat->setProperty("contentY", 120.0));
        const auto size = study->explanation()->answer().size();
        append(paragraph);
        QTRY_VERIFY(study->explanation()->answer().size() > size);
        QTest::qWait(80);
        QVERIFY(std::abs(chat->property("contentY").toReal() - 120) < 1);
        finish();

        QVERIFY(std::abs(chat->property("contentY").toReal() - 120) < 1);

        // A new explanation of the same phrase must reset the viewport even when count stays one.
        session->explainCue();
        QTRY_COMPARE(requests, 2);
        QTRY_VERIFY(std::abs(chat->property("contentY").toReal()) < 1);
        append(paragraph.repeated(40));
        QTRY_VERIFY(study->explanation()->answer().size() > 1000);
        finish();
        const QString question = "Why is this form used here?";
        QVERIFY(session->followUp(question));
        QTRY_COMPARE(requests, 3);
        QTRY_COMPARE(chat->property("count").toInt(), 2);
        QTest::qWait(100);
        QQuickItem *questionItem = nullptr;
        bool displayedModel = false;
        std::function<void(QQuickItem *)> inspect = [&](QQuickItem *item) {
            if (item->objectName() == "conversationQuestion" && item->property("text").toString() == question)
                questionItem = item;
            if (item->isVisible() && item->property("text").toString() == hiddenModel)
                displayedModel = true;
            for (auto *child : item->childItems())
                inspect(child);
        };
        inspect(chat);
        QVERIFY(questionItem && questionItem->isVisible());
        QVERIFY(!displayedModel);
        const auto top = chat->mapToScene(QPointF()).y();
        QVERIFY(std::abs(questionItem->mapToScene(QPointF()).y() - top) < 2);
        const auto questionY = chat->property("contentY").toReal();
        append(paragraph.repeated(40));
        QTRY_VERIFY(study->explanation()->answer().size() > 1000);
        QTest::qWait(100);
        QVERIFY(std::abs(chat->property("contentY").toReal() - questionY) < 1);
        QVERIFY(std::abs(questionItem->mapToScene(QPointF()).y() - top) < 2);
        finish();
        QVERIFY(chat->setProperty("contentY", questionY + 100));
        // Retrying an interrupted answer also anchors its question rather than following its tail.
        QVERIFY(session->followUp("Could you give an example?"));
        QTRY_COMPARE(requests, 4);
        append(paragraph.repeated(40));
        QTRY_VERIFY(study->explanation()->answer().size() > 1000);
        QTest::qWait(80);
        const auto retryTop = chat->property("contentY").toReal();
        QVERIFY(chat->setProperty("contentY", retryTop + 100));
        study->explanation()->stop();
        study->explanation()->retryExplanation();
        QTRY_COMPARE(requests, 5);
        QTRY_VERIFY(std::abs(chat->property("contentY").toReal() - retryTop) < 1);
        append(paragraph);
        QTRY_VERIFY(!study->explanation()->answer().isEmpty());
        finish();
    }

    void closeReopenResumeAndRemoveThroughTheUi() {

        // Restore the episode's real tracks after earlier cases loaded synthetic fixtures.
        session->openSubtitles(QUrl::fromLocalFile(qEnvironmentVariable("NEDEN_CHECK_PRIMARY")));
        session->openTranslation(QUrl::fromLocalFile(qEnvironmentVariable("NEDEN_CHECK_SECONDARY")));
        session->seek(stoppedAt);
        session->setPaused(true);
        QTRY_VERIFY(std::abs(session->position() - stoppedAt) < 0.15);
        session->checkpoint();

        // 1. Destroy the whole window and service graph, then reconstruct from saved disk state.
        destroyWindow();
        createWindow();
        QCOMPARE(recent->count(), 1);
        QVERIFY(!session->hasMedia());
        const auto artifacts = qEnvironmentVariable("NEDEN_CHECK_ARTIFACTS");
        if (!artifacts.isEmpty()) {
            QTest::qWait(200);
            QVERIFY(window->grabWindow().save(artifacts + "/recent-videos.png"));
        }
        click("resumeVideoButton");
        QTRY_VERIFY_WITH_TIMEOUT(session->duration() > 0, 15000);
        QTRY_VERIFY_WITH_TIMEOUT(std::abs(session->position() - stoppedAt) < 1, 10000);
        session->setPaused(true);
        QTRY_VERIFY(!session->subtitle().isEmpty());
        QTRY_VERIFY(!session->translation().isEmpty());
        QCOMPARE(session->sourceSubtitleName(),
                 QFileInfo(qEnvironmentVariable("NEDEN_CHECK_PRIMARY")).fileName());
        QCOMPARE(session->translationSubtitleName(),
                 QFileInfo(qEnvironmentVariable("NEDEN_CHECK_SECONDARY")).fileName());
        QVERIFY(visualItem(window->contentItem(), "primarySubtitleText")->isVisible());
        QVERIFY(visualItem(window->contentItem(), "secondarySubtitleText")->isVisible());

        // 2. Remove the playing video, close, and reopen without silently recreating its entry.
        QTest::keyClick(window, Qt::Key_L, Qt::ControlModifier);
        click("removeRecentButton");
        QCOMPARE(recent->count(), 0);
        session->checkpoint();
        destroyWindow();
        createWindow();
        QCOMPARE(recent->count(), 0);
        QVERIFY(QFile::exists(qEnvironmentVariable("NEDEN_CHECK_VIDEO")));
    }

    void cleanScreenshotUsesTheBundledRuntimeAndQtEncoder() {
        session->openVideo(QUrl::fromLocalFile(qEnvironmentVariable("NEDEN_CHECK_VIDEO")));
        QTRY_VERIFY_WITH_TIMEOUT(session->duration() > 0, 15000);
        session->setPaused(true);
        session->seek(5);
        QTRY_VERIFY(std::abs(session->position() - 5) < 0.15);
        const auto path = profile.filePath("clean-frame.png");
        QSignalSpy saved(playback.get(), &PlaybackEngine::screenshotSaved);
        session->saveScreenshot(QUrl::fromLocalFile(path));
        QTRY_COMPARE_WITH_TIMEOUT(saved.count(), 1, 10000);
        const QImage image(path);
        QVERIFY(!image.isNull());
        QVERIFY(image.width() > 100 && image.height() > 100);
    }

    void pausedCanvasResumesAndFullscreenControlsHideAfterKeyboardFocus() {
        session->openVideo(QUrl::fromLocalFile(qEnvironmentVariable("NEDEN_CHECK_VIDEO")));
        QTRY_VERIFY_WITH_TIMEOUT(session->duration() > 0, 15000);
        session->setPaused(true);
        window->setProperty("showRecent", false);
        window->setProperty("inspectorMode", 1);
        auto *video = visualItem(window->contentItem(), "videoSurface");
        QVERIFY(video);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                          video->mapToScene(QPointF(video->width() / 2, video->height() / 3)).toPoint());
        QTRY_COMPARE(window->property("inspectorMode").toInt(), 0);
        QTRY_VERIFY(!session->paused());
        auto *speed = visualItem(window->contentItem(), "speedChoice");
        QVERIFY(speed);
        speed->forceActiveFocus();
        fullscreen->setActive(true);
        QTRY_VERIFY(fullscreen->active());
        QTest::mouseMove(window,
                         video->mapToScene(QPointF(video->width() / 3, video->height() / 3)).toPoint());
        QTRY_VERIFY_WITH_TIMEOUT(window->property("hidePlaybackControls").toBool(), 5000);
        QTest::mouseMove(window,
                         video->mapToScene(QPointF(video->width() / 2, video->height() / 3)).toPoint());
        QTRY_VERIFY(!window->property("hidePlaybackControls").toBool());
        fullscreen->setActive(false);
        session->setPaused(true);
    }

    void embeddedTextReachesTheInteractiveTranscript() {
        const auto fixture = QFINDTESTDATA("fixtures/subtitles/text.mkv");
        QVERIFY(!fixture.isEmpty());
        session->openVideo(QUrl::fromLocalFile(fixture));
        window->setProperty("showRecent", false);
        QTRY_VERIFY_WITH_TIMEOUT(session->duration() > 4, 10000);
        session->setPaused(true);
        QString embeddedId;
        for (const auto &entry : session->mediaTracks()) {
            const auto track = entry.toMap();
            if (track.value("type") == "sub") {
                embeddedId = track.value("id").toString();
                QCOMPARE(track.value("ff-index").toInt(), 1);
            }
        }
        QVERIFY(!embeddedId.isEmpty());
        session->selectNativeSubtitle(embeddedId, false);
        QTRY_COMPARE_WITH_TIMEOUT(session->cueModel()->rowCount(), 2, 10000);
        QVERIFY(session->primaryNativeTrack().isEmpty());
        session->seek(0.75);
        QTRY_COMPARE(session->subtitle(), QString("Hello, world."));
    }

    void subtitleToolsRequireExplicitApply() {
        session->openSubtitles(QUrl::fromLocalFile(QFINDTESTDATA("fixtures/subtitles/source.srt")));
        window->setProperty("showRecent", false);
        auto *options = window->findChild<QObject *>("playerOptions");
        QVERIFY(options);
        QVERIFY(QMetaObject::invokeMethod(options, "subtitleToolsRequested", Q_ARG(bool, false)));
        auto *dialog = window->findChild<QObject *>("subtitleToolsDialog");
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("opened").toBool());
        QTest::qWait(200);
        auto *language = visualItem(window->contentItem(), "trackTranslationLanguage");
        QVERIFY(language && language->isVisible());
        QVERIFY(language->property("count").toInt() > 10);
        QVERIFY(!language->property("currentText").toString().isEmpty());
        click("translateTrackButton");
        auto *confirmation = window->findChild<QObject *>("trackTranslationConfirmation");
        QVERIFY(confirmation);
        QTRY_VERIFY(confirmation->property("opened").toBool());
        QVERIFY(confirmation->property("height").toDouble() > 100);
        QVERIFY(confirmation->property("height").toDouble() < window->height());
        QVERIFY(QMetaObject::invokeMethod(confirmation, "close"));
        QTRY_VERIFY(!confirmation->property("visible").toBool());
        auto *tools = study->subtitleTools();
        QVERIFY(!tools->busy());
        QCOMPARE(tools->lineCount(), 2);
        auto *firstPreview = window->findChild<QObject *>("firstMatchingLinePreview");
        auto *secondPreview = window->findChild<QObject *>("secondMatchingLinePreview");
        QVERIFY(firstPreview && secondPreview);
        QTRY_VERIFY(firstPreview->property("text").toString().contains("Hello, world."));
        QTRY_VERIFY(secondPreview->property("text").toString().contains("Hello, world."));
        QSignalSpy published(tools, &SubtitleTools::trackReady);
        tools->matchLines(0, 1.5);
        QVERIFY(tools->hasRepair());
        QCOMPARE(published.size(), 0);
        QCOMPARE(session->cueModel()->rowCount(), 2);
        click("applyTimingRepairButton");
        QCOMPARE(published.size(), 1);
        const auto saved = SubtitleTimeline::read(QUrl::fromLocalFile(published.first()[0].toString()));
        QCOMPARE(saved.first().startMs, 1500);
        QCOMPARE(saved.last().endMs, 5500);
        QCOMPARE(session->primaryDelay(), 0.0);
        QVERIFY(QMetaObject::invokeMethod(dialog, "close"));
    }

    void pausedNativeSubtitleSelectionDisplaysCurrentCue() {
        // 1. Establish a stable paused frame before selecting the embedded ASS track.
        const auto videoPath = qEnvironmentVariable("NEDEN_CHECK_ASS_VIDEO");
        if (videoPath.isEmpty())
            QSKIP("Set NEDEN_CHECK_ASS_VIDEO to the documented native ASS fixture.");
        destroyWindow();
        createWindow();
        QVERIFY(QFile::exists(videoPath));
        window->setProperty("showRecent", false);
        window->setProperty("inspectorMode", 1);
        session->openVideo(QUrl::fromLocalFile(videoPath));
        QTRY_VERIFY_WITH_TIMEOUT(session->duration() > 10, 10000);
        session->setPaused(true);
        session->seek(2);
        QTRY_VERIFY(std::abs(session->position() - 2) < 0.1);
        QTest::qWait(200);
        auto *video = visualItem(window->contentItem(), "videoSurface");
        QVERIFY(video);
        const auto beforeFrame = window->grabWindow();
        const qreal scaleX = qreal(beforeFrame.width()) / window->width();
        const qreal scaleY = qreal(beforeFrame.height()) / window->height();
        const auto bounds = video->mapRectToScene(video->boundingRect());
        const QRect pixelBounds(qRound(bounds.x() * scaleX), qRound(bounds.y() * scaleY),
                                qRound(bounds.width() * scaleX), qRound(bounds.height() * scaleY));
        const auto beforePixels = beforeFrame.copy(pixelBounds);
        const double beforePosition = session->position();
        QSignalSpy pauseChanges(session.get(), &LearningSession::pausedChanged);
        // Embedded text is promoted into the interactive timeline, without a seek.
        session->selectNativeSubtitle("2", false);
        QTRY_VERIFY_WITH_TIMEOUT(session->subtitle().contains("STYLED EMBEDDED TEST"), 10000);
        QVERIFY(session->primaryNativeTrack().isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(window->grabWindow().copy(pixelBounds) != beforePixels, 2000);
        // The new subtitle may change its glyph pixels, but the paused video
        // must remain intact. This catches a cue rendered over a corrupt frame.
        const auto originalFrame = beforePixels.convertToFormat(QImage::Format_RGB32);
        const auto updatedFrame =
            window->grabWindow().copy(pixelBounds).convertToFormat(QImage::Format_RGB32);
        QCOMPARE(updatedFrame.size(), originalFrame.size());
        qsizetype changedPixels = 0;
        for (int row = 0; row < originalFrame.height(); ++row) {
            const auto *originalPixels = reinterpret_cast<const QRgb *>(originalFrame.constScanLine(row));
            const auto *updatedPixels = reinterpret_cast<const QRgb *>(updatedFrame.constScanLine(row));
            for (int column = 0; column < originalFrame.width(); ++column) {
                if (std::abs(qRed(originalPixels[column]) - qRed(updatedPixels[column])) > 15 ||
                    std::abs(qGreen(originalPixels[column]) - qGreen(updatedPixels[column])) > 15 ||
                    std::abs(qBlue(originalPixels[column]) - qBlue(updatedPixels[column])) > 15)
                    ++changedPixels;
            }
        }
        QVERIFY2(changedPixels < qsizetype(originalFrame.width()) * originalFrame.height() / 5,
                 "Selecting a subtitle corrupted the paused video outside its glyphs.");
        QVERIFY(session->paused());
        QCOMPARE(pauseChanges.count(), 0);
        QVERIFY(std::abs(session->position() - beforePosition) < 0.1);
        const auto artifacts = qEnvironmentVariable("NEDEN_CHECK_ARTIFACTS");
        if (!artifacts.isEmpty())
            QVERIFY(window->grabWindow().save(artifacts + "/ass-direct-paused-selection.png"));
    }

    void cleanupTestCase() { destroyWindow(); }
};

int main(int argc, char **argv) {
    qputenv("QSG_RENDER_LOOP", "basic");
    QQuickWindow::setGraphicsApi(qEnvironmentVariableIsSet("NEDEN_CHECK_HEADLESS")
                                     ? QSGRendererInterface::Software
                                     : QSGRendererInterface::OpenGL);
    QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);
    QSurfaceFormat format;
    format.setVersion(3, 2);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setColorSpace(QColorSpace::SRgb);
    QSurfaceFormat::setDefaultFormat(format);
    QtWebView::initialize();
    QGuiApplication application(argc, argv);
    initializePlaybackEnvironment();
    application.setApplicationName("Neden UI checks");
    application.setQuitOnLastWindowClosed(false);
    QQuickStyle::setStyle("Basic");
    for (const auto &weight : {"Regular", "Medium", "SemiBold"}) {
        QFontDatabase::addApplicationFont(QString(":/fonts/Inter-%1.otf").arg(weight));
    }
    NativeUiChecks checks;
    QTimer::singleShot(0, &application, [&] { application.exit(QTest::qExec(&checks, argc, argv)); });
    return application.exec();
}
#include "native_ui_checks.moc"
