#include <QDebug>
#include "imageannotation.h"

/// Perspective Helper

PerspectiveHelper::PerspectiveHelper() {
    numPoint = 0;
    toolSwitched = false;
    stroking = false;
    singleCharacter = false;
    textDirection = DIRECTION_AUTO;
}

void PerspectiveHelper::onStartPoint(QPointF p, BlockAnnotation *block) {
    if (numPoint == 0) {
        base.setP1(p);
        base.setP2(p);
        top.setP1(p);
        top.setP2(p);
        numPoint++;
    } else if (numPoint == 1) {
        base.setP2(p);
        numPoint++;
    } else if (numPoint == 2) {
        top.setP1(p);
        top.setP2(p);
        numPoint++;
    } else if (numPoint == 3) {
        top.setP2(p);
        numPoint++;
        if (singleCharacter) {
            makePolyToCharacterBox(block);
            numPoint = 0;
        }
    } else {
        Q_ASSERT(numPoint == 4);
        if (stroking == false) {
            stroking = true;
            stroke.setP1(p);
            stroke.setP2(p);
        } else {
            stroke.setP2(p);
            if (textDirection == DIRECTION_AUTO)
                textDirection = detectTextDirection(stroke);
            QVector<QPolygonF> characterPoly = getPendingCharacterPoly();
            if (!characterPoly.empty()) {
                CharacterAnnotation charAnno;
                charAnno.box = characterPoly.first();
                charAnno.text = QString("");
                block->characters.push_back(charAnno);
            }
            stroking = false;
        }
    }
}

void PerspectiveHelper::onPendingPoint(QPointF p, BlockAnnotation *) {
    if (numPoint == 1) {
        base.setP2(p);
    } else if (numPoint == 2) {
        top.setP1(p);
        top.setP2(p);
    } else if (numPoint == 3) {
        top.setP2(p);
    } else {
        Q_ASSERT(numPoint == 4);
        stroke.setP2(p);
    }
}

void PerspectiveHelper::onSwitchTool(BlockAnnotation *block) {
    toolSwitched = !toolSwitched;
    block->characters.clear();
}

bool PerspectiveHelper::onEnterPressed(BlockAnnotation *block) {
    if (numPoint < 4 || block->characters.size() > 0)
        return false;
    singleCharacter = true;
    makePolyToCharacterBox(block);
    return true;
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
    if (numPoint > 0 && numPoint < 4)
        return QObject::tr("已绘制%1/%2个点").arg(numPoint).arg(4);
    else if (numPoint == 4 && !toolSwitched &&
             (textDirection == DIRECTION_BY_BASE || textDirection == DIRECTION_TO_BASE || stroking)) {
        if (isHorizontalText(stroke))
            return QObject::tr("横排文字自动锁定竖直边，按Tab可解除锁定");
    }
    return QString("");
}

QPolygonF PerspectiveHelper::poly() const {
    QLineF left(base.p1(), top.p1());
    QLineF right(base.p2(), top.p2());
    QPointF intersect;
    if (left.intersect(right, &intersect) == QLineF::BoundedIntersection)
        return QPolygonF({base.p1(), base.p2(), top.p1(), top.p2()});
    else
        return QPolygonF({base.p1(), base.p2(), top.p2(), top.p1()});
}

PerspectiveHelper::TextDirection PerspectiveHelper::detectTextDirection(QLineF stroke) const {
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

void PerspectiveHelper::makePolyToCharacterBox(BlockAnnotation *block) {
    CharacterAnnotation charAnno;
    charAnno.box = poly();
    charAnno.text = QString("");
    block->characters.push_back(charAnno);
    numPoint = 0;
}

/// Block Annotation

bool BlockAnnotation::isStringOk() const {
    foreach (CharacterAnnotation const &charAnno, characters)
        if (charAnno.text.isEmpty())
            return false;
    return true;
}

void BlockAnnotation::onInputString(QString const &s) {
    QString trimed = s;
    trimed.replace(" ", "").replace("\t", "").replace("　", "");
    int n = qMin(trimed.length(), characters.size());
    for (int i = 0; i < n; i++)
        characters[i].text = trimed.mid(i, 1);
}
