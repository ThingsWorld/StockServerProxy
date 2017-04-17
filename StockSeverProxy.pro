TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle

SOURCES += main.cpp \
    ../StockCommon/zmqutils.cpp

LIBS += -lzmq

INCLUDEPATH += ../StockCommon/
DEFINES += DEBUG
