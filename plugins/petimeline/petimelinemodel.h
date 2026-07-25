/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace PeTimeline {

struct Event
{
    int id = -1;
    int pe = 0;
    qint64 start = 0;
    qint64 duration = 0;
    QString name;
    QString category;
    QString source;
    int line = 1;
    QString loop;
    QList<int> reads;
    QList<int> writes;
    QStringList conflicts;
    QStringList advisories;
};

struct Track
{
    int pe = 0;
    QList<Event> events;
    qint64 cycles = 0;
    int iterPe = 0;
    int iterNum = 0;
    int repeat = 1;
};

struct Options
{
    int pea = 0;
    int bankCount = 64;
    double clockNs = 10.0;
    int maxEvents = 500000;
    bool compilerFirstWait = true;
    bool pea64BankLayout = true;
    bool allowMccBroadcast = true;
};

struct Model
{
    QString title;
    QString sourceFile;
    int pea = 0;
    QList<Track> tracks;
    qint64 maxCycle = 0;
    int eventCount = 0;
    int bankCount = 64;
    double clockNs = 10.0;
    QStringList warnings;
    int conflictCount = 0;
    int advisoryCount = 0;
    QString bankLayout;
};

class Analyzer
{
public:
    static bool analyze(const QString& text, const QString& sourceFile, const Options& options, Model* model,
                        QString* error);
    static QByteArray toJson(const Model& model);
};

}
