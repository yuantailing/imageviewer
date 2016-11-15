#include <QStringList>
#include <QDebug>
#include "imageannotation.h"

/// Perspective Helper

PerspectiveHelper::PerspectiveHelper() {
    numPoint = 0;
    stroke = QLineF(QPointF(0, 0), QPointF(0, 0));
    toolSwitched = false;
    stroking = false;
    singleCharacter = false;
    textDirection = DIRECTION_AUTO;
}

void PerspectiveHelper::onStartPoint(QPointF p, bool regular, BlockAnnotation *block) {
    setPoint(p, regular, block, false);
}

void PerspectiveHelper::onPendingPoint(QPointF p, bool regular, BlockAnnotation *block) {
    setPoint(p, regular, block, true);
}

void PerspectiveHelper::setPoint(QPointF p, bool regular, BlockAnnotation *block, bool pending) {
    if (numPoint == 0) {
        points[0] = points[1] = points[2] = points[3] = p;
        if (!pending)
            numPoint++;
    } else if (numPoint == 1) {
        if (regular) {
            points[3] = p;
            points[1] = QPointF(points[3].x(), points[0].y());
            points[2] = QPointF(points[0].x(), points[3].y());
            if (!pending)
                numPoint += 3;
        } else {
            points[1] = points[2] = points[3] = p;
            if (!pending)
                numPoint++;
        }
    } else if (numPoint == 2) {
        if (regular) {
            points[3] = p;
            points[2] = points[3] - points[1] + points[0];
            if (!pending)
                numPoint += 2;
        } else {
            points[2] = points[3] = p;
            if (!pending)
                numPoint++;
        }
    } else if (numPoint == 3) {
        points[3] = p;
        if (!pending) {
            numPoint++;
            if (singleCharacter) {
                addNewCharacterBoxToBlock(poly(), block);
                numPoint = 0;
            }
        }
    } else {
        Q_ASSERT(numPoint == 4);
        if (stroking == false) {
            stroke.setP1(p);
            stroke.setP2(p);
            if (!pending)
                stroking = true;
        } else {
            stroke.setP2(p);
            if (!pending) {
                if (textDirection == DIRECTION_AUTO)
                    textDirection = detectTextDirection(stroke);
                QVector<QPolygonF> characterPoly = getPendingCharacterPoly();
                if (!characterPoly.empty())
                    addNewCharacterBoxToBlock(characterPoly.first(), block);
                stroking = false;
            }
        }
    }
}

void PerspectiveHelper::onSwitchTool(BlockAnnotation *) {
    toolSwitched = !toolSwitched;
}

bool PerspectiveHelper::onEnterPressed(BlockAnnotation *block) {
    if (numPoint < 4)
        return false;
    if (stroking) {
        onStartPoint(stroke.p2(), false, block);
        onEndPoint(stroke.p2(), false, block);
    }
    if (block->characters.size() > 0)
        return false;
    singleCharacter = true;
    addNewCharacterBoxToBlock(poly(), block);
    numPoint = 0;
    return true;
}

QVector<QPolygonF> PerspectiveHelper::getHelperPoly() const {
    if (numPoint > 0)
        return QVector<QPolygonF>({poly()});
    else
        return QVector<QPolygonF>();
}

QVector<QPolygonF> PerspectiveHelper::getPendingCharacterPoly() const {
    if (!stroking)
        return QVector<QPolygonF>();
    QPolygonF bound = poly();
    TextDirection textDirection = this->textDirection;
    if (textDirection == DIRECTION_AUTO)
        textDirection = detectTextDirection(stroke);
    if (textDirection == DIRECTION_BY_BASE || textDirection == DIRECTION_TO_BASE) {
        QLineF left, right;
        if (textDirection == DIRECTION_BY_BASE) {
            left = QLineF(bound[0], bound[3]);
            right = QLineF(bound[1], bound[2]);
        } else {
            left = QLineF(bound[3], bound[2]);
            right = QLineF(bound[0], bound[1]);
        }
        QLineF bottom(left.p1(), right.p1()), up(left.p2(), right.p2());
        if (!toolSwitched && isHorizontalText(stroke)) {
            left.setP2(left.p1() + QPointF(0.0, 1.0));
            right.setP2(right.p1() + QPointF(0.0, 1.0));
        }
        QPointF intersectPoint;
        QLineF::IntersectType intersectType = left.intersect(right, &intersectPoint);
        QLineF l1, l2;
        if (intersectType == QLineF::BoundedIntersection || intersectType == QLineF::UnboundedIntersection) {
            l1 = QLineF(intersectPoint, stroke.p1());
            l2 = QLineF(intersectPoint, stroke.p2());
        } else {
            QPointF delta = left.p2() - left.p1() + right.p2() - right.p1();
            l1 = QLineF(stroke.p1(), stroke.p1() + delta);
            l2 = QLineF(stroke.p2(), stroke.p2() + delta);
        }
        QPointF p1, p2, p3, p4;
        l1.intersect(bottom, &p1);
        l2.intersect(bottom, &p2);
        l2.intersect(up, &p3);
        l1.intersect(up, &p4);
        return {QPolygonF({p1, p2, p3, p4})};
    }
    Q_ASSERT(false);
    return QVector<QPolygonF>();
}

QString PerspectiveHelper::getTips() const {
    QString tips("");
    if (numPoint > 0 && numPoint < 4) {
        tips +=  QObject::tr("已绘制%1/%2个点 ").arg(numPoint).arg(4);
        if (numPoint == 1)
            tips += QObject::tr("按住Shift添加矩形约束 ");
        else if (numPoint == 2)
            tips += QObject::tr("按住Shift添加平行四边形约束 ");
    }
    if (numPoint == 4 && !toolSwitched) {
        if (isHorizontalText(stroke))
            tips += QObject::tr("横排文字自动锁定竖直边，按Tab可解除锁定 ");
    }
    return tips;
}

QPolygonF PerspectiveHelper::poly() const {
    QLineF base(points[0], points[1]);
    QLineF top(points[2], points[3]);
    QLineF left(points[0], points[2]);
    QLineF right(points[1], points[3]);
    QPointF intersect;
    if (base.intersect(top, &intersect) == QLineF::BoundedIntersection)
        return QPolygonF({points[0], points[2], points[1], points[3]});
    else if (left.intersect(right, &intersect) == QLineF::BoundedIntersection)
        return QPolygonF({points[0], points[1], points[2], points[3]});
    else
        return QPolygonF({points[0], points[1], points[3], points[2]});

}

PerspectiveHelper::TextDirection PerspectiveHelper::detectTextDirection(QLineF stroke) const {
    if (stroke.p1() == stroke.p2())
        stroke.setP2(stroke.p1() + QPointF(1, 0));
    QPolygonF bound = poly();
    QLineF horiDir = QLineF((bound[0] + bound[3]) / 2, (bound[1] + bound[2]) / 2).unitVector();
    QLineF vertDir = QLineF((bound[0] + bound[1]) / 2, (bound[2] + bound[3]) / 2).unitVector();
    if (qAbs(stroke.dx() * horiDir.dx() + stroke.dy() * horiDir.dy()) * 1.2 >=
            qAbs(stroke.dx() * vertDir.dx() + stroke.dy() * vertDir.dy()))
        return DIRECTION_BY_BASE;
    else
        return DIRECTION_TO_BASE;
}

bool PerspectiveHelper::isHorizontalText(QLineF stroke) const {
    if (numPoint < 4)
        return false;
    QPolygonF bound = poly();
    TextDirection textDirection = this->textDirection;
    if (textDirection == DIRECTION_AUTO)
        textDirection = detectTextDirection(stroke);
    if (textDirection == DIRECTION_BY_BASE || textDirection == DIRECTION_TO_BASE) {
        QLineF left, right;
        if (textDirection == DIRECTION_BY_BASE) {
            left = QLineF(bound[0], bound[3]);
            right = QLineF(bound[1], bound[2]);
        } else {
            left = QLineF(bound[3], bound[2]);
            right = QLineF(bound[0], bound[1]);
        }
        if (qMax(qAbs(left.unitVector().dx()), qAbs(right.unitVector().dx())) < 0.3)
            return true;
        else
            return false;
    }
    Q_ASSERT(false);
    return false;
}

void PerspectiveHelper::addNewCharacterBoxToBlock(QPolygonF poly, BlockAnnotation *block) {
    CharacterAnnotation charAnno;
    charAnno.box = poly;
    charAnno.text = QString("");
    block->characters.push_back(charAnno);
}

/// Block Annotation

bool BlockAnnotation::isStringOk() const {
    foreach (CharacterAnnotation const &charAnno, characters)
        if (charAnno.text.isEmpty())
            return false;
    return true;
}

void BlockAnnotation::onInputString(QString const &s) {
    QStringList list = s.split(" ", QString::SkipEmptyParts);
    if (list.isEmpty()) {
        // do nothing
    } else if (list.size() == 1 && characters.size() != 1) {
        QString const &trimed = list.first();
        int n = qMin(trimed.length(), characters.size());
        for (int i = 0; i < n; i++)
            characters[i].text = trimed.mid(i, 1);
        for (int i = n; i < characters.size(); i++)
            characters[i].text.clear();
    } else {
        int n = qMin(list.size(), characters.size());
        for (int i = 0; i < n; i++)
            characters[i].text = list[i];
        for (int i = n; i < characters.size(); i++)
            characters[i].text.clear();
    }
}
