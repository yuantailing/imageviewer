#ifndef IMAGEANNOTATION_HPP
#define IMAGEANNOTATION_HPP

#include <QVector>
#include <QString>
#include <QPolygonF>
#include <QLineF>
#include <QQueue>

class CharacterAnnotation;
class PerspectiveHelper;
class BlockAnnotation;
class ImageAnnotation;

// class AnnotationOptions {
//
// };

// 标注一个字
class CharacterAnnotation {
public:
    QPolygonF box;
    QString text;
};

class PerspectiveHelper {
public:
    int numPoint;
    QLineF base;
    QLineF top;
    bool stroking;
    QLineF stroke;

public:
    PerspectiveHelper() {
        numPoint = 0;
        stroking = false;
    }

    void onStartPoint(QPointF p, BlockAnnotation *block);
    void onPendingPoint(QPointF p, BlockAnnotation *block);
    void onEndPoint(QPointF p, BlockAnnotation *block);
    QVector<QPolygonF> getHelperPoly() const;
    QVector<QPolygonF> getPendingCharacterPoly() const;

private:
    static bool isIntersect(QLineF a, QLineF b);
};

class BlockAnnotation {
public:
    enum HelperType {NO_HELPER, PERSPECTIVE_HELPER};
    HelperType helperType;
    PerspectiveHelper perspectiveHelper;
    QVector<CharacterAnnotation> characters;

public:
    BlockAnnotation() {
        helperType = PERSPECTIVE_HELPER;
    }

    void onStartPoint(QPointF p) {
        if (helperType == PERSPECTIVE_HELPER)
            perspectiveHelper.onStartPoint(p, this);
    }

    void onPendingPoint(QPointF p) {
        if (helperType == PERSPECTIVE_HELPER)
            perspectiveHelper.onPendingPoint(p, this);
    }

    void onEndPoint(QPointF p) {
        if (helperType == PERSPECTIVE_HELPER)
            perspectiveHelper.onEndPoint(p, this);
    }

    int numWordNeeded() const {
        return characters.size();
    }

    bool isStringOk() const;

    void onInputString(QString const &s);

    void onReturnPressed();

    QVector<QPolygonF> getHelperPoly() const {
        if (helperType == PERSPECTIVE_HELPER)
            return perspectiveHelper.getHelperPoly();
        return QVector<QPolygonF>();
    }

    QVector<QPolygonF> getPendingCharacterPoly() const {
        if (helperType == PERSPECTIVE_HELPER)
            return perspectiveHelper.getPendingCharacterPoly();
        return QVector<QPolygonF>();
    }

    QVector<CharacterAnnotation> const &getCharacterAnnotation() const {
        return characters;
    }
};

class ImageAnnotation {
public:
    int version;
    QString imageId;
    QVector<BlockAnnotation> blocks;

public:
    ImageAnnotation() {
        version = 1000;
        imageId = QString("");
        onNewBlock();
    }

    void onStartPoint(QPointF p) {
        blocks.last().onStartPoint(p);
    }

    void onPendingPoint(QPointF p) {
        blocks.last().onPendingPoint(p);
    }

    void onEndPoint(QPointF p) {
        blocks.last().onEndPoint(p);
    }

    int numWordNeeded() const {
        return blocks.last().numWordNeeded();
    }

    bool isStringOk() const {
        return blocks.last().isStringOk();
    }

    void onInputString(QString const &s) {
        blocks.last().onInputString(s);
    }

    void onReturnPressed() {
        blocks.last().onReturnPressed();
    }

    void onNewBlock() {
        blocks.push_back(BlockAnnotation());
    }

    QVector<QPolygonF> getHelperPoly() const {
        QVector<QPolygonF> v;
        foreach (BlockAnnotation const &block, blocks) {
            v += block.getHelperPoly();
        }
        return v;
    }

    QVector<QPolygonF> getPendingCharacterPoly() const {
        QVector<QPolygonF> v;
        foreach (BlockAnnotation const &block, blocks) {
            v += block.getPendingCharacterPoly();
        }
        return v;
    }

    QVector<CharacterAnnotation> getCharacterAnnotation() const {
        QVector<CharacterAnnotation> v;
        foreach (BlockAnnotation const &block, blocks) {
            v += block.getCharacterAnnotation();
        }
        return v;
    }
};

#endif // IMAGEANNOTATION_HPP
