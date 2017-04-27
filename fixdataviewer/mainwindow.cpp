#include "mainwindow.h"
#include <QGuiApplication>
#include <QSettings>
#include <QScreen>
#include <QLabel>
#include <QScrollArea>
#include <QMouseEvent>
#include <QMessageBox>
#include <QMimeData>
#include <QDir>
#include <QFileInfo>
#include <QDataStream>
#include <QImage>
#include <QPainter>
#include <QPolygon>
#include <QDebug>
#include "dialog.h"

class ScrollArea: public QScrollArea {
public:
    ScrollArea(MainWindow *parent): QScrollArea(parent), parent(parent) { }
protected:
    void scrollContentsBy(int dx, int dy) {
        QScrollArea::scrollContentsBy(dx, dy);
        parent->updateBigImage();
    }
private:
    MainWindow *parent;
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , imageLabel(new QLabel)
    , scrollArea(new ScrollArea(this))
{
    QSettings settings("config.ini", QSettings::IniFormat);

    imageLabel->setBackgroundRole(QPalette::Base);
    imageLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    imageLabel->setScaledContents(true);

    scrollArea->setBackgroundRole(QPalette::Dark);
    scrollArea->setWidget(imageLabel);
    setCentralWidget(scrollArea);

    setAcceptDrops(true);
    QSize size(QGuiApplication::primaryScreen()->availableSize() * 4 / 5);
    resize(size);

    smallSize = 32;
    smallGapX = 6;
    smallGapY = 6;
    if (settings.contains("/size/smallSize"))
        smallSize = settings.value("/size/smallSize").toInt();
    if (settings.contains("/size/smallGapX"))
        smallGapX = settings.value("/size/smallGapX").toInt();
    if (settings.contains("/size/smallGapY"))
        smallGapY = settings.value("/size/smallGapY").toInt();
    bigWidth = 324;
    bigHeight = 200;
    smallNum = qMax(5, (size.width() - bigWidth) / (smallSize + smallGapX) + 1);
    smallMax = 0;

    imageLabel->setText(tr("1. 把multicharpack-****.doubt拖拽到此处\n") +
                        tr("2. 用键盘←↑→↓移动光标，看到标错的（或无法识别的）汉字时按回车\n") +
                        tr("   或者用鼠标左键点击，可以直接修改光标位置\n") +
                        tr("3. 弹出的对话框会显示这个字及其周围的一小片区域。显示“上下文”是为了帮助你判断\n") +
                        tr("4. 如果是标错的，勾选“能识别”并输入正确的文字，\n") +
                        tr("   如果无法识别，勾选“无法识别”或“没有字”（快捷键F2、F3）\n") +
                        tr("5. 确定后，标错的字会显示绿框，无法识别的字会显示红框\n") +
                        tr("6. 你的每次更改会自动保存到multicharpack-****.correction文件里\n") +
                        tr("\n") +
                        tr("   请不要重命名这些文件（.doubt、.correction）\n") +
                        tr("   请把繁简体错误也更正过来\n") +
                        tr("   你可以拖动窗口大小，每行图片数量会自动调整\n") +
                        tr("   点开对话框后，可以按Esc或Enter来关闭它并保存，不一定要用鼠标点“×”或“OK”\n"));
    imageLabel->adjustSize();

    currentFocus = QPoint(1, 0);
    bigImageLabel = new QLabel(this);
    scrollArea->installEventFilter(this);
    this->installEventFilter(this);
}

MainWindow::~MainWindow()
{

}

void MainWindow::updateBigImage() {
    int j = currentFocus.x();
    int i = currentFocus.y();
    QPair<int, int> p(i, j);
    QImage big;
    if (pos2key.contains(p)) {
        int key = pos2key[p];
        big = resizedBigWithBox(locators[key].second);
    } else {
        big = QImage(10, 4, QImage::Format_RGB32);
        big.fill(Qt::white);
    }
    QImage boardedImage(bigWidth, bigHeight, QImage::Format_RGB32);
    boardedImage.fill(QColor(224, 224, 255));
    QPainter painter(&boardedImage);
    painter.drawImage(2, 16, big);
    painter.setBrush(QBrush(QColor(255, 0, 255)));
    QPolygon poly;
    poly.append(QPoint(smallSize / 2, 0));
    poly.append(QPoint(smallSize / 2 - 8, 14));
    poly.append(QPoint(smallSize / 2 + 8, 14));
    painter.drawPolygon(poly);
    QPoint pos(j * (smallSize + smallGapX), i * (smallSize + smallGapY) + smallSize);
    pos = imageLabel->mapTo(this, pos);
    bigImageLabel->setGeometry(QRect(pos, boardedImage.size()));
    bigImageLabel->setPixmap(QPixmap::fromImage(boardedImage));
}

void MainWindow::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        if (bigImageLabel->geometry().contains(event->pos())) {
            modifyAt(currentFocus.y(), currentFocus.x());
        } else {
            QPoint pos = imageLabel->mapFrom(this, event->pos());
            int i = pos.y() / (smallSize + smallGapY);
            int j = pos.x() / (smallSize + smallGapX);
            if (pos.y() % (smallSize + smallGapY) < smallSize && pos.x() % (smallSize + smallGapX) < smallSize && j < smallNum) {
                currentFocus = QPoint(j, i);
                updateBigImage();
                modifyAt(i, j);
            }
        }
    }
    QMainWindow::mousePressEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    smallNum = qMax(5, (event->size().width() - bigWidth) / (smallSize + smallGapX) + 1);
    if (!packFilename.isEmpty())
        setPack();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasFormat("text/uri-list"))
        event->acceptProposedAction();
    QMainWindow::dragEnterEvent(event);
}

void MainWindow::dropEvent(QDropEvent *event) {
    QList<QUrl> urls = event->mimeData()->urls();
    if (urls.isEmpty())
        return;
    loadFile(urls.first().toLocalFile());
    QMainWindow::dropEvent(event);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = dynamic_cast<QKeyEvent *>(event);
        if (!keyEvent)
            return false;
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            modifyAt(currentFocus.y(), currentFocus.x());
            return true;
        }
        QPoint oldFocus = currentFocus;
        int j = currentFocus.x();
        int i = currentFocus.y();
        for (int k = 0; k < smallNum; k++) {
            if (keyEvent->key() == Qt::Key_Right) {
                j++;
            } else if (keyEvent->key() == Qt::Key_Left) {
                j--;
            } else if (keyEvent->key() == Qt::Key_Up) {
                i--;
            } else if (keyEvent->key() == Qt::Key_Down) {
                i++;
            }
            if (j < 0) {
                j = smallNum - 1;
                i--;
            }
            if (j >= smallNum) {
                j = 0;
                i++;
            }
            i = qMax(0, qMin(smallMax - 1, i));
            j = qMax(0, qMin(smallNum - 1, j));
            if (pos2key.contains(qMakePair(i, j)))
                break;
        }
        currentFocus = QPoint(j, i);
        if (currentFocus != oldFocus) {
            updateBigImage();
            QPoint bigImageCenter = imageLabel->mapFrom(this, bigImageLabel->geometry().center());
            scrollArea->ensureVisible(bigImageCenter.x(), bigImageCenter.y() - (smallSize + smallGapY) / 2,
                                      0, bigHeight / 2 + smallSize + smallGapY);
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::loadFile(QString filename) {
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::information(this, tr("Error"),
                                 tr("Cannot load %1.").arg(file.fileName()));
        return;
    }
    QByteArray array(file.readAll());
    file.close();
    array = qUncompress(array);
    QMap<QString, QVector<QPair<Locator, QPair<QPair<QImage, QPolygonF>, QPair<QImage, QRect> > > > > map_loaded;
    QDataStream stream(&array, QIODevice::ReadOnly);
    stream >> map_loaded;
    if (!stream.atEnd() || !(stream.status() == QDataStream::Ok)) {
        QMessageBox::information(this, tr("Error"),
                                 tr("stream is bad: %1.").arg(file.fileName()));
        return;
    }
    pack.clear();
    pack = map_loaded;
    packFilename = filename;
    setPack();
}

void MainWindow::setPack() {
    locators.clear();
    correction.clear();
    pos2key.clear();
    int i = 0, j = 0;
    for (auto it = pack.begin(); it != pack.end(); it++) {
        j += 1;
        j += it.value().size();
        while (j >= smallNum) {
            i++;
            j -= smallNum;
        }
        if (j != 0)
            i++;
        j = 0;
    }
    smallMax = i;
    image = QImage(smallNum * (smallSize + smallGapX), i * (smallSize + smallGapY) + bigHeight, QImage::Format_RGB32);
    image.fill(QColor(255, 255, 255));
    QPainter painter(&image);
    i = 0;
    j = 0;
    auto putNext = [&](QImage const &image) {
        painter.drawImage(QPoint(j * (smallSize + smallGapX), i * (smallSize + smallGapY)),
                          image.scaled(QSize(smallSize, smallSize), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        j += 1;
        while (j >= smallNum) {
            i++;
            j -= smallNum;
        }
    };
    auto putNextWithKey = [&](Locator const &locator, QPair<QPair<QImage, QPolygonF>, QPair<QImage, QRect> > const &info) {
        int key = locators.size();
        locators.push_back(qMakePair(locator, info));
        pos2key[qMakePair(i, j)] = key;
        key2ij[key] = qMakePair(i, j);
        putNext(info.first.first);
    };
    auto newLine = [&]() {
        if (j != 0)
            i++;
        j = 0;
    };
    auto text2img = [](QString text) {
        int fontsize = 64;
        QSize size(fontsize, fontsize);
        QImage image(size, QImage::Format_ARGB32);
        image.fill(qRgba(255, 255, 255, 255));
        QPainter painter(&image);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        QPen pen = painter.pen();
        pen.setColor(qRgba(0, 0, 0, 255));
        QFont font = painter.font();
        font.setBold(false);
        font.setPixelSize(fontsize);
        painter.setPen(pen);
        painter.setFont(font);
        painter.drawText(image.rect(), Qt::AlignCenter, text);
        return image;
    };
    for (auto it = pack.begin(); it != pack.end(); it++) {
        QString originText = it.key();
        putNext(text2img(originText));
        foreach (auto pit, it.value()) {
            Locator const &locator = pit.first;
            putNextWithKey(locator, pit.second);
        }
        newLine();
    }
    QFileInfo fileInfo(packFilename);
    QDir dir(fileInfo.dir());
    correctionFilename = dir.filePath(fileInfo.completeBaseName() + ".correction");
    QFile file(correctionFilename);
    if (file.exists()) {
        if (!file.open(QIODevice::ReadOnly)) {
            QMessageBox::information(this, tr("Error"),
                                     tr("Cannot load %1.").arg(file.fileName()));
            exit(1);
        }
        QByteArray array(file.readAll());
        file.close();
        array = qUncompress(array);
        QVector<QPair<Locator, QPair<QString, int> > > correction_loaded;
        QDataStream stream(&array, QIODevice::ReadOnly);
        stream >> correction_loaded;
        if (!stream.atEnd() || !(stream.status() == QDataStream::Ok)) {
            QMessageBox::information(this, tr("Error"),
                                     tr("stream is bad: %1.").arg(file.fileName()));
            exit(1);
        }
        for (auto it = correction_loaded.begin(); it != correction_loaded.end(); it++) {
            Locator const &locator = it->first;
            QPair<QString, int> const &c = it->second;
            auto isSame = [](Locator const &a, Locator const &b) {
                if (a.first != b.first || a.second.second != b.second.second)
                    return false;
                QRectF fa = a.second.first;
                QRectF fb = b.second.first;
                QRectF inter = fa.intersected(fb);
                if (inter.width() < 0.99 * qMax(fa.width(), fb.width()) ||
                        inter.height() < 0.99 * qMax(fa.height(), fb.height()))
                    return false;
                return true;
            };
            for (int i = 0; i < locators.size(); i++) {
                if (isSame(locators[i].first, locator)) {
                    correction[i] = c;
                    QPair<int, int> ij = key2ij[i];
                    updateImageAt(ij.first, ij.second, painter);
                    break;
                }
            }
        }
        imageLabel->setPixmap(QPixmap::fromImage(image));
    }
    imageLabel->setPixmap(QPixmap::fromImage(image));
    imageLabel->adjustSize();
    updateBigImage();
}

void MainWindow::modifyAt(int i, int j) {
    QPair<int, int> p(i, j);
    if (pos2key.contains(p)) {
        int key = pos2key[p];
        Dialog d(this);
        QString originText = locators[key].first.second.second;
        QString resultText;
        int clearFlag; //  0 清晰， 1 有字但无法识别， 2 无字， 3 多个字
        if (correction.contains(key)) {
            resultText = correction[key].first;
            clearFlag = correction[key].second;
        } else {
            clearFlag = 0;
        }
        d.setData(locators[key].second.first.first, resizedBigWithBox(locators[key].second), originText);
        d.setReturn(resultText, clearFlag);
        d.show();
        d.exec();
        resultText = resultText.trimmed();
        if (resultText == originText)
            resultText.clear();
        if (clearFlag == 0 && resultText.length() > 1) {
            QMessageBox::information(this, tr("Error"),
                                    tr("只允许输入一个字，请重新编辑"));
        } else {
            if (resultText.isEmpty() && clearFlag == 0) {
                if (correction.contains(key))
                    correction.remove(key);
            } else {
                correction[key] = qMakePair(resultText, clearFlag);
            }
            QPainter painter(&image);
            updateImageAt(i, j, painter);
            imageLabel->setPixmap(QPixmap::fromImage(image));
            QVector<QPair<Locator, QPair<QString, int> > > correctionVec;
            for (auto it = correction.begin(); it != correction.end(); it++) {
                correctionVec.push_back(qMakePair(locators[it.key()].first, it.value()));
            }
            QByteArray array;
            QDataStream stream(&array, QIODevice::WriteOnly);
            stream << correctionVec;
            array = qCompress(array, 9);
            QFile file(correctionFilename);
            if (!file.open(QIODevice::WriteOnly)) {
                QMessageBox::information(this, tr("Error"),
                                         tr("Cannot write %1.").arg(file.fileName()));
                exit(1);
            }
            file.write(array);
            file.close();
        }
    }
}

void MainWindow::updateImageAt(int i, int j, QPainter &painter) {
    QPair<int, int> p(i, j);
    int key = pos2key[p];
    int penwidth = 4;
    QImage const &small(locators[key].second.first.first);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QBrush(Qt::white), penwidth));
    painter.drawRect(QRect(QPoint(j * (smallSize + smallGapX), i * (smallSize + smallGapY)),
                           QSize(smallSize, smallSize)));
    painter.drawImage(QPoint(j * (smallSize + smallGapX), i * (smallSize + smallGapY)),
                      small.scaled(QSize(smallSize, smallSize), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    if (correction.contains(key)) {
        bool correct = correction[key].first == locators[key].first.second.second;
        int clearFlag = correction[key].second;
        if (clearFlag != 0) {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(QBrush(Qt::red), penwidth));
            painter.drawRect(QRect(QPoint(j * (smallSize + smallGapX), i * (smallSize + smallGapY)),
                                   QSize(smallSize, smallSize)));
        } else if (!correct) {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(QBrush(Qt::green), penwidth));
            painter.drawRect(QRect(QPoint(j * (smallSize + smallGapX), i * (smallSize + smallGapY)),
                                   QSize(smallSize, smallSize)));
        }
    }
}

QImage MainWindow::resizedBigWithBox(const QPair<QPair<QImage, QPolygonF>, QPair<QImage, QRect> > &info) {
    QImage const &big = info.second.first;
    QImage resized = big.scaled(QSize(bigWidth - 4, bigHeight - 32), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QPolygonF poly0 = info.first.second;
    QRect rect0 = info.second.second;
    QRect rect1(0, 0, resized.width(), resized.height());
    QPainter painter(&resized);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QBrush(Qt::green), 1));
    QPolygonF poly1;
    foreach (QPointF const &p, poly0) {
        qreal x = (p.x() - rect0.left()) / rect0.width() * rect1.width();
        qreal y = (p.y() - rect0.top()) / rect0.height() * rect1.height();
        poly1.push_back(QPoint(x, y));
    }
    painter.drawPolygon(poly1);
    return resized;
}
