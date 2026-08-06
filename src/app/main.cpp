#include <QCoreApplication>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include "appcontroller.h"
#include "permissions.h"
#include "platform.h"
#include "qrimageprovider.h"
#include "theme.h"
#include "updater.h"
#ifdef BINGO_HAS_CAMERA
#  include "qrscanner.h"
#endif

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("OpenBingo"));
    app.setApplicationName(QStringLiteral("OpenBingo"));
    app.setWindowIcon(QIcon(QStringLiteral("qrc:/icons/openbingo.png")));

    QQuickStyle::setStyle(QStringLiteral("Material"));
    app::initNotifications();

    app::AppController controller;
    if (!controller.init()) {
        qCritical("AppController::init() failed");
        return 1;
    }

    QDesktopServices::setUrlHandler(QStringLiteral("openbingo"), &controller,
                                    "handleJoinUrl");

    QObject::connect(&app, &QGuiApplication::applicationStateChanged,
                     &controller, [&controller](Qt::ApplicationState state) {
        if (state == Qt::ApplicationActive)
            controller.reloadProjects();
    });

    app::Permissions permissions;
    qmlRegisterSingletonInstance("OpenBingo", 1, 0, "Permissions", &permissions);
#ifdef BINGO_HAS_CAMERA
    qmlRegisterType<app::QrScanner>("OpenBingo", 1, 0, "QrScanner");
#endif

    app::Theme theme;
    app::Updater updater;
    updater.check();

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("qr"), new app::QrImageProvider());
    engine.rootContext()->setContextProperty(QStringLiteral("AppController"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("Theme"), &theme);
    engine.rootContext()->setContextProperty(QStringLiteral("Updater"), &updater);
    engine.rootContext()->setContextProperty(QStringLiteral("bingoForcedW"),
        qEnvironmentVariableIntValue("BINGO_TEST_W"));
    engine.rootContext()->setContextProperty(QStringLiteral("bingoForcedH"),
        qEnvironmentVariableIntValue("BINGO_TEST_H"));
    engine.rootContext()->setContextProperty(QStringLiteral("bingoScreenshotDir"),
        QString::fromUtf8(qgetenv("BINGO_SCREENSHOT_DIR")));

    const QUrl url(QStringLiteral("qrc:/OpenBingo/qml/Main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject* obj, const QUrl& objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);

    engine.load(url);
    return app.exec();
}
