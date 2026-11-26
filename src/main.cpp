// OWNER: Member 1. Application entry point: loads DataPaths, ManagerAuth, shows RoleDialog, then MainWindow.
#include <QApplication>
#include <filesystem>

#include "DataPaths.h"
#include "ManagerAuth.h"
#include "MainWindow.h"
#include "RoleDialog.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    DataPaths paths(QCoreApplication::applicationDirPath().toStdString());
    paths.ensureDataFolder();

    ManagerAuth auth;
    const std::string settingsPath =
        (std::filesystem::path(paths.dataDir()) / "Settings.txt").generic_string();
    OperationResult authNotice = auth.load(settingsPath);

    RoleDialog roleDialog(auth);
    if (roleDialog.exec() != QDialog::Accepted) {
        return 0;
    }

    MainWindow window(paths, auth, roleDialog.selectedRole(), authNotice);
    window.show();
    return app.exec();
}
