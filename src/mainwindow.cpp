#include "mainwindow.h"

#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QMenuBar>
#include <QStatusBar>
#include <QHeaderView>
#include <QDateTime>
#include <QLocale>
#include <QFileInfo>
#include <QDir>
#include <QHostInfo>

MainWindow::MainWindow(TransferEngine::Role role, QWidget* parent)
    : QMainWindow(parent)
    , m_role(role)
{
    // 创建传输引擎
    m_engine = new TransferEngine(role, this);

    setupUI();
    setupConnections();
    updateUIState();

    QString mode_name = (role == TransferEngine::Role::Server)
        ? QStringLiteral("服务端") : QStringLiteral("客户端");
    setWindowTitle(QStringLiteral("文件传输工具 - ") + mode_name);

    // 如果是服务端模式，自动启动
    if (role == TransferEngine::Role::Server) {
        onStartServer();
    }
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUI() {
    QWidget* central_widget = new QWidget(this);
    setCentralWidget(central_widget);

    QVBoxLayout* main_layout = new QVBoxLayout(central_widget);
    main_layout->setSpacing(8);
    main_layout->setContentsMargins(12, 12, 12, 12);

    // ============================================================
    // 连接区域
    // ============================================================
    m_connect_group = new QGroupBox(QStringLiteral("连接设置"), this);
    QHBoxLayout* connect_layout = new QHBoxLayout(m_connect_group);

    bool is_server = (m_role == TransferEngine::Role::Server);

    if (is_server) {
        m_port_label = new QLabel(QStringLiteral("监听端口:"), this);
        m_port_input = new QSpinBox(this);
        m_port_input->setRange(1024, 65535);
        m_port_input->setValue(DEFAULT_PORT);

        m_connect_btn = new QPushButton(QStringLiteral("启动服务"), this);
        m_connect_btn->setStyleSheet(
            "QPushButton {"
            "  background-color: #4CAF50; color: white;"
            "  padding: 6px 16px; border-radius: 4px; font-weight: bold;"
            "}"
            "QPushButton:hover { background-color: #45a049; }");

        m_disconnect_btn = new QPushButton(QStringLiteral("停止服务"), this);
        m_disconnect_btn->setStyleSheet(
            "QPushButton {"
            "  background-color: #f44336; color: white;"
            "  padding: 6px 16px; border-radius: 4px; font-weight: bold;"
            "}"
            "QPushButton:hover { background-color: #da190b; }");
        m_disconnect_btn->setEnabled(false);

        connect_layout->addWidget(m_port_label);
        connect_layout->addWidget(m_port_input);
        connect_layout->addWidget(m_connect_btn);
        connect_layout->addWidget(m_disconnect_btn);
    } else {
        m_ip_label = new QLabel(QStringLiteral("服务器IP:"), this);
        m_ip_input = new QLineEdit(this);
        m_ip_input->setPlaceholderText(QStringLiteral("输入服务器 IP 地址"));
        m_ip_input->setText(QStringLiteral("192.168.1."));

        m_port_label = new QLabel(QStringLiteral("端口:"), this);
        m_port_input = new QSpinBox(this);
        m_port_input->setRange(1024, 65535);
        m_port_input->setValue(DEFAULT_PORT);

        m_connect_btn = new QPushButton(QStringLiteral("连接服务器"), this);
        m_connect_btn->setStyleSheet(
            "QPushButton {"
            "  background-color: #2196F3; color: white;"
            "  padding: 6px 16px; border-radius: 4px; font-weight: bold;"
            "}"
            "QPushButton:hover { background-color: #0b7dda; }");

        m_disconnect_btn = new QPushButton(QStringLiteral("断开连接"), this);
        m_disconnect_btn->setStyleSheet(
            "QPushButton {"
            "  background-color: #f44336; color: white;"
            "  padding: 6px 16px; border-radius: 4px; font-weight: bold;"
            "}"
            "QPushButton:hover { background-color: #da190b; }");
        m_disconnect_btn->setEnabled(false);

        connect_layout->addWidget(m_ip_label);
        connect_layout->addWidget(m_ip_input, 1);
        connect_layout->addWidget(m_port_label);
        connect_layout->addWidget(m_port_input);
        connect_layout->addWidget(m_connect_btn);
        connect_layout->addWidget(m_disconnect_btn);
    }

    // 连接状态
    m_status_label = new QLabel(QStringLiteral("状态: 未连接"), this);
    m_status_label->setStyleSheet(
        "QLabel { color: #666; padding: 4px 8px;"
        "  border: 1px solid #ddd; border-radius: 4px; }");
    connect_layout->addWidget(m_status_label, is_server ? 1 : 0);

    main_layout->addWidget(m_connect_group);

    // ============================================================
    // 文件选择区域
    // ============================================================
    m_file_group = new QGroupBox(QStringLiteral("文件选择"), this);
    QVBoxLayout* file_layout = new QVBoxLayout(m_file_group);

    // 文件列表
    m_file_list = new QListWidget(this);
    m_file_list->setAlternatingRowColors(true);
    m_file_list->setSelectionMode(QAbstractItemView::ExtendedSelection);

    // 按钮行
    QHBoxLayout* file_btn_layout = new QHBoxLayout();
    m_select_file_btn = new QPushButton(QStringLiteral("选择文件..."), this);
    m_select_dir_btn = new QPushButton(QStringLiteral("选择目录..."), this);
    m_clear_list_btn = new QPushButton(QStringLiteral("清空列表"), this);

    file_btn_layout->addWidget(m_select_file_btn);
    file_btn_layout->addWidget(m_select_dir_btn);
    file_btn_layout->addWidget(m_clear_list_btn);
    file_btn_layout->addStretch();

    // 分块传输选项
    m_block_transfer_cb = new QCheckBox(QStringLiteral("启用分块传输"), this);
    m_block_transfer_cb->setChecked(true);

    m_block_size_label = new QLabel(QStringLiteral("块大小 (MB):"), this);
    m_block_size_input = new QSpinBox(this);
    m_block_size_input->setRange(1, 64);
    m_block_size_input->setValue(4);
    m_block_size_input->setSuffix(QStringLiteral(" MB"));

    QHBoxLayout* block_layout = new QHBoxLayout();
    block_layout->addWidget(m_block_transfer_cb);
    block_layout->addWidget(m_block_size_label);
    block_layout->addWidget(m_block_size_input);
    block_layout->addStretch();

    file_layout->addWidget(m_file_list, 1);
    file_layout->addLayout(file_btn_layout);
    file_layout->addLayout(block_layout);

    main_layout->addWidget(m_file_group, 1);

    // ============================================================
    // 传输控制区域
    // ============================================================
    QHBoxLayout* control_layout = new QHBoxLayout();

    m_start_btn = new QPushButton(QStringLiteral("开始传输"), this);
    m_start_btn->setStyleSheet(
        "QPushButton {"
        "  background-color: #4CAF50; color: white;"
        "  padding: 8px 24px; border-radius: 4px; font-size: 14px;"
        "}"
        "QPushButton:hover { background-color: #45a049; }"
        "QPushButton:disabled { background-color: #ccc; }");

    m_pause_btn = new QPushButton(QStringLiteral("暂停"), this);
    m_pause_btn->setStyleSheet(
        "QPushButton {"
        "  background-color: #FF9800; color: white;"
        "  padding: 8px 24px; border-radius: 4px; font-size: 14px;"
        "}"
        "QPushButton:hover { background-color: #e68900; }");
    m_pause_btn->setEnabled(false);

    m_resume_btn = new QPushButton(QStringLiteral("恢复"), this);
    m_resume_btn->setStyleSheet(
        "QPushButton {"
        "  background-color: #2196F3; color: white;"
        "  padding: 8px 24px; border-radius: 4px; font-size: 14px;"
        "}"
        "QPushButton:hover { background-color: #0b7dda; }");
    m_resume_btn->setEnabled(false);

    m_cancel_btn = new QPushButton(QStringLiteral("取消"), this);
    m_cancel_btn->setStyleSheet(
        "QPushButton {"
        "  background-color: #f44336; color: white;"
        "  padding: 8px 24px; border-radius: 4px; font-size: 14px;"
        "}"
        "QPushButton:hover { background-color: #da190b; }");
    m_cancel_btn->setEnabled(false);

    control_layout->addWidget(m_start_btn);
    control_layout->addWidget(m_pause_btn);
    control_layout->addWidget(m_resume_btn);
    control_layout->addWidget(m_cancel_btn);
    control_layout->addStretch();

    main_layout->addLayout(control_layout);

    // ============================================================
    // 进度区域
    // ============================================================
    m_file_name_label = new QLabel(QStringLiteral("未选择文件"), this);
    m_file_name_label->setStyleSheet("QLabel { font-weight: bold; }");

    m_progress_bar = new QProgressBar(this);
    m_progress_bar->setRange(0, 100);
    m_progress_bar->setValue(0);
    m_progress_bar->setTextVisible(true);
    m_progress_bar->setFixedHeight(24);

    m_speed_label = new QLabel(QStringLiteral("速度: -- KB/s"), this);

    QVBoxLayout* progress_layout = new QVBoxLayout();
    QHBoxLayout* progress_info_layout = new QHBoxLayout();
    progress_info_layout->addWidget(m_file_name_label, 1);
    progress_info_layout->addWidget(m_speed_label);
    progress_layout->addLayout(progress_info_layout);
    progress_layout->addWidget(m_progress_bar);

    main_layout->addLayout(progress_layout);

    // ============================================================
    // 日志区域
    // ============================================================
    QLabel* log_label = new QLabel(QStringLiteral("运行日志:"), this);
    main_layout->addWidget(log_label);

    m_log_view = new QTextEdit(this);
    m_log_view->setReadOnly(true);
    m_log_view->document()->setMaximumBlockCount(1000);
    m_log_view->setStyleSheet(
        "QTextEdit {"
        "  background-color: #1e1e1e; color: #d4d4d4;"
        "  font-family: 'Consolas', 'Courier New', monospace;"
        "  font-size: 12px;"
        "}");

    main_layout->addWidget(m_log_view, 1);

    // 状态栏
    statusBar()->showMessage(QStringLiteral("就绪"));

    // 设置窗口大小
    resize(800, 700);
    setMinimumSize(600, 500);
}

void MainWindow::setupConnections() {
    // 传输引擎信号
    connect(m_engine, &TransferEngine::progressUpdated,
            this, &MainWindow::onProgressUpdated);
    connect(m_engine, &TransferEngine::fileTransferFinished,
            this, &MainWindow::onFileFinished);
    connect(m_engine, &TransferEngine::allTransfersFinished,
            this, &MainWindow::onAllFinished);
    connect(m_engine, &TransferEngine::logMessage,
            this, &MainWindow::onLogMessage);
    connect(m_engine, &TransferEngine::stateChanged,
            this, &MainWindow::onStateChanged);
    connect(m_engine, &TransferEngine::connected,
            this, &MainWindow::onConnected);
    connect(m_engine, &TransferEngine::disconnected,
            this, &MainWindow::onDisconnected);

    // 按钮信号
    connect(m_select_file_btn, &QPushButton::clicked,
            this, &MainWindow::onSelectFiles);
    connect(m_select_dir_btn, &QPushButton::clicked,
            this, &MainWindow::onSelectDirectory);
    connect(m_clear_list_btn, &QPushButton::clicked, [this]() {
        m_file_list->clear();
        m_selected_files.clear();
    });

    connect(m_start_btn, &QPushButton::clicked,
            this, &MainWindow::onStartTransfer);
    connect(m_pause_btn, &QPushButton::clicked,
            this, &MainWindow::onPauseTransfer);
    connect(m_resume_btn, &QPushButton::clicked,
            this, &MainWindow::onResumeTransfer);
    connect(m_cancel_btn, &QPushButton::clicked,
            this, &MainWindow::onCancelTransfer);

    if (m_role == TransferEngine::Role::Server) {
        connect(m_connect_btn, &QPushButton::clicked,
                this, &MainWindow::onStartServer);
        connect(m_disconnect_btn, &QPushButton::clicked,
                this, &MainWindow::onStopServer);
    } else {
        connect(m_connect_btn, &QPushButton::clicked,
                this, &MainWindow::onConnect);
        connect(m_disconnect_btn, &QPushButton::clicked,
                this, &MainWindow::onDisconnect);
    }

    // 分块设置联动
    connect(m_block_transfer_cb, &QCheckBox::toggled,
            m_block_size_label, &QLabel::setEnabled);
    connect(m_block_transfer_cb, &QCheckBox::toggled,
            m_block_size_input, &QSpinBox::setEnabled);
}

void MainWindow::updateUIState() {
    TransferEngine::State state = m_engine->state();
    bool connected = m_engine->isConnected();
    bool is_idle = (state == TransferEngine::State::Idle ||
                    state == TransferEngine::State::Finished);
    bool is_transferring = (state == TransferEngine::State::Transferring);
    bool is_paused = (state == TransferEngine::State::Paused);

    // 连接区域
    if (m_role == TransferEngine::Role::Server) {
        m_port_input->setEnabled(!connected);
        m_connect_btn->setEnabled(!connected);
        m_disconnect_btn->setEnabled(connected);
    } else {
        m_ip_input->setEnabled(!connected && is_idle);
        m_port_input->setEnabled(!connected && is_idle);
        m_connect_btn->setEnabled(!connected && is_idle);
        m_disconnect_btn->setEnabled(connected);
    }

    // 文件选择
    m_select_file_btn->setEnabled(connected && is_idle);
    m_select_dir_btn->setEnabled(connected && is_idle);
    m_clear_list_btn->setEnabled(connected && is_idle);
    m_block_transfer_cb->setEnabled(connected && is_idle);
    m_block_size_input->setEnabled(connected && is_idle
                                   && m_block_transfer_cb->isChecked());
    m_block_size_label->setEnabled(connected && is_idle
                                   && m_block_transfer_cb->isChecked());

    // 传输控制
    bool has_files = (m_selected_files.size() > 0);
    m_start_btn->setEnabled(connected && has_files && is_idle);
    m_pause_btn->setEnabled(is_transferring);
    m_resume_btn->setEnabled(is_paused);
    m_cancel_btn->setEnabled(is_transferring || is_paused);

    // 更新状态标签
    QString status_text;
    if (!connected && m_role == TransferEngine::Role::Server) {
        status_text = QStringLiteral("状态: 未启动");
    } else if (!connected) {
        status_text = QStringLiteral("状态: 未连接");
    } else if (is_idle) {
        status_text = QStringLiteral("状态: 已连接（空闲）");
    } else if (is_transferring) {
        status_text = QStringLiteral("状态: 传输中...");
    } else if (is_paused) {
        status_text = QStringLiteral("状态: 已暂停");
    } else if (state == TransferEngine::State::Handshaking) {
        status_text = QStringLiteral("状态: 握手...");
    } else if (state == TransferEngine::State::Error) {
        status_text = QStringLiteral("状态: 错误");
    }
    m_status_label->setText(status_text);
}

QString MainWindow::stateToString(TransferEngine::State state) {
    switch (state) {
        case TransferEngine::State::Idle:
            return QStringLiteral("空闲");
        case TransferEngine::State::Handshaking:
            return QStringLiteral("握手");
        case TransferEngine::State::Transferring:
            return QStringLiteral("传输中");
        case TransferEngine::State::Paused:
            return QStringLiteral("暂停");
        case TransferEngine::State::Finished:
            return QStringLiteral("完成");
        case TransferEngine::State::Error:
            return QStringLiteral("错误");
        default:
            return QStringLiteral("未知");
    }
}

// ============================================================
// 文件选择
// ============================================================

void MainWindow::onSelectFiles() {
    QStringList files = QFileDialog::getOpenFileNames(
        this, QStringLiteral("选择要传输的文件"), QString(),
        QStringLiteral("所有文件 (*)"));

    for (const QString& file : files) {
        QFileInfo fi(file);
        m_selected_files.append(file);

        QString display = QStringLiteral("%1  (%2)")
            .arg(fi.fileName())
            .arg(QLocale().formattedDataSize(fi.size()));
        m_file_list->addItem(display);
    }

    updateUIState();
}

void MainWindow::onSelectDirectory() {
    QString dir = QFileDialog::getExistingDirectory(
        this, QStringLiteral("选择要传输的目录"));

    if (dir.isEmpty()) return;

    QDir d(dir);
    m_selected_files.append(dir);

    // 计算文件数量
    std::vector<std::string> files_vec;
    FileUtil::listDirectory(dir.toStdString(), files_vec);

    QString display = QStringLiteral("文件夹 %1  (共 %2 个文件)")
        .arg(d.dirName())
        .arg(files_vec.size());
    m_file_list->addItem(display);

    updateUIState();
}

// ============================================================
// 传输控制
// ============================================================

void MainWindow::onStartTransfer() {
    if (m_selected_files.isEmpty()) return;

    // 设置分块大小
    m_engine->setBlockSize(m_block_size_input->value() * 1024 * 1024);

    // 检查每个项是文件还是目录，分别处理
    // 混合模式：先区分文件和目录
    QStringList files;
    QStringList dirs;

    for (const QString& path : m_selected_files) {
        QFileInfo fi(path);
        if (fi.isDir()) {
            dirs.append(path);
        } else {
            files.append(path);
        }
    }

    if (!files.isEmpty()) {
        m_engine->sendFiles(files);
    }

    for (const QString& dir : dirs) {
        m_engine->sendDirectory(dir);
    }

    updateUIState();
}

void MainWindow::onCancelTransfer() {
    m_engine->cancelTransfer();
}

void MainWindow::onPauseTransfer() {
    m_engine->pauseTransfer();
}

void MainWindow::onResumeTransfer() {
    m_engine->resumeTransfer();
}

// ============================================================
// 连接控制
// ============================================================

void MainWindow::onConnect() {
    QString host = m_ip_input->text().trimmed();
    uint16_t port = static_cast<uint16_t>(m_port_input->value());

    if (host.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("请输入服务器 IP 地址"));
        return;
    }

    m_connect_btn->setEnabled(false);
    m_engine->connectToServer(host, port);
}

void MainWindow::onDisconnect() {
    m_engine->disconnectServer();
}

void MainWindow::onStartServer() {
    uint16_t port = static_cast<uint16_t>(m_port_input->value());
    m_engine->startServer(port);
}

void MainWindow::onStopServer() {
    m_engine->stopServer();
}

// ============================================================
// 传输引擎信号处理
// ============================================================

void MainWindow::onProgressUpdated(const QString& file_name,
                                    int percent, double speed_kbps) {
    m_file_name_label->setText(file_name);
    m_progress_bar->setValue(percent);

    if (speed_kbps > 1024) {
        m_speed_label->setText(
            QStringLiteral("速度: %1 MB/s")
            .arg(speed_kbps / 1024.0, 0, 'f', 2));
    } else {
        m_speed_label->setText(
            QStringLiteral("速度: %1 KB/s")
            .arg(speed_kbps, 0, 'f', 1));
    }
}

void MainWindow::onFileFinished(const QString& file_name, bool success) {
    if (success) {
        onLogMessage(QStringLiteral("文件传输成功: ") + file_name);
    } else {
        onLogMessage(QStringLiteral("文件传输失败: ") + file_name);
    }

    // 从列表标记已完成的文件
    for (int i = 0; i < m_file_list->count(); i++) {
        if (m_file_list->item(i)->text().contains(file_name)) {
            m_file_list->item(i)->setForeground(
                success ? QColor(100, 180, 100) : QColor(220, 80, 80));
            break;
        }
    }
}

void MainWindow::onAllFinished() {
    onLogMessage(QStringLiteral("所有传输任务已完成！"));
    m_progress_bar->setValue(100);
    m_speed_label->setText(QStringLiteral("速度: -- KB/s"));
    updateUIState();
}

void MainWindow::onLogMessage(const QString& msg) {
    QString timestamp = QDateTime::currentDateTime()
        .toString(QStringLiteral("hh:mm:ss.zzz"));
    m_log_view->append(
        QStringLiteral("[%1] %2").arg(timestamp).arg(msg));
}

void MainWindow::onStateChanged(TransferEngine::State new_state) {
    Q_UNUSED(new_state);
    updateUIState();
}

void MainWindow::onConnected() {
    onLogMessage(QStringLiteral("连接已建立"));
    m_status_label->setStyleSheet(
        "QLabel {"
        "  color: #4CAF50; padding: 4px 8px;"
        "  border: 1px solid #4CAF50; border-radius: 4px;"
        "}");
    updateUIState();
}

void MainWindow::onDisconnected() {
    onLogMessage(QStringLiteral("连接已断开"));
    m_status_label->setStyleSheet(
        "QLabel {"
        "  color: #666; padding: 4px 8px;"
        "  border: 1px solid #ddd; border-radius: 4px;"
        "}");
    updateUIState();
}
