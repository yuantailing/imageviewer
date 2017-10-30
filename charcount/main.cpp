#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QDataStream>
#include <QDebug>
#include <functional>
#include "../imageviewer/imageannotation.h"

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
            cout << "open failed: " << file.fileName() << endl;
            return false;
        }
        QDataStream st(&file);
        ImageAnnotation anno;
        st >> anno;
        file.close();
        if (st.status() != QDataStream::Ok) {
            cout << "stream is bad: " << file.fileName() << endl;
            return false;
        }
        Statistics stat;
        stat.cnt = 1;
        foreach (BlockAnnotation const &block, anno.blocks) {
            bool hasMask = false;
            bool hasNotMask = false;
            foreach (CharacterAnnotation const &ch, block.characters) {
                if (1 == ch.props.value("mask", 0)) {
                    hasMask = true;
                    continue;
                }
                if (ch.text.isEmpty())
                    continue;
                hasNotMask = true;
                QString const &text = ch.text;
                if (text == "*") {
                    stat.numStar++;
                } else {
                    if (0 == ch.props.value("pass", 0))
                        stat.numCharWithoutProps++;
                    else
                        stat.numCharWithProps++;
                    if (bucket.find(text) == bucket.end())
                        bucket[text] = 0;
                    bucket[text]++;
                }
            }
            if (hasMask)
                stat.numMask++;
            else if (hasNotMask)
                stat.numBlkChar++;
        }
        QFileInfo fileInfo(file.fileName());
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
            cout << QString(QObject::tr("%1 ID 文件数 词组数 标注属性的字数 未标属性的字数 星数 Mask数  文件夹"))
                    .arg("#", 5) << endl;
        }
        void print(QString id, QString name, QString folder) const {
            cout << QString("%1 %2 %3 %4 %5 %6 %7 %8  %9").
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
