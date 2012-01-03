#ifndef UPDRAFT_SRC_CORE_SETTINGSMANAGER_H_
#define UPDRAFT_SRC_CORE_SETTINGSMANAGER_H_

#include <QtGui>
#include "ui/settingsdialog.h"
#include "../settinginterface.h"

namespace Updraft {
namespace Core {

class SettingsManager: public QObject {
Q_OBJECT

 public:
  SettingsManager();
  ~SettingsManager();

  SettingInterface* addSetting(
    const QString& settingId,
    const QString& description,
    QVariant initValue);

  QModelIndex addGroup(
    const QString& groupId,
    const QString& description,
    QIcon icon);

 private:
  QModelIndex getGroup(const QString& groupId);
  QModelIndex getSetting(const QString& settingId, QModelIndex groupIndex);

  QAction* settingsAction;

  SettingsDialog* dialog;

  // TODO(cestmir): This needs to be converted to our own editable model
  // So far, this is here only to test the specialized settings view
  QStandardItemModel* model;

  QRegExp idRegExp;
};

}  // End namespace Core
}  // End namespace Updraft

#endif  // UPDRAFT_SRC_CORE_SETTINGSMANAGER_H_

