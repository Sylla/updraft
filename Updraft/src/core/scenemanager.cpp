#include <osgQt/GraphicsWindowQt>
#include <osg/CoordinateSystemNode>
#include <osg/Vec3d>
#include <osgEarth/Viewpoint>
#include <osgEarthUtil/ObjectPlacer>
#include <osgEarth/Map>
#include <osgEarth/MapNode>
#include <osgEarthUtil/ElevationManager>
#include <string>

#include "scenemanager.h"
#include "mapmanipulator.h"
#include "pickhandler.h"

#include "updraft.h"
#include "settingsmanager.h"
#include "ui/mainwindow.h"
#include "ui/menu.h"
#include "../menuinterface.h"
#include "statesaver.h"

namespace Updraft {
namespace Core {

const float SceneManager::animDuration = 1.0;

SceneManager::SceneManager() {
  // Create a group for map settings
  updraft->settingsManager->addGroup(
    "map", tr("Map options"), GROUP_ADVANCED, ":/core/icons/map.png");

  homePositionSetting = updraft->settingsManager->addSetting(
    "state:homePos", "Home position", StateSaver::saveViewpoint(
    osgEarth::Util::Viewpoint(14.42046, 50.087811, 0, 0.0, -90.0, 15e5)),
    GROUP_HIDDEN);

  osg::DisplaySettings::instance()->setMinimumNumStencilBits(8);

  viewer = new osgViewer::Viewer();
  viewer->setThreadingModel(osgViewer::ViewerBase::SingleThreaded);

  // set graphic traits
  osg::GraphicsContext::Traits* traits = createGraphicsTraits(0, 0, 100, 100);
  // create window
  graphicsWindow = createGraphicsWindow(traits);
  // create camera
  camera = createCamera(traits);
  camera->setGraphicsContext(graphicsWindow);
  camera->setClearStencil(128);
  camera->setClearMask
    (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  viewer->setCamera(camera);

  createMapManagers();
  activeMapIndex = 0;
  elevationManager = createElevationManager();

  // create and set scene
  sceneRoot = new osg::Group();

  // create and add background
  background = new osg::ClearNode();
  background->setClearColor(osg::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
  sceneRoot->addChild(background);

  // add basic group for nodes
  simpleGroup = updraft->mainWindow->getInvisibleRootMapLayerGroup()->
    getNodeGroup();
  sceneRoot->addChild(simpleGroup);

  // add active map
  mapNode = mapManagers[activeMapIndex]->getMapNode();
  mapManagers[activeMapIndex]->attach(sceneRoot);
  // Make map node pickable
  registerOsgNode(mapNode, mapManagers[activeMapIndex]->getMapObject());

  viewer->setSceneData(sceneRoot);

  manipulator = new MapManipulator();
  mapManagers[activeMapIndex]->bindManipulator(manipulator);
  manipulator->setHomeViewpoint(getInitialPosition());

  viewer->setCameraManipulator(manipulator);

  // Create a picking handler
  viewer->addEventHandler(new PickHandler());
  viewer->addEventHandler(new osgViewer::StatsHandler);

  menuItems();
  mapLayerGroup();

  placer = new osgEarth::Util::ObjectPlacer(mapNode, 0, false);

  // start drawing
  timer = new QTimer(this);
  connect(timer, SIGNAL(timeout()), this, SLOT(redrawScene()));
  timer->start(20);
}

void SceneManager::saveHomePosition() {
  osgEarth::Viewpoint viewpoint = manipulator->getViewpoint();
  manipulator->setHomeViewpoint(viewpoint, animDuration);
  QByteArray saved = StateSaver::saveViewpoint(viewpoint);
  homePositionSetting->set(saved);
}

osgEarth::Util::Viewpoint SceneManager::getHomePosition() {
  return correctedViewpoint(StateSaver::restoreViewpoint(
    homePositionSetting->get().toByteArray()));
}

osgEarth::Util::Viewpoint SceneManager::getInitialPosition() {
  osgEarth::Util::Viewpoint start = getHomePosition();
  start.setRange(start.getRange() * 1.3);
  return start;
}

void SceneManager::menuItems() {
  Menu* viewMenu = updraft->mainWindow->getSystemMenu(MENU_VIEW);

  QAction* resetNorthAction = new QAction(tr("Rotate to &north"), this);
  resetNorthAction->setShortcut(QKeySequence(tr("Ctrl+n")));
  connect(resetNorthAction, SIGNAL(triggered()), this, SLOT(resetNorth()));
  viewMenu->insertAction(200, resetNorthAction);

  QAction* untiltAction = new QAction(tr("&Restore 2D view"), this);
  connect(untiltAction, SIGNAL(triggered()), this, SLOT(untilt()));
  viewMenu->insertAction(300, untiltAction);

  QAction* setHomePosAction = new QAction(tr("Set &home position"), this);
  viewMenu->insertAction(400, setHomePosAction);
  connect(setHomePosAction, SIGNAL(triggered()),
    this, SLOT(saveHomePosition()));
}

void SceneManager::mapLayerGroup() {
  MapLayerGroupInterface* mapLayerGroup = updraft->mainWindow->
    getInvisibleRootMapLayerGroup()->createMapLayerGroup(tr("Maps"));
  mapLayerGroup->setId("maps");
  mapLayerGroup->setCheckable(false);

  for (int i = 0; i < mapManagers.size(); i++) {
    MapManager *manager = mapManagers[i];

    MapLayerInterface* layer = mapLayerGroup->createMapLayerNoInsert(
      manager->getMapNode(), manager->getName());

    layer->connectSignalChecked(
      this, SLOT(checkedMap(bool, MapLayerInterface*)));

    layer->setChecked(i == activeMapIndex);

    layers.append(layer);
  }
}

SceneManager::~SceneManager() {
  // We should unregister all the registered osg nodes
  QList<osg::Node*> registeredNodes = pickingMap.keys();
  foreach(osg::Node* node, registeredNodes) {
    unregisterOsgNode(node);
  }

  foreach(MapManager *m, mapManagers) {
    delete m;
  }

  delete placer;
}

QWidget* SceneManager::getWidget() {
  if (graphicsWindow != NULL) {
    return graphicsWindow->getGLWidget();
  } else {
    return NULL;
  }
}

void SceneManager::startInitialAnimation() {
  osgEarth::Util::Viewpoint home = getHomePosition();
  // set home position for ACTION_HOME
  manipulator->setHomeViewpoint(home, animDuration);
  // go to home position now
  manipulator->setViewpoint(home, 2.0);
}

const osg::EllipsoidModel* SceneManager::getCurrentMapEllipsoid() {
  return manipulator->getSRS()->getEllipsoid();
}

void SceneManager::redrawScene() {
  // start initial animation in second frame to prevent jerks
  static int i = 0;
  if (i == 1) startInitialAnimation();
  if (i < 2) ++i;

  updateCameraProjection();

  viewer->frame();
}

void SceneManager::updateCameraProjection() {
  manipulator->updateCameraProjection();
}

bool SceneManager::removeGroup(osg::Group* group) {
  return sceneRoot->removeChild(group);
}

osgEarth::MapNode* SceneManager::getMapNode() {
  return mapNode;
}

osg::Group* SceneManager::getSimpleGroup() {
  return simpleGroup;
}

osgEarth::Util::ElevationManager* SceneManager::getElevationManager() {
  return elevationManager;
}

void SceneManager::registerOsgNode(osg::Node* node, MapObject* mapObject) {
  pickingMap.insert(node, mapObject);
}

void SceneManager::unregisterOsgNode(osg::Node* node) {
  pickingMap.remove(node);
}

osgEarth::Util::ElevationManager* SceneManager::createElevationManager() {
  for (int i = 0; i < mapManagers.size(); i++) {
    if (mapManagers[i]->hasElevation()) {
      return (new osgEarth::Util::ElevationManager(
        mapManagers[i]->getMap()));
    }
  }
    // if no map had the elevation specified
    // create a dummy elevation manager
  return (new osgEarth::Util::ElevationManager(
    mapManagers[activeMapIndex]->getMap()));
}

MapObject* SceneManager::getNodeMapObject(osg::Node* node) {
  QHash<osg::Node*, MapObject*>::iterator it = pickingMap.find(node);
  if (it != pickingMap.end()) {
    return it.value();
  } else {
    return NULL;
  }
}

void SceneManager::checkedMap(bool value, MapLayerInterface* object) {
  // find the map layer:
  int index = 0;
  for (index = 0; index < layers.size(); index++) {
    if (layers[index] == object) break;
  }
  if (index == activeMapIndex) {
      // cannot uncheck the active map
    if (value == false) {
      // force the chcekbox to be visible
      layers[index]->setChecked(true);
    }
  } else {
    // checked non active map
    // replace the map in the scene:
    if (value == true) {
      manipulator->getSettings()->setCameraProjection(
      osgEarth::Util::EarthManipulator::PROJ_PERSPECTIVE);
      viewer->frame();

      osgEarth::Util::Viewpoint viewpoint = manipulator->getViewpoint();

      int oldIndex = activeMapIndex;
      activeMapIndex = index;
      unregisterOsgNode(mapManagers[oldIndex]->getMapNode());
      mapManagers[oldIndex]->detach(sceneRoot);

      // set current map node
      mapNode = mapManagers[activeMapIndex]->getMapNode();
      mapManagers[activeMapIndex]->attach(sceneRoot);

      manipulator = new MapManipulator();

      mapManagers[activeMapIndex]->bindManipulator(manipulator);

      manipulator->setHomeViewpoint(correctedViewpoint(viewpoint));
      viewer->setCameraManipulator(manipulator);
      manipulator->setHomeViewpoint(getHomePosition(), animDuration);

      registerOsgNode(mapNode, mapManagers[activeMapIndex]->getMapObject());
      layers[oldIndex]->setChecked(false);
      layers[activeMapIndex]->setChecked(true);
    }
  }
}

MapManager* SceneManager::getMapManager() {
  return mapManagers[activeMapIndex];
}

osg::GraphicsContext::Traits* SceneManager::createGraphicsTraits
    (int x, int y, int w, int h, const std::string& name,
    bool windowDecoration) {
  osg::DisplaySettings* ds = osg::DisplaySettings::instance().get();
  osg::GraphicsContext::Traits* traits = new osg::GraphicsContext::Traits();
  traits->windowName = name;
  traits->windowDecoration = windowDecoration;
  traits->x = x;
  traits->y = y;
  traits->width = w;
  traits->height = h;
  traits->doubleBuffer = true;
  traits->alpha = ds->getMinimumNumAlphaBits();
  traits->stencil = ds->getMinimumNumStencilBits();
  traits->sampleBuffers = ds->getMultiSamples();
  traits->samples = ds->getNumMultiSamples();

  return traits;
}

osgQt::GraphicsWindowQt* SceneManager::createGraphicsWindow
    (osg::GraphicsContext::Traits* traits) {
  return new osgQt::GraphicsWindowQt(traits);
}

osg::Camera* SceneManager::createCamera(osg::GraphicsContext::Traits* traits) {
  osg::Camera* camera = new osg::Camera();

  camera->setClearColor(osg::Vec4(0.2, 0.2, 0.6, 1.0));
  camera->setViewport(new osg::Viewport(0, 0, traits->width, traits->height));

  camera->setProjectionMatrixAsPerspective(
    30.0f, getAspectRatio(),
    1.0f, 10000.0f);

  return camera;
}

double SceneManager::getAspectRatio() {
  const osg::GraphicsContext::Traits *traits = graphicsWindow->getTraits();
  double aspectRatio = static_cast<double>(traits->width)/
    static_cast<double>(traits->height);
  return aspectRatio;
}

void SceneManager::resetNorth() {
  manipulator->resetNorth(1);
}

void SceneManager::untilt() {
  manipulator->untilt(1);
}

void SceneManager::createMapManagers() {
  mapManagers.append(
    new MapManager("initial1.earth", tr("OpenStreetMaps")));
  mapManagers.append(
    new MapManager("initial2.earth", tr("ArcGIS, Satellite Imagery")));
  mapManagers.append(
    new MapManager("initial3.earth", tr("ArcGIS, Topographic Map")));
  mapManagers.append(
    new MapManager("initial4.earth", tr("Offline Map")));
}

void SceneManager::destroyMaps() {
  saveViewpoint = manipulator->getViewpoint();
  mapManagers[activeMapIndex]->detach(sceneRoot);
  manipulator->getSettings()->setCameraProjection(
  osgEarth::Util::EarthManipulator::PROJ_PERSPECTIVE);
  viewer->frame();
  for (int i = 0; i < mapManagers.size(); i++) {
    mapManagers[i]->destroyMap();
  }
}

void SceneManager::createMaps() {
  for (int i = 0; i < mapManagers.size(); i++) {
    mapManagers[i]->createMap();
  }
  manipulator->getSettings()->setCameraProjection(
  osgEarth::Util::EarthManipulator::PROJ_PERSPECTIVE);
  viewer->frame();
  mapManagers[activeMapIndex]->attach(sceneRoot);
  manipulator = new MapManipulator();
  manipulator->setHomeViewpoint(saveViewpoint);
  mapManagers[activeMapIndex]->bindManipulator(manipulator);
  viewer->setCameraManipulator(manipulator);
  manipulator->setHomeViewpoint(getHomePosition(), animDuration);
}

osgEarth::Util::ObjectPlacer* SceneManager::getObjectPlacer() {
  return placer;
}

osgEarth::Util::Viewpoint SceneManager::getViewpoint() {
  return manipulator->getViewpoint();
}

osgEarth::Util::Viewpoint SceneManager::correctedViewpoint(
  osgEarth::Util::Viewpoint viewpoint) {
  if (!getMapManager()->hasElevation()) {
    // If the current map doesn't have a elevation layer, then
    // there isn't anything to check.
    return viewpoint;
  }

  double elevation;
  double ignored;

  // First we check if the camera focus is above ground and
  // move it if it isn't.
  elevationManager->getElevation(viewpoint.x(), viewpoint.y(),
    0, 0, elevation, ignored);

  if (viewpoint.z() < elevation) {
    viewpoint.z() = elevation;
  }

  // Now comes the tricky part.
  // We need to check the camera position for terrain collisions,
  // but we only know azimuth, elevation and range from camera to focal point.
  //
  // We go around this by creating a transformation from
  // local coordinates (at the camera focal point)
  osg::Matrixd localToWorld;
  getCurrentMapEllipsoid()->
    computeLocalToWorldTransformFromLatLongHeight(
      osg::DegreesToRadians(viewpoint.y()),
      osg::DegreesToRadians(viewpoint.x()),
      viewpoint.z(),
      localToWorld);

  // Next  calculate the camera position in this local coordinate frame.
  double pitch = osg::DegreesToRadians(viewpoint.getPitch());
  double heading = osg::DegreesToRadians(viewpoint.getHeading());
  double range = viewpoint.getRange();

  osg::Vec3d localCameraPos;
  localCameraPos.z() = - sin(pitch) * range;
  double dist = cos(pitch) * range;  // horizontal distance between
    // focal point and camera
  localCameraPos.x() = - cos(-heading + osg::PI_2) * dist;
  localCameraPos.y() = - sin(-heading + osg::PI_2) * dist;

  // ... convert it to global coords (ECEF) ...
  osg::Vec3d globalCameraPos = localCameraPos * localToWorld;

  // ... to Lat/Lon ...
  double lat;
  double lon;
  double height;
  getCurrentMapEllipsoid()->
    convertXYZToLatLongHeight(
      globalCameraPos.x(),
      globalCameraPos.y(),
      globalCameraPos.z(),
      lat, lon, height);
  lat = osg::RadiansToDegrees(lat);
  lon = osg::RadiansToDegrees(lon);

  // Now we can query the elevation under the camera. Finally!
  elevationManager->getElevation(lon, lat, 0, 0, elevation, ignored);
  elevation += 10;  // camera should be at least 10 meters above terrain.

  if (height < elevation) {
    // For the conversion back to pitch and range we're taking a shortcut
    // by assuming flat earth. This won't cause any problems, because
    // if the camera is below terrain we know it's very close to earth
    // and earth curvatue is negligible at this scale.

    viewpoint.setRange(sqrt(dist * dist + elevation * elevation));
    viewpoint.setPitch(atan(elevation / dist));
  }

  return viewpoint;
}
void SceneManager::animateToViewpoint(
  const osgEarth::Util::Viewpoint& viewpoint) {
  manipulator->setViewpoint(viewpoint, animDuration);
}

}  // end namespace Core
}  // end namespace Updraft
