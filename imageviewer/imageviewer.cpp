#include <QtWidgets>
#include <QDebug>
#include "imageviewer.h"

/// ImageViewer

ImageViewer::ImageViewer(QWidget *parent)
    : QMainWindow(parent)
{
    createActions();
    createMenus();

    centralWidget = new QWidget(this);
    centralWidget->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    listWidget = new QSoftSelectListWidget(centralWidget);
    listWidget->setFocusPolicy(Qt::NoFocus);
    statusLabel = new QLabel(this);
    statusBar()->addWidget(statusLabel);
    statusBar()->setStyleSheet(QString("QStatusBar::item{border: 0px;background:#ffd;}"));
    connect(listWidget, SIGNAL(currentRowChanged(int)), this, SLOT(onListWidgetSelect()));
    connect(listWidget, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(onListWidgetDoubleClicked(QModelIndex)));

    controlPressed = false;
    shiftPressed = false;
    mousePressed = false;
    mouseLastPos = QPoint(0, 0);
    draggingImage = false;
    drawingLabel = false;
    resetHistory();

    setWindowTitle(tr("Image Viewer"));
    setCentralWidget(centralWidget);
    this->setMouseTracking(true);
    centralWidget->setMouseTracking(true);
    this->setAcceptDrops(true);
    resize(1000, 800);
    resetLocation();
}

ImageViewer::~ImageViewer() {
}

void ImageViewer::resizeEvent(QResizeEvent *event) {
    resetLocation(event->size());
    int w = 160;
    int x1 = width() - w - 20;
    int y1 = 20;
    int h = qMin((int)(height() * 0.7), (height() - y1 - 60));
    x1 = qMax(x1, 0);
    h = qMax(h, 30);
    listWidget->setGeometry(x1, y1, w, h);
    QMainWindow::resizeEvent(event);
}

void ImageViewer::paintEvent(QPaintEvent *event) {
    QPainter painter;
    painter.begin(this);

    // 绘制图片
    if (!scaledImage.isNull())
        painter.drawImage(imageLeftTop, scaledImage);

    // 确定选中的Block
    int selectedBlock = -1;
    if (!listWidget->selectedItems().isEmpty())
        selectedBlock = listWidget->row(listWidget->selectedItems().first());

    if (!controlPressed || !shiftPressed)
        for (int i = 0; i < anno.blocks.size(); i++) {
            BlockAnnotation const &block(anno.blocks[i]);
            qreal polyOpacity = i == selectedBlock ? 0.5 : 0.3;
            qreal charOpacity = i == selectedBlock ? 1.0 : 0.75;

            // 绘制辅助图层
            painter.setOpacity(polyOpacity);
            painter.setPen(QPen(Qt::blue, 3.0));
            painter.setBrush(QBrush(Qt::yellow));
            foreach (QPolygonF const &poly, block.getHelperPoly())
                painter.drawPolygon(toScreenPoly(poly));

            // 绘制正在标注的字符区域
            painter.setOpacity(polyOpacity);
            painter.setPen(QPen(Qt::green));
            painter.setBrush(QBrush(Qt::red));
            foreach (QPolygonF const &poly, block.getPendingCharacterPoly())
                painter.drawPolygon(toScreenPoly(poly));

            // 绘制已标注的字符区域
            foreach (CharacterAnnotation const &charAnno, block.getCharacterAnnotation()) {
                QPolygonF polyScreen;
                foreach (QPointF p, charAnno.box)
                    polyScreen.append(toScreenUV(p));

                // 字符包围盒
                painter.setOpacity(polyOpacity);
                painter.setPen(QPen(Qt::green));
                painter.setBrush(QBrush(Qt::red));
                painter.drawPolygon(polyScreen);

                // 打印文字
                painter.setOpacity(charOpacity);
                painter.setPen(QPen(Qt::black));
                painter.setBrush(QBrush(Qt::red));
                QRectF bounding = polyScreen.boundingRect();
                qreal fontSize = qMax(qMin(bounding.width() / charAnno.text.length(), bounding.height()) * 0.67, 5.0);
                painter.setFont(QFont("Arial", fontSize, QFont::Bold));
                painter.drawText(bounding, Qt::AlignCenter, charAnno.text);
            }
        }

    // 绘制Tips
    statusLabel->setText(anno.getTips());

    painter.end();
    QMainWindow::paintEvent(event);
}

void ImageViewer::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Control) {
        controlPressed = true;
        update();
    } else if (event->key() == Qt::Key_Shift) {
        shiftPressed = true;
        anno.onPendingPoint(toImageUV(mapFromGlobal(cursor().pos())), shiftPressed);
        update();
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        bool annoChanged = anno.onEnterPressed();
        if (annoChanged) {
            addHistoryPoint();
            updateBlockList();
            update();
        } else {
            inputStringToAnnotation(anno.blocks.size() - 1);
        }
    } else if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right) {
        int current = imagesInFolder.indexOf(QFileInfo(imageFileName).fileName());
        if (event->key() == Qt::Key_Left) {
            if (current > 0)
                loadFile(imageFolder.filePath(imagesInFolder[current - 1]));
        } else {
            if (current >= 0 && current + 1 < imagesInFolder.size())
                loadFile(imageFolder.filePath(imagesInFolder[current + 1]));
        }
    }
    QMainWindow::keyPressEvent(event);
}

void ImageViewer::keyReleaseEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Control) {
        controlPressed = false;
        update();
    } else if (event->key() == Qt::Key_Shift) {
        shiftPressed = false;
        anno.onPendingPoint(toImageUV(mapFromGlobal(cursor().pos())), shiftPressed);
        update();
    }
    QMainWindow::keyReleaseEvent(event);
}

void ImageViewer::wheelEvent(QWheelEvent *event) {
    if (controlPressed) {
        if (event->delta() > 0) {
            zoomIn();
        } else if (event->delta() < 0) {
            zoomOut();
        }
    }
    QMainWindow::wheelEvent(event);
}

void ImageViewer::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        mousePressed = true;
        if (controlPressed) {
            draggingImage = true;
        } else {
            drawingLabel = true;
            anno.onStartPoint(toImageUV(event->pos()), shiftPressed);
            update();
        }
        mouseLastPos = event->pos();
    }
    QMainWindow::mousePressEvent(event);
}

void ImageViewer::mouseMoveEvent(QMouseEvent *event) {
    if (draggingImage) {
        setLocation(imageLeftTop + event->pos() - mouseLastPos);
    } else {
        anno.onPendingPoint(toImageUV(event->pos()), shiftPressed);
        update();
    }
    mouseLastPos = event->pos();
    QMainWindow::mouseMoveEvent(event);
}

void ImageViewer::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        if (draggingImage) {
            // do nothing
        } else {
            bool weak = anno.onEndPoint(toImageUV(event->pos()), shiftPressed);
            addHistoryPoint(weak);
            updateBlockList();
            update();
        }
        if (event->button() == Qt::LeftButton) {
            mousePressed = false;
            draggingImage = false;
            drawingLabel = false;
        }
    }
    QMainWindow::mouseReleaseEvent(event);
}

void ImageViewer::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasFormat("text/uri-list"))
        event->acceptProposedAction();
    QMainWindow::dragEnterEvent(event);
}

void ImageViewer::dropEvent(QDropEvent *event){
    QList<QUrl> urls = event->mimeData()->urls();
    if (urls.isEmpty())
        return;
    loadFile(urls.first().toLocalFile());
    QMainWindow::dropEvent(event);
}

void ImageViewer::closeEvent(QCloseEvent *event) {
    save();
    QMainWindow::closeEvent(event);
}

void ImageViewer::createActions() {
    openAct = new QAction(tr("&Open"), this);
    openAct->setShortcut(tr("Ctrl+O"));
    connect(openAct, SIGNAL(triggered()), this, SLOT(open()));

    saveAct = new QAction(tr("&Save"), this);
    saveAct->setShortcut(tr("Ctrl+S"));
    connect(saveAct, SIGNAL(triggered()), this, SLOT(save()));

    exportPackageAct = new QAction(tr("&Export package"), this);
    connect(exportPackageAct, SIGNAL(triggered()), this, SLOT(exportPackage()));

    undoAct = new QAction(tr("&Undo"), this);
    undoAct->setShortcut(tr("Ctrl+Z"));
    connect(undoAct, SIGNAL(triggered()), this, SLOT(undo()));

    redoAct = new QAction(tr("&Redo"), this);
    redoAct->setShortcuts({tr("Ctrl+Y"), tr("Ctrl+Shift+Z")});
    connect(redoAct, SIGNAL(triggered()), this, SLOT(redo()));

    switchToolAct = new QAction(tr("&Switch tool"), this);
    switchToolAct->setShortcut(tr("Tab"));
    connect(switchToolAct, SIGNAL(triggered()), this, SLOT(switchTool()));

    deleteBlockAct = new QAction(tr("&Delete block"), this);
    deleteBlockAct->setShortcut(tr("Delete"));
    connect(deleteBlockAct, SIGNAL(triggered()), this, SLOT(deleteBlock()));

    zoomInAct = new QAction(tr("Zoom &In (25%)"), this);
    zoomInAct->setShortcuts({tr("Ctrl++"), tr("Ctrl+=")});
    connect(zoomInAct, SIGNAL(triggered()), this, SLOT(zoomIn()));

    zoomOutAct = new QAction(tr("Zoom &Out (25%)"), this);
    zoomOutAct->setShortcut(tr("Ctrl+-"));
    connect(zoomOutAct, SIGNAL(triggered()), this, SLOT(zoomOut()));

    resetLocationAct = new QAction(tr("&Reset Location \\& scale"), this);
    resetLocationAct->setShortcut(tr("Ctrl+0"));
    connect(resetLocationAct, SIGNAL(triggered()), this, SLOT(resetLocation()));
}

void ImageViewer::createMenus() {
    fileMenu = new QMenu(tr("&File"), this);
    fileMenu->addAction(openAct);
    fileMenu->addAction(saveAct);
    fileMenu->addAction(exportPackageAct);
    \
    editMenu = new QMenu(tr("&Edit"), this);
    editMenu->addAction(undoAct);
    editMenu->addAction(redoAct);
    editMenu->addAction(switchToolAct);
    editMenu->addAction(deleteBlockAct);

    viewMenu = new QMenu(tr("&View"), this);
    viewMenu->addAction(zoomInAct);
    viewMenu->addAction(zoomOutAct);
    viewMenu->addAction(resetLocationAct);

    menuBar()->addMenu(fileMenu);
    menuBar()->addMenu(editMenu);
    menuBar()->addMenu(viewMenu);
}

void ImageViewer::loadFile(QString const &fileName) {
    QImage newImage(fileName);
    if (newImage.isNull()) {
        QMessageBox::information(this, tr("Image Viewer"),
                                 tr("Cannot load %1.").arg(fileName));
        return;
    }
    save();

    QDir dir_old(imageFileName);
    bool flag_old = dir_old.cdUp();
    QDir dir_new(fileName);
    dir_new.cdUp();
    if (flag_old == false || dir_old.absolutePath() != dir_new.absolutePath()) {
        imageFolder = dir_new;
        QStringList filters;
        filters << "*.jpg" << "*.png" << "*.bmp" << "*.jpeg" << "*.gif";
        imagesInFolder = dir_new.entryList(filters, QDir::Files | QDir::Readable);
    }

    imageFileName = fileName;
    setWindowTitle(tr("[第%1/%2张] %3").arg(1 + imagesInFolder.indexOf(QFileInfo(imageFileName).fileName())).
                   arg(imagesInFolder.size()).arg(QFileInfo(imageFileName).fileName()));
    image = newImage;
    resetLocation();
    updateScaledImage();
    resetHistory();
    QFile file(annotationFileName(imageFileName));
    if (file.open(QIODevice::ReadOnly)) {
        QDataStream stream(&file);
        QByteArray annoArray, historyArray;
        stream >> annoArray;
        annoArray = qUncompress(annoArray);
        QDataStream::Status status1 = fromQByteArray(annoArray, anno);
        stream >> historyArray;
        historyArray = qUncompress(historyArray);
        QDataStream::Status status2 = fromQByteArray(historyArray, history);
        file.close();
        if (status1 != QDataStream::Ok || status2 != QDataStream::Ok) {
            file.copy(file.fileName() + ".bak");
            QMessageBox::information(this, tr("Image Viewer"),
                                     tr("File is corrupted, data will be cleared: %1").arg(file.fileName()));
            if (status1 != QDataStream::Ok) {
                if (status2 == QDataStream::Ok && !history.isEmpty())
                    anno = history.back();
                else
                    resetHistory();
            }
            if (status2 != QDataStream::Ok) {
                history.clear();
                history.append(ImageAnnotation());
                ImageAnnotation loadedAnno = anno;
                resetHistory();
                anno = loadedAnno;
                addHistoryPoint();
            }
        }
    }
    updateBlockList();
    update();
}

void ImageViewer::updateScaledImage() {
    if (image.isNull()) {
        scaledImage = image.copy();
        return;
    }
    scaledImage = image.scaled(QSize(image.width() * scaleFactor, image.height() * scaleFactor),
                               Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

void ImageViewer::resetHistory() {
    redoHistory.clear();
    history.clear();
    anno = ImageAnnotation();
    anno.onNewBlock();
    updateBlockList();
    history.push_back(anno);
    keepHistoryOnUndo = false;
}

void ImageViewer::addHistoryPoint(bool weak) {
    if (weak) {
        keepHistoryOnUndo = true;
        return;
    } else {
        keepHistoryOnUndo = false;
    }
    redoHistory.clear();
    history.append(anno);
}

void ImageViewer::updateBlockList() {
    listWidget->clear();
    for (int i = 0; i < anno.blocks.size(); i++) {
        BlockAnnotation const &block(anno.blocks[i]);
        QString text("");
        foreach (CharacterAnnotation const &charAnno, block.characters) {
            if (!text.isEmpty())
                text += " ";
            if (charAnno.text.isEmpty())
                text += tr("-");
            else
                text += charAnno.text;
        }
        if (text.isEmpty())
            text = tr("<empty>");
        listWidget->addItem(tr("%1. %2").arg(i + 1).arg(text));
    }
}

QPointF ImageViewer::toImageUV(QPoint screenUV) const {
    QPoint delta = screenUV - imageLeftTop;
    return QPointF(delta.x() / scaleFactor, delta.y() / scaleFactor);
}

QPointF ImageViewer::toScreenUV(QPointF imageUV) const {
    QPoint delta(qRound(imageUV.x() * scaleFactor), qRound(imageUV.y() * scaleFactor));
    return imageLeftTop + delta;
}

QPolygonF ImageViewer::toScreenPoly(QPolygonF const &poly) const {
    QPolygonF res;
    foreach (QPointF const &p, poly)
        res.append(toScreenUV(p));
    return res;
}

void ImageViewer::inputStringToAnnotation(int index) {
    Q_ASSERT(0 <= index && index < anno.blocks.size());
    int wordNeeded = anno.numWordNeeded(index);
    if (wordNeeded > 0) {
        bool ok;
        QString originText;
        foreach (CharacterAnnotation const &charAnno, anno.blocks[index].characters) {
            if (!charAnno.text.isEmpty()) {
                if (!originText.isEmpty())
                    originText += " ";
                originText += charAnno.text;
            }
        }
        QString text = QInputDialog::getText(this, tr("输入标注文字"), tr("输入%1个字（或英文单词）").arg(wordNeeded),
                                             QLineEdit::Normal, originText, &ok);
        if (ok) {
            anno.onInputString(text, index);
            update();
            if (anno.isStringOk(index)) {
                if (index == anno.blocks.size() - 1)
                    anno.onNewBlock();
            } else {
                QMessageBox::information(this, tr("Image Viewer"), tr("输入字数不足"));
            }
            addHistoryPoint();
            updateBlockList();
            update();
        }
    }
}

QString ImageViewer::annotationFileName(QString const &imageFileName) const {
    if (imageFileName.isEmpty())
        return QString();
    return imageFolder.filePath(QFileInfo(imageFileName).completeBaseName() + ".array");
}

template <typename T>
QByteArray ImageViewer::toQByteArray(T const &t) {
    QByteArray array;
    QDataStream stream(&array, QIODevice::WriteOnly | QIODevice::Unbuffered);
    stream << t;
    return array;
}

template <typename T>
QDataStream::Status ImageViewer::fromQByteArray(QByteArray &array, T &t) {
    QDataStream stream(&array, QIODevice::ReadOnly);
    stream >> t;
    return stream.status();
}

void ImageViewer::open() {
    QString fileName = QFileDialog::getOpenFileName(this);
    if (!fileName.isEmpty()) {
        loadFile(fileName);
    }
}

void ImageViewer::save() {
    if (imageFileName.isEmpty())
        return;
    QFile file(annotationFileName(imageFileName));
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::information(this, tr("Image Viewer"),
                                 tr("Cannot write %1.").arg(file.fileName()));
        return;
    }
    QDataStream stream(&file);
    stream << qCompress(toQByteArray(anno));
    stream << qCompress(toQByteArray(history));
    file.close();
}

void ImageViewer::exportPackage() {
    save();
    QString fileName = QFileDialog::getSaveFileName(this, QString(), imageFolder.filePath("annotation.package"));
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        QStringList exportList;
        if (!file.open(QIODevice::WriteOnly)) {
            QMessageBox::information(this, tr("Image Viewer"),
                                     tr("Cannot write %1.").arg(file.fileName()));
            return;
        }
        QDataStream stream(&file);
        for (QStringList::Iterator it = imagesInFolder.begin(); it != imagesInFolder.end(); it++) {
            QFile f(annotationFileName(imageFolder.filePath(*it)));
            if (f.open(QIODevice::ReadOnly)) {
                QFileInfo fileInfo(f);
                stream << fileInfo.completeBaseName();
                stream << f.readAll();
                f.close();
                exportList << fileInfo.fileName();
            }
        }
        file.close();
        QString info = tr("导出了 %1 张图片的标注：").arg(exportList.size());
        info += "\n";
        foreach (QString const &s, exportList)
            info += s + ", ";
        QMessageBox::information(this, tr("Image Viewer"), info);
    }
}

void ImageViewer::undo() {
    if (drawingLabel)
        return;
    if (keepHistoryOnUndo) {
        anno = history.back();
        keepHistoryOnUndo = false;
        update();
        return;
    }
    if (history.size() <= 1)
        return;
    redoHistory.push_back(history.back());
    history.pop_back();
    anno = history.back();
    anno.onPendingPoint(toImageUV(mapFromGlobal(cursor().pos())), shiftPressed);
    updateBlockList();
    update();
}

void ImageViewer::redo() {
    if (drawingLabel)
        return;
    if (redoHistory.empty())
        return;
    history.push_back(redoHistory.back());
    redoHistory.pop_back();
    anno = history.back();
    anno.onPendingPoint(toImageUV(mapFromGlobal(cursor().pos())), shiftPressed);
    updateBlockList();
    update();
}

void ImageViewer::switchTool() {
    anno.onSwitchTool();
    addHistoryPoint(true);
    update();
}

void ImageViewer::deleteBlock() {
    if (!listWidget->selectedItems().isEmpty()) {
        int selectedBlock = listWidget->row(listWidget->selectedItems().first());
        if (selectedBlock >= 0 && selectedBlock < anno.blocks.size()) {
            if (selectedBlock == anno.blocks.size() - 1)
                anno.onNewBlock();
            anno.blocks.remove(selectedBlock);
            addHistoryPoint();
            updateBlockList();
            update();
        }
    }
}

void ImageViewer::zoomIn() {
    if (scaleFactor > 2.0)
        return;
    QPoint mousePos = mapFromGlobal(cursor().pos());
    qreal oldFactor = scaleFactor;
    scaleFactor /= 0.8;
    imageLeftTop = (imageLeftTop - mousePos) / oldFactor * scaleFactor + mousePos;
    updateScaledImage();
    update();
}

void ImageViewer::zoomOut() {
    if (scaleFactor < 0.2)
        return;
    QPoint mousePos = mapFromGlobal(cursor().pos());
    qreal oldFactor = scaleFactor;
    scaleFactor *= 0.8;
    imageLeftTop = (imageLeftTop - mousePos) / oldFactor * scaleFactor + mousePos;
    updateScaledImage();
    update();
}

void ImageViewer::setLocation(QPoint loc) {
    imageLeftTop = loc;
    update();
}

void ImageViewer::resetLocation() {
    resetLocation(size());
}

void ImageViewer::resetLocation(QSize size) {
    int w = size.width();
    int h = size.height() - menuBar()->height();
    if (image.isNull()) {
        scaleFactor = 1;
        updateScaledImage();
        setLocation(QPoint(0, 0));
    } else {
        scaleFactor = qMin((qreal)w / image.width(), (qreal)h / image.height());
        updateScaledImage();
        setLocation(QPoint((w - scaledImage.width()) / 2, menuBar()->height() + (h - scaledImage.height()) / 2));
    }
    update();
}

void ImageViewer::onListWidgetSelect() {
    update();
}

void ImageViewer::onListWidgetDoubleClicked(QModelIndex index) {
    if (index.isValid() && 0 <= index.row() && index.row() < anno.blocks.size()) {
        inputStringToAnnotation(index.row());
    }
}

/// QSoftSelectListWidget

QSoftSelectListWidget::QSoftSelectListWidget(QWidget *parent): QListWidget(parent) {
    setMouseTracking(true);
}

QSoftSelectListWidget::~QSoftSelectListWidget() { }

void QSoftSelectListWidget::mouseMoveEvent(QMouseEvent *e) {
    setCurrentRow(-1);
    setCurrentItem(itemAt(e->pos()));
    QListWidget::mouseMoveEvent(e);
}

void QSoftSelectListWidget::leaveEvent(QEvent *e) {
    clearSelection();
    QListWidget::leaveEvent(e);
}
