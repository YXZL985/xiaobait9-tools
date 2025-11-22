#include <DApplication>
#include <DMainWindow>
#include <DWidgetUtil>
#include <DApplicationSettings>
#include <DTitlebar>
#include <DProgressBar>
#include <DFontSizeManager>
#include <QPushButton>

#include <QPropertyAnimation>
#include <QDate>
#include <QLayout>
#include <QProcess>  
#include <QDir>
#include <QStandardPaths>
#include <QMessageBox>
#include <QTemporaryFile>
#include <QFile>
#include <QIODevice>



DWIDGET_USE_NAMESPACE

int main(int argc, char *argv[])
{
    QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    DApplication a(argc, argv);
    a.setOrganizationName("t9.xiaobai.pro");
    a.setApplicationName("xiaobait9-tools");
    a.setApplicationVersion("1.0");
    a.setProductIcon(QIcon(":/images/logo.svg"));
    a.setProductName("小白T9工具箱");
    a.setApplicationDescription("小白T9工具箱");

    // 尝试加载翻译文件，如果失败则忽略
    a.loadTranslator();
    a.setApplicationDisplayName(QCoreApplication::translate("Main", "小白T9工具箱"));

    DMainWindow w;
    w.titlebar()->setIcon(QIcon(":/images/logo.svg"));
    w.titlebar()->setTitle("小白T9工具箱");
    // 设置标题，宽度不够会隐藏标题文字
    w.setMinimumSize(QSize(600, 400));

    QWidget *cw = new QWidget(&w);
    QVBoxLayout *layout = new QVBoxLayout(cw);
    
    // 添加4个垂直排列等距的按钮控件
    QPushButton *button1 = new QPushButton("安装中州韵输入法引擎");
    QPushButton *button2 = new QPushButton("安装小白输入法九键拼音方案");
    QPushButton *button3 = new QPushButton("重新部署输入法引擎");
    QPushButton *button4 = new QPushButton("退出程序");
    
    // 绑定字体大小，使用更小的字体 T3 替代 T1
    DFontSizeManager::instance()->bind(button1, DFontSizeManager::T3);
    DFontSizeManager::instance()->bind(button2, DFontSizeManager::T3);
    DFontSizeManager::instance()->bind(button3, DFontSizeManager::T3);
    DFontSizeManager::instance()->bind(button4, DFontSizeManager::T3);
    
    layout->addWidget(button1);
    layout->addWidget(button2);
    layout->addWidget(button3);
    layout->addWidget(button4);
    
    // 设置布局间距使按钮等距分布
    layout->setSpacing(20);
    layout->setContentsMargins(50, 50, 50, 50);
    w.setCentralWidget(cw);
    
    // 连接退出按钮的信号槽
    QObject::connect(button4, &QPushButton::clicked, &w, &DMainWindow::close);
    
    // 连接安装中州韵输入法引擎按钮的信号槽
    QObject::connect(button1, &QPushButton::clicked, [&]() {
        QProcess::startDetached("deepin-terminal", QStringList() << "-e" << "./shell/install-rime.sh");
    });
    
    // 连接安装小白输入法九键拼音方案按钮的信号槽
    QObject::connect(button2, &QPushButton::clicked, [&]() {
        // 1. 构建目标目录路径 (~/.local/share/fcitx5/rime)
        QString targetDirPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/fcitx5/rime";
        qDebug() << "目标目录:" << targetDirPath;

        // 2. 创建目录（如果不存在）
        QDir targetDir(targetDirPath);
        if (!targetDir.exists()) {
            bool ok = targetDir.mkpath("."); // 创建所有父目录
            if (!ok) {
                QMessageBox::critical(&w, "错误", "无法创建目录: " + targetDirPath + "\n请检查权限。");
                return;
            }
            qDebug() << "目录创建成功:" << targetDirPath;
        } else {
            qDebug() << "目录已存在，跳过创建。";
        }

        // 3. 准备解压命令
        // 从 Qt 资源中读取文件
        // 注意：QProcess 不能直接使用 ":/" 路径执行外部命令，需要先将文件释放到临时目录
        QTemporaryFile *tempFile = new QTemporaryFile(&w);  // 改为动态分配，确保在进程运行期间有效
        if (!tempFile->open()) {
            QMessageBox::critical(&w, "错误", "无法创建临时文件。");
            delete tempFile;
            return;
        }

        QFile schemaFile(":/schema/xiaobai_schema.tar.gz");
        if (!schemaFile.open(QIODevice::ReadOnly)) {
            QMessageBox::critical(&w, "错误", "无法从资源中读取文件: :/schema/xiaobai_schema.tar.gz");
            delete tempFile;
            return;
        }

        char buffer[1024 * 1024]; // 1MB缓冲区
        qint64 bytesRead;
        while ((bytesRead = schemaFile.read(buffer, sizeof(buffer))) > 0) {
            tempFile->write(buffer, bytesRead);
        }
        tempFile->flush(); // 确保数据写入临时文件

        QString tarFilePath = tempFile->fileName();
        qDebug() << "临时文件路径:" << tarFilePath;

        // 4. 构建并执行 tar 命令
        QStringList arguments;
        arguments << "-xzf" << tarFilePath << "-C" << targetDirPath;

        QProcess *extractProcess = new QProcess(&w);
        QObject::connect(extractProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                &w, [&, extractProcess, tempFile](int exitCode, QProcess::ExitStatus exitStatus) {
            if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
                // 解压成功后，执行编辑default.yaml文件的操作
                QString command = "sed -i '/schema_list:/a\\  - schema: xiaobai_simp' /usr/share/rime-data/default.yaml";
                QStringList terminalArgs;
                terminalArgs << "-e" << "pkexec" << "bash" << "-c" << command;
                QProcess::startDetached("deepin-terminal", terminalArgs);
                
                QMessageBox::information(&w, "成功", "输入法方案已成功安装/更新！");
            } else {
                QString errorMsg = extractProcess->readAllStandardError();
                qDebug() << "解压失败，错误信息:" << errorMsg;
                QMessageBox::critical(&w, "失败", "安装失败！\n错误信息: " + errorMsg);
            }
            extractProcess->deleteLater(); // 清理进程对象
            tempFile->deleteLater();       // 清理临时文件
        });

        qDebug() << "执行命令: tar" << arguments;
        extractProcess->start("tar", arguments);
    });
    
    w.show();
    return a.exec();
    
}
