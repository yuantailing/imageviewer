QT += core

CONFIG += c++11

TARGET = fixdataapply02
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += main.cpp \
    ../imageviewer/imageannotation.cpp

HEADERS += \
    ../imageviewer/imageannotation.h
