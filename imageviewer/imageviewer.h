#ifndef IMAGEVIEWER_H
#define IMAGEVIEWER_H

#include <QMainWindow>
#include <QListWidget>
#include <QDir>
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
    void dragEnterEvent(QDragEnterEvent *);
    void dropEvent(QDropEvent *);
    void closeEvent(QCloseEvent *);

private:
    void createActions();
    void createMenus();
    void loadFile(QString const &fileName);
    void updateScaledImage();
    void resetLocation(QSize size);
    void resetHistory();
    void addHistoryPoint(bool weak = false);
    void updateBlockList();
    QPointF toImageUV(QPoint screenUV) const;
    QPointF toScreenUV(QPointF imageUV) const;
    QPolygonF toScreenPoly(QPolygonF const &poly) const;
    void inputStringToAnnotation(int index);

    QString annotationFileName(QString const &imageFileName) const;

    template <typename T>
    static QByteArray toQByteArray(T const &t);
    template <typename T>
    static QDataStream::Status fromQByteArray(QByteArray &array, T &t);

private slots:
    void open();
    void save();
    void exportPackage();
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
    QAction *exportPackageAct;
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
    QLabel* statusLabel;
    QDir imageFolder;
    QStringList imagesInFolder;
    QString imageFileName;
    QImage image;
    QImage scaledImage;
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
