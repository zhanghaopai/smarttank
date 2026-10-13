#include <QApplication>
#include <QTimer>
#include <QGuiApplication>
#include <QPainter>
#include <QAction>
#include <iostream>
#include "ui/MainWindow.h"
#include "view/OcctView.h"

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("SmartTank"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    smarttank::MainWindow window;
    window.show();
    if (application.arguments().contains(QStringLiteral("--preview-example")))
        window.findChild<QAction*>(QStringLiteral("previewExampleAction"))->trigger();
    if (application.arguments().contains(QStringLiteral("--smoke-test"))) {
        QTimer::singleShot(1200, &application, [&] {
            const auto native = QGuiApplication::platformName() == "windows";
            const auto ready = !native || window.view()->initialized();
            std::cout << "Workspace ready; OCCT native view: " << (native && ready ? "ready" : "not tested") << '\n';
            if (!window.view()->failure().isEmpty()) std::cerr << window.view()->failure().toStdString() << '\n';
            const auto args = application.arguments();
            const auto captureIndex = args.indexOf(QStringLiteral("--capture"));
            bool captured = true;
            if (captureIndex >= 0 && captureIndex + 1 < args.size()) {
                auto screenshot = window.grab();
                if (native && ready) {
                    const auto viewImage = window.view()->snapshot();
                    captured = !viewImage.isNull();
                    QPainter painter(&screenshot);
                    painter.drawImage(QRect(window.view()->mapTo(&window, QPoint(0, 0)), window.view()->size()), viewImage);
                }
                captured = screenshot.save(args.at(captureIndex + 1)) && captured;
            }
            application.exit(ready && captured ? 0 : 1);
        });
    }
    return application.exec();
}
