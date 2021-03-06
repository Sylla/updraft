#include "basicsetting.h"

namespace Updraft {
namespace Core {

BasicSetting::~BasicSetting() {
  // If item is not valid anymore, no need to unregister the setting
  if (item) {
    settingsManager->unregisterSetting(item, this);
  }
}

QVariant BasicSetting::get() {
  if (item) {
    SettingsModel* model = item->getModel();
    QModelIndex index = model->indexFromItem(item);
    return model->data(index, ValueRole);
  } else {
    return QVariant();
  }
}

void BasicSetting::set(const QVariant& newValue) {
  if (item) {
    SettingsModel* model = item->getModel();
    QModelIndex index = model->indexFromItem(item);
    model->setData(index, newValue, ValueRole);
  }
}

void BasicSetting::setDescription(const QString& newDescription) {
  if (item) {
    SettingsModel* model = item->getModel();
    QModelIndex index = model->indexFromItem(item);
    model->setData(index, QVariant(newDescription), DescriptionRole);
  }
}

void BasicSetting::setNeedsRestart(bool needsRestart) {
  if (item) {
    SettingsModel* model = item->getModel();
    QModelIndex index = model->indexFromItem(item);
    model->setData(index, QVariant(needsRestart), NeedsRestartRole);
  }
}

void BasicSetting::callOnValueChanged(QObject* slotOwner, const char* slot) {
  QObject::connect(this, SIGNAL(valueChanged()), slotOwner, slot);
}

BasicSetting::BasicSetting(SettingsItem* settingItem, SettingsManager* mgr) {
  item = settingItem;
  settingsManager = mgr;
}

}  // End namespace Core
}  // End namespace Updraft

