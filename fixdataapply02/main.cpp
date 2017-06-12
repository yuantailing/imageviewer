#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QDataStream>
#include <QRectF>
#include <QImage>
#include <QDebug>
#include <functional>
#include "../imageviewer/imageannotation.h"

static QTextStream cin(stdin);
static QTextStream cout(stdout);

bool eachFile(QDir dir, std::function<bool(QString)> const &cb, QStringList const &nameFilters) {
    if (!dir.exists()) {
        cout << "directory not exists: " << dir.path() << endl;
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

class CharCounter {
public:
    CharCounter() {
        sumNumBlock = sumNumCharacter = idx = 0;
        cout << QString("%1 %2 %3 %4  %5").arg("#", 5).
                arg("ID", 19).
                arg("#Doubt", 6).
                arg("#Edit", 6).
                arg("Folder") << endl;
    }
    ~CharCounter() {
        cout << QString("%1 %2 %3 %4  %5").arg("#", 5).
                arg("Total", 19).
                arg(sumNumBlock, 6).
                arg(sumNumCharacter, 6).
                arg("") << endl;
        for (auto it = folderCount.begin(); it != folderCount.end(); it++) {
            cout << QString("%1 %2 %3 %4  %5").arg("#", 5).
                    arg("sum", 19).
                    arg(it.value().first, 6).
                    arg(it.value().second, 6).
                    arg(it.key()) << endl;
        }
    }
    bool operator()(QString filePath) {
        if (!filePath.endsWith(".correction"))
            return true;
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            cout << "open failed: " << file.fileName() << endl;
            return false;
        }
        QByteArray array(file.readAll());
        file.close();
        array = qUncompress(array);
        QDataStream st(&array, QIODevice::ReadOnly);
        QMap<QString, QVector<QPair<Locator, QPair<QPair<QImage, QPolygonF>, QPair<QImage, QRect> > > > > map_loaded;
        QVector<QPair<Locator, QPair<QString, int> > > correction_loaded;
        st >> correction_loaded;
        for (auto it = correction_loaded.begin(); it != correction_loaded.end(); it++) {
            if (it->second.second != 0 && it->second.second != 2)
                continue;
            Locator const &locator(it->first);
            QDir streamBaseDir("E:\\tiger\\ch-anno-results\\streams_v3");
            QFile file(streamBaseDir.filePath(locator.first + ".stream"));
            if (!file.open(QIODevice::ReadOnly)) {
                cout << "open failed: " << file.fileName() << endl;
                return false;
            }
            QDataStream stream(&file);
            ImageAnnotation anno;
            QVector<ImageAnnotation> history;
            stream >> anno;
            bool okAnno = stream.status() == QDataStream::Ok;
            stream >> history;
            bool okHistory = stream.status() == QDataStream::Ok;
            file.close();
            if (!okAnno || !okHistory) {
                cout << "stream is bad: " << file.fileName() << endl;
                return false;
            }
            bool found = false;
            for (BlockAnnotation &block: anno.blocks) {
                for (auto cit = block.characters.begin(); cit != block.characters.end(); cit++) {
                    QRectF box = cit->box.boundingRect();
                    QRectF const &target(locator.second.first);
                    QRectF inter = box.intersected(target);
                    if (inter.width() * inter.height() / (box.width() * box.height() + target.width() * target.height() - inter.width() * inter.height()) > 0.99) {
                        if (locator.second.second != cit->text) {
                            cout << "error: ch-not-matched" << endl;
                            return false;
                        }
                        if (found) {
                            cout << "error: multi-found" << endl;
                            return false;
                        }
                        found = true;
                        if (it->second.second == 2) {
                            block.characters.erase(cit, cit + 1);
                        } else if (it->second.second == 0) {
                            if (it->second.first.length() != 1) {
                                cout << "error: length != 1" << endl;
                                return false;
                            }
                            cit->text = it->second.first;
                        } else {
                            cout << "error: case not in {0, 2}" << endl;
                            return false;
                        }
                        history.push_back(anno);
                        break;
                    }
                }
            }
            if (!found) {
                cout << "error: not-found" << endl;
                return false;
            }
            if (!file.open(QIODevice::WriteOnly)) {
                cout << "open (write) failed: " << file.fileName() << endl;
                return false;
            }
            QDataStream streamWrite(&file);
            streamWrite << anno;
            streamWrite << history;
            file.close();
        }
        if (st.status() != QDataStream::Ok) {
            cout << "stream is bad: " << file.fileName() << endl;
            return false;
        }
        int numBlock = 0;
        int numCharacter = correction_loaded.size();
        QFileInfo fileInfo(file.fileName());
        QString dirName = fileInfo.dir().path();
        QFile doubtFile(QDir(dirName).filePath(fileInfo.completeBaseName() + ".doubt"));
        if (doubtFile.exists()) {
            if (!doubtFile.open(QIODevice::ReadOnly)) {
                cout << "open failed: " << doubtFile.fileName() << endl;
                return false;
            }
            QByteArray array(doubtFile.readAll());
            doubtFile.close();
            array = qUncompress(array);
            QDataStream st(&array, QIODevice::ReadOnly);
            st >> map_loaded;
            if (st.status() != QDataStream::Ok) {
                cout << "stream is bad: " << file.fileName() << endl;
                return false;
            }
            for (auto it = map_loaded.begin(); it != map_loaded.end(); it++)
                numBlock += it.value().size();
        }
        cout << QString("%1 %2 %3 %4  %5").arg(++idx, 5).
                arg(fileInfo.completeBaseName(), 19).
                arg(numBlock, 6).
                arg(numCharacter, 6).
                arg(dirName) << endl;
        sumNumBlock += numBlock;
        sumNumCharacter += numCharacter;
        if (folderCount.find(dirName) == folderCount.end())
            folderCount[dirName] = qMakePair(0, 0);
        ([](QPair<int, int> &p, int nb, int nc) {
            p.first += nb;
            p.second += nc;
        })(folderCount[dirName], numBlock, numCharacter);
        return true;
    }
private:
    CharCounter(CharCounter const &);
    int sumNumBlock;
    int sumNumCharacter;
    int idx;
    QMap<QString, QPair<int, int> > folderCount;
    typedef QPair<QString, QPair<QRectF, QString> > Locator;
};

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    app.setApplicationVersion("v0.0.1");

    QCommandLineParser parser;
    parser.process(app);
    const QStringList args = parser.positionalArguments();
    if (args.size() < 1) {
        cout << "missing parameter: folder path" << endl;
        return 1;
    }
    QDir rootDir(args[0]);

    CharCounter charCounter;
    QStringList nameFilters;
    nameFilters << "*.correction";
    std::function<bool(QString)> cb([&](QString filePath) {
        return charCounter(filePath);
    });
    if (!eachFile(rootDir, cb, nameFilters)) {
        cout << "error occurred" << endl;
        return 1;
    }

    return 0;
}
