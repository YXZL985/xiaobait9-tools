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

DWIDGET_USE_NAMESPACE

struct UiText {
    QString productName;
    QString appDescription;
    QString windowTitle;
    QString btnInstallEngine;
    QString btnInstallSchema;
    QString btnRedeploy;
    QString btnExit;
    QString titleError;
    QString titleInfo;
    QString titleSuccess;
    QString msgCreateDirFail;
    QString msgTmpFileFail;
    QString msgResourceReadFail;
    QString msgWriteTmpFail;
    QString msgSchemaExists;
    QString msgSchemaInstalled;
    QString msgSchemaInstallFail;
    QString msgDefaultReadFail;
    QString msgDefaultWriteFail;
    QString msgRedeploySuccess;
    QString msgRedeployFail;
};

UiText makeUiText()
{
    auto tr = [](const char *text) { return QCoreApplication::translate("Main", text); };
    UiText t;
    t.productName = tr("Xiaobai T9 Toolkit");
    t.appDescription = tr("Xiaobai T9 Toolkit");
    t.windowTitle = tr("Xiaobai T9 Toolkit");
    t.btnInstallEngine = tr("Install Rime engine");
    t.btnInstallSchema = tr("Install Xiaobai T9 scheme");
    t.btnRedeploy = tr("Redeploy input method");
    t.btnExit = tr("Exit");
    t.titleError = tr("Error");
    t.titleInfo = tr("Info");
    t.titleSuccess = tr("Success");
    t.msgCreateDirFail = tr("Cannot create directory: %1\nPlease check permissions.");
    t.msgTmpFileFail = tr("Cannot create temporary file.");
    t.msgResourceReadFail = tr("Cannot read resource file: %1");
    t.msgWriteTmpFail = tr("Failed to write temporary file.");
    t.msgSchemaExists = tr("Scheme already exists, skip installing.");
    t.msgSchemaInstalled = tr("Scheme installed to user directory. You can enable it now.");
    t.msgSchemaInstallFail = tr("Installation failed.\nError: %1");
    t.msgDefaultReadFail = tr("Cannot read: %1");
    t.msgDefaultWriteFail = tr("Cannot write: %1");
    t.msgRedeploySuccess = tr("Input method redeployed / reloaded.");
    t.msgRedeployFail = tr("Redeploy failed:\n%1");
    return t;
}

// Write xiaobai scheme into user's default.custom.yaml, avoid touching system files.
bool ensureSchemaInstalled(const QString &targetDirPath, QWidget *parent, const UiText &t)
{
    const QString defaultCustomPath = targetDirPath + "/default.custom.yaml";
    QFile defaultCustom(defaultCustomPath);
    QString content;

    if (defaultCustom.exists()) {
        if (!defaultCustom.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QMessageBox::critical(parent, t.titleError, t.msgDefaultReadFail.arg(defaultCustomPath));
            return false;
        }
        content = QString::fromUtf8(defaultCustom.readAll());
        defaultCustom.close();
    }

    if (content.contains("schema: xiaobai")) {
        QMessageBox::information(parent, t.titleInfo, t.msgSchemaExists);
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
    content += "    - schema: xiaobai\n";

    if (!defaultCustom.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QMessageBox::critical(parent, t.titleError, t.msgDefaultWriteFail.arg(defaultCustomPath));
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

    // Load translation from resources
    auto translator = new QTranslator(&a);
    translator->load(QLocale(), "xiaobait9-tools", "_", ":/translations");
    a.installTranslator(translator);
    a.loadTranslator();

    const UiText t = makeUiText();

    a.setOrganizationName("t9.xiaobai.pro");
    a.setApplicationName("xiaobait9-tools");
    a.setApplicationVersion("1.0");
    a.setProductIcon(QIcon(":/images/logo.svg"));
    a.setProductName(t.productName);
    a.setApplicationDescription(t.appDescription);

    a.setApplicationDisplayName(t.windowTitle);

    DMainWindow w;
    w.titlebar()->setIcon(QIcon(":/images/logo.svg"));
    w.titlebar()->setTitle(t.windowTitle);
    w.setMinimumSize(QSize(600, 400));

    QWidget *cw = new QWidget(&w);
    QVBoxLayout *layout = new QVBoxLayout(cw);

    QPushButton *button1 = new QPushButton(t.btnInstallEngine);
    QPushButton *button2 = new QPushButton(t.btnInstallSchema);
    QPushButton *button3 = new QPushButton(t.btnRedeploy);
    QPushButton *button4 = new QPushButton(t.btnExit);

    DFontSizeManager::instance()->bind(button1, DFontSizeManager::T3);
    DFontSizeManager::instance()->bind(button2, DFontSizeManager::T3);
    DFontSizeManager::instance()->bind(button3, DFontSizeManager::T3);
    DFontSizeManager::instance()->bind(button4, DFontSizeManager::T3);

    layout->addWidget(button1);
    layout->addWidget(button2);
    layout->addWidget(button3);
    layout->addWidget(button4);
    layout->setSpacing(20);
    layout->setContentsMargins(50, 50, 50, 50);
    w.setCentralWidget(cw);

    QObject::connect(button4, &QPushButton::clicked, &w, &DMainWindow::close);

    // Install engine: run installer script in terminal with pkexec
    QObject::connect(button1, &QPushButton::clicked, [&]() {
        QProcess::startDetached("deepin-terminal", QStringList() << "-e" << "./shell/install-rime.sh");
    });

    // Install schema: extract archive into user dir and patch default.custom.yaml
    QObject::connect(button2, &QPushButton::clicked, [&]() {
        const QString targetDirPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/fcitx5/rime";
        QDir targetDir(targetDirPath);
        if (!targetDir.exists() && !targetDir.mkpath(".")) {
            QMessageBox::critical(&w, t.titleError, t.msgCreateDirFail.arg(targetDirPath));
            return;
        }

        QTemporaryFile *tempFile = new QTemporaryFile(&w);
        if (!tempFile->open()) {
            QMessageBox::critical(&w, t.titleError, t.msgTmpFileFail);
            tempFile->deleteLater();
            return;
        }

        QFile schemaFile(":/schema/xiaobai_schema.tar.gz");
        if (!schemaFile.open(QIODevice::ReadOnly)) {
            QMessageBox::critical(&w, t.titleError, t.msgResourceReadFail.arg(":/schema/xiaobai_schema.tar.gz"));
            tempFile->deleteLater();
            return;
        }

        if (tempFile->write(schemaFile.readAll()) <= 0) {
            QMessageBox::critical(&w, t.titleError, t.msgWriteTmpFail);
            tempFile->deleteLater();
            return;
        }
        tempFile->flush();

        QProcess *extractProcess = new QProcess(&w);
        QStringList arguments;
        arguments << "-xzf" << tempFile->fileName() << "-C" << targetDirPath;
        QObject::connect(extractProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                         &w, [&, extractProcess, tempFile, targetDirPath, t](int exitCode, QProcess::ExitStatus exitStatus) {
            if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
                if (ensureSchemaInstalled(targetDirPath, &w, t)) {
                    QMessageBox::information(&w, t.titleSuccess, t.msgSchemaInstalled);
                }
            } else {
                const QString errorMsg = extractProcess->readAllStandardError();
                QMessageBox::critical(&w, t.titleError, t.msgSchemaInstallFail.arg(errorMsg));
            }
            extractProcess->deleteLater();
            tempFile->deleteLater();
        });

        extractProcess->start("tar", arguments);
    });

    // Redeploy: try to reload input method
    QObject::connect(button3, &QPushButton::clicked, [&]() {
        QProcess *process = new QProcess(&w);
        QObject::connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                         &w, [process, t](int exitCode, QProcess::ExitStatus status) {
            if (exitCode == 0 && status == QProcess::NormalExit) {
                QMessageBox::information(nullptr, t.titleSuccess, t.msgRedeploySuccess);
            } else {
                const QString err = process->readAllStandardError();
                QMessageBox::critical(nullptr, t.titleError, t.msgRedeployFail.arg(err));
            }
            process->deleteLater();
        });
        process->start("bash", {"-c", "rime_deployer || fcitx5-remote -r"});
    });

    w.show();
    return a.exec();
}
