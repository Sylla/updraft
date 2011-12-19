#include "plugin.h"

#include <QPushButton>
#include <QHBoxLayout>

#include "../../coreinterface.h"
#include "../../core/ui/maplayergroup.h"

namespace Updraft {

TestPlugin::TestPlugin() {}

TestPlugin::~TestPlugin() {
  delete mapLayerGroup;
}

QString TestPlugin::getName() {
  return "testplugin";
}

unsigned TestPlugin::getPriority() {
  return 1;
}

void TestPlugin::initialize() {
  // Help about this plugin
  helpAction = new QAction("About testplugin...", this);
  connect(helpAction, SIGNAL(triggered()), this, SLOT(showHelp()));

  helpAction2 = new QAction("About testplugin (higher priority)...", this);
  connect(helpAction2, SIGNAL(triggered()), this, SLOT(showHelp()));

  core->getSystemMenu(MENU_HELP)->insertAction(1, helpAction);
  core->getSystemMenu(MENU_HELP)->insertAction(0, helpAction2);

  // Create a new menu and insert a test action into it as well
  myMenu = core->createMenu("Test plugin");

  helpAction3 = new QAction("About testplugin...", this);
  connect(helpAction3, SIGNAL(triggered()), this, SLOT(showHelp()));

  myMenu->insertAction(0, helpAction3);

  // Add an action into a context menu
  core->getSystemMenu(MENU_CONTEXT)->insertAction(0, helpAction);

  createTab("Button tab 1");
  createTab("Button tab 2");

  // Create map layers items in the left pane.
  mapLayerGroup = core->createMapLayerGroup("Test group");
  if (mapLayerGroup != NULL) {
    mapLayerGroup->insertMapLayer(0, (osg::Node*)0, "Layer A");
    mapLayerGroup->insertMapLayer(1, (osg::Node*)1, "Layer B");

    // Connect display/hide signals with slots.
    connect(mapLayerGroup, SIGNAL(mapLayerDisplayed(osg::Node*)),
            this, SLOT(mapLayerDisplayed(osg::Node*)));
    connect(mapLayerGroup, SIGNAL(mapLayerHidden(osg::Node*)),
            this, SLOT(mapLayerHidden(osg::Node*)));
  }

  qDebug("testplugin loaded");
}

void TestPlugin::createTab(QString title) {
  QWidget* container = new QWidget();
  QHBoxLayout* layout = new QHBoxLayout(container);

  QPushButton* btn1 = new QPushButton("Show help", container);
  QPushButton* btn2 = new QPushButton("Close this tab", container);

  layout->addWidget(btn1);
  layout->addWidget(btn2);

  TabInterface* tab = core->createTab(container, title);

  connect(btn1, SIGNAL(clicked()), this, SLOT(showHelp()));
  // connect(btn2, SIGNAL(clicked()), tab, SLOT(showHelp()));
  tab->connectSlotClose(btn2, SIGNAL(clicked()));
}

void TestPlugin::deinitialize() {
  qDebug("testplugin unloaded");
}

void TestPlugin::showHelp() {
  QMainWindow* win = core->getMainWindow();

  QMessageBox::information(win, "About testplugin",
    "Testplugin is just a test plugin to see whether our api is working");
}

void TestPlugin::mapLayerDisplayed(osg::Node* layer) {
  qDebug("testplugin: Map layer displayed.");
}

void TestPlugin::mapLayerHidden(osg::Node* layer) {
  qDebug("testplugin: Map layer hidden.");
}

Q_EXPORT_PLUGIN2(testplugin, TestPlugin)

}  // End namespace Updraft

