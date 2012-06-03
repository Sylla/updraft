#include "infotexts.h"

#include <QDebug>
#include "plotaxes.h"

namespace Updraft {
namespace IgcViewer {

const QPen PickedLabel::LABEL_PEN = QPen(Qt::white);

  // Picked Label:
Qt::Orientations PickedLabel::expandingDirections() const {
  return Qt::Horizontal;
}

QSize PickedLabel::maximumSize() const {
  return QSize(65536, 65536);
}

QSize PickedLabel::sizeHint() const {
  return QSize(100, 100);
}

QSize PickedLabel::minimumSize() const {
  return QSize(MIN_WIDTH, MIN_HEIGHT);
}

void PickedLabel::draw(QPainter *painter) {
  if (texts->empty()) return;
  if (pickedPositions->empty()) return;

  QFont font("Helvetica", 8);
  painter->setPen(LABEL_PEN);
  painter->setFont(font);

  QVector<int> space;
  int spos = pickedPositions->at(0);
  int epos = 0;
  for (int i = 0; i < pickedPositions->size()-1; i++) {
    epos = pickedPositions->at(i+1);
    space.append(epos - spos);
    spos = epos;
  }
  space.append(rect.right() - spos);

  spos = pickedPositions->at(0);
  for (int i = 0; i < pickedPositions->size()-1; i++) {
    epos = pickedPositions->at(i+1);
    int cspos = spos;
    spos = epos;
    if ((epos - cspos) < TEXT_WIDTH) {
      int res = (TEXT_WIDTH - (epos - cspos)) / 2 - SPACE;
      if (i < 1) continue;
      if (((space[i-1]-TEXT_WIDTH) / 2) < res) continue;
      if (((space[i+1]-TEXT_WIDTH) / 2) < res) continue;
    }
    int center = (epos - cspos)/2 + cspos;

    QRect textbox(QPoint(center-TEXT_WIDTH/2, rect.top()),
      QPoint(center+TEXT_WIDTH/2, rect.bottom()));
    painter->drawText(textbox, texts->at(i));
  }
}

// IGC Text Info Widget

Qt::Orientations IgcTextWidget::expandingDirections() const {
  return Qt::Vertical;
}

QSize IgcTextWidget::sizeHint() const {
  return QSize(100, 1000);
}

void IgcTextWidget::setMouseOverText(const QString& text) {
  mouseOverText = text;
  updateText();
}

void IgcTextWidget::setSegmentsTexts(QList<QString>* text) {
  segmentsTexts = text;
  updateText();
}

void IgcTextWidget::setPointsTexts(QList<QString>* text) {
  pointsTexts = text;
  updateText();
}

void IgcTextWidget::updateText() {
  QFont font("Helvetica", 8);
  setFont(font);
  QString string;
  QString s;
  if (!mouseOverText.isEmpty()) {
    s = mouseOverText;
    s.replace("\n", "<br>");
    string += "<b>" + s + "</b><br>";
    string += "-----------<br>";
  }
  for (int i = 0; i < segmentsTexts->size()-1; i++) {
    s = segmentsTexts->at(i);
    s.replace("\n", "<br>");
    string += "<i>" + s + "</i><br>";
    s = pointsTexts->at(i);
    s.replace("\n", "<br>");
    string += "<b>" + s + "</b><br>";
  }
  if (!segmentsTexts->isEmpty()) {
    s = segmentsTexts->last();
    s.replace("\n", "<br>");
    string += "<i>" + s + "</i>";
  }
  setHtml(string);
}
}
}
