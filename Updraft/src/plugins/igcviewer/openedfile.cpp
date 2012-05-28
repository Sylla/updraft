#include "openedfile.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QSplitter>
#include <QVBoxLayout>
#include <QDebug>

#include <osg/Depth>
#include <osg/Geode>
#include <osg/LineWidth>
#include <osg/ShapeDrawable>
#include <osg/Vec3>
#include <osg/Texture2D>
#include <osgDB/ReadFile>
#include <osg/BlendFunc>

#include "pluginbase.h"
#include "igc/igc.h"

#include "plotwidget.h"

namespace Updraft {
namespace IgcViewer {

OpenedFile::~OpenedFile() {
  if (tab) {
    TabInterface *tmp = tab;
    tab = NULL;
    delete tmp;
  }

  foreach(IgcInfo* info, igcInfo) {
    delete info;
  }

  foreach(Coloring* coloring, colorings) {
    delete coloring;
  }

  viewer->mapLayerGroup->removeMapLayer(track);

  viewer->fileClose(this);
}

bool OpenedFile::loadIgc(const QString& filename) {
  Igc::IgcFile igc;

  if (!igc.load(filename)) {
    qDebug() << "Loading IGC file failed.";
    return false;
  }

  const Util::Ellipsoid* ellipsoid = g_core->getEllipsoid();

  fixList.clear();

  foreach(Igc::Event const* ev, igc.events()) {
    if (ev->type != Igc::Event::FIX) {
      continue;
    }
    Igc::Fix const* igcFix = static_cast<Igc::Fix const*>(ev);

    if (!igcFix->valid) {
      continue;
    }

    qreal x, y, z;

    ellipsoid->getOsgEllipsoidModel()->convertLatLongHeightToXYZ(
      igcFix->gpsLoc.lat_radians(), igcFix->gpsLoc.lon_radians(),
      igcFix->gpsLoc.alt,
      x, y, z);

    /// \todo fill terrain height
    fixList.append(TrackFix(ev->timestamp, igcFix->gpsLoc, x, y, z, 0));
  }

  return true;
}

bool OpenedFile::init(IgcViewer* viewer,
  const QString& filename, QColor color) {
  this->viewer = viewer;
  fileInfo = QFileInfo(filename);
  automaticColor = color;

  if (!loadIgc(filename)) {
    return false;
  }

  colorsCombo = new QComboBox();

  gradient = Util::Gradient(Qt::blue, Qt::red, true);

  #define ADD_IGCINFO(variable, pointer) \
    do { \
      igcInfo.append(pointer); \
      igcInfo[igcInfo.count() - 1]->init(&fixList); \
      variable = igcInfo[igcInfo.count() - 1]; \
    } while (0)

  ADD_IGCINFO(altitudeInfo, new AltitudeIgcInfo());
  ADD_IGCINFO(verticalSpeedInfo, new VerticalSpeedIgcInfo());
  ADD_IGCINFO(groundSpeedInfo, new GroundSpeedIgcInfo());
  ADD_IGCINFO(timeInfo, new TimeIgcInfo());

  trackData = new TrackData();
  trackData->init(&fixList);

  #define ADD_COLORING(name, pointer) \
    do { \
      colorings.append(pointer); \
      colorsCombo->addItem(name); \
    } while (0)

  ADD_COLORING(tr("Automatic"),
    new ConstantColoring(color));
  ADD_COLORING(tr("Vertical Speed"),
    new SymmetricColoring(verticalSpeedInfo, &gradient));
  ADD_COLORING(tr("Ground Speed"),
    new DefaultColoring(groundSpeedInfo, &gradient));
  ADD_COLORING(tr("Altitude"),
    new DefaultColoring(altitudeInfo, &gradient));
  ADD_COLORING(tr("Time"),
    new LocalColoring(timeInfo, &gradient));


  QWidget* tabWidget = new QWidget();
  QWidget* leftPart = new QWidget();
  QHBoxLayout* layout = new QHBoxLayout();
  // QSplitter* splitter = new QSplitter(tabWidget);
  QVBoxLayout* verticalLayout = new QVBoxLayout();

  layout->setContentsMargins(0, 0, 0, 0);

  plotWidget = new PlotWidget(
    trackData, altitudeInfo, verticalSpeedInfo, groundSpeedInfo);

  textBox = new IGCTextWidget(plotWidget->getSegmentsStatTexts(),
    plotWidget->getPointsStatTexts());
  textBox->setReadOnly(true);

  connect(plotWidget, SIGNAL(updateCurrentInfo(const QString&)),
    textBox, SLOT(setMouseOverText(const QString&)));
  connect(plotWidget, SIGNAL(updateText()),
    textBox, SLOT(updateText()));
  connect(plotWidget, SIGNAL(timeWasPicked(QTime)),
    this, SLOT(timePicked(QTime)));
  connect(plotWidget, SIGNAL(clearMarkers()),
    this, SLOT(clearMarkers()));

  tabWidget->setLayout(layout);
  verticalLayout->addWidget(colorsCombo, 0, Qt::AlignTop);
  verticalLayout->addWidget(textBox, 1, Qt::AlignTop);
  layout->addLayout(verticalLayout, 0);
  // leftPart->setLayout(verticalLayout);
  // splitter->addWidget(leftPart);
  // layout->add
  layout->addWidget(plotWidget, 1.0);
  // splitter->addWidget(plotWidget);

  tab = g_core->createTab(tabWidget, fileInfo.fileName());

  tab->connectSignalCloseRequested(this, SLOT(close()));
  connect(colorsCombo, SIGNAL(currentIndexChanged(int)),
    viewer, SLOT(coloringChanged(int)));

  // Sets automatic tab closing on request (without prompt).
  tab->connectCloseRequestToClose();

  createGroup();

  coloringChanged();

  return true;
}

void OpenedFile::redraw() {
  setColors(colorings[viewer->currentColoring]);
}

void OpenedFile::close() {
  if (!tab) {
    // If we were destroying this file starting from the regular consturctor,
    // then the tab is already set to null and we don't have to do anything.
    return;
  }

  tab = NULL;  // tab will be deleted after we drop out of this function
  delete this;
}

void OpenedFile::coloringChanged() {
  colorsCombo->setCurrentIndex(viewer->currentColoring);
  setColors(colorings[viewer->currentColoring]);
}

void OpenedFile::createGroup() {
  sceneRoot = new osg::Group();

  sceneRoot->addChild(createTrack());
  sceneRoot->addChild(createSkirt());
  trackPositionMarker = createMarker(25.);
  trackPositionMarker->setNodeMask(0xffffffff);

  // push the scene
  track = viewer->mapLayerGroup->insertMapLayer(sceneRoot, fileInfo.fileName());
  track->connectCheckedToVisibility();
}

osg::Node* OpenedFile::createTrack() {
  trackGeode = new osg::Geode();
  geom = new osg::Geometry();

  trackGeode->addDrawable(geom);

  osg::DrawArrays* drawArrayLines =
    new osg::DrawArrays(osg::PrimitiveSet::LINE_STRIP);
  geom->addPrimitiveSet(drawArrayLines);

  osg::Vec3Array* vertices = new osg::Vec3Array();
  geom->setVertexArray(vertices);
  geom->setColorBinding(osg::Geometry::BIND_PER_VERTEX);

  foreach(TrackFix fix, fixList) {
    vertices->push_back(osg::Vec3(fix.x, fix.y, fix.z));
  }

  drawArrayLines->setFirst(0);
  drawArrayLines->setCount(vertices->size());

  osg::StateSet* stateSet = trackGeode->getOrCreateStateSet();
  stateSet->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
  stateSet->setMode(GL_LINE_SMOOTH, osg::StateAttribute::ON);
  stateSet->setAttributeAndModes(new osg::LineWidth(3));
  // stateSet->setAttributeAndModes(
  //   new osg::LineWidth(viewer->lineWidthSetting->get().toFloat()));

  return trackGeode;
}

osg::Node* OpenedFile::createSkirt() {
  osg::Geode *geode = new osg::Geode();
  osg::Geometry *skirtGeom = new osg::Geometry();

  geode->addDrawable(skirtGeom);

  osg::DrawArrays* drawArray =
    new osg::DrawArrays(osg::PrimitiveSet::TRIANGLE_STRIP);
  skirtGeom->addPrimitiveSet(drawArray);

  osg::Vec3Array* vertices = new osg::Vec3Array();
  skirtGeom->setVertexArray(vertices);

  skirtGeom->setColorBinding(osg::Geometry::BIND_OVERALL);

  const osg::EllipsoidModel* ellipsoid =
    g_core->getEllipsoid()->getOsgEllipsoidModel();
  foreach(TrackFix fix, fixList) {
    double x, y, z;
    ellipsoid->convertLatLongHeightToXYZ(
      fix.location.lat_radians(), fix.location.lon_radians(),
      0,
      x, y, z);
    vertices->push_back(osg::Vec3(fix.x, fix.y, fix.z));
    vertices->push_back(osg::Vec3(x, y, z));
  }

  drawArray->setFirst(0);
  drawArray->setCount(vertices->size());

  osg::Vec4Array* color = new osg::Vec4Array();
  color->push_back(osg::Vec4(0.5, 0.5, 0.5, 0.5));
  skirtGeom->setColorArray(color);

  osg::StateSet* stateSet = geode->getOrCreateStateSet();
  stateSet->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
  stateSet->setMode(GL_BLEND, osg::StateAttribute::ON);
  stateSet->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
  stateSet->setMode(GL_DEPTH_TEST, osg::StateAttribute::ON);

  osg::Depth* depth = new osg::Depth;
  depth->setWriteMask(false);
  stateSet->setAttributeAndModes(depth, osg::StateAttribute::ON);

  stateSet->setMode(GL_LIGHTING, osg::StateAttribute::OFF);

  return geode;
}

void OpenedFile::setColors(Coloring *coloring) {
  currentColoring = coloring;

  osg::Vec4Array* colors = new osg::Vec4Array();

  for (int i = 0; i < fixList.count(); ++i) {
    QColor color = coloring->color(i);
    colors->push_back(osg::Vec4(
      color.redF(), color.greenF(), color.blueF(), color.alphaF()));
  }

  geom->setColorArray(colors);
}

void OpenedFile::updateScales(const OpenedFile *other) {
  for (int i = 0; i < igcInfo.count(); ++i) {
    igcInfo[i]->addGlobalScale(other->igcInfo[i]);
  }
}

void OpenedFile::selectTab() {
  tab->select();
}

void OpenedFile::resetScales() {
  for (int i = 0; i < igcInfo.count(); ++i) {
    igcInfo[i]->resetGlobalScale();
  }
}

QString OpenedFile::fileName() {
  return fileInfo.absoluteFilePath();
}

osg::Node* OpenedFile::getNode() {
  return trackGeode;
}

void OpenedFile::trackClicked(const EventInfo* eventInfo) {
    // find nearest fix:
  if (fixList.empty()) return;
    // index of nearest trackFix
  QList<TrackFix>::iterator nearest = fixList.begin();
  float dx = nearest->x - eventInfo->intersection.x();
  float dy = nearest->y - eventInfo->intersection.y();
  float dz = nearest->z - eventInfo->intersection.z();
  float distance = dx*dx + dy*dy + dz*dz;
  float minDistance = distance;

  for (QList<TrackFix>::iterator it = fixList.begin();
    it != fixList.end(); it++) {
    dx = it->x - eventInfo->intersection.x();
    dy = it->y - eventInfo->intersection.y();
    dz = it->z - eventInfo->intersection.z();
    distance = dx*dx + dy*dy + dz*dz;
    if (distance < minDistance) {
      minDistance = distance;
      nearest = it;
    }
  }

  qDebug() << minDistance;

  QTime time = nearest->timestamp;
  plotWidget->addPickedTime(time);

  osg::AutoTransform* currentMarkerTransform = new osg::AutoTransform();
  currentMarkerTransform->setAutoRotateMode(
    osg::AutoTransform::ROTATE_TO_SCREEN);
  currentMarkerTransform->setAutoScaleToScreen(true);
  currentMarkerTransform->setMinimumScale(0.1);
  currentMarkerTransform->setMaximumScale(100);
  currentMarkerTransform->addChild(trackPositionMarker);
  sceneRoot->addChild(currentMarkerTransform);
  currentMarkerTransform->setPosition(
    osg::Vec3(nearest->x, nearest->y, nearest->z));
  currentMarkers.append(currentMarkerTransform);
}

void OpenedFile::timePicked(QTime time) {
    // find fix with nearest time:
  if (fixList.empty()) return;
    // index of nearest trackFix

  QList<TrackFix>::iterator nearest = fixList.begin();
  float distance = qAbs(time.secsTo(nearest->timestamp));
  float minDistance = distance;

  for (QList<TrackFix>::iterator it = fixList.begin();
    it != fixList.end(); it++) {
    distance = qAbs(time.secsTo(it->timestamp));
    if (distance < minDistance) {
      minDistance = distance;
      nearest = it;
    }
  }

    // do the transformation of the marker to the position
    // of the nearest trackFix.
  // create a new transform node:
  osg::AutoTransform* currentMarkerTransform = new osg::AutoTransform();
  currentMarkerTransform->setAutoRotateMode(
    osg::AutoTransform::ROTATE_TO_SCREEN);
  currentMarkerTransform->setAutoScaleToScreen(true);
  currentMarkerTransform->setMinimumScale(0.1);
  currentMarkerTransform->setMaximumScale(100);
  currentMarkerTransform->addChild(trackPositionMarker);
  sceneRoot->addChild(currentMarkerTransform);
  currentMarkerTransform->setPosition(
    osg::Vec3(nearest->x, nearest->y, nearest->z));
  currentMarkers.append(currentMarkerTransform);
}

osg::Geode* OpenedFile::createMarker(qreal scale) {
  osg::Geometry* geometry = new osg::Geometry();

  osg::Vec3Array* vertices = new osg::Vec3Array(4);
  (*vertices)[0] = osg::Vec3(-scale, -scale, 0.0);
  (*vertices)[1] = osg::Vec3( scale, -scale, 0.0);
  (*vertices)[2] = osg::Vec3( scale, scale, 0.0);
  (*vertices)[3] = osg::Vec3(-scale, scale, 0.0);
  geometry->setVertexArray(vertices);

  osg::Vec2Array* texCoords = new osg::Vec2Array(4);
  (*texCoords)[0] = osg::Vec2(0.0, 0.0);
  (*texCoords)[1] = osg::Vec2(1.0, 0.0);
  (*texCoords)[2] = osg::Vec2(1.0, 1.0);
  (*texCoords)[3] = osg::Vec2(0.0, 1.0);
  geometry->setTexCoordArray(0, texCoords);

  geometry->addPrimitiveSet(new osg::DrawArrays(
    osg::PrimitiveSet::QUADS, 0, 4));

  osg::Geode* geode = new osg::Geode();

  osg::StateSet* stateSet = geode->getOrCreateStateSet();
  QString path = QCoreApplication::applicationDirPath()
    + "/data/igcmarker.png";
  osg::Image* image = osgDB::readImageFile(path.toStdString());
  osg::Texture2D* texture = new osg::Texture2D();
  texture->setImage(image);

  // Turn off lighting for the billboard.
  stateSet->setMode(GL_LIGHTING, osg::StateAttribute::OFF);

  // Turn on texturing, bind texture.
  stateSet->setTextureAttributeAndModes(0, texture);

  // Turn on blending.
  stateSet->setAttributeAndModes(new osg::BlendFunc());
  stateSet->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);

  geode->addDrawable(geometry);

  return geode;
}

void OpenedFile::clearMarkers() {
  for (int i = 0; i < currentMarkers.size(); i++) {
    sceneRoot->removeChild(currentMarkers[i]);
  }
  currentMarkers.clear();
}

}  // End namespace IgcViewer
}  // End namespace Updraft

