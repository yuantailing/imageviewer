#include <QCoreApplication>
#include <QCommandLineParser>
#include <QVector>
#include <QByteArray>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QDebug>
#include <functional>

#include "imageannotation.h"

static QTextStream cin(stdin);
static QTextStream cout(stdout);

bool eachFile(QDir dir, std::function<bool(QFileInfo const &)> const &cb, QStringList const &nameFilters) {
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

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QCommandLineParser parser;
    parser.process(app);
    QStringList args = parser.positionalArguments();
    if (args.size() < 2) {
        cout << "missing parameter" << endl;
        return 1;
    }
    QDir inDir(args[0]);
    QDir outDir(args[1]);
    auto convert = [&](QFileInfo const &fileinfo) {
        qDebug() << fileinfo.fileName();
        QFile inFile(inDir.filePath(fileinfo.fileName()));
        QFile outFile(outDir.filePath(fileinfo.fileName()));

        if (inFile.open(QIODevice::ReadOnly)) {
            ImageAnnotation anno;
            QVector<ImageAnnotation> history;
            QDataStream stream(&inFile);
            stream >> anno;
            stream >> history;
            if (stream.status() != QDataStream::Ok)
                return false;
            QByteArray array;
            QDataStream st2(&array, QIODevice::WriteOnly);
            st2 << history;
            if (outFile.open(QIODevice::WriteOnly)) {
                QDataStream st3(&outFile);
                st3 << anno;
                st3 << qCompress(array);
                return true;
            }
        }
        return false;
    };
    QStringList nameFilters;
    nameFilters << "*.stream";
    bool res = eachFile(inDir, convert, nameFilters);
    if (res == false)
        cout << "error occurred" << endl;
    return 0;
}
