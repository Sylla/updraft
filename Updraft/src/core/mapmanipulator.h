#ifndef UPDRAFT_SRC_CORE_MAPMANIPULATOR_H_
#define UPDRAFT_SRC_CORE_MAPMANIPULATOR_H_

#include <osgEarthUtil/EarthManipulator>
#include <QObject>

namespace Updraft {

class SettingInterface;

namespace Core {

/// Earth manipulator supporting custom settings and keybindings.
class MapManipulator: public QObject, public osgEarth::Util::EarthManipulator {
  Q_OBJECT

 public:
  MapManipulator();
  ~MapManipulator();
  void resetNorth(double duration);
  void untilt(double duration);
  void updateCameraProjection();

 private slots:
  void mouseZoomSensitivityChanged();

 private:
  SettingInterface* mouseZoomSensitivity;
  void bindKeyboardEvents(Settings* settings);
  void bindMouseEvents(Settings* settings);
};

}  // End namespace Core
}  // End namespace Updraft

#endif  // UPDRAFT_SRC_CORE_MAPMANIPULATOR_H_
