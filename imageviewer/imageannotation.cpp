#include <QDebug>
#include "imageannotation.h"

void PerspectiveHelper::onStartPoint(QPointF p, BlockAnnotation *) {
    if (numPoint == 0) {
        base.setP1(p);
        base.setP2(p);
        numPoint++;
    } else if (numPoint == 2) {
        top.setP1(p);
        top.setP2(p);
        numPoint++;
    } else {
        Q_ASSERT(numPoint == 4);
        stroking = true;
        stroke.setP1(p);
        stroke.setP2(p);
    }
}

void PerspectiveHelper::onPendingPoint(QPointF p, BlockAnnotation *) {
    if (numPoint == 1) {
        base.setP2(p);
    } else if (numPoint == 3) {
        top.setP2(p);
    } else {
        Q_ASSERT(numPoint == 4);
        stroke.setP2(p);
    }
}

void PerspectiveHelper::onEndPoint(QPointF p, BlockAnnotation *block) {
    if (numPoint == 1) {
        base.setP2(p);
        numPoint++;
    } else if (numPoint == 3) {
        top.setP2(p);
        numPoint++;
    } else {
        Q_ASSERT(numPoint == 4);
        stroke.setP2(p);
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

QVector<QPolygonF> PerspectiveHelper::getHelperPoly() const {
    QVector<QPolygonF> v;
    if (numPoint > 0) {
        QPolygonF poly;
        poly.append(base.p1());
        poly.append(base.p2());
        if (numPoint > 2) {
            if (isIntersect(QLineF(base.p1(), top.p1()), QLineF(base.p2(), top.p2()))) {
                poly.append(top.p1());
                poly.append(top.p2());
            } else {
                poly.append(top.p2());
                poly.append(top.p1());
            }
        }
        v.push_back(poly);
    }
    return v;
}

QVector<QPolygonF> PerspectiveHelper::getPendingCharacterPoly() const {
    QVector<QPolygonF> v;
    auto lineInterpolation = [](QLineF line, qreal x) {
        return line.p1().y() + (line.p2().y() - line.p1().y()) *
                (x - line.p1().x()) / (line.p2().x() - line.p1().x());
    };
    if (stroking) {
        if (base.p1().x() != base.p2().x() && top.p1().x() != top.p2().x()) {
            QPolygonF poly;
            qreal x1 = qMin(stroke.p1().x(), stroke.p2().x());
            qreal x2 = qMax(stroke.p1().x(), stroke.p2().x());
            qreal y1b = lineInterpolation(base, x1);
            qreal y1t = lineInterpolation(top, x1);
            qreal y2b = lineInterpolation(base, x2);
            qreal y2t = lineInterpolation(top, x2);
            poly.append(QPointF(x1, y1b));
            poly.append(QPointF(x2, y2b));
            poly.append(QPointF(x2, y2t));
            poly.append(QPointF(x1, y1t));
            v.push_back(poly);
        }
    }
    return v;
}

bool PerspectiveHelper::isIntersect(QLineF a, QLineF b) {
    auto cross = [](QPointF a, QPointF b) {
        return a.x() * b.y() - a.y() * b.x();
    };
    QPointF p12 = a.p2() - a.p1();
    QPointF p34 = b.p2() - b.p1();
    QPointF p31 = a.p1() - b.p1();
    QPointF p13 = b.p1() - a.p1();
    qreal c1 =  cross(p13, p34);
    qreal c2 = cross(p12, p34);
    qreal c3 = cross(p12, p31);
    if (c2 < 0) {
        c2 = -c2; c1 = -c1; c3 = -c3;
    }
    if (c2 != 0 && c1 >= 0 && c1 <= c2 && c3 >= 0 && c3 <= c2)
        return true;
    else
        return false;
}

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

void BlockAnnotation::onReturnPressed() {
    foreach (QPolygonF const &poly, getHelperPoly()) {
        if (poly.size() <= 2)
            continue;
        CharacterAnnotation charAnno;
        charAnno.box = poly;
        charAnno.text = QString("");
        characters.push_back(charAnno);
    }
}
