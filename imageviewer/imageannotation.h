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

// 标注一个字
class CharacterAnnotation {
public:
    QPolygonF box;
    QString text;
};

class PerspectiveHelper {
public:
    int numPoint;      // 已确定的点数
    QLineF base;       // 用户输入的第一条线
    QLineF top;        // 用户输入的第二条线
    bool toolSwitched; // false：横排文字垂直锁定 true：解除垂直锁定
    bool stroking;     // 是否正在标注文字区域
    QLineF stroke;
    // 自动检测框选文字方向是否与base同向 / 强制与base同向 / 强制与base垂直
    enum TextDirection { DIRECTION_AUTO, DIRECTION_BY_BASE, DIRECTION_TO_BASE } textDirection;

public:
    PerspectiveHelper();
    void onStartPoint(QPointF p, BlockAnnotation *block);
    void onPendingPoint(QPointF p, BlockAnnotation *block);
    bool onEndPoint(QPointF, BlockAnnotation *) { return stroking; }
    void onSwitchTool(BlockAnnotation *block);
    QPolygonF poly() const;
    QVector<QPolygonF> getHelperPoly() const { return QVector<QPolygonF>({poly()}); }
    QVector<QPolygonF> getPendingCharacterPoly() const;
    QString getTips() const;

private:
    TextDirection detectTextDirection(QLineF stroke) const;
    bool isHorizontalText(QLineF stroke) const;
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

    bool onEndPoint(QPointF p) {
        if (helperType == PERSPECTIVE_HELPER)
            return perspectiveHelper.onEndPoint(p, this);
        return false;
    }

    void onSwitchTool() {
        if (helperType == PERSPECTIVE_HELPER)
            perspectiveHelper.onSwitchTool(this);
    }

    int numWordNeeded() const {
        return characters.size();
    }

    bool isStringOk() const;

    void onInputString(QString const &s);

    void onEnterPressed();

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

    QString getTips() const {
        if (helperType == PERSPECTIVE_HELPER)
            return perspectiveHelper.getTips();
        return "";
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

    // 返回值是本次鼠标点击操作是否重要，若重要则加入history
    bool onEndPoint(QPointF p) {
        return blocks.last().onEndPoint(p);
    }

    void onSwitchTool() {
        blocks.last().onSwitchTool();
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

    void onEnterPressed() {
        blocks.last().onEnterPressed();
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

    QString getTips() const {
        return blocks.last().getTips();
    }
};

#endif // IMAGEANNOTATION_HPP
