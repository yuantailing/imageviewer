#include <QtWidgets>
#include "imageviewer.h"

ImageViewer::ImageViewer(QWidget *parent)
    : QMainWindow(parent)
{
    createActions();
    createMenus();

    centralWidget = new QWidget(this);
    resetHistory();
    image = new QImage;
    scaledImage = new QImage;
    applyImage(QImage("148.jpg"));           //! Test
    controlPressed = false;
    mousePressed = false;
    mouseLastPos = QPoint(0, 0);
    draggingImage = false;
    drawingLabel = false;

    setWindowTitle(tr("Image Viewer"));
    setCentralWidget(centralWidget);
    resize(1000, 800);
}

ImageViewer::~ImageViewer() {
    delete image;
    delete scaledImage;
}

void ImageViewer::paintEvent(QPaintEvent *) {
    QPainter painter;
    painter.begin(this);
    // 绘制图片
    painter.drawImage(imageLeftTop, *scaledImage);

    // 绘制辅助图层
    painter.setOpacity(0.3);
    painter.setPen(QPen(Qt::blue, 3.0));
    painter.setBrush(QBrush(Qt::yellow));
    QVector<QPolygonF> helperPoly = anno.getHelperPoly();
    foreach (QPolygonF const &polyImage, helperPoly) {
        QPolygonF polyScreen;
        foreach (QPointF p, polyImage)
            polyScreen.append(toScreenUV(p));
        painter.drawPolygon(polyScreen);
    }

    // 绘制正在标注的字符
    painter.setOpacity(0.3);
    painter.setPen(QPen(Qt::green));
    painter.setBrush(QBrush(Qt::red));
    QVector<QPolygonF> pendingCharacterPoly = anno.getPendingCharacterPoly();
    foreach (QPolygonF const &polyImage, pendingCharacterPoly) {
        QPolygonF polyScreen;
        foreach (QPointF p, polyImage)
            polyScreen.append(toScreenUV(p));
        painter.drawPolygon(polyScreen);
    }

    // 绘制字符区域
    QVector<CharacterAnnotation> charAnnoVec = anno.getCharacterAnnotation();
    foreach (CharacterAnnotation const &charAnno, charAnnoVec) {
        QPolygonF polyScreen;
        foreach (QPointF p, charAnno.box)
            polyScreen.append(toScreenUV(p));

        painter.setOpacity(0.3);
        painter.setPen(QPen(Qt::green));
        painter.setBrush(QBrush(Qt::red));
        painter.drawPolygon(polyScreen);

        painter.setOpacity(0.8);
        painter.setPen(QPen(Qt::black));
        painter.setBrush(QBrush(Qt::red));
        QRectF bounding = polyScreen.boundingRect();
        qreal fontSize = qMax(qMin(bounding.width(), bounding.height()) * 0.67, 5.0);
        painter.setFont(QFont("Arial", fontSize, QFont::Bold));
        painter.drawText(bounding, Qt::AlignCenter, charAnno.text);
    }

    painter.end();
}

void ImageViewer::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Control)
        controlPressed = true;
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        int wordNeeded = anno.numWordNeeded();
        if (wordNeeded == 0) {
            anno.onEnterPressed();
            update();
            wordNeeded = anno.numWordNeeded();
        }
        if (wordNeeded > 0) {
            bool ok;
            QString text = QInputDialog::getText(this, tr("输入标注文字"), tr("输入%1个字").arg(wordNeeded), QLineEdit::Normal, QString::null, &ok);
            if (ok) {
                anno.onInputString(text);
                update();
                if (anno.isStringOk())
                    anno.onNewBlock();
                else
                    QMessageBox::information(this, tr("Image Viewer"), tr("输入字数不足"));
                addHistoryPoint();
            }
        }
    }
}

void ImageViewer::keyReleaseEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Control)
        controlPressed = false;
}

void ImageViewer::wheelEvent(QWheelEvent *event) {
    if (controlPressed) {
        if (event->delta() > 0) {
            zoomIn();
        } else if (event->delta() < 0) {
            zoomOut();
        }
    }
}

void ImageViewer::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        mousePressed = true;
        if (controlPressed)
            draggingImage = true;
        else
            drawingLabel = true;
        mouseLastPos = event->pos();
    }
    if (drawingLabel) {
        anno.onStartPoint(toImageUV(event->pos()));
        update();
    }
}

void ImageViewer::mouseMoveEvent(QMouseEvent *event) {
    if (draggingImage) {
        setLocation(imageLeftTop + event->pos() - mouseLastPos);
    }
    mouseLastPos = event->pos();
    if (drawingLabel) {
        anno.onPendingPoint(toImageUV(event->pos()));
        update();
    }
}

void ImageViewer::mouseReleaseEvent(QMouseEvent *event) {
    if (drawingLabel) {
        anno.onEndPoint(toImageUV(event->pos()));
        addHistoryPoint();
        update();
    }
    if (event->button() == Qt::LeftButton) {
        mousePressed = false;
        draggingImage = false;
        drawingLabel = false;
    }
}

void ImageViewer::createActions() {
    openAct = new QAction(tr("&Open..."), this);
    openAct->setShortcut(tr("Ctrl+O"));
    connect(openAct, SIGNAL(triggered()), this, SLOT(open()));

    undoAct = new QAction(tr("&Undo"), this);
    undoAct->setShortcut(tr("Ctrl+Z"));
    connect(undoAct, SIGNAL(triggered()), this, SLOT(undo()));

    redoAct = new QAction(tr("&Redo"), this);
    redoAct->setShortcuts({tr("Ctrl+Y"), tr("Ctrl+Shift+Z")});
    connect(redoAct, SIGNAL(triggered()), this, SLOT(redo()));

    switchToolAct = new QAction(tr("&Switch tool"), this);
    switchToolAct->setShortcut(tr("Tab"));
    connect(switchToolAct, SIGNAL(triggered()), this, SLOT(switchTool()));

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
    \
    editMenu = new QMenu(tr("&Edit"), this);
    editMenu->addAction(undoAct);
    editMenu->addAction(redoAct);
    editMenu->addAction(switchToolAct);

    viewMenu = new QMenu(tr("&View"), this);
    viewMenu->addAction(zoomInAct);
    viewMenu->addAction(zoomOutAct);
    viewMenu->addAction(resetLocationAct);

    menuBar()->addMenu(fileMenu);
    menuBar()->addMenu(editMenu);
    menuBar()->addMenu(viewMenu);
}

void ImageViewer::applyImage(const QImage &im) {
    *image = im;
    resetLocation();
    updateScaledImage();
    resetHistory();
    update();
}

void ImageViewer::updateScaledImage() {
    *scaledImage = image->scaled(QSize(image->width() * scaleFactor, image->height() * scaleFactor),
                                 Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

void ImageViewer::resetHistory() {
    redoHistory.clear();
    history.clear();
    anno = ImageAnnotation();
    history.push_back(anno);
}

void ImageViewer::addHistoryPoint() {
    redoHistory.clear();
    history.append(anno);
}

QPointF ImageViewer::toImageUV(QPoint screenUV) {
    QPoint delta = screenUV - imageLeftTop;
    return QPointF(delta.x() / scaleFactor, delta.y() / scaleFactor);
}

QPoint ImageViewer::toScreenUV(QPointF imageUV) {
    QPoint delta(qRound(imageUV.x() * scaleFactor), qRound(imageUV.y() * scaleFactor));
    return imageLeftTop + delta;
}

void ImageViewer::open() {
    QString fileName = QFileDialog::getOpenFileName(this,
                                    tr("Open File"), QDir::currentPath());
    if (!fileName.isEmpty()) {
        QImage image(fileName);
        if (image.isNull()) {
            QMessageBox::information(this, tr("Image Viewer"),
                                     tr("Cannot load %1.").arg(fileName));
            return;
        }
        applyImage(image);
    }
}

void ImageViewer::undo() {
    if (drawingLabel)
        return;
    if (history.size() <= 1)
        return;
    redoHistory.push_back(history.back());
    history.pop_back();
    anno = history.back();
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
    update();
}

void ImageViewer::switchTool() {
    anno.onSwitchTool();
    addHistoryPoint();
    update();
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
    if (scaleFactor < 0.1)
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
    scaleFactor = 0.4;
    updateScaledImage();
    setLocation(QPoint(0, 0));
    update();
}
