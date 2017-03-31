#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <cmath>
#include <QVector>
#include <QMap>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    QFile file("predictions.json");
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::information(this, tr("Error"),
                                 tr("Cannot load %1.").arg(file.fileName()));
        exit(1);
    }
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonArray predictions = doc.array();
    file.close();

    file.setFileName("test_locators.json");
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::information(this, tr("Error"),
                                 tr("Cannot load %1.").arg(file.fileName()));
        exit(1);
    }
    doc = QJsonDocument::fromJson(file.readAll());
    QJsonArray test_locators = doc.array();
    file.close();

    qDebug() << predictions.size() << test_locators.size();
    if (predictions.size() != test_locators.size()) {
        QMessageBox::information(this, tr("Error"),
                                 tr("predictions.size() != test_locators.size()"));
        exit(1);
    }
    typedef QPair<QString, QPair<QRectF, QString> > Locator;
    QMap<QString, QVector<Locator> > ignoredLocators;
    for (int i = 0; i < test_locators.size(); i++) {
        bool isCorrect = predictions[i].toArray()[0].toBool();
        double prob = predictions[i].toArray()[1].toDouble();
        if (!isCorrect || prob < 0.95)
            continue;
        QString imgid = test_locators[i].toArray()[0].toString();
        QString text = test_locators[i].toArray()[2].toString();
        QJsonArray bbox = test_locators[i].toArray()[1].toArray();
        double left = bbox[0].toDouble();
        double top = bbox[1].toDouble();
        double w = bbox[2].toDouble();
        double h = bbox[3].toDouble();
        QRectF rect(QPointF(left, top), QSizeF(w, h));
        if (!ignoredLocators.contains(imgid))
            ignoredLocators[imgid] = QVector<Locator>();
        ignoredLocators[imgid].append(qMakePair(imgid, qMakePair(rect, text)));
    }
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

    file.setFileName("../ch.v2.jsonlines");
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::information(this, tr("Error"),
                                 tr("Cannot load %1.").arg(file.fileName()));
        exit(1);
    }
    int _ = 0;
    QVector<QPair<QString, QPair<QRectF, QString> > > all;
    QVector<QString> vecImgid;
    while (!file.atEnd()) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readLine());
        foreach (QString const &imgid, doc.object().keys()) {
            qDebug() << imgid;
            vecImgid.append(imgid);
            QJsonArray const &arr(doc.object()[imgid].toArray());
            for (int i = 0; i < arr.size(); i++) {
                QJsonArray const &arr2 = arr[i].toArray();
                for (int j = 0; j < arr2.size(); j++) {
                    QJsonObject const &obj(arr2[j].toObject());
                    QString text = obj["text"].toString();
                    QJsonArray box = obj["box"].toArray();
                    qreal xmin, xmax, ymin, ymax;
                    xmin = xmax = box[0].toArray()[0].toDouble();
                    ymin = ymax = box[0].toArray()[1].toDouble();
                    for (int k = 0; k < box.size(); k++) {
                        qreal x = box[k].toArray()[0].toDouble();
                        qreal y = box[k].toArray()[1].toDouble();
                        xmin = qMin(xmin, x);
                        xmax = qMax(xmax, x);
                        ymin = qMin(ymin, y);
                        ymax = qMax(ymax, y);
                    }
                    bool isIgnored = false;
                    Locator thisLocator(qMakePair(imgid, qMakePair(QRectF(xmin, ymin, xmax - xmin, ymax - ymin), text)));
                    if (ignoredLocators.contains(imgid))
                        foreach (Locator const &ignored, ignoredLocators[imgid])
                            if (isSame(thisLocator, ignored)) {
                                isIgnored = true;
                                break;
                            }
                    if (!isIgnored)
                        all.append(qMakePair(imgid, qMakePair(QRectF(xmin, ymin, xmax - xmin, ymax - ymin), text)));
                }
            }
        }
        if (++_ >= 100) break;
    }
    file.close();
    QMap<QString, int> charCountMap;
    for (int i = 0; i < all.size(); i++) {
        QString text = all[i].second.second;
        if (!charCountMap.contains(text))
            charCountMap[text] = 0;
        charCountMap[text]++;
    }
    QVector<QPair<QString, int> > charCount;
    for (QMap<QString, int>::iterator it = charCountMap.begin(); it != charCountMap.end(); it++) {
        charCount.push_back(qMakePair(it.key(), it.value()));
    }
    qSort(charCount.begin(), charCount.end(), [&](QPair<QString, int> const &a, QPair<QString, int> const &b) {
        return qMakePair(a.second, a.first) > qMakePair(b.second, b.first);
    });
    qDebug() << all.size();
    qDebug() << all[0];
    int max_small_size = 32;
    int max_big_size = 96;
    QMap<QString, QVector<QPair<QPair<QString, QPair<QRectF, QString> >, QPair<QImage, QImage> > > > chars;
    QString lastimgid;
    QImage lastimg;
    int numImgLoaded = 0;
    for (int i = 0; i < all.size(); i++) {
        QString imgid = all[i].first;
        QRectF box = all[i].second.first;
        QString text = all[i].second.second;
        if (imgid != lastimgid) {
            qDebug() << imgid << numImgLoaded;
            lastimg = QImage(QString("E:/tiger/char_renamed/%1/%2.jpg").arg(imgid[0]).arg(imgid));
            if (lastimg.isNull()) {
                QMessageBox::information(this, tr("Error"),
                                         tr("Cannot load %1.").arg(imgid));
                exit(1);
            }
            lastimgid = imgid;
            numImgLoaded++;
        }
        auto toRect = [](QRectF const &r) {
            qreal xmin = r.x();
            qreal ymin = r.y();
            qreal xmax = xmin + r.width();
            qreal ymax = ymin + r.height();
            int xmini = (int)std::floor(xmin);
            int ymini = (int)std::floor(ymin);
            int xmaxi = (int)std::ceil(xmax);
            int ymaxi = (int)std::ceil(ymax);
            return QRect(xmini, ymini, xmaxi - xmini, ymaxi - ymini);
        };
        auto toBigRect = [](QRectF const &r) {
            qreal xmin = r.x();
            qreal ymin = r.y();
            qreal w = r.width();
            qreal h = r.height();
            qreal longsize = qMax(w, h);
            qreal xmax = (xmin + w / 2) + longsize * 2.0;
            xmin = (xmin + w / 2) - longsize * 2.0;
            qreal ymax = (ymin + h / 2) + longsize * 1.0;
            ymin = (ymin + h / 2) - longsize * 1.0;
            int xmini = (int)floor(xmin);
            int ymini = (int)floor(ymin);
            int xmaxi = (int)ceil(xmax);
            int ymaxi = (int)ceil(ymax);
            return QRect(xmini, ymini, xmaxi - xmini, ymaxi - ymini);\
        };
        auto scale_max_longsize = [](QImage &img, int longsize) {
            if (img.width() > img.height()) {
                if (img.width() > longsize)
                    img = img.scaledToWidth(longsize, Qt::SmoothTransformation);
            } else {
                if (img.height() > longsize)
                    img = img.scaledToHeight(longsize, Qt::SmoothTransformation);
            }
        };
        QImage small = lastimg.copy(toRect(box));
        QImage big = lastimg.copy(toBigRect(box));
        scale_max_longsize(small, max_small_size);
        scale_max_longsize(big, max_big_size);
        if (!chars.contains(text))
            chars[text] = QVector<QPair<QPair<QString, QPair<QRectF, QString> >, QPair<QImage, QImage> > >();
        chars[text].append(qMakePair(all[i], qMakePair(small, big)));
    }
    int min_pack = 1000;
    int sum = 0;
    QMap<QString, QVector<QPair<QPair<QString, QPair<QRectF, QString> >, QPair<QImage, QImage> > > > map_to_write;
    int packcnt = 0;
    for (int i = 0; i < charCount.size(); i++) {
        QString text = charCount[i].first;
        int thisn = charCount[i].second;
        sum += thisn;
        map_to_write[text] = chars[text];
        if (sum >= min_pack || i == charCount.size() - 1) {
            QString filename = QString("multicharpack-%1.doubt").arg(++packcnt, 4, 10, QChar('0'));
            QFile file(filename);
            QByteArray array;
            QDataStream stream(&array, QIODevice::WriteOnly);
            stream << map_to_write;
            array = ::qCompress(array, 9);
            qDebug() << array.size() << sum << map_to_write.keys().size();
            sum = 0;
            map_to_write.clear();
            if (!file.open(QIODevice::WriteOnly)) {
                QMessageBox::information(this, tr("Error"),
                                         tr("Cannot write %1.").arg(file.fileName()));
                exit(1);
            }
            file.write(array);
            file.close();
        }
    }

    exit(0);
}

MainWindow::~MainWindow()
{
    delete ui;
}
