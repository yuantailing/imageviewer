#ifndef IMAGEVIEWER_H
#define IMAGEVIEWER_H

#include <QMainWindow>
#include "imageannotation.h"

QT_BEGIN_NAMESPACE
class QAction;
class QMenu;
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
    void resetHistory();
    void addHistoryPoint();
    QPointF toImageUV(QPoint screenUV);
    QPoint toScreenUV(QPointF imageUV);

private slots:
    void open();
    void undo();
    void redo();
    void switchTool();
    void zoomIn();
    void zoomOut();
    void setLocation(QPoint loc);
    void resetLocation();

private:
    QAction *openAct;
    QAction *undoAct;
    QAction *redoAct;
    QAction *switchToolAct;
    QAction *zoomInAct;
    QAction *zoomOutAct;
    QAction *resetLocationAct;
    QMenu *fileMenu;
    QMenu *editMenu;
    QMenu *viewMenu;

    bool controlPressed;
    bool mousePressed;
    QPoint mouseLastPos;

    bool draggingImage;
    bool drawingLabel;

    QWidget *centralWidget;
    QImage *image;
    QImage *scaledImage;
    QPoint imageLeftTop;
    qreal scaleFactor;

    ImageAnnotation anno;
    QVector<ImageAnnotation> history;
    QVector<ImageAnnotation> redoHistory;
};

#endif // IMAGEVIEWER_H
