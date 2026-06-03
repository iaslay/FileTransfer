#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QProgressBar>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include <QCheckBox>
#include <QSplitter>
#include <QGroupBox>

#include "transfer.h"

/**
 * 统一主窗口类
 * 通过 Role 参数区分服务端/客户端模式
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(TransferEngine::Role role, QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    // 文件选择
    void onSelectFiles();
    void onSelectDirectory();

    // 传输控制
    void onStartTransfer();
    void onCancelTransfer();
    void onPauseTransfer();
    void onResumeTransfer();

    // 连接控制
    void onConnect();
    void onDisconnect();
    void onStartServer();
    void onStopServer();

    // 传输引擎信号
    void onProgressUpdated(const QString& file_name, int percent, double speed_kbps);
    void onFileFinished(const QString& file_name, bool success);
    void onAllFinished();
    void onLogMessage(const QString& msg);
    void onStateChanged(TransferEngine::State new_state);
    void onConnected();
    void onDisconnected();

public:
    // 命令行自动连接
    void autoConnect(const QString& host, uint16_t port) { m_ip_input->setText(host); m_port_input->setValue(port); onConnect(); }

private:
    void setupUI();
    void setupConnections();
    void updateUIState();
    QString stateToString(TransferEngine::State state);

    // 传输引擎
    TransferEngine*    m_engine;
    TransferEngine::Role m_role;

    // 界面组件 - 连接区域
    QGroupBox*         m_connect_group;
    QLabel*            m_ip_label;
    QLineEdit*         m_ip_input;
    QLabel*            m_port_label;
    QSpinBox*          m_port_input;
    QPushButton*       m_connect_btn;
    QPushButton*       m_disconnect_btn;
    QLabel*            m_status_label;

    // 界面组件 - 文件选择区域
    QGroupBox*         m_file_group;
    QListWidget*       m_file_list;
    QPushButton*       m_select_file_btn;
    QPushButton*       m_select_dir_btn;
    QPushButton*       m_clear_list_btn;
    QCheckBox*         m_block_transfer_cb;
    QLabel*            m_block_size_label;
    QSpinBox*          m_block_size_input;

    // 界面组件 - 传输控制区域
    QPushButton*       m_start_btn;
    QPushButton*       m_pause_btn;
    QPushButton*       m_resume_btn;
    QPushButton*       m_cancel_btn;

    // 界面组件 - 进度区域
    QProgressBar*      m_progress_bar;
    QLabel*            m_file_name_label;
    QLabel*            m_speed_label;

    // 界面组件 - 日志区域
    QTextEdit*         m_log_view;

    // 文件列表（完整路径）
    QStringList         m_selected_files;
};

#endif // MAINWINDOW_H