#ifndef IMAGEVIEWER_H
#define IMAGEVIEWER_H

#include <QMainWindow>
#include <QListWidget>
#include "imageannotation.h"

QT_BEGIN_NAMESPACE
class QAction;
class QMenu;
class QListWidget;
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

private:
    void createActions();
    void createMenus();
    void applyImage(QImage const &im);
    void updateScaledImage();
    void resetLocation(QSize size);
    void resetHistory();
    void addHistoryPoint(bool weak = false);
    void updateBlockList();
    QPointF toImageUV(QPoint screenUV) const;
    QPointF toScreenUV(QPointF imageUV) const;
    QPolygonF toScreenPoly(QPolygonF const &poly) const;

private slots:
    void open();
    void undo();
    void redo();
    void switchTool();
    void deleteBlock();
    void zoomIn();
    void zoomOut();
    void setLocation(QPoint loc);
    void resetLocation();
    void onListWidgetSelect();

private:
    QAction *openAct;
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
    bool mousePressed;
    QPoint mouseLastPos;

    bool draggingImage;
    bool drawingLabel;

    QWidget *centralWidget;
    QListWidget *listWidget;
    QLabel* statusLabel;
    QImage *image;
    QImage *scaledImage;
    QPoint imageLeftTop;
    qreal scaleFactor;

    ImageAnnotation anno;
    QVector<ImageAnnotation> history;
    QVector<ImageAnnotation> redoHistory;
    bool keepHistoryOnUndo;
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
