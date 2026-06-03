#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QLocale>
#include <QTranslator>

#include "mainwindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("FileTransfer"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));
    app.setOrganizationName(QStringLiteral("FileTransfer"));

    // 命令行参数解析
    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("网络文件传输工具 - 支持 Windows 与树莓派 ARM 平台互传"));

    QCommandLineOption mode_option(
        QStringLiteral("mode"),
        QStringLiteral("运行模式: server (服务端/监听端) 或 client (客户端/连接端)"),
        QStringLiteral("mode"),
        QStringLiteral("server"));

    QCommandLineOption port_option(
        QStringLiteral("port"),
        QStringLiteral("服务端口 (默认: 8888)"),
        QStringLiteral("port"));

    QCommandLineOption host_option(
        QStringLiteral("host"),
        QStringLiteral("服务器地址 (客户端模式)"),
        QStringLiteral("host"));

    QCommandLineOption files_option(
        QStringLiteral("files"),
        QStringLiteral("要传输的文件或目录 (逗号分隔)"),
        QStringLiteral("files"));

    QCommandLineOption no_gui_option(
        QStringLiteral("no-gui"),
        QStringLiteral("无 GUI 模式，仅命令行运行"));

    parser.addOption(mode_option);
    parser.addOption(port_option);
    parser.addOption(host_option);
    parser.addOption(files_option);
    parser.addOption(no_gui_option);
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(app);

    // 确定运行模式
    QString mode_str = parser.value(mode_option).toLower();
    TransferEngine::Role role;

    if (mode_str == QStringLiteral("client") || mode_str == QStringLiteral("c")) {
        role = TransferEngine::Role::Client;
    } else {
        role = TransferEngine::Role::Server;
    }

    // 无 GUI 模式
    if (parser.isSet(no_gui_option)) {
        // 待实现：纯命令行模式
        // 当前版本仍启动 GUI
        qWarning("无 GUI 模式暂未实现，将启动 GUI 界面");
    }

    // 启动主窗口
    MainWindow main_window(role);
    main_window.show();

    // 如果指定了端口，直接设置
    if (parser.isSet(port_option)) {
        bool ok;
        uint16_t port = static_cast<uint16_t>(parser.value(port_option).toUInt(&ok));
        if (ok && port > 0) {
            // 端口通过界面设置
        }
    }

    // 如果指定了主机，自动连接（客户端模式）
    if (role == TransferEngine::Role::Client && parser.isSet(host_option)) {
        QString host = parser.value(host_option);
        uint16_t port = DEFAULT_PORT;
        if (parser.isSet(port_option)) {
            bool ok;
            uint16_t p = static_cast<uint16_t>(parser.value(port_option).toUInt(&ok));
            if (ok) port = p;
        }
        // 自动连接
        main_window.autoConnect(host, port);
    }

    return app.exec();
}
