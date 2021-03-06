#ifndef IMAGEANNOTATION_HPP
#define IMAGEANNOTATION_HPP

#include <QVector>
#include <QMap>
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
    QMap<QString, int> props;
};

class PerspectiveHelper {
public:
    int numPoint;         // 已确定的点数
    QPointF points[4];    // 保存用户确定的顶点
    QLineF stroke;
    bool toolSwitched;    // false：横排文字垂直锁定 true：解除垂直锁定
    bool stroking;        // 是否正在标注文字区域
    bool singleCharacter; // 是否每次只标注一个字
    // 自动检测框选文字方向是否与base同向 / 强制与base同向 / 强制与base垂直
    enum TextDirection { DIRECTION_AUTO, DIRECTION_BY_BASE, DIRECTION_TO_BASE } textDirection;

public: // Do not serialize them
    bool lastPointInvalid;

public:
    PerspectiveHelper();
    void onStartPoint(QPointF p, qreal scale, bool regular, BlockAnnotation *block);
    void onPendingPoint(QPointF p, qreal scale, bool regular, BlockAnnotation *block);
    int onEndPoint(QPointF, qreal, bool, BlockAnnotation *);
    void onSwitchTool(BlockAnnotation *block);
    bool onEnterPressed(BlockAnnotation *block);
    void setPoint(QPointF p, qreal scale, bool regular, BlockAnnotation *block, bool pending);
    QVector<QPolygonF> getHelperPoly() const;
    QVector<QPolygonF> getPendingCharacterPoly() const;
    QString getTips() const;

private:
    QPolygonF poly() const;
    TextDirection detectTextDirection(QLineF stroke) const;
    bool isHorizontalText(QLineF stroke) const;
    void addNewCharacterBoxToBlock(QPolygonF poly, BlockAnnotation *block);
};

class BlockAnnotation {
public:
    QVector<CharacterAnnotation> characters;
    enum HelperType {NO_HELPER, PERSPECTIVE_HELPER};
    HelperType helperType;
    PerspectiveHelper perspectiveHelper;

public:
    BlockAnnotation() {
        helperType = PERSPECTIVE_HELPER;
    }

    void onStartPoint(QPointF p, qreal scale, bool regular) {
        if (helperType == PERSPECTIVE_HELPER)
            perspectiveHelper.onStartPoint(p, scale, regular, this);
    }

    void onPendingPoint(QPointF p, qreal scale, bool regular) {
        if (helperType == PERSPECTIVE_HELPER)
            perspectiveHelper.onPendingPoint(p, scale, regular, this);
    }

    int onEndPoint(QPointF p, qreal scale, bool regular) {
        if (helperType == PERSPECTIVE_HELPER)
            return perspectiveHelper.onEndPoint(p, scale, regular, this);
        return false;
    }

    void onSwitchTool() {
        if (helperType == PERSPECTIVE_HELPER)
            perspectiveHelper.onSwitchTool(this);
    }

    bool onEnterPressed() {
        if (helperType == PERSPECTIVE_HELPER)
            return perspectiveHelper.onEnterPressed(this);
        return false;
    }

    int numWordNeeded() const {
        return characters.size();
    }

    bool isStringOk() const;

    QString onInputString(QString const &s);

    QString onMask();

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
    QVector<BlockAnnotation> blocks;
    QPointF focusPoint;
    enum { VERSION = 0x1002 };

public:
    ImageAnnotation() {
        focusPoint = QPointF(0, 0);
    }

    friend QDataStream &operator <<(QDataStream &stream, ImageAnnotation const &anno);
    friend QDataStream &operator >>(QDataStream &stream, ImageAnnotation &anno);

    void onStartPoint(QPointF p, qreal scale, bool regular) {
        Q_ASSERT(!blocks.isEmpty());
        blocks.last().onStartPoint(p, scale, regular);
        focusPoint = p;
    }

    void onPendingPoint(QPointF p, qreal scale, bool regular) {
        Q_ASSERT(!blocks.isEmpty());
        blocks.last().onPendingPoint(p, scale, regular);
    }

    // 返回值：0一般操作；1本次鼠标点击操作是否是weak history，弱历史会被之后的历史覆盖；2无效点
    int onEndPoint(QPointF p, qreal scale, bool regular) {
        Q_ASSERT(!blocks.isEmpty());
        return blocks.last().onEndPoint(p, scale, regular);
    }

    void onSwitchTool() {
        Q_ASSERT(!blocks.isEmpty());
        blocks.last().onSwitchTool();
    }

    // 返回值是本次操作是否对标注类有实质修改，若没有实质修改则视为用户想要开始输入字符串
    bool onEnterPressed() {
        Q_ASSERT(!blocks.isEmpty());
        return blocks.last().onEnterPressed();
    }

    int numWordNeeded(int index) const {
        Q_ASSERT(0 <= index && index < blocks.size());
        return blocks[index].numWordNeeded();
    }

    bool isStringOk(int index) const {
        Q_ASSERT(0 <= index && index < blocks.size());
        return blocks[index].isStringOk();
    }

    QString onInputString(QString const &s, int index) {
        Q_ASSERT(0 <= index && index < blocks.size());
        return blocks[index].onInputString(s);
    }

    QString onMask(int index) {
        Q_ASSERT(0 <= index && index < blocks.size());
        return blocks[index].onMask();
    }

    void onNewBlock() {
        blocks.push_back(BlockAnnotation());
    }

    QString getTips() const {
        Q_ASSERT(!blocks.isEmpty());
        return blocks.last().getTips();
    }
};

#endif // IMAGEANNOTATION_HPP
