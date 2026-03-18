QT += quick qml gui quick3d bluetooth dbus
CONFIG += release
SOURCES += main.cpp
RESOURCES += qml.qrc
TARGET = kiosk
LIBS += -lrdkafka
