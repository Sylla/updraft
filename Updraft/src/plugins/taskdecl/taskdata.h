#ifndef UPDRAFT_SRC_PLUGINS_TASKDECL_TASKDATA_H_
#define UPDRAFT_SRC_PLUGINS_TASKDECL_TASKDATA_H_

#include <QVector>

namespace Updraft {

class DataHistory;
class TaskPoint;

/// This class encapsulates all data related to one task.
/// It can be used only with DataHistory and TaskFile.
/// These three classes together form a Memento design pattern.
class TaskData {
 public:
  virtual ~TaskData();

  /// \return Xml text representation of TaskData
  QString toXml() const;

  /// Find and return TaskPoint on specified position.
  /// This function serves for both reading and editing of TaskPoint.
  /// \param position position of TaskPoint in sequence
  /// \return Pointer to a task point on desired position
  /// If position is out of range, it returns NULL.
  TaskPoint* getTaskPoint(int position);

  /// Same as non-const getTaskPoint()
  const TaskPoint* getTaskPoint(int position) const;

  /// Tries to parse xml file. If it fails, TaskData is empty.
  /// \param serialized string with xml file content
  /// \return True on success, otherwise false.
  bool fromXml(const QString &serialized);

  /// Inserts new TaskPoint. All TaskPoints are deleted in ~TaskData.
  /// \param position target position of TaskPoint in sequence
  /// \param taskPoint The task point to be inserted
  /// If position is invalid, TaskPoint is appended to end of sequence.
  bool insertTaskPoint(TaskPoint *taskPoint, int position);

  /// Changes position of TaskPoint in sequence.
  /// \param from position of desired TaskPoint
  /// \param to destination position
  /// If "to" is invalid position, TaskPoint is moved to end of sequence.
  void moveTaskPoint(int from, int to);

  /// Deletes TaskPoint on desired position
  /// \param position position of target TaskPoint
  void deleteTaskPoint(int position);

  /// Return number of task points in this declaration
  int size() const {
    return taskPoints.size();
  }

  /// Set the information about the checked add TaskPoint button
  /// \param position new position of the checked TaskPoint button
  void setAddTaskPointButton(int position);

  /// Get the currently checked add TaskPoint button
  int getAddTaskPointButton() const;

  /// Return length between all turn points in meters.
  qreal totalDistance() const;

  /// Return the FAI official length of the track.
  qreal officialDistance() const;

  /// Return true if the task is a closed course.
  /// This function accepts a endpoints located at most 500 m from start points.
  /// This is a bit broader definition than FAI rules has,
  /// but shouldn't be too much trouble.
  bool isClosed() const;

  /// Return true if the task is a valid FAI triangle.
  bool isFaiTriangle() const;

  /// Returns shortest distance from turn point i to turn point j.
  /// Doesn't do any checks on the values i,j.
  /// \return Length of shortest path from i-th to j-th turn point
  qreal distance(int i, int j) const;

  /// Returns shortest distance from turn point i to turn point j
  /// and optionally azimuths i to j and j to i.
  /// Doesn't do any checks on the values i,j.
  /// \param i Index of the first turn-point
  /// \param j Index of the second turn-point
  /// \param [out] azimuth azimuth from point i to j
  /// \return Length of shortest path from i-th to j-th turn point
  qreal distanceAzimuth(int i, int j, qreal *azimuth) const;

 private:
  /// Only DataHistory objects can create TaskData.
  TaskData() {}

  /// Return the distance reduction of this task point caused by
  /// the use of FAI cylinders.
  /// This reduction must be used twice for task points inside the track
  /// (that are entered and left).
  qreal distanceReduction(int i) const;

  /// Calculate official distance, or -1 if the task
  /// is not a FAI triangle
  float faiTriangleDistance() const;

  /// Only DataHistory objects can make copies of TaskData.
  TaskData(const TaskData& taskData);

  typedef QVector<TaskPoint*> PointsContainer;

  /// Ordered sequence of TaskPoints
  PointsContainer taskPoints;

  /// Selected add TaskPoint button in the gui.
  /// If it is -1, it means that no button is selected
  int selectedAddButton;

  friend class DataHistory;
};

}  // End namespace Updraft

#endif  // UPDRAFT_SRC_PLUGINS_TASKDECL_TASKDATA_H_
