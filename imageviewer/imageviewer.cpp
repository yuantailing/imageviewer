#include <QtWidgets>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>
#include <functional>
#include "imageviewer.h"

/// ImageViewer

static QVector<QString> radioIdx2propname({
    "covered", "bgcomplex", "raised", "perspective", "wordart", "handwritten", "pass"
});


ImageViewer::ImageViewer(QWidget *parent)
    : QMainWindow(parent)
{
    createActions();
    createMenus();

    centralWidget = new QWidget(this);
    centralWidget->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    listWidget = new QListWidget(centralWidget);
    listWidget->setFocusPolicy(Qt::NoFocus);
    statusLabel = new QLabel(this);
    statusBar()->addWidget(statusLabel);
    statusBar()->setStyleSheet(QString("QStatusBar::item{border: 0px;background:#ffd;}"));
    connect(listWidget, SIGNAL(currentRowChanged(int)), this, SLOT(onListWidgetSelect()));
    connect(listWidget, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(onListWidgetDoubleClicked(QModelIndex)));

    QVBoxLayout *verticalLayout_1;
    QVBoxLayout *verticalLayout_2;
    QVBoxLayout *verticalLayout_3;
    QHBoxLayout *horizontalLayout_1;
    char const *propsText[sizeof(checkBoxProps) / sizeof(*checkBoxProps)] = {
            "(A)遮挡",
            "(S)背景复杂",
            "(D)变形/旋转",
            "(F)立体字（有厚度）",
            "(Z)艺术字",
            "(X)手写",
            "(C)标记为\"已标注属性\"",
    };
    propWidget = new QWidget;
    propWidget->setAutoFillBackground(true);
    propWidget->installEventFilter(this);
    verticalLayout_3 = new QVBoxLayout(propWidget);
    verticalLayout_3->setSpacing(6);
    verticalLayout_3->setContentsMargins(3, 3, 3, 3);
    verticalLayout_1 = new QVBoxLayout();
    verticalLayout_1->setSpacing(6);
    radioButtonAnno = new QRadioButton(propWidget);
    radioButtonAnno->setFocusPolicy(Qt::NoFocus);
    radioButtonAnno->setChecked(true);
    verticalLayout_1->addWidget(radioButtonAnno);
    radioButtonProp = new QRadioButton(propWidget);
    radioButtonProp->setFocusPolicy(Qt::NoFocus);
    verticalLayout_1->addWidget(radioButtonProp);

    horizontalLayout_1 = new QHBoxLayout();
    horizontalLayout_1->setSpacing(6);
    radioButtonInsp = new QRadioButton(propWidget);
    radioButtonInsp->setFocusPolicy(Qt::NoFocus);
    radioButtonInsp->setVisible(false);
    horizontalLayout_1->addWidget(radioButtonInsp);
    horizontalSliderInsp = new QSlider(propWidget);
    horizontalSliderInsp->setMinimum(32);
    horizontalSliderInsp->setMaximum(96);
    horizontalSliderInsp->setPageStep(5);
    horizontalSliderInsp->setValue(40);
    horizontalSliderInsp->setOrientation(Qt::Horizontal);
    horizontalSliderInsp->setFocusPolicy(Qt::NoFocus);
    horizontalSliderInsp->setVisible(false);
    connect(horizontalSliderInsp, SIGNAL(valueChanged(int)), this, SLOT(update()));
    horizontalLayout_1->addWidget(horizontalSliderInsp);
    verticalLayout_1->addLayout(horizontalLayout_1);

    verticalLayout_3->addLayout(verticalLayout_1);
    verticalLayout_2 = new QVBoxLayout();
    verticalLayout_2->setSpacing(6);

    for (size_t i = 0; i < sizeof(checkBoxProps) / sizeof(*checkBoxProps); i++) {
        checkBoxProps[i] = new QCheckBox(propWidget);
        checkBoxProps[i]->setFocusPolicy(Qt::NoFocus);
        verticalLayout_2->addWidget(checkBoxProps[i]);
        checkBoxProps[i]->setText(QApplication::translate("MainWindow", propsText[i], Q_NULLPTR));
        connect(checkBoxProps[i], SIGNAL(clicked(bool)), new PropCheckReciever(this, i), SLOT(changeCheckState(bool)));
    }
    verticalLayout_3->addLayout(verticalLayout_2);
    radioButtonAnno->setText(QApplication::translate("MainWindow", "标注", Q_NULLPTR));
    radioButtonProp->setText(QApplication::translate("MainWindow", "属性", Q_NULLPTR));
    radioButtonInsp->setText(QApplication::translate("MainWindow", "属性检查", Q_NULLPTR));
    connect(radioButtonAnno, SIGNAL(toggled(bool)), this, SLOT(update()));
    connect(radioButtonProp, SIGNAL(toggled(bool)), this, SLOT(update()));
    connect(radioButtonInsp, SIGNAL(toggled(bool)), this, SLOT(update()));

    controlPressed = false;
    shiftPressed = false;
    spacePressed = false;
    mousePressed = false;
    mouseLastPos = QPoint(0, 0);
    draggingImage = false;
    drawingLabel = false;
    annotationSuffix = QString("faces");
    resetHistory();
    changingPerspectiveHelper = false;
    selectedPerspectiveHelperIndex = -1;
    selectedBlockIndex = -1;
    selectedCharIndex = -1;

    setWindowTitle(tr("Image Viewer"));
    setCentralWidget(centralWidget);
    this->setMouseTracking(true);
    centralWidget->setMouseTracking(true);
    this->setAcceptDrops(true);
    this->setMinimumSize(480, 360);
    this->setWindowState(Qt::WindowMaximized);
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

    int prop_w = 150;
    int prop_h = 210;
    int gap_w = 5;
    propWidget->setGeometry(QRect(x1 - gap_w - prop_w, y1, prop_w, prop_h));

    QMainWindow::resizeEvent(event);
}

void ImageViewer::paintEvent(QPaintEvent *event) {
    QPainter painter;
    painter.begin(this);

    auto paintCharacter = [&](QPolygonF const &box, QString const &text, qreal polyOpacity, qreal charBgOpacity, qreal charOpacity,
            QColor penColor, QColor brushColor, QColor charColor, qreal penWidth) {
        QPolygonF polyScreen = toScreenPoly(box);

        // 字符包围盒
        painter.setOpacity(polyOpacity);
        painter.setPen(QPen(penColor, penWidth));
        painter.setBrush(Qt::NoBrush);
        painter.drawPolygon(polyScreen);

        // 打印文字
        if (!text.isEmpty()) {
            painter.setOpacity(charBgOpacity);
            painter.setPen(Qt::NoPen);
            painter.setBrush(brushColor);
            painter.drawPolygon(polyScreen);

            painter.setOpacity(charOpacity);
            painter.setPen(QPen(charColor));
            painter.setBrush(Qt::NoBrush);
            QRectF bounding = polyScreen.boundingRect();
            qreal fontSize = qMax(qMin(bounding.width() / text.length(), bounding.height()) * 0.67, 5.0);
            painter.setFont(QFont("Arial", fontSize, QFont::Bold));
            painter.drawText(bounding, Qt::AlignCenter, text);
        }
    };

    if (radioButtonAnno->isChecked()) {
        // 绘制图片
        if (!scaledImage.isNull())
            painter.drawImage(imageLeftTop, scaledImage);

        // 确定选中的Block
        int listSelectedBlock = -1;
        if (!listWidget->selectedItems().isEmpty())
            listSelectedBlock = listWidget->row(listWidget->selectedItems().first());

        if (!controlPressed || !shiftPressed)
            for (int i = 0; i < anno.blocks.size(); i++) {
                BlockAnnotation const &block(anno.blocks[i]);
                qreal polyOpacity = i == listSelectedBlock ? 1.0 : 0.6;
                qreal charBgOpacity = i == listSelectedBlock ? 0.3 : 0.07;
                qreal charOpacity = i == listSelectedBlock ? 1.0 : 1.0;

                // 绘制辅助图层
                if (i == listSelectedBlock) {
                    painter.setOpacity(0.3);
                    painter.setPen(Qt::NoPen);
                    painter.setBrush(Qt::white);
                    foreach (QPolygonF const &poly, block.getHelperPoly())
                        painter.drawPolygon(toScreenPoly(poly));
                }
                painter.setOpacity(polyOpacity);
                painter.setPen(QPen(Qt::blue, 3.0));
                painter.setBrush(Qt::NoBrush);
                foreach (QPolygonF const &poly, block.getHelperPoly())
                    painter.drawPolygon(toScreenPoly(poly));

                // 绘制正在标注的字符区域
                painter.setOpacity(polyOpacity);
                painter.setPen(QPen(Qt::green, 1.0));
                painter.setBrush(Qt::NoBrush);
                foreach (QPolygonF const &poly, block.getPendingCharacterPoly())
                    painter.drawPolygon(toScreenPoly(poly));

                // 绘制已标注的字符区域
                foreach (CharacterAnnotation const &charAnno, block.getCharacterAnnotation()) {
                    QString text = charAnno.text;
                    QColor penColor = Qt::green;
                    qreal penWidth = 1.0;
                    if (0 != charAnno.props.value("mask", 0)) {
                        penColor = Qt::red;
                        penWidth = 2.0;
                        text = " ";
                        charBgOpacity = i == listSelectedBlock ? 0.3 : 0;
                    }
                    paintCharacter(charAnno.box, text, polyOpacity, charBgOpacity, charOpacity, penColor, Qt::yellow, Qt::black, penWidth);
                }
            }

        // 绘制用户关注的焦点（上一次鼠标点击位置）
        if (!controlPressed || !shiftPressed) {
            if ((anno.focusPoint - QPointF(0, 0)).manhattanLength() > 0.001) {
                painter.setOpacity(0.8);
                painter.setPen(QPen(Qt::red));
                QPointF foc = toScreenUV(anno.focusPoint);
                qreal len = 5.0;
                QLineF lineH = QLineF(foc + QPointF(-len, 0), foc + QPointF(len, 0));
                QLineF lineV = QLineF(foc + QPointF(0, -len), foc + QPointF(0, len));
                painter.drawLine(lineH);
                painter.drawLine(lineV);
            }
        }

        // 绘制用户选中的编辑点
        if (selectedPerspectiveHelperIndex >= 0) {
            QPointF *points = lastPerspectiveHelperPoints();
            if (points) {
                QPointF p = points[selectedPerspectiveHelperIndex];
                p = toScreenUV(p);
                painter.setOpacity(0.8);
                painter.setPen(QPen(Qt::red));
                painter.drawEllipse(p, 6.0, 6.0);
            }
        }

        // 绘制Tips
        statusLabel->setText(anno.getTips());
    } else if (radioButtonProp->isChecked()) {
        // 绘制图片
        if (!scaledImage.isNull())
            painter.drawImage(imageLeftTop, scaledImage);

        if (!controlPressed || !shiftPressed)
            for (int i = 0; i < anno.blocks.size(); i++) {
                BlockAnnotation const &block(anno.blocks[i]);
                qreal polyOpacity = 0.8;
                qreal bgOpacity = 0.15;

                // 绘制辅助图层
                painter.setOpacity(polyOpacity);
                painter.setPen(QPen(Qt::blue, 3.0));
                painter.setBrush(Qt::NoBrush);
                foreach (QPolygonF const &poly, block.getHelperPoly())
                    painter.drawPolygon(toScreenPoly(poly));

                // 绘制正在标注的字符区域
                painter.setOpacity(polyOpacity);
                painter.setPen(QPen(Qt::green, 1.0));
                painter.setBrush(Qt::NoBrush);
                foreach (QPolygonF const &poly, block.getPendingCharacterPoly())
                    painter.drawPolygon(toScreenPoly(poly));

                // 绘制已标注的字符区域
                for (int j = 0; j < block.characters.size(); j++) {
                    CharacterAnnotation const &charAnno(block.characters[j]);
                    QColor aroundColor(0 == charAnno.props.value("mask", 0) ?
                                           0 == charAnno.props.value("pass", 0) ? Qt::green : Qt::blue : Qt::red);
                    if (i == selectedBlockIndex && (-1 == selectedCharIndex || j == selectedCharIndex)) {
                        painter.setOpacity(bgOpacity);
                        painter.setPen(Qt::NoPen);
                        painter.setBrush(Qt::yellow);
                        painter.drawPolygon(toScreenPoly(charAnno.box));
                        painter.setOpacity(polyOpacity);
                        painter.setPen(QPen(aroundColor, 2.0));
                        painter.setBrush(Qt::NoBrush);
                        painter.drawPolygon(toScreenPoly(charAnno.box));
                    } else {
                        painter.setOpacity(polyOpacity);
                        painter.setPen(QPen(aroundColor, 1.0));
                        painter.setBrush(Qt::NoBrush);
                        painter.drawPolygon(toScreenPoly(charAnno.box));
                    }
                }
            }
        statusLabel->setText(QString(""));
    } else if (radioButtonInsp->isChecked()) {
        int xy_char = horizontalSliderInsp->value();
        int xy_gap = 8;
        int y_off_base = 30;
        int x_off = 0, y_off = y_off_base - respDisplayYOff;
        int x_top = 0, y_top = 0;
        respRegions.clear();
        auto eachChar = [&](std::function<void(CharacterAnnotation const &, int, int)> cb) {
            for (int i = 0; i < anno.blocks.size(); i++) {
                BlockAnnotation &block(anno.blocks[i]);
                for (int j = 0; j < block.characters.size(); j++) {
                    cb(block.characters[j], i, j);
                }
            }
        };
        auto addGroup = [&](std::function<bool(CharacterAnnotation const &)> ex, QString const &showText) {
            // 属性文本
            if (x_top != 0) {
                x_top = 0;
                y_top++;
            }
            int x_start = x_off + x_top * (xy_char + xy_gap);
            int y_start = y_off + y_top * (xy_char + xy_gap);
            painter.setPen(Qt::black);
            QTextOption opt;
            opt.setAlignment(Qt::AlignCenter);
            painter.drawText(QRectF(x_start, y_start, xy_char, xy_char), showText, opt);
            x_top++;

            // 含有属性的字的图片
            eachChar([&](CharacterAnnotation const &ch, int i, int j) {
                if (ex(ch)) {
                    QRectF bounding(ch.box.boundingRect());
                    qreal x = bounding.x();
                    qreal y = bounding.y();
                    qreal w = bounding.width();
                    qreal h = bounding.height();
                    x -= 1.0;
                    y -= 1.0;
                    w += 2.0;
                    h += 2.0;
                    if (w <= h) {
                        x = x + w / 2 - h / 2;
                        w = h;
                    } else {
                        y = y + h / 2 - w / 2;
                        h = w;
                    }
                    QRect bound(round(x), round(y), round(w), round(h));
                    QImage cropped(image.copy(bound));
                    QImage resized = cropped.scaled(QSize(xy_char, xy_char),
                                                    Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                    int x_start = x_off + x_top * (xy_char + xy_gap);
                    int y_start = y_off + y_top * (xy_char + xy_gap);
                    painter.drawImage(x_start, y_start, resized);
                    QPolygonF poly;
                    foreach (QPointF const &p, ch.box)
                        poly.append(QPointF((p.x() - x) / bound.width() * xy_char + x_start,
                                            (p.y() - y) / bound.width() * xy_char + y_start));
                    painter.setPen(Qt::green);
                    painter.setBrush(Qt::NoBrush);
                    painter.drawPolygon(poly);
                    if (i == selectedBlockIndex && (-1 == selectedCharIndex || j == selectedCharIndex)) {
                        painter.setPen(QPen(Qt::red, 3));
                        painter.setBrush(Qt::NoBrush);
                        painter.drawRect(QRect(x_start - 2, y_start - 2, xy_char + 3, xy_char + 3));
                    }
                    respRegions.append(qMakePair(QRect(x_start, y_start, xy_char, xy_char), QPoint(i, j)));
                    x_top++;
                    if (x_top * (xy_char + xy_gap) + 390 >= width()) {
                        x_top = 0;
                        y_top++;
                    }
                }
            });
        };

        for (int i = 0; i < radioIdx2propname.size() - 1; i++) {
            QString prop_str(radioIdx2propname[i]);
            QMap<QString, QString> trans({{"covered", "遮挡"},
                                          {"bgcomplex", "背景复杂"},
                                          {"raised", "变形"},
                                          {"perspective", "立体字"},
                                          {"wordart", "艺术字"},
                                          {"handwritten", "手写"},
                                         });
            QString translated = trans.contains(prop_str) ? trans[prop_str] : prop_str;
            addGroup([&](CharacterAnnotation const &ch) {
                return 1 == ch.props.value(prop_str, 0) && 0 == ch.props.value("mask", 0) && ch.text != "*";
            }, translated);
        }
        addGroup([&](CharacterAnnotation const &ch) {
            if (1 == ch.props.value("mask", 0) || ch.text == "*" || 0 == ch.props.value("pass", 0))
                return false;
            for (int i = 0; i < radioIdx2propname.size() - 1; i++) {
                QString prop_str(radioIdx2propname[i]);
                if (1 == ch.props.value(prop_str, 0))
                    return false;
            }
            return true;
        }, "不包含上述属性");
        addGroup([&](CharacterAnnotation const &ch) {
            return ch.text == "*";
        }, "*");
        addGroup([&](CharacterAnnotation const &ch) {
            return 0 == ch.props.value("pass", 0) && 0 == ch.props.value("mask", 0) && ch.text != "*";
        }, "未标注属性");
        addGroup([&](CharacterAnnotation const &) {
            return false;
        }, "(底部)");
        respDisplayYMaxOff = qMax(0, y_off_base + (y_top + 2) * (xy_char + xy_gap) - height());
    }

    painter.end();
    updatePropsCheckBox();
    updateBlockList();
    QMainWindow::paintEvent(event);
}

void ImageViewer::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Control) {
        controlPressed = true;
        update();
    } else if (event->key() == Qt::Key_Space) {
        spacePressed = true;
    } else if (event->key() == Qt::Key_Shift) {
        shiftPressed = true;
        updatePendingAnnotation();
    } else if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right) {
        int current = imagesInFolder.indexOf(QFileInfo(imageFileName).fileName());
        if (event->key() == Qt::Key_Left) {
            if (current > 0)
                loadFile(imageFolder.filePath(imagesInFolder[current - 1]));
        } else {
            if (current >= 0 && current + 1 < imagesInFolder.size())
                loadFile(imageFolder.filePath(imagesInFolder[current + 1]));
        }
    } else if (event->key() == Qt::Key_F8 || event->key() == Qt::Key_F12) {
        static bool last_key_is_F8 = false;
        if (last_key_is_F8 && event->key() == Qt::Key_F12) {
            radioButtonInsp->setVisible(true);
            horizontalSliderInsp->setVisible(true);
        }
        last_key_is_F8 = event->key() == Qt::Key_F8;
    } else if (radioButtonAnno->isChecked()) {
        // do nothing
    } else if (radioButtonProp->isChecked()) {
        QMap<int, int> key2index({{Qt::Key_A, 0},
                                  {Qt::Key_S, 1},
                                  {Qt::Key_D, 2},
                                  {Qt::Key_F, 3},
                                  {Qt::Key_Z, 4},
                                  {Qt::Key_X, 5},
                                  {Qt::Key_C, 6},
                                 });
        int index = key2index.value(event->key(), -1);
        if (-1 != index)
            changePropStatus(index, checkBoxProps[index]->checkState() != Qt::Checked);
    }
    QMainWindow::keyPressEvent(event);
}

void ImageViewer::keyReleaseEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Control) {
        controlPressed = false;
        update();
    } else if (event->key() == Qt::Key_Space) {
        spacePressed = false;
    } else if (event->key() == Qt::Key_Shift) {
        shiftPressed = false;
        updatePendingAnnotation();
    }
    QMainWindow::keyReleaseEvent(event);
}

void ImageViewer::wheelEvent(QWheelEvent *event) {
    if (radioButtonAnno->isChecked() || radioButtonProp->isChecked()) {
        if (event->delta() > 0) {
            zoomIn();
        } else if (event->delta() < 0) {
            zoomOut();
        }
    } else if (radioButtonInsp->isChecked()) {
        if (event->delta() > 0) {
            respDisplayYOff = qMax(0, respDisplayYOff - 80);
        } else if (event->delta() < 0) {
            respDisplayYOff = qMin(respDisplayYMaxOff, respDisplayYOff + 80);
        }
        update();
    }
    QMainWindow::wheelEvent(event);
}

void ImageViewer::mousePressEvent(QMouseEvent *event) {
    auto selectBlock = [&](bool wholeBlock) {
        QPointF p(toImageUV(event->pos()));
        selectedBlockIndex = selectedCharIndex = -1;
        qreal mindist = 0;
        for (int i = 0; i < anno.blocks.size(); i++) {
            BlockAnnotation const &blockAnno(anno.blocks[i]);
            for (int j = 0; j < blockAnno.characters.size(); j++) {
                CharacterAnnotation const &charAnno(blockAnno.characters[j]);
                if (charAnno.box.containsPoint(p, Qt::OddEvenFill)) {
                    qreal dist = (p - charAnno.box.boundingRect().center()).manhattanLength();
                    if (-1 == selectedBlockIndex || dist < mindist) {
                        selectedBlockIndex = i;
                        if (!wholeBlock)
                            selectedCharIndex = j;
                        mindist = dist;
                    }
                }
            }
        }
    };
    auto inspSelectBlock = [&](bool wholeBlock) {
        selectedBlockIndex = selectedCharIndex = -1;
        for (QPair<QRect, QPoint> const &p: respRegions) {
            if (p.first.contains(event->pos(), false)) {
                selectedBlockIndex = p.second.x();
                if (!wholeBlock)
                    selectedCharIndex = p.second.y();
                break;
            }
        }
    };

    if (event->button() == Qt::LeftButton) {
        mousePressed = true;
        if (controlPressed || spacePressed) {
            draggingImage = true;
        } else if (radioButtonAnno->isChecked()) {
            if (selectedPerspectiveHelperIndex >= 0) {
                changingPerspectiveHelper = true;
            } else {
                drawingLabel = true;
                anno.onStartPoint(toImageUV(event->pos()), scaleFactor, shiftPressed);
                if (!anno.blocks.empty() && anno.blocks.last().perspectiveHelper.numPoint == 4) {
                    anno.onNewBlock();
                }
                update();
            }
        }
        if (radioButtonProp->isChecked()) {
            draggingImage = true;
            selectBlock(false);
            listWidget->setCurrentRow(selectedBlockIndex);
            update();
        }
        if (radioButtonInsp->isChecked()) {
            inspSelectBlock(false);
            listWidget->setCurrentRow(selectedBlockIndex);
            update();
        }
    }
    if (event->button() == Qt::RightButton) {
        if (radioButtonProp->isChecked()) {
            selectBlock(true);
            listWidget->setCurrentRow(selectedBlockIndex);
            update();
        }
        if (radioButtonInsp->isChecked()) {
            inspSelectBlock(true);
            listWidget->setCurrentRow(selectedBlockIndex);
            update();
        }
    }
    mouseLastPos = event->pos();
    QMainWindow::mousePressEvent(event);
}

void ImageViewer::mouseMoveEvent(QMouseEvent *event) {
    if (draggingImage) {
        setLocation(imageLeftTop + event->pos() - mouseLastPos);
    } else if (changingPerspectiveHelper) {
        Q_ASSERT(selectedPerspectiveHelperIndex >= 0);
        QPointF *points = lastPerspectiveHelperPoints();
        if (points) {
            anno.focusPoint = points[selectedPerspectiveHelperIndex] = toImageUV(event->pos());
        }
        update();
    } else if (radioButtonAnno->isChecked()) {
        selectedPerspectiveHelperIndex = -1;
        QPointF *points = lastPerspectiveHelperPoints();
        if (points) {
            for (int i = 0; i < 4; i++) {
                QPointF delta = event->pos() - toScreenUV(points[i]);
                if (QPointF::dotProduct(delta, delta) < 49.001) {
                    selectedPerspectiveHelperIndex = i;
                    break;
                }
            }
        }
        updatePendingAnnotation();
    }
    mouseLastPos = event->pos();
    QMainWindow::mouseMoveEvent(event);
}

void ImageViewer::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        if (draggingImage) {
            // do nothing
        } else if (changingPerspectiveHelper) {
            addHistoryPoint(2); // replace last history
            update();
        } else if (drawingLabel) {
            int status = anno.onEndPoint(toImageUV(event->pos()), scaleFactor, shiftPressed);
            if (status == 2) {
                // do nothing
            } else {
                addHistoryPoint(status == 1 ? 1 : 0); // status: 0 normal, 1 weak
                update();
            }
        }
        mousePressed = false;
        draggingImage = false;
        drawingLabel = false;
        changingPerspectiveHelper = false;
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

bool ImageViewer::eventFilter(QObject *watched, QEvent *event) {
    if (watched == propWidget) {
        if (nullptr != dynamic_cast<QMouseEvent *>(event))
            return true;
    }
    return false;
}

void ImageViewer::createActions() {
    openAct = new QAction(tr("&Open"), this);
    openAct->setShortcut(tr("Ctrl+O"));
    connect(openAct, SIGNAL(triggered()), this, SLOT(open()));

    saveAct = new QAction(tr("&Save"), this);
    saveAct->setShortcut(tr("Ctrl+S"));
    connect(saveAct, SIGNAL(triggered()), this, SLOT(save()));

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

    zoomInAct = new QAction(tr("Zoom &In"), this);
    zoomInAct->setShortcuts({tr("Ctrl++"), tr("Ctrl+=")});
    connect(zoomInAct, SIGNAL(triggered()), this, SLOT(zoomIn()));

    zoomOutAct = new QAction(tr("Zoom &Out"), this);
    zoomOutAct->setShortcut(tr("Ctrl+-"));
    connect(zoomOutAct, SIGNAL(triggered()), this, SLOT(zoomOut()));

    resetLocationAct = new QAction(tr("&Reset location / scale"), this);
    resetLocationAct->setShortcut(tr("Ctrl+0"));
    connect(resetLocationAct, SIGNAL(triggered()), this, SLOT(resetLocation()));
}

void ImageViewer::createMenus() {
    fileMenu = new QMenu(tr("&File"), this);
    fileMenu->addAction(openAct);
    fileMenu->addAction(saveAct);
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

    QString fileNameOld = imageFileName;
    QDir dir_old(imageFileName);
    bool flag_old = dir_old.cdUp();
    imageFileName = fileName;
    imageBaseName = QFileInfo(imageFileName).baseName();
    QDir dir_new(imageFileName);
    dir_new.cdUp();
    if (fileNameOld.isEmpty() || flag_old == false || dir_old.absolutePath() != dir_new.absolutePath()) {
        imageFolder = dir_new;
        QStringList filters;
        filters << "*.jpg" << "*.png" << "*.bmp" << "*.jpeg" << "*.gif";
        imagesInFolder = dir_new.entryList(filters, QDir::Files | QDir::Readable);
    }

    setWindowTitle(tr("[第%1/%2张] %3").arg(1 + imagesInFolder.indexOf(QFileInfo(imageFileName).fileName())).
                   arg(imagesInFolder.size()).arg(QFileInfo(imageFileName).fileName()));
    image = newImage;
    resetLocation();
    updateScaledImage();
    resetHistory();
    selectedBlockIndex = selectedCharIndex = -1;
    listWidget->setCurrentRow(-1);
    QFile file(annotationFileName(imageFileName));
    if (file.open(QIODevice::ReadOnly)) {
        QDataStream stream(&file);
        stream >> anno;
        bool okAnno = stream.status() == QDataStream::Ok;
        QByteArray array;
        stream >> array;
        array = qUncompress(array);
        QDataStream st2(&array, QIODevice::ReadOnly);
        st2 >> history;
        bool okHistory = stream.status() == QDataStream::Ok && st2.status() == QDataStream::Ok;
        file.close();
        if (!okHistory) {
            file.copy(file.fileName() + ".bak");
            QMessageBox::information(this, tr("Image Viewer"),
                                     tr("File is corrupted, data will be cleared: %1").arg(file.fileName()));
            if (!okAnno) {
                resetHistory();
            }
            ImageAnnotation loadedAnno = anno;
            resetHistory();
            anno = loadedAnno;
            addHistoryPoint();
        }
        historyMergeKey = "";
        respDisplayYMaxOff = 0;
        respDisplayYOff = 0;
    }
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

void ImageViewer::addHistoryPoint(int flag) {
    historyMergeKey = "";
    if (flag == 1) {
        keepHistoryOnUndo = true;
        return;
    } else if (flag == 2) {
        if (!history.isEmpty())
            history.pop_back();
        keepHistoryOnUndo = false;
    } else { // flag == 0
        keepHistoryOnUndo = false;
    }
    redoHistory.clear();
    history.append(anno);
}

void ImageViewer::addHistoryPoint(QString const &mergeKey) {
    if (!historyMergeKey.isEmpty() && mergeKey == historyMergeKey) {
        addHistoryPoint(2);
    } else {
        addHistoryPoint();
    }
    historyMergeKey = mergeKey;
}

void ImageViewer::changePropStatus(int index, bool checked) {
    if (index < 0 || radioIdx2propname.size() <= index)
        return;
    QString const &propname(radioIdx2propname[index]);
    if (selectedBlockIndex < 0 || anno.blocks.size() <= selectedBlockIndex)
        return;
    BlockAnnotation &blockAnno(anno.blocks[selectedBlockIndex]);
    if (selectedCharIndex == -1) {
        for (CharacterAnnotation &charAnno: blockAnno.characters) {
            if (checked)
                charAnno.props.insert(propname, 1);
            else
                charAnno.props.remove(propname);
            if (propname != "pass")
                charAnno.props.insert("pass", 1);
        }
    } else {
        if (selectedCharIndex < 0 || blockAnno.characters.size() <= selectedCharIndex)
            return;
        CharacterAnnotation &charAnno(blockAnno.characters[selectedCharIndex]);
        if (checked)
            charAnno.props.insert(propname, 1);
        else
            charAnno.props.remove(propname);
        if (propname != "pass")
            charAnno.props.insert("pass", 1);
    }
    addHistoryPoint(QString("changePropStatus::propblk=%1").arg(selectedBlockIndex));
    update();
}

void ImageViewer::updatePropsCheckBox() {
    for (size_t i = 0; i < sizeof(checkBoxProps) / sizeof(*checkBoxProps); i++)
        checkBoxProps[i]->setCheckState(Qt::Unchecked);
    if (selectedBlockIndex < 0 || anno.blocks.size() <= selectedBlockIndex)
        return;
    BlockAnnotation const &blockAnno(anno.blocks[selectedBlockIndex]);
    if (selectedCharIndex == -1) {
        for (int i = 0; i < radioIdx2propname.size(); i++) {
            if (i < (int)(sizeof(checkBoxProps) / sizeof(*checkBoxProps))) {
                QString const &propname(radioIdx2propname[i]);
                bool has0 = false, has1 = false;
                for (CharacterAnnotation const &charAnno: blockAnno.characters) {
                    if (0 == charAnno.props.value(propname, 0))
                        has0 = true;
                    else
                        has1 = true;
                }
                if (!has1)
                    checkBoxProps[i]->setCheckState(Qt::Unchecked);
                else if (has0)
                    checkBoxProps[i]->setCheckState(Qt::PartiallyChecked);
                else
                    checkBoxProps[i]->setCheckState(Qt::Checked);
            }
        }
    } else {
        if (selectedCharIndex < 0 || blockAnno.characters.size() <= selectedCharIndex)
            return;
        CharacterAnnotation const &charAnno(blockAnno.characters[selectedCharIndex]);
        for (int i = 0; i < radioIdx2propname.size(); i++) {
            if (i < (int)(sizeof(checkBoxProps) / sizeof(*checkBoxProps))) {
                QString const &propname(radioIdx2propname[i]);
                if (0 != charAnno.props.value(propname, 0))
                    checkBoxProps[i]->setCheckState(Qt::Checked);
            }
        }
    }

}

void ImageViewer::updateBlockList() {
    bool propsFinished = true;
    while (listWidget->count() > anno.blocks.size())
        listWidget->takeItem(listWidget->count() - 1);
    while (listWidget->count() < anno.blocks.size())
        listWidget->addItem("");
    for (int i = 0; i < anno.blocks.size(); i++) {
        BlockAnnotation const &block(anno.blocks[i]);
        QString text("");
        bool hasProps = true;
        foreach (CharacterAnnotation const &charAnno, block.characters) {
            if (!text.isEmpty())
                text += " ";
            if (charAnno.text.isEmpty())
                text += tr("-");
            else
                text += charAnno.text;
            if (0 == charAnno.props.value("pass", 0))
                hasProps = false;
        }
        if (block.characters.size() > 0 && 0 != block.characters.first().props.value("mask", 0)) {
            text = tr("<mask>");
        } else if (block.perspectiveHelper.numPoint == 0) {
            text = tr("<empty>");
        } else if (block.perspectiveHelper.numPoint == 4) {
            text = tr("Rect");
        } else {
            if (hasProps) {
                if (radioButtonProp->isChecked())
                    text = tr("● ") + text;
            } else {
                propsFinished = false;
            }
        }
        listWidget->item(i)->setText(tr("%1. %2").arg(i + 1).arg(text));
    }
    radioButtonProp->setText(propsFinished ? tr("属性（已全部标注）") : tr("属性"));
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

void ImageViewer::updatePendingAnnotation() {
    anno.onPendingPoint(toImageUV(mapFromGlobal(cursor().pos())), scaleFactor, shiftPressed);
    update();
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
        QString text = QInputDialog::getText(this, tr("输入标注文字"), tr("输入 %1 个字").arg(wordNeeded),
                                             QLineEdit::Normal, originText, &ok);
        if (ok) {
            QString inputRes = anno.onInputString(text, index);
            update();
            if (!inputRes.isEmpty()) {
                QMessageBox::information(this, tr("Image Viewer"), inputRes);
            } else if (!anno.isStringOk(index)) {
                QMessageBox::information(this, tr("Image Viewer"), tr("输入字数不足，或一个位置有多个字"));
            } else {
                if (index == anno.blocks.size() - 1)
                    anno.onNewBlock();
            }
            addHistoryPoint();
            update();
        }
    }
}

QString ImageViewer::annotationFileName(QString const &imageFileName) const {
    if (imageFileName.isEmpty())
        return QString();
    return imageFolder.filePath(QFileInfo(imageFileName).completeBaseName() + "." + annotationSuffix);
}

QPointF *ImageViewer::lastPerspectiveHelperPoints() {
    if (anno.blocks.size() > 0 &&
            anno.blocks.last().helperType == BlockAnnotation::PERSPECTIVE_HELPER &&
            anno.blocks.last().perspectiveHelper.numPoint == 4 &&
            anno.blocks.last().perspectiveHelper.stroking == false &&
            anno.blocks.last().characters.size() == 0) {
        return anno.blocks.last().perspectiveHelper.points;
    }
    return nullptr;
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
    QString fileName = QFileDialog::getOpenFileName(this, QString(), QString(), "Images (*.jpg *.png)");
    if (!fileName.isEmpty()) {
        loadFile(fileName);
    }
}

void ImageViewer::save() {
    if (imageFileName.isEmpty())
        return;
    QFile file0(annotationFileName(imageFileName));
    QFile file1(annotationFileName(imageFileName) + ".tmp");
    if (file1.exists()) {
        QMessageBox::information(this, tr("Image Viewer"),
                                 tr("Already exists %1.").arg(file1.fileName()));
        return;
    }
    if (!file1.open(QIODevice::WriteOnly)) {
        QMessageBox::information(this, tr("Image Viewer"),
                                 tr("Cannot write %1.").arg(file1.fileName()));
        return;
    }
    QDataStream stream(&file1);
    stream << anno;
    QByteArray array;
    QDataStream st2(&array, QIODevice::WriteOnly);
    st2 << history;
    stream << qCompress(array);
    file1.close();
    file0.remove();
    if (!file1.rename(file0.fileName())) {
        QMessageBox::information(this, tr("Image Viewer"),
                                 tr("Cannot rename %1.").arg(file0.fileName()));
        return;
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
    updatePendingAnnotation();
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
    updatePendingAnnotation();
    update();
}

void ImageViewer::switchTool() {
    anno.onSwitchTool();
    addHistoryPoint(1);
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
    if (0 <= listWidget->currentRow() && selectedBlockIndex != listWidget->currentRow()) {
        selectedBlockIndex = listWidget->currentRow();
        selectedCharIndex = -1;
    }
    update();
}

void ImageViewer::onListWidgetDoubleClicked(QModelIndex index) {
    if (index.isValid() && 0 <= index.row() && index.row() < anno.blocks.size()) {
        // do nothing
    }
}


/// PropCheckReciever

PropCheckReciever::PropCheckReciever(ImageViewer *parent, int idx): QObject(parent), idx(idx) { }

PropCheckReciever::~PropCheckReciever() { }

void PropCheckReciever::changeCheckState(bool checked) {
    ImageViewer *p = dynamic_cast<ImageViewer *>(parent());
    p->changePropStatus(idx, checked);
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
