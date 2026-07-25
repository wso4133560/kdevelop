/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "../petimelinemodel.h"

#include <QFile>
#include <QTest>

class TestPeTimelineModel : public QObject
{
    Q_OBJECT

private:
    static PeTimeline::Model analyze(const QString& body, const PeTimeline::Options& options = {})
    {
        const QString source = QStringLiteral("\\input{base/GReP.tex}\n\\PeaTop{0}{") + body + QStringLiteral("}\n");
        PeTimeline::Model model;
        QString error;
        const bool ok = PeTimeline::Analyzer::analyze(source, QStringLiteral("test.tex"), options, &model, &error);
        if (!ok)
            qWarning().noquote() << error;
        Q_ASSERT(ok);
        return model;
    }

private Q_SLOTS:
    void expandsForAndEmits64Tracks()
    {
        const auto model = analyze(QStringLiteral("\\PeTop{0}{0}{0}{0}{0}{0}{\\For{3}{\\Add{0}{0}{0}{0}}}"));
        QCOMPARE(model.tracks.size(), 64);
        QCOMPARE(model.eventCount, 4);
        QCOMPARE(model.tracks.first().cycles, 4);
        QCOMPARE(model.tracks.first().events.at(3).start, 3);
    }

    void compilerFirstWaitSetsRepeat()
    {
        const QString body = QStringLiteral("\\PeTop{0}{0}{0}{0}{0}{0}{\\Wait{2}\\Add{0}{0}{0}{0}}");
        auto compiler = analyze(body);
        PeTimeline::Options timelineOptions;
        timelineOptions.compilerFirstWait = false;
        auto timeline = analyze(body, timelineOptions);
        QCOMPARE(compiler.eventCount, 3);
        QCOMPARE(compiler.tracks.first().cycles, 3);
        QCOMPARE(timeline.eventCount, 2);
        QCOMPARE(timeline.tracks.first().cycles, 3);
    }

    void detectsSameBankWriteConflict()
    {
        const QString pe0 = QStringLiteral("\\PeTop{0}{0}{0}{0}{0}{0}{\\Add{\\SM{0}}{0}{0}{0}}");
        const QString pe1 = QStringLiteral("\\PeTop{0}{0}{0}{0}{0}{0}{\\Sub{\\SM{64}}{0}{0}{0}}");
        const auto model = analyze(pe0 + pe1);
        QVERIFY(model.conflictCount > 0);
        QVERIFY(!model.tracks.at(0).events.first().conflicts.isEmpty());
        QVERIFY(!model.tracks.at(1).events.first().conflicts.isEmpty());
    }

    void accumulatorDoesNotSelfConflict()
    {
        const auto model = analyze(QStringLiteral("\\PeTop{0}{0}{0}{0}{0}{0}{\\Add{\\SM{7}}{\\SM{7}}{\\SM{8}}{0}}"));
        QCOMPARE(model.conflictCount, 0);
    }

    void inputSmIncrementAddsHelper()
    {
        const auto model = analyze(QStringLiteral("\\PeTop{0}{0}{0}{0}{0}{0}{\\Add{0}{\\SM[2]{3}}{0}{0}}"));
        QCOMPARE(model.tracks.first().cycles, 2);
        QCOMPARE(model.tracks.first().events.at(1).name, QStringLiteral("SMInc"));
    }

    void exportsJson()
    {
        const auto model = analyze(QStringLiteral("\\PeTop{0}{0}{0}{0}{0}{0}{\\MCCA{0}{0}{0}{0}}"));
        const QByteArray json = PeTimeline::Analyzer::toJson(model);
        QVERIFY(json.contains("\"tracks\""));
        QVERIFY(json.contains("\"MCCA\""));
    }

    void analyzesExternalFixtureWhenRequested()
    {
        const QString path = qEnvironmentVariable("PE_TIMELINE_REAL_TEX");
        if (path.isEmpty())
            QSKIP("PE_TIMELINE_REAL_TEX is not set");
        QFile file(path);
        QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(file.errorString()));
        PeTimeline::Model model;
        PeTimeline::Options options;
        QString error;
        QVERIFY2(PeTimeline::Analyzer::analyze(QString::fromUtf8(file.readAll()), path, options, &model, &error),
                 qPrintable(error));
        QCOMPARE(model.tracks.size(), 64);
        QVERIFY(model.eventCount > 0);
        qInfo().noquote() << QStringLiteral("real fixture: cycles=%1 events=%2 conflicts=%3 advisories=%4")
                                 .arg(model.maxCycle)
                                 .arg(model.eventCount)
                                 .arg(model.conflictCount)
                                 .arg(model.advisoryCount);
    }
};

QTEST_GUILESS_MAIN(TestPeTimelineModel)

#include "test_petimelinemodel.moc"
