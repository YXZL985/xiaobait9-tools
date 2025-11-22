#include <DApplication>
#include <DMainWindow>
#include <DWidgetUtil>
#include <DApplicationSettings>
#include <DTitlebar>
#include <DProgressBar>
#include <DFontSizeManager>
#include <QPushButton>

#include <QLayout>
#include <QProcess>  
#include <QDir>
#include <QStandardPaths>
#include <QMessageBox>
#include <QTemporaryFile>
#include <QFile>
#include <QIODevice>
#include <QDebug>
#include <QLocale>
#include <QTranslator>
#include <QCoreApplication>
#include <QObject>

DWIDGET_USE_NAMESPACE

namespace {
class MainTr {
    Q_DECLARE_TR_FUNCTIONS(Main)
};
}

// 将方案写入用户目录 default.custom.yaml，避免修改系统文件导致闪退
bool ensureSchemaInstalled(const QString &targetDirPath, QWidget *parent)
{
    const QString defaultCustomPath = targetDirPath + "/default.custom.yaml";
    QFile defaultCustom(defaultCustomPath);
    QString content;

    if (defaultCustom.exists()) {
        if (!defaultCustom.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QMessageBox::critical(parent, MainTr::tr("Error"), MainTr::tr("Cannot read: %1").arg(defaultCustomPath));
            return false;
        }
        content = QString::fromUtf8(defaultCustom.readAll());
        defaultCustom.close();
    }

    if (content.contains("schema: xiaobai_simp") || content.contains("schema: xiaobai_tw")) {
        QMessageBox::information(parent, MainTr::tr("Info"), MainTr::tr("Scheme already exists, skip installing."));
        return true;
    }

    if (!content.endsWith('\n') && !content.isEmpty()) {
        content += '\n';
    }
    if (!content.contains("patch:")) {
        content += "patch:\n";
    }
    if (!content.contains("schema_list:")) {
        content += "  schema_list:\n";
    }
    content += "    - schema: xiaobai_simp\n";
    content += "    - schema: xiaobai_tw\n";

    if (!defaultCustom.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QMessageBox::critical(parent, MainTr::tr("Error"), MainTr::tr("Cannot write: %1").arg(defaultCustomPath));
        return false;
    }
    defaultCustom.write(content.toUtf8());
    defaultCustom.close();
    return true;
}

int main(int argc, char *argv[])
{
    QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    DApplication a(argc, argv);

    // 载入资源中的翻译文件
    QTranslator translator;
    translator.load(QLocale(), "xiaobait9-tools", "_", ":/translations");
    a.installTranslator(&translator);
    a.setOrganizationName("t9.xiaobai.pro");
    a.setApplicationName("xiaobait9-tools");
    a.setApplicationVersion("1.0");
    a.setProductIcon(QIcon(":/images/logo.svg"));
    a.setProductName(MainTr::tr("Xiaobai T9 Toolkit"));
    a.setApplicationDescription(MainTr::tr("Xiaobai T9 Toolkit"));

    // 尝试加载深度自带翻译文件，如果失败则忽略
    a.loadTranslator();
    a.setApplicationDisplayName(MainTr::tr("Xiaobai T9 Toolkit"));

    DMainWindow w;
    w.titlebar()->setIcon(QIcon(":/images/logo.svg"));
    w.titlebar()->setTitle(MainTr::tr("Xiaobai T9 Toolkit"));
    // 设置标题，宽度不够会隐藏标题文字
    w.setMinimumSize(QSize(600, 400));

    QWidget *cw = new QWidget(&w);
    QVBoxLayout *layout = new QVBoxLayout(cw);
    
    // 添加4个垂直排列等距的按钮控件
    QPushButton *button1 = new QPushButton(MainTr::tr("Install Rime engine"));
    QPushButton *button2 = new QPushButton(MainTr::tr("Install Xiaobai T9 scheme"));
    QPushButton *button3 = new QPushButton(MainTr::tr("Redeploy input method"));
    QPushButton *button4 = new QPushButton(MainTr::tr("Exit"));
    
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
                QMessageBox::critical(&w, MainTr::tr("Error"), MainTr::tr("Cannot create directory: %1\nPlease check permissions.").arg(targetDirPath));
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
            QMessageBox::critical(&w, MainTr::tr("Error"), MainTr::tr("Cannot create temporary file."));
            delete tempFile;
            return;
        }

        QFile schemaFile(":/schema/xiaobai_schema.tar.gz");
        if (!schemaFile.open(QIODevice::ReadOnly)) {
            QMessageBox::critical(&w, MainTr::tr("Error"), MainTr::tr("Cannot read resource file: %1").arg(":/schema/xiaobai_schema.tar.gz"));
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
                &w, [&, extractProcess, tempFile, targetDirPath](int exitCode, QProcess::ExitStatus exitStatus) {
            if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
                if (ensureSchemaInstalled(targetDirPath, &w)) {
                    QMessageBox::information(&w, MainTr::tr("Success"), MainTr::tr("Scheme installed to user directory. You can enable it now."));
                }
            } else {
                QString errorMsg = extractProcess->readAllStandardError();
                qDebug() << "解压失败，错误信息:" << errorMsg;
                QMessageBox::critical(&w, MainTr::tr("Error"), MainTr::tr("Installation failed.\nError: %1").arg(errorMsg));
            }
            extractProcess->deleteLater(); // 清理进程对象
            tempFile->deleteLater();       // 清理临时文件
        });

        qDebug() << "执行命令: tar" << arguments;
        extractProcess->start("tar", arguments);
    });
    
    // 连接重新部署输入法引擎按钮的信号槽
    QObject::connect(button3, &QPushButton::clicked, [&]() {
        QProcess *process = new QProcess(&w);
        QObject::connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                         &w, [process](int exitCode, QProcess::ExitStatus status) {
            if (exitCode == 0 && status == QProcess::NormalExit) {
                QMessageBox::information(nullptr, MainTr::tr("Success"), MainTr::tr("Input method redeployed / reloaded."));
            } else {
                const QString err = process->readAllStandardError();
                QMessageBox::critical(nullptr, MainTr::tr("Error"), MainTr::tr("Redeploy failed:\n%1").arg(err));
            }
            process->deleteLater();
        });
        process->start("bash", {"-c", "rime_deployer || fcitx5-remote -r"});
    });
    
    w.show();
    return a.exec();
    
}
