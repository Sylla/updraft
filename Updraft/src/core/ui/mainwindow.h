#ifndef UPDRAFT_SRC_CORE_UI_MAINWINDOW_H_
#define UPDRAFT_SRC_CORE_UI_MAINWINDOW_H_

#include <QtGui>

#include "../../coreinterface.h"
#include "menu.h"
#include "tab.h"


namespace Ui { class MainWindow; }

namespace Updraft {

class PluginBase;

namespace Core {

class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget *parent = 0);
  ~MainWindow();

  Menu* getSystemMenu(SystemMenu menu);

  Tab* createTab(QWidget* content, QString title);

  Menu* createMenu(QString title);
  void removeMenu(Menu* menu);
  void setMapWidget(QWidget *widget);

 private slots:
  /// Called when tab in the bottom pane is closed using the little cross
  void tabClose(int index);

  /// Handles switchin an active tab in the bottom pane.
  void tabSwitch(int index);

 protected:
  // TODO(cestmir): Just a temporary method to test context menu
  void contextMenuEvent(QContextMenuEvent* event);

 private:
  Ui::MainWindow *ui;

  Menu* menuFile;
  Menu* menuEdit;
  Menu* menuTools;
  Menu* menuHelp;

  Menu* menuContext;

  Tab* activeTab;

  QSet<Menu*> customMenus;
};

}  // End namespace Core
}  // End namespace Updraft

#endif  // UPDRAFT_SRC_CORE_UI_MAINWINDOW_H_
