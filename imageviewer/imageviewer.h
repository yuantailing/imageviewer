#ifndef IMAGEVIEWER_H
#define IMAGEVIEWER_H

#include <QMainWindow>
#include <QListWidget>
#include <QDir>
#include <QJsonObject>
#include "imageannotation.h"

QT_BEGIN_NAMESPACE
class QAction;
class QMenu;
class QListWidget;
class QRadioButton;
class QCheckBox;
class QLabel;
class QImage;
QT_END_NAMESPACE

class ImageViewer : public QMainWindow
{
    Q_OBJECT
    
public:
    ImageViewer(QWidget *parent = 0);
    ~ImageViewer();

protected:
    void resizeEvent(QResizeEvent *);
    void paintEvent(QPaintEvent *);
    void keyPressEvent(QKeyEvent *);
    void keyReleaseEvent(QKeyEvent *);
    void wheelEvent(QWheelEvent *);
    void mousePressEvent(QMouseEvent *);
    void mouseMoveEvent(QMouseEvent *);
    void mouseReleaseEvent(QMouseEvent *);
    void dragEnterEvent(QDragEnterEvent *);
    void dropEvent(QDropEvent *);
    void closeEvent(QCloseEvent *);
    bool eventFilter(QObject *watched, QEvent *event);

private:
    void createActions();
    void createMenus();
    void loadFile(QString const &fileName);
    void updateScaledImage();
    void resetLocation(QSize size);
    void resetHistory();
    void addHistoryPoint(int flag = 0); // 0: strong history; 1: week history; 2: replace last history
    void addHistoryPoint(QString const &mergeKey);
    void changePropStatus(int index, bool checked);
    void updatePropsCheckBox();
    void updateBlockList();
    QPointF toImageUV(QPoint screenUV) const;
    QPointF toScreenUV(QPointF imageUV) const;
    QPolygonF toScreenPoly(QPolygonF const &poly) const;
    void updatePendingAnnotation();
    void inputStringToAnnotation(int index);

    QString annotationFileName(QString const &imageFileName) const;
    QPointF *lastPerspectiveHelperPoints();

    template <typename T>
    static QByteArray toQByteArray(T const &t);
    template <typename T>
    static QDataStream::Status fromQByteArray(QByteArray &array, T &t);

private slots:
    void open();
    void save();
    void undo();
    void redo();
    void switchTool();
    void deleteBlock();
    void zoomIn();
    void zoomOut();
    void setLocation(QPoint loc);
    void resetLocation();
    void onListWidgetSelect();
    void onListWidgetDoubleClicked(QModelIndex index);

private:
    QAction *openAct;
    QAction *saveAct;
    QAction *undoAct;
    QAction *redoAct;
    QAction *switchToolAct;
    QAction *deleteBlockAct;
    QAction *zoomInAct;
    QAction *zoomOutAct;
    QAction *resetLocationAct;
    QMenu *fileMenu;
    QMenu *editMenu;
    QMenu *viewMenu;

    bool controlPressed;
    bool shiftPressed;
    bool spacePressed;
    bool mousePressed;
    QPoint mouseLastPos;

    bool draggingImage;
    bool drawingLabel;

    QWidget *centralWidget;
    QListWidget *listWidget;
    QWidget *propWidget;
    QRadioButton *radioButtonAnno;
    QRadioButton *radioButtonProp;
    QCheckBox *checkBoxProps[6];
    QLabel* statusLabel;
    QString annotationSuffix;
    QDir imageFolder;
    QStringList imagesInFolder;
    QString imageFileName;
    QString imageBaseName;
    QImage image;
    QImage scaledImage;
    QPoint imageLeftTop;
    qreal scaleFactor;

    ImageAnnotation anno;
    QVector<ImageAnnotation> history;
    QVector<ImageAnnotation> redoHistory;
    bool keepHistoryOnUndo;
    QString historyMergeKey;

    bool changingPerspectiveHelper;
    int selectedPerspectiveHelperIndex;
    int selectedBlockIndex; // -1 respects to `none`
    int selectedCharIndex; // -1 respects to `all`
};

class QSoftSelectListWidget: public QListWidget {
    Q_OBJECT
public:
    QSoftSelectListWidget(QWidget *parent);
    ~QSoftSelectListWidget();
protected:
    void mouseMoveEvent(QMouseEvent *);
    void leaveEvent(QEvent *);
};

#endif // IMAGEVIEWER_H
