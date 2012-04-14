#include "pluginmanager.h"

#include <QtGui>

#include "../pluginbase.h"
#include "coreimplementation.h"

namespace Updraft {
namespace Core {

/// Constructor loads static plugins.
PluginManager::PluginManager() {
  qDebug("Searching for plugins in plugin directory.");

  QDir plugins(QCoreApplication::applicationDirPath());
  if (!plugins.cd("plugins")) {
    qDebug("Could not find 'plugins' directory");
    return;
  }

  QStringList pluginDirs = plugins.entryList
    (QDir::Dirs | QDir::NoDotAndDotDot);

  QStringList dllsMask;
  dllsMask.push_back(QString("*.so"));
  dllsMask.push_back(QString("*.dll"));
  dllsMask.push_back(QString("*.dylib"));

  foreach(QString pluginDir, pluginDirs) {
    plugins.cd(pluginDir);
    QStringList dlls = plugins.entryList(dllsMask);

    // Search for the first loadable library.
    for (QList<QString>::iterator itDll = dlls.begin();
        itDll != dlls.end(); ++itDll) {
      if (load(plugins.absolutePath() + "/" + (*itDll)) != NULL) {
        break;
      }
    }
    plugins.cdUp();
  }
}

PluginManager::~PluginManager() {
  foreach(LoadedPlugin* p, plugins.values()) {
    p->plugin->deinitialize();
    delete p->plugin;
    delete p;
  }
}

/// Load the plugin, initialize it and put it to the list of
/// loaded plugins.
PluginBase* PluginManager::load(QString fileName) {
  qDebug("Loading plugin %s", fileName.toAscii().data());
  QPluginLoader loader(fileName);
  QObject *pluginInstance = loader.instance();
  if (pluginInstance == NULL)
    qDebug("Loading error report: %s ", loader.errorString().toAscii().data());

  return finishLoading(pluginInstance);
}

PluginBase* PluginManager::getPlugin(QString name) {
  LoadedPlugin* lp = plugins.value(name);
  if (!lp) return NULL;

  return lp->plugin;
}

QVector<PluginBase*> PluginManager::getAllPlugins() {
  QVector<PluginBase*> ret;

  foreach(LoadedPlugin *p, plugins.values()) {
    ret.append(p->plugin);
  }

  return ret;
}

/// Create core interface, initialize plugin and
/// place it in the list of loaded plugins.
/// Logs a debug message about the reason of failure.
/// \return Pointer to loaded plugin or NULL.
/// \param obj Loaded plugin before casting to PluginBase.
///   Can be NULL if loading failed (this will be logged).
PluginBase* PluginManager::finishLoading(QObject* obj) {
  if (!obj) {
    qDebug("Loading plugin failed.");
    return NULL;
  }

  LoadedPlugin* lp = new LoadedPlugin;
  PluginBase* plugin = qobject_cast<PluginBase*>(obj);

  if (!plugin) {
    qDebug("Plugin doesn't have correct interface.");
    return NULL;
  }

  unsigned apiVersion = plugin->getPluginApiVersion();

  if (apiVersion != PLUGIN_API_VERSION) {
    qDebug("Wrong version of plugin API (expected %d, got %d).",
      PLUGIN_API_VERSION, apiVersion);
    return NULL;
  }

  if (plugins.contains(plugin->getName())) {
    qDebug("Plugin named %s already loaded",
      plugin->getName().toAscii().data());
    return NULL;
  }

  lp->plugin = plugin;

  lp->plugin->setCoreInterface(new CoreImplementation(plugin));
  plugin->initialize();

  plugins.insert(plugin->getName(), lp);

  return plugin;
}

PluginManager::LoadedPlugin* PluginManager::findByPointer(PluginBase* pointer) {
  foreach(LoadedPlugin *p, plugins.values()) {
    if (p->plugin == pointer) {
      return p;
    }
  }

  return NULL;
}

}  // namespace Core
}  // namespace Updraft
