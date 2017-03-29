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

protected:
    void mousePressEvent(QMouseEvent *event);
    void resizeEvent(QResizeEvent *event);
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);

private:
    void loadFile(QString filename);
    void setPack();
    void updateImageAt(int i, int j, QPainter &painter);

private:
    QImage image;
    QLabel *imageLabel;
    QScrollArea *scrollArea;
    QString packFilename;
    QString correctionFilename;
    int smallSize;
    int smallGap;
    int smallNum;
    typedef QPair<QString, QPair<QRectF, QString> > Locator;
    QMap<QString, QVector<QPair<Locator, QPair<QImage, QImage> > > > pack;
    QVector<QPair<Locator, QPair<QImage, QImage> > > locators;
    QMap<int, QPair<QString, int> > correction;
    QMap<QPair<int, int>, int> pos2key;
    QMap<int, QPair<int, int> > key2ij;
};

#endif // MAINWINDOW_H
