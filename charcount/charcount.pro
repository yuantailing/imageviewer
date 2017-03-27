QT += core
# QT -= gui

CONFIG += c++11

TARGET = charcount
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += main.cpp \
    ../imageviewer/imageannotation.cpp
