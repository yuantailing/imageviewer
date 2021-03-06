#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>
#include <QImage>

QT_BEGIN_NAMESPACE
class QLabel;
class QScrollArea;
class QPainter;
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = 0);
    ~MainWindow();
    void updateBigImage();

protected:
    void mousePressEvent(QMouseEvent *event);
    void resizeEvent(QResizeEvent *event);
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);
    bool eventFilter(QObject *watched, QEvent *event);

private:
    void loadFile(QString filename);
    void setPack();
    void modifyAt(int i, int j);
    void updateImageAt(int i, int j, QPainter &painter);
    QImage resizedBigWithBox(QPair<QPair<QImage, QPolygonF>, QPair<QImage, QRect> > const &info);

private:
    QImage image;
    QLabel *imageLabel;
    QScrollArea *scrollArea;
    QPoint currentFocus;
    QLabel *bigImageLabel;
    QString packFilename;
    QString correctionFilename;
    int smallSize;
    int smallGapX;
    int smallGapY;
    int smallNum;
    int smallMax;
    int bigWidth;
    int bigHeight;
    typedef QPair<QString, QPair<QRectF, QString> > Locator;
    QMap<QString, QVector<QPair<Locator, QPair<QPair<QImage, QPolygonF>, QPair<QImage, QRect> > > > > pack;
    QVector<QPair<Locator, QPair<QPair<QImage, QPolygonF>, QPair<QImage, QRect> > > > locators;
    QMap<int, QPair<QString, int> > correction;
    QMap<QPair<int, int>, int> pos2key;
    QMap<int, QPair<int, int> > key2ij;
};

#endif // MAINWINDOW_H
