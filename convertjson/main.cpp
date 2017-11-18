#include <QCoreApplication>
#include <QCommandLineParser>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QFile>
#include <QDataStream>
#include <QDebug>
#include <functional>
#include <stdexcept>
#include "../imageviewer/imageannotation.h"

static QTextStream cin(stdin);
static QTextStream cout(stdout);
static QTextStream cerr(stderr);

bool eachFile(QDir dir, std::function<bool(QString)> const &cb, QStringList const &nameFilters) {
    if (!dir.exists()) {
        cerr << "directory not exists: " << dir.path() << endl;
        return false;
    }
    dir.setFilter(QDir::Dirs | QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);
    dir.setNameFilters(nameFilters);
    QFileInfoList list = dir.entryInfoList();
    foreach (QFileInfo fileInfo, list) {
        if (fileInfo.fileName() == "." || fileInfo.fileName() == "..")
            continue;
        if (fileInfo.isFile()) {
            if (!cb(fileInfo.filePath()))
                return false;
        } else {
            if (!eachFile(QDir(fileInfo.filePath()), cb, nameFilters))
                return false;
        }
    }
    return true;
}

QJsonArray poly2json(QPolygonF const &poly) {
    QJsonArray res;
    foreach (QPointF const &p, poly) {
        QJsonArray xy;
        xy.append(p.x());
        xy.append(p.y());
        res.append(xy);
    }
    return res;
}

QJsonArray poly2bbox(QPolygonF const &poly) {
    if (poly.size() == 0)
        return {0, 0, 0, 0};
    qreal xmin = poly[0].x(), ymin = poly[0].y();
    qreal xmax = xmin, ymax = ymin;
    foreach (QPointF const &p, poly) {
        xmin = qMin(xmin, p.x());
        xmax = qMax(xmax, p.x());
        ymin = qMin(ymin, p.y());
        ymax = qMax(ymax, p.y());
    }
    return {xmin, ymin, xmax - xmin, ymax - ymin};
}

QJsonArray poly2adjustedbbox(QPolygonF const &poly) {
    if (poly.size() == 0)
        return {0, 0, 0, 0};
    qreal xmin = 0, xmax = 0, ymin = 0, ymax = 0;
    for (int i = 0; i < poly.size(); i++) {
        QPointF const &p1(poly[i]);
        QPointF const &p2(poly[(i + 1) % poly.size()]);
        for (int j = 1; j < 3; j++) {
            QPointF q = p1 * j / 3 + p2 * (3 - j) / 3;
            if (i == 0 && j == 1) {
                xmin = xmax = q.x();
                ymin = ymax = q.y();
            } else {
                xmin = qMin(xmin, q.x());
                xmax = qMax(xmax, q.x());
                ymin = qMin(ymin, q.y());
                ymax = qMax(ymax, q.y());
            }
        }

    }
    return {xmin, ymin, xmax - xmin, ymax - ymin};
}

bool isChinese(QString const &text) {
    if (text.length() != 1)
        throw std::invalid_argument("text.length() != 1");
    QChar c = text[0];
    ushort code = c.unicode();
    bool flag = code >= 0x4e00 && code <= 0x9fff;
    return flag;
}

QJsonObject character2json(CharacterAnnotation const &charAnno) {
    if (ImageAnnotation::VERSION != 0x1002)
        throw std::invalid_argument("ImageAnnotation::VERSION != 0x1002");
    if (charAnno.box.size() != 4)
        throw std::invalid_argument("charAnno.box.size() != 4");
    if (charAnno.text.length() != 1)
        throw std::invalid_argument("charAnno.text.length() != 1");
    QJsonObject res;
    res["polygon"] = poly2json(charAnno.box);
    res["adjusted_bbox"] = poly2adjustedbbox(charAnno.box);
    res["text"] = charAnno.text;
    res["is_chinese"] = isChinese(charAnno.text);
    QVector<QString> props;
    if (1 == charAnno.props.value("covered", 0)) props.append("occluded");
    if (1 == charAnno.props.value("bgcomplex", 0)) props.append("bgcomplex");
    if (1 == charAnno.props.value("raised", 0)) props.append("distorted");
    if (1 == charAnno.props.value("perspective", 0)) props.append("raised");
    if (1 == charAnno.props.value("wordart", 0)) props.append("wordart");
    if (1 == charAnno.props.value("handwritten", 0)) props.append("handwritten");
    std::sort(props.begin(), props.end());
    QJsonArray jprops;
    for (QString const &s: props)
        jprops.append(s);
    res["attributes"] = jprops;
    return res;
}

class CharCounter {
public:
    CharCounter() {
        top = 0;
        Statistics::printTitle();
    }
    ~CharCounter() {
        Statistics total;
        for (QMap<QString, Statistics>::iterator it = folderStat.begin(); it != folderStat.end(); it++) {
            it.value().print("", "#", it.key());
            total = total + it.value();
        }
        total.print("", "Total", "");
    }
    bool operator()(QString filePath) {
        if (!filePath.endsWith(".stream"))
            return true;
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            cerr << "open failed: " << file.fileName() << endl;
            return false;
        }
        QDataStream st(&file);
        ImageAnnotation anno;
        st >> anno;
        file.close();
        if (st.status() != QDataStream::Ok) {
            cerr << "stream is bad: " << file.fileName() << endl;
            return false;
        }
        QJsonArray imageBlocks, imageMasks;
        Statistics stat;
        stat.cnt = 1;
        foreach (BlockAnnotation const &block, anno.blocks) {
            bool hasMask = false;
            bool hasNotMask = false;
            QJsonArray blockArray;
            foreach (CharacterAnnotation const &_ch, block.characters) {
                CharacterAnnotation ch(_ch);
                if (1 == ch.props.value("mask", 0)) {
                    QJsonObject mask;
                    mask["polygon"] = poly2json(ch.box);
                    mask["bbox"] = poly2bbox(ch.box);
                    imageMasks.append(mask);
                    hasMask = true;
                    continue;
                }
                if (ch.text.isEmpty())
                    continue;
                if (ch.box.size() != 4) {
                    cerr << "polygon size != 4: " << file.fileName() << endl;
                    return false;
                }
                hasNotMask = true;
                QString &text = ch.text;
                if (text.length() != 1) {
                    if (text != "𫔭") {
                        cerr << "Warning: text length != 1 @ " << file.fileName() << endl;
                        cerr << text.length() << " " << text << endl;
                    }
                    text = "*";
                }
                if (text == "*") {
                    QJsonObject mask;
                    mask["polygon"] = poly2json(ch.box);
                    mask["bbox"] = poly2bbox(ch.box);
                    imageMasks.append(mask);
                    stat.numStar++;
                } else {
                    blockArray.append(character2json(ch));
                    if (0 == ch.props.value("pass", 0))
                        stat.numCharWithoutProps++;
                    else
                        stat.numCharWithProps++;
                    if (bucket.find(text) == bucket.end())
                        bucket[text] = 0;
                    bucket[text]++;
                }
            }
            if (!blockArray.isEmpty())
                imageBlocks.append(blockArray);
            if (hasMask && hasNotMask) {
                cerr << "impossible block has mask and text: " << file.fileName() << endl;
                return false;
            }
            if (hasMask)
                stat.numMask++;
            else if (hasNotMask)
                stat.numBlkChar++;
        }
        QFileInfo fileInfo(file.fileName());
        QJsonObject json;
        json["image_id"] = fileInfo.baseName();
        json["file_name"] = fileInfo.baseName() + ".jpg";
        json["width"] = 2048;
        json["height"] = 2048;
        json["annotations"] = imageBlocks;
        json["ignore"] = imageMasks;
        QJsonDocument doc;
        doc.setObject(json);
        cout << doc.toJson(QJsonDocument::Compact) << endl;

        QString dirName = fileInfo.dir().path();
        stat.print(QString("%1").arg(++top), fileInfo.completeBaseName(), dirName);
        folderStat[dirName] = folderStat[dirName] + stat;
        return true;
    }
private:
    CharCounter(CharCounter const &);
    struct Statistics {
        Statistics(): cnt(0), numBlkChar(0), numCharWithProps(0), numCharWithoutProps(0), numStar(0), numMask(0) { }
        int cnt;
        int numBlkChar; // 不包含 mask
        int numCharWithProps;
        int numCharWithoutProps;
        int numStar;
        int numMask;
        Statistics operator +(Statistics const &o) {
            Statistics r;
            r.cnt = cnt + o.cnt;
            r.numBlkChar = numBlkChar + o.numBlkChar;
            r.numCharWithProps = numCharWithProps + o.numCharWithProps;
            r.numCharWithoutProps = numCharWithoutProps + o.numCharWithoutProps;
            r.numStar = numStar + o.numStar;
            r.numMask = numMask + o.numMask;
            return r;
        }
        static void printTitle() {
            cerr << QString(QObject::tr("%1 %2 %3 %4 %5 %6 %7 %8  %9")).
                    arg("#", 5).
                    arg("name", 7).
                    arg("#file", 7).
                    arg("#blk", 7).
                    arg("#charP", 7).
                    arg("#charNoP", 7).
                    arg("#star", 7).
                    arg("#mask", 7).
                    arg("filename") << endl;
        }
        void print(QString id, QString name, QString folder) const {
            cerr << QString("%1 %2 %3 %4 %5 %6 %7 %8  %9").
                    arg(id, 5).
                    arg(name, 7).
                    arg(cnt, 7).
                    arg(numBlkChar, 7).
                    arg(numCharWithProps, 7).
                    arg(numCharWithoutProps, 7).
                    arg(numStar, 7).
                    arg(numMask, 7).
                    arg(folder) << endl;
        }
    };
    int top;
    QMap<QString, int> bucket;
    QMap<QString, Statistics> folderStat;
};

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    app.setApplicationVersion("v0.0.1");
    cout.setCodec("utf8");

    QCommandLineParser parser;
    parser.process(app);
    const QStringList args = parser.positionalArguments();
    if (args.size() < 1) {
        cerr << "missing parameter: folder path" << endl;
        return 1;
    }
    QDir rootDir(args[0]);

    CharCounter charCounter;
    QStringList nameFilters;
    nameFilters << "*.stream";
    std::function<bool(QString)> cb([&](QString filePath) {
        return charCounter(filePath);
    });
    if (!eachFile(rootDir, cb, nameFilters)) {
        cerr << "error occurred" << endl;
        return 1;
    }

    return 0;
}
