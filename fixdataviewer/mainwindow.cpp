#include "mainwindow.h"
#include <QGuiApplication>
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
    smallGap = 6;
    bigWidth = 324;
    bigHeight = 200;
    smallNum = qMax(5, (size.width() - bigWidth) / (smallSize + smallGap) + 1);

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
                        tr("   点开对话框后，可以按Esc或Enter来关闭它，不一定要用鼠标点“×”或“OK”\n"));
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
        big = locators[key].second.second;
    } else {
        big = QImage(10, 4, QImage::Format_RGB32);
        big.fill(Qt::white);
    }
    QImage resized = big.scaledToWidth(bigWidth - 4, Qt::SmoothTransformation);
    if (resized.height() > bigHeight - 32)
        resized = big.scaledToHeight(bigWidth - 32, Qt::SmoothTransformation);
    QImage boardedImage(bigWidth, bigHeight, QImage::Format_RGB32);
    boardedImage.fill(QColor(224, 224, 255));
    QPainter painter(&boardedImage);
    painter.drawImage(2, 16, resized);
    painter.setBrush(QBrush(QColor(255, 0, 255)));
    QPolygon poly;
    poly.append(QPoint(smallSize / 2, 0));
    poly.append(QPoint(smallSize / 2 - 8, 14));
    poly.append(QPoint(smallSize / 2 + 8, 14));
    painter.drawPolygon(poly);
    QPoint pos(j * (smallSize + smallGap), i * (smallSize + smallGap) + smallSize);
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
            int i = pos.y() / (smallSize + smallGap);
            int j = pos.x() / (smallSize + smallGap);
            if (pos.y() % (smallSize + smallGap) < smallSize && pos.x() % (smallSize + smallGap) < smallSize && j < smallNum) {
                currentFocus = QPoint(j, i);
                updateBigImage();
                modifyAt(i, j);
            }
        }
    }
    QMainWindow::mousePressEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    smallNum = qMax(5, (event->size().width() - bigWidth) / (smallSize + smallGap) + 1);
    if (!packFilename.isEmpty())
        loadFile(packFilename);
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
        if (keyEvent->key() == Qt::Key_Right) {
            j++;
            if (j >= smallNum) {
                i++;
                j = 0;
            }
        } else if (keyEvent->key() == Qt::Key_Left) {
            j--;
            if (j < 0) {
                i--;
                j = smallNum - 1;
            }
        } else if (keyEvent->key() == Qt::Key_Up) {
            if (i > 0) i--;
            if (j < 0) j = 0;
            if (j >= smallNum) j = smallNum - 1;
        } else if (keyEvent->key() == Qt::Key_Down) {
            i++;
            if (j < 0) j = 0;
            if (j >= smallNum) j = smallNum - 1;
        }
        currentFocus = QPoint(j, i);
        if (currentFocus != oldFocus) {
            updateBigImage();
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
    QMap<QString, QVector<QPair<Locator, QPair<QImage, QImage> > > > map_loaded;
    QDataStream stream(&array, QIODevice::ReadOnly);
    stream >> map_loaded;
    if (!stream.atEnd() || !(stream.status() == QDataStream::Ok)) {
        QMessageBox::information(this, tr("Error"),
                                 tr("stream is bad: %1.").arg(file.fileName()));
        return;
    }
    pack.clear();
    locators.clear();
    correction.clear();
    pos2key.clear();
    pack = map_loaded;
    packFilename = filename;
    setPack();
}

void MainWindow::setPack() {
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
    image = QImage(smallNum * (smallSize + smallGap), i * (smallSize + smallGap) + bigHeight, QImage::Format_RGB32);
    image.fill(QColor(255, 255, 255));
    QPainter painter(&image);
    i = 0;
    j = 0;
    auto putNext = [&](QImage const &image) {
        painter.drawImage(QPoint(j * (smallSize + smallGap), i * (smallSize + smallGap)),
                          image.scaled(QSize(smallSize, smallSize), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        j += 1;
        while (j >= smallNum) {
            i++;
            j -= smallNum;
        }
    };
    auto putNextWithKey = [&](QImage const &small, Locator const &locator, QImage const &big) {
        int key = locators.size();
        locators.push_back(qMakePair(locator, qMakePair(small, big)));
        pos2key[qMakePair(i, j)] = key;
        key2ij[key] = qMakePair(i, j);
        putNext(small);
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
            QImage const &small = pit.second.first;
            QImage const &big = pit.second.second;
            putNextWithKey(small, locator, big);
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
        int clearFlag; //  0 清晰， 1 有字但无法识别， 2 无字
        if (correction.contains(key)) {
            resultText = correction[key].first;
            clearFlag = correction[key].second;
        } else {
            clearFlag = 0;
        }
        d.setData(locators[key].second.first, locators[key].second.second, originText);
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
    QImage const &small(locators[key].second.first);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QBrush(Qt::white), penwidth));
    painter.drawRect(QRect(QPoint(j * (smallSize + smallGap), i * (smallSize + smallGap)),
                           QSize(smallSize, smallSize)));
    painter.drawImage(QPoint(j * (smallSize + smallGap), i * (smallSize + smallGap)),
                      small.scaled(QSize(smallSize, smallSize), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    if (correction.contains(key)) {
        bool correct = correction[key].first == locators[key].first.second.second;
        int clearFlag = correction[key].second;
        if (clearFlag != 0) {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(QBrush(Qt::red), penwidth));
            painter.drawRect(QRect(QPoint(j * (smallSize + smallGap), i * (smallSize + smallGap)),
                                   QSize(smallSize, smallSize)));
        } else if (!correct) {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(QBrush(Qt::green), penwidth));
            painter.drawRect(QRect(QPoint(j * (smallSize + smallGap), i * (smallSize + smallGap)),
                                   QSize(smallSize, smallSize)));
        }
    }
}
