#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QDataStream>
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
                arg("ID", 10).
                arg("#Blk", 6).
                arg("#Char", 6).
                arg("Folder") << endl;
    }
    ~CharCounter() {
        cout << QString("%1 %2 %3 %4  %5").arg("#", 5).
                arg("Total", 10).
                arg(sumNumBlock, 6).
                arg(sumNumCharacter, 6).
                arg("") << endl;
        for (auto it = folderCount.begin(); it != folderCount.end(); it++) {
            cout << QString("%1 %2 %3 %4  %5").arg("#", 5).
                    arg("sum", 10).
                    arg(it.value().first, 6).
                    arg(it.value().second, 6).
                    arg(it.key()) << endl;
        }

        QVector<QPair<QString, int> > a;
        for (auto it = bucket.begin(); it != bucket.end(); it++)
            a.push_back(qMakePair(it.key(), it.value()));
        std::sort(a.begin(), a.end(), [](QPair<QString, int> const &p, QPair<QString, int> const &q) {
            return p.second == q.second ? p.first < q.first : p.second > q.second; }
        );

        foreach (auto &p, a) {
            cout << p.first << ' ' << p.second << endl;
        }
    }
    bool operator()(QString filePath) {
        if (!filePath.endsWith(".stream"))
            return true;
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            cout << "open failed: " << file.fileName() << endl;
            return false;
        }
        QDataStream st(&file);
        ImageAnnotation anno;
        st >> anno;
        if (st.status() != QDataStream::Ok) {
            cout << "stream is bad: " << file.fileName() << endl;
            return false;
        }
        int numBlock = 0;
        int numCharacter = 0;
        foreach (BlockAnnotation const &block, anno.blocks) {
            int cnt = 0;
            foreach (CharacterAnnotation const &ch, block.characters) {
                if (ch.text.isEmpty())
                    continue;
                QString const &text = ch.text;
                if (bucket.find(text) == bucket.end())
                    bucket[text] = 0;
                bucket[text]++;
                cnt++;
            }
            if (cnt > 0) {
                numBlock++;
                numCharacter += cnt;
            }
        }
        QFileInfo fileInfo(file.fileName());
        QString dirName = fileInfo.dir().path();
        cout << QString("%1 %2 %3 %4  %5").arg(++idx, 5).
                arg(fileInfo.completeBaseName(), 10).
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
        file.close();
        return true;
    }
private:
    CharCounter(CharCounter const &);
    int sumNumBlock;
    int sumNumCharacter;
    int idx;
    QMap<QString, int> bucket;
    QMap<QString, QPair<int, int> > folderCount;
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
    nameFilters << "*.stream";
    std::function<bool(QString)> cb([&](QString filePath) {
        return charCounter(filePath);
    });
    if (!eachFile(rootDir, cb, nameFilters)) {
        cout << "error occurred" << endl;
        return 1;
    }

    return 0;
}
