#include <QCoreApplication>
#include <QCommandLineParser>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDataStream>
#include <QDebug>
#include <queue>
#include "../imageviewer/imageannotation.h"

static QTextStream cout(stdout);

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

QJsonObject character2json(CharacterAnnotation charAnno) {
    QJsonObject res;
    res["box"] = poly2json(charAnno.box);
    res["text"] = charAnno.text;
    return res;
}

QJsonObject validateSingle(QString fileName, QMap<QString, QVector<CharacterAnnotation> > *m = nullptr) {
    QJsonObject res;
    QJsonObject images;
    QFile file(fileName);
    if (file.open(QIODevice::ReadOnly)) {
        QDataStream stream(&file);
        while (!stream.atEnd()) {
            QString baseName;
            QByteArray fileContent;
            stream >> baseName;
            stream >> fileContent;
            if (stream.status() != QDataStream::Ok) {
                res["error"] = 2;
                res["errorMessage"] = QCoreApplication::tr("stream is bad");
                return res;
            }
            QDataStream st1(&fileContent, QIODevice::ReadOnly);
            QByteArray annoArray;
            st1 >> annoArray;
            if (st1.status() != QDataStream::Ok) {
                res["error"] = 2;
                res["errorMessage"] = QCoreApplication::tr("stream is bad");
                return res;
            }
            annoArray = qUncompress(annoArray);
            QDataStream st2(&annoArray, QIODevice::ReadOnly);
            ImageAnnotation anno;
            st2 >> anno;
            if (st2.status() != QDataStream::Ok) {
                res["error"] = 2;
                res["errorMessage"] = QCoreApplication::tr("stream is bad");
                return res;
            }
            int numBlock = 0;
            int numCharacter = 0;
            QJsonArray numCharInBlock;
            QJsonArray characters;
            QVector<CharacterAnnotation> v;
            foreach (BlockAnnotation const &block, anno.blocks) {
                int cnt = 0;
                foreach (CharacterAnnotation const &ch, block.characters) {
                    if (ch.text.isEmpty())
                        continue;
                    cnt++;
                    characters.append(character2json(ch));
                    v.append(ch);
                }
                if (cnt > 0) {
                    numBlock++;
                    numCharacter += cnt;
                    numCharInBlock.append(cnt);
                }
            }
            QJsonObject img;
            img["numBlock"] = numBlock;
            img["numCharacter"] = numCharacter;
            img["numCharInBlock"] = numCharInBlock;
            img["characters"] = characters;
            images[baseName] = img;
            if (m != nullptr)
                (*m)[baseName] = v;
        }
        file.close();
    } else {
        res["error"] = 3;
        res["errorMessage"] = QCoreApplication::tr("file open failed");
    }
    res["error"] = 0;
    res["images"] = images;
    return res;
}

struct DistPair {
    qreal distance;
    int i;
    int j;
    bool operator<(DistPair const &other) const {
        return distance > other.distance;
    }
};

QJsonObject feedback(QMap<QString, QVector<CharacterAnnotation> > images, QMap<QString, QVector<CharacterAnnotation> > reference, qreal ratio) {
    QJsonObject res;
    QJsonObject feed, feed_ref;
    for (auto it = images.begin(); it != images.end(); it++) {
        auto it_ref = reference.find(it.key());
        if (it_ref == reference.end()) {
            continue;
        }
        QJsonObject json, json_ref;
        QVector<QPointF> center;
        QVector<QPointF> center_ref;
        foreach (CharacterAnnotation const &ch, it.value())
            center.append(ch.box.boundingRect().center());
        foreach (CharacterAnnotation const &ch, it_ref.value())
            center_ref.append(ch.box.boundingRect().center());
        auto polyArea = [](QPolygonF const &poly) {
            if (poly.size() < 3)
                return (qreal)0.0;
            QPointF o(poly[0]);
            qreal sum = 0.0;
            for (int i = 2; i < poly.size(); i++) {
                QPointF oa = poly[i - 1] - o;
                QPointF ob = poly[i] - o;
                qreal cr = oa.x() * ob.y() - oa.y() * ob.x();
                sum += cr;
            }
            return qAbs(sum / 2);
        };

        std::priority_queue<DistPair> q;
        for (int i = 0; i < center.size(); i++) {
            for (int j = 0; j < center_ref.size(); j++) {
                qreal distance = (center[i] - center_ref[j]).manhattanLength();
                q.push(DistPair({distance, i, j}));
            }
        }
        QJsonArray error, miss, reduntant;
        QJsonArray error_ref, miss_ref, reduntant_ref;
        QVector<int> nearToRef(it.value().size(), -1), nearFromRef(it_ref.value().size(), -1);
        QVector<int> matchToRef(it.value().size(), -1), matchFromRef(it_ref.value().size(), -1);
        while (!q.empty()) {
            DistPair p = q.top();
            q.pop();
            if (nearToRef[p.i] != -1)
                continue;
            if (nearFromRef[p.j] != -1)
                continue;
            nearToRef[p.i] = p.j;
            nearFromRef[p.j] = p.i;
            CharacterAnnotation character = it.value()[p.i];
            CharacterAnnotation character_ref = it_ref.value()[p.j];
            qreal area = polyArea(character.box);
            qreal area_ref = polyArea(character_ref.box);
            qreal intersected = polyArea(character.box.intersected(character_ref.box));
            qreal overlap_ratio = intersected / (area + area_ref - intersected);
            if (overlap_ratio > ratio) {
                matchToRef[p.i] = p.j;
                matchFromRef[p.j] = p.i;
                if (character.text != character_ref.text) {
                    error.append(character2json(character_ref));
                    error_ref.append(character2json(character));
                }
            }
        }
        for (int i = 0; i < matchToRef.size(); i++) {
            if (matchToRef[i] == -1) {
                reduntant.append(character2json(it.value()[i]));
                miss_ref.append(character2json(it.value()[i]));
            }
        }
        for (int i = 0; i < matchFromRef.size(); i++) {
            if (matchFromRef[i] == -1) {
                reduntant_ref.append(character2json(it_ref.value()[i]));
                miss.append(character2json(it_ref.value()[i]));
            }
        }
        json["error"] = error;
        json["miss"] = miss;
        json["reduntant"] = reduntant;
        json_ref["error"] = error_ref;
        json_ref["miss"] = miss_ref;
        json_ref["reduntant"] = reduntant_ref;
        feed[it.key()] = json;
        feed_ref[it.key()] = json_ref;
    }
    res["feedback"] = feed;
    res["feedbackRef"] = feed_ref;
    return res;
}

QJsonObject validateCross(QString fileName1, QString fileName2, qreal ratio) {
    QJsonObject res;
    QMap<QString, QVector<CharacterAnnotation> > images1;
    QMap<QString, QVector<CharacterAnnotation> > images2;
    QJsonObject res1 = validateSingle(fileName1, &images1);
    QJsonObject res2 = validateSingle(fileName2, &images2);
    if (res1["error"] != 0) {
        res["error"] = res1["error"];
        res["errorMessage"] = res1["errorMessage"];
        return res;
    }
    if (res2["error"] != 0) {
        res["error"] = res2["error"];
        res["errorMessage"] = res2["errorMessage"];
        return res;
    }

    res["error"] = 0;
    res["images1"] = res1["images"].toObject();
    res["images2"] = res2["images"].toObject();
    QJsonObject feed = feedback(images1, images2, ratio);
    res["feedback1"] = feed["feedback"];
    res["feedback2"] = feed["feedbackRef"];

    return res;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationVersion("v0.0.1");
    cout.setCodec("UTF-8");

    QCommandLineParser parser;
    parser.setApplicationDescription("Cross Validation");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("package-1", QCoreApplication::translate("main", "The first package."));
    parser.addPositionalArgument("package-2", QCoreApplication::translate("main", "The second package (if --single is not set)."));

    QCommandLineOption singleOption(QStringList() << "s" << "single",
            QCoreApplication::translate("main", "Only validate single package."));
    parser.addOption(singleOption);

    QCommandLineOption ratioOption(QStringList() << "r" << "ratio",
            QCoreApplication::translate("main", "Minimum overlap ratio between pylogons."),
            QCoreApplication::translate("main", "ratio"));
    parser.addOption(ratioOption);

    // Process the actual command line arguments given by the user
    parser.process(app);

    const QStringList args = parser.positionalArguments();
    // package-1 is args.at(0), package-2 is args.at(1)

    QJsonObject json;
    if (parser.isSet(singleOption)) {
        if (args.size() != 1) {
            json["error"] = 1;
            json["errorMessage"] = "invalid argument";
        } else {
            json = validateSingle(args.at(0));
        }
    } else {
        if (args.size() != 2 || !parser.isSet(ratioOption)) {
            json["error"] = 1;
            json["errorMessage"] = "invalid argument";
        } else {
            qreal ratio = (qreal)parser.value(ratioOption).toDouble();
            json = validateCross(args.at(0), args.at(1), ratio);
        }
    }
    QJsonDocument doc;
    doc.setObject(json);
    cout << doc.toJson(QJsonDocument::Compact);

    return 0;
}
