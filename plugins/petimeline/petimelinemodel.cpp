/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "petimelinemodel.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <limits>
#include <memory>

namespace PeTimeline {
namespace {

struct Node
{
    QString name;
    QList<QList<Node>> args;
    QString value;
    QString option;
    int line = 1;
};

QString stripComments(const QString& text)
{
    QString output;
    const auto lines = text.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        const QString& line = lines.at(lineIndex);
        int cut = -1;
        for (int i = 0; i < line.size(); ++i) {
            if (line.at(i) == QLatin1Char('%') && (i == 0 || line.at(i - 1) != QLatin1Char('\\'))) {
                cut = i;
                break;
            }
        }
        output += cut < 0 ? line : line.left(cut);
        if (lineIndex + 1 < lines.size()) {
            output += QLatin1Char('\n');
        }
    }
    return output;
}

class Parser
{
public:
    explicit Parser(const QString& text)
        : m_text(stripComments(text))
    {
    }

    bool parse(QList<Node>* nodes, QString* error)
    {
        return items(QChar(), nodes, error);
    }

private:
    bool items(QChar terminator, QList<Node>* nodes, QString* error)
    {
        while (true) {
            whitespace();
            if (m_pos >= m_text.size()) {
                if (!terminator.isNull()) {
                    *error = QStringLiteral("line %1: missing '%2'").arg(m_line).arg(terminator);
                    return false;
                }
                return true;
            }
            const QChar current = m_text.at(m_pos);
            if (!terminator.isNull() && current == terminator) {
                advance();
                return true;
            }
            if (current == QLatin1Char('\\')) {
                Node node;
                if (!command(&node, error)) {
                    return false;
                }
                nodes->append(std::move(node));
            } else if (current == QLatin1Char('{')) {
                Node block;
                block.name = QStringLiteral("block");
                block.line = m_line;
                advance();
                QList<Node> body;
                if (!items(QLatin1Char('}'), &body, error)) {
                    return false;
                }
                block.args.append(std::move(body));
                nodes->append(std::move(block));
            } else if (current == QLatin1Char('}')) {
                *error = QStringLiteral("line %1: unmatched '}'").arg(m_line);
                return false;
            } else {
                Node node;
                if (!atom(&node, error)) {
                    return false;
                }
                nodes->append(std::move(node));
            }
        }
    }

    bool command(Node* node, QString* error)
    {
        node->line = m_line;
        advance();
        const int start = m_pos;
        while (m_pos < m_text.size() && (m_text.at(m_pos).isLetter() || m_text.at(m_pos) == QLatin1Char('_'))) {
            advance();
        }
        if (m_pos == start) {
            *error = QStringLiteral("line %1: expected command name").arg(node->line);
            return false;
        }
        node->name = m_text.mid(start, m_pos - start);
        whitespace();
        if (m_pos < m_text.size() && m_text.at(m_pos) == QLatin1Char('[')) {
            advance();
            const int optionStart = m_pos;
            while (m_pos < m_text.size() && m_text.at(m_pos) != QLatin1Char(']')) {
                advance();
            }
            if (m_pos >= m_text.size()) {
                *error = QStringLiteral("line %1: unterminated option").arg(node->line);
                return false;
            }
            node->option = m_text.mid(optionStart, m_pos - optionStart).trimmed();
            advance();
        }
        while (true) {
            whitespace();
            if (m_pos >= m_text.size() || m_text.at(m_pos) != QLatin1Char('{')) {
                break;
            }
            advance();
            QList<Node> argument;
            if (!items(QLatin1Char('}'), &argument, error)) {
                return false;
            }
            node->args.append(std::move(argument));
        }
        return true;
    }

    bool atom(Node* node, QString* error)
    {
        node->name = QStringLiteral("atom");
        node->line = m_line;
        const int start = m_pos;
        while (m_pos < m_text.size()) {
            const QChar current = m_text.at(m_pos);
            if (current == QLatin1Char('{') || current == QLatin1Char('}') || current == QLatin1Char('\\')
                || current.isSpace()) {
                break;
            }
            advance();
        }
        if (m_pos == start) {
            *error = QStringLiteral("line %1: unexpected input").arg(m_line);
            return false;
        }
        node->value = m_text.mid(start, m_pos - start);
        return true;
    }

    void whitespace()
    {
        while (m_pos < m_text.size() && m_text.at(m_pos).isSpace()) {
            advance();
        }
    }

    void advance()
    {
        if (m_text.at(m_pos) == QLatin1Char('\n')) {
            ++m_line;
        }
        ++m_pos;
    }

    const QString m_text;
    int m_pos = 0;
    int m_line = 1;
};

QString argText(const QList<Node>& nodes)
{
    QString output;
    for (const auto& node : nodes) {
        if (node.name == QLatin1String("atom")) {
            output += node.value;
        } else if (node.name == QLatin1String("block")) {
            output += QLatin1Char('{') + argText(node.args.value(0)) + QLatin1Char('}');
        } else {
            output += QLatin1Char('\\') + node.name;
            if (!node.option.isNull()) {
                output += QLatin1Char('[') + node.option + QLatin1Char(']');
            }
            for (const auto& argument : node.args) {
                output += QLatin1Char('{') + argText(argument) + QLatin1Char('}');
            }
        }
    }
    return output;
}

bool integerArgument(const QList<Node>& nodes, int line, const QString& command, int* value, QString* error)
{
    const QString text = argText(nodes).trimmed();
    bool ok = false;
    *value = text.toInt(&ok);
    if (!ok || !QRegularExpression(QStringLiteral("^-?\\d+$")).match(text).hasMatch()) {
        *error = QStringLiteral("line %1: \\%2 expected integer, got '%3'").arg(line).arg(command, text);
        return false;
    }
    return true;
}

QString commandSource(const Node& node)
{
    QString output = QLatin1Char('\\') + node.name;
    if (!node.option.isNull()) {
        output += QLatin1Char('[') + node.option + QLatin1Char(']');
    }
    for (const auto& argument : node.args) {
        output += QLatin1Char('{') + argText(argument) + QLatin1Char('}');
    }
    return output;
}

void findPeaTops(const QList<Node>& nodes, QList<const Node*>* output)
{
    for (const auto& node : nodes) {
        if (node.name == QLatin1String("PeaTop")) {
            output->append(&node);
        }
        for (const auto& argument : node.args) {
            findPeaTops(argument, output);
        }
    }
}

const QSet<QString>& operationNames()
{
    static const QSet<QString> names = {
        QStringLiteral("Add"),   QStringLiteral("Sub"),    QStringLiteral("And"),    QStringLiteral("Or"),
        QStringLiteral("Xor"),   QStringLiteral("Mux"),    QStringLiteral("Lls"),    QStringLiteral("Lrs"),
        QStringLiteral("Cmp"),   QStringLiteral("IfftdN"), QStringLiteral("Cat"),    QStringLiteral("RSftS"),
        QStringLiteral("LSftS"), QStringLiteral("AddSpl"), QStringLiteral("SubSpl"), QStringLiteral("Square"),
        QStringLiteral("MultS"), QStringLiteral("RFa"),    QStringLiteral("CFa"),    QStringLiteral("RFb"),
        QStringLiteral("CFb"),   QStringLiteral("CMult"),  QStringLiteral("MCC"),    QStringLiteral("Cmac"),
        QStringLiteral("Cmsub"), QStringLiteral("MCCA"),   QStringLiteral("MCCS"),   QStringLiteral("MFir"),
        QStringLiteral("CFir"),  QStringLiteral("FpTin"),  QStringLiteral("InTfp")};
    return names;
}

QString categoryFor(const QString& name)
{
    if (name == QLatin1String("Wait") || name == QLatin1String("NOP")) {
        return QStringLiteral("wait");
    }
    static const QSet<QString> compute = {QStringLiteral("MCC"),   QStringLiteral("Cmac"),  QStringLiteral("Cmsub"),
                                          QStringLiteral("MCCA"),  QStringLiteral("MCCS"),  QStringLiteral("MFir"),
                                          QStringLiteral("CFir"),  QStringLiteral("CMult"), QStringLiteral("MultS"),
                                          QStringLiteral("Square")};
    static const QSet<QString> route = {QStringLiteral("RFb"), QStringLiteral("CFb"), QStringLiteral("RFa"),
                                        QStringLiteral("CFa"), QStringLiteral("SMInc")};
    static const QSet<QString> logic = {QStringLiteral("And"),   QStringLiteral("Or"),  QStringLiteral("Xor"),
                                        QStringLiteral("Lls"),   QStringLiteral("Lrs"), QStringLiteral("RSftS"),
                                        QStringLiteral("LSftS"), QStringLiteral("Cat"), QStringLiteral("Cmp"),
                                        QStringLiteral("Mux")};
    static const QSet<QString> convert = {QStringLiteral("FpTin"), QStringLiteral("InTfp"), QStringLiteral("IfftdN")};
    if (compute.contains(name))
        return QStringLiteral("compute");
    if (route.contains(name))
        return QStringLiteral("route");
    if (logic.contains(name))
        return QStringLiteral("logic");
    if (convert.contains(name))
        return QStringLiteral("convert");
    if (name == QLatin1String("Break"))
        return QStringLiteral("control");
    return QStringLiteral("alu");
}

QList<int> smAddresses(const QList<Node>& argument)
{
    QList<int> result;
    static const QRegularExpression pattern(QStringLiteral(R"(\\SM(?:\[[^]]*\])?\{(-?\d+)\})"));
    auto matches = pattern.globalMatch(argText(argument));
    while (matches.hasNext()) {
        result.append(matches.next().captured(1).toInt());
    }
    return result;
}

bool needsSmIncrement(const Node& node)
{
    static const QRegularExpression pattern(QStringLiteral(R"(\\SM\[(-?\d+)\]\{)"));
    for (int i = 1; i < std::min(3, static_cast<int>(node.args.size())); ++i) {
        auto matches = pattern.globalMatch(argText(node.args.at(i)));
        while (matches.hasNext()) {
            if (matches.next().captured(1).toInt() != 0)
                return true;
        }
    }
    return false;
}

class ScheduleBuilder
{
public:
    explicit ScheduleBuilder(const Options& options)
        : m_options(options)
    {
    }

    bool build(const Node& node, int pe, Track* track, QString* error)
    {
        if (node.args.size() < 3) {
            *error = QStringLiteral("line %1: \\PeTop needs header arguments and a body").arg(node.line);
            return false;
        }
        QList<int> headers;
        for (int i = 0; i + 1 < node.args.size(); ++i) {
            int value = 0;
            if (!integerArgument(node.args.at(i), node.line, QStringLiteral("PeTop"), &value, error))
                return false;
            headers.append(value);
        }
        track->pe = pe;
        track->iterPe = headers.value(0);
        track->iterNum = headers.value(1);
        QList<Node> statements;
        for (const auto& child : node.args.constLast()) {
            if (child.name != QLatin1String("atom"))
                statements.append(child);
        }
        if (m_options.compilerFirstWait && !statements.isEmpty() && statements.first().name == QLatin1String("Wait")) {
            const Node first = statements.takeFirst();
            if (first.args.isEmpty()
                || !integerArgument(first.args.first(), first.line, QStringLiteral("Wait"), &track->iterPe, error)) {
                return false;
            }
        }
        track->repeat = std::max(1, track->iterPe + 1);
        qint64 cycle = 0;
        for (int i = 0; i < track->repeat; ++i) {
            QStringList loops;
            if (track->repeat > 1)
                loops << QStringLiteral("PE %1/%2").arg(i + 1).arg(track->repeat);
            if (!emitBlock(statements, track, &cycle, loops, error))
                return false;
        }
        track->cycles = cycle;
        return true;
    }

    QStringList warnings() const
    {
        return m_warnings;
    }

private:
    bool emitBlock(const QList<Node>& nodes, Track* track, qint64* cycle, const QStringList& loops, QString* error)
    {
        for (const auto& node : nodes) {
            if (node.name == QLatin1String("atom") || node.name == QLatin1String("input")
                || node.name == QLatin1String("label"))
                continue;
            if (operationNames().contains(node.name)) {
                if (!addEvent(node, track, cycle, 1, loops, error))
                    return false;
                if (needsSmIncrement(node)) {
                    Node helper;
                    helper.name = QStringLiteral("SMInc");
                    helper.value = QStringLiteral("compiler input-address helper for %1").arg(commandSource(node));
                    helper.line = node.line;
                    if (!addEvent(helper, track, cycle, 1, loops, error))
                        return false;
                }
            } else if (node.name == QLatin1String("Wait")) {
                int duration = 0;
                if (node.args.isEmpty() || !integerArgument(node.args.first(), node.line, node.name, &duration, error))
                    return false;
                if (duration > 0 && !addEvent(node, track, cycle, duration, loops, error))
                    return false;
            } else if (node.name == QLatin1String("For")) {
                int count = 0;
                if (node.args.size() != 2
                    || !integerArgument(node.args.value(0), node.line, node.name, &count, error)) {
                    if (error->isEmpty())
                        *error = QStringLiteral("line %1: \\For needs count and body").arg(node.line);
                    return false;
                }
                ++count;
                if (count < 0) {
                    *error = QStringLiteral("line %1: negative \\For count").arg(node.line);
                    return false;
                }
                for (int i = 0; i < count; ++i) {
                    if (!emitBlock(node.args.at(1), track, cycle,
                                   loops + QStringList{QStringLiteral("For %1/%2").arg(i + 1).arg(count)}, error))
                        return false;
                }
            } else if (node.name == QLatin1String("If")) {
                if (node.args.size() >= 2) {
                    if (!emitBlock(node.args.constLast(), track, cycle,
                                   loops + QStringList{QStringLiteral("If (shown as taken)")}, error))
                        return false;
                    m_warnings << QStringLiteral("line %1: If branch is visualized as taken").arg(node.line);
                }
            } else if (node.name == QLatin1String("While")) {
                if (!node.args.isEmpty()) {
                    if (!emitBlock(node.args.constLast(), track, cycle,
                                   loops + QStringList{QStringLiteral("While (one iteration)")}, error))
                        return false;
                    m_warnings << QStringLiteral("line %1: While trip count is dynamic; one body iteration is shown")
                                      .arg(node.line);
                }
            } else if (node.name == QLatin1String("Break")) {
                if (!addEvent(node, track, cycle, 1, loops, error))
                    return false;
            } else if (node.name == QLatin1String("block")) {
                if (!emitBlock(node.args.value(0), track, cycle, loops, error))
                    return false;
            } else {
                m_warnings << QStringLiteral("line %1: ignored unsupported command \\%2").arg(node.line).arg(node.name);
            }
        }
        return true;
    }

    bool addEvent(const Node& node, Track* track, qint64* cycle, qint64 duration, const QStringList& loops,
                  QString* error)
    {
        if (m_nextId >= m_options.maxEvents) {
            *error = QStringLiteral("event limit %1 exceeded").arg(m_options.maxEvents);
            return false;
        }
        Event event;
        event.id = m_nextId++;
        event.pe = track->pe;
        event.start = *cycle;
        event.duration = duration;
        event.name = node.name;
        if (node.name == QLatin1String("Add") && node.args.size() == 4
            && std::all_of(node.args.cbegin(), node.args.cend(), [](const QList<Node>& arg) {
                   return argText(arg).trimmed() == QLatin1String("0");
               })) {
            event.name = QStringLiteral("NOP");
        }
        event.category = categoryFor(event.name);
        event.source = node.name == QLatin1String("SMInc") ? node.value : commandSource(node);
        event.line = node.line;
        event.loop = loops.join(QStringLiteral(" > "));
        if (operationNames().contains(node.name)) {
            event.writes = smAddresses(node.args.value(0));
            event.reads = smAddresses(node.args.value(1));
            event.reads += smAddresses(node.args.value(2));
        }
        track->events.append(std::move(event));
        *cycle += duration;
        return true;
    }

    const Options& m_options;
    int m_nextId = 0;
    QStringList m_warnings;
};

struct StorageKey
{
    int bank = 0;
    int depth = 0;
    bool operator==(const StorageKey& other) const
    {
        return bank == other.bank && depth == other.depth;
    }
};

size_t qHash(const StorageKey& key, size_t seed = 0)
{
    return qHashMulti(seed, key.bank, key.depth);
}

int bankFor(int address, const Options& options)
{
    if (!options.pea64BankLayout)
        return ((address % options.bankCount) + options.bankCount) % options.bankCount;
    return (address >= 2048 ? 32 : 0) + (address & 31);
}

StorageKey storageFor(const Event& event, int address, bool write, const Options& options)
{
    if (options.pea64BankLayout && write && event.name == QLatin1String("MCC") && address >= 32 && address < 2048) {
        return {32 + (address & 31), (address - 32) >> 5};
    }
    const int bank = bankFor(address, options);
    return {bank, options.pea64BankLayout ? ((address & 2047) >> 5) : address / options.bankCount};
}

void addMark(const QList<Event*>& events, const QString& message, bool advisory, QSet<QString>* groups)
{
    QSet<int> ids;
    for (auto* event : events) {
        if (!event)
            continue;
        ids.insert(event->id);
        auto& target = advisory ? event->advisories : event->conflicts;
        if (!target.contains(message))
            target.append(message);
    }
    QList<int> sorted(ids.cbegin(), ids.cend());
    std::sort(sorted.begin(), sorted.end());
    if (!sorted.isEmpty())
        groups->insert(QStringLiteral("%1:%2:%3").arg(sorted.first()).arg(sorted.last()).arg(message));
}

void detectConflicts(Model* model, const Options& options)
{
    QMap<qint64, QList<Event*>> byCycle;
    for (auto& track : model->tracks) {
        for (auto& event : track.events) {
            if (event.reads.isEmpty() && event.writes.isEmpty())
                continue;
            for (qint64 cycle = event.start; cycle < event.start + event.duration; ++cycle)
                byCycle[cycle].append(&event);
        }
    }
    QSet<QString> conflicts;
    QSet<QString> advisories;
    for (auto it = byCycle.cbegin(); it != byCycle.cend(); ++it) {
        QHash<int, QList<QPair<Event*, int>>> readsByBank;
        QHash<int, QList<Event*>> writesByBank;
        QHash<StorageKey, QList<QPair<Event*, int>>> readsByStorage;
        QHash<StorageKey, QList<QPair<Event*, int>>> writesByStorage;
        for (auto* event : it.value()) {
            for (int address : event->reads) {
                readsByBank[bankFor(address, options)].append({event, address});
                readsByStorage[storageFor(*event, address, false, options)].append({event, address});
            }
            for (int address : event->writes) {
                const auto key = storageFor(*event, address, true, options);
                writesByBank[key.bank].append(event);
                writesByStorage[key].append({event, address});
            }
        }
        for (auto readIt = readsByBank.cbegin(); readIt != readsByBank.cend(); ++readIt) {
            QList<QPair<Event*, int>> ordinary;
            QList<QPair<Event*, int>> mcc;
            for (const auto& item : readIt.value())
                (item.first->name == QLatin1String("MCC") ? mcc : ordinary).append(item);
            if (!options.allowMccBroadcast)
                ordinary = readIt.value();
            if (ordinary.size() > 1) {
                QList<Event*> owners;
                for (const auto& item : ordinary)
                    owners << item.first;
                addMark(owners,
                        QStringLiteral("cycle %1: read bank %2 has %3 accesses")
                            .arg(it.key())
                            .arg(readIt.key())
                            .arg(ordinary.size()),
                        false, &conflicts);
            }
            if (options.allowMccBroadcast && !mcc.isEmpty()) {
                QSet<int> addresses;
                QList<Event*> owners;
                for (const auto& item : mcc) {
                    addresses.insert(item.second);
                    owners << item.first;
                }
                if (addresses.size() > 1) {
                    addMark(owners,
                            QStringLiteral("cycle %1: read bank %2 has %3 distinct MCC addresses")
                                .arg(it.key())
                                .arg(readIt.key())
                                .arg(addresses.size()),
                            false, &conflicts);
                }
                if (!ordinary.isEmpty()) {
                    for (const auto& item : ordinary)
                        owners << item.first;
                    addMark(owners,
                            QStringLiteral(
                                "cycle %1: bank %2 has MCC/ordinary read overlap; MCC pipeline phase is not modeled")
                                .arg(it.key())
                                .arg(readIt.key()),
                            true, &advisories);
                }
            }
        }
        for (auto writeIt = writesByBank.cbegin(); writeIt != writesByBank.cend(); ++writeIt) {
            if (writeIt.value().size() > 1) {
                addMark(writeIt.value(),
                        QStringLiteral("cycle %1: write bank %2 has %3 accesses")
                            .arg(it.key())
                            .arg(writeIt.key())
                            .arg(writeIt.value().size()),
                        false, &conflicts);
            }
        }
        for (auto readIt = readsByStorage.cbegin(); readIt != readsByStorage.cend(); ++readIt) {
            const auto writeIt = writesByStorage.constFind(readIt.key());
            if (writeIt == writesByStorage.cend())
                continue;
            QList<Event*> involved;
            for (const auto& read : readIt.value()) {
                for (const auto& write : writeIt.value()) {
                    if (read.first->id != write.first->id)
                        involved << read.first << write.first;
                }
            }
            if (!involved.isEmpty()) {
                addMark(involved,
                        QStringLiteral("cycle %1: bank %2 depth %3 read/write overlap")
                            .arg(it.key())
                            .arg(readIt.key().bank)
                            .arg(readIt.key().depth),
                        false, &conflicts);
            }
        }
    }

    QHash<StorageKey, QList<QPair<Event*, bool>>> accesses;
    for (auto& track : model->tracks) {
        for (auto& event : track.events) {
            for (int address : event.writes)
                accesses[storageFor(event, address, true, options)].append({&event, true});
            for (int address : event.reads)
                accesses[storageFor(event, address, false, options)].append({&event, false});
        }
    }
    for (auto it = accesses.begin(); it != accesses.end(); ++it) {
        auto items = it.value();
        std::sort(items.begin(), items.end(), [](const auto& left, const auto& right) {
            if (left.first->start != right.first->start)
                return left.first->start < right.first->start;
            return left.second && !right.second;
        });
        Event* lastWrite = nullptr;
        for (const auto& item : items) {
            if (item.second) {
                lastWrite = item.first;
            } else if (lastWrite && item.first->start > lastWrite->start && item.first->start - lastWrite->start < 9) {
                const qint64 gap = item.first->start - lastWrite->start;
                addMark({lastWrite, item.first},
                        QStringLiteral("bank %1 depth %2 write-to-read gap %3 cycles (<9): PE%4 -> PE%5")
                            .arg(it.key().bank)
                            .arg(it.key().depth)
                            .arg(gap)
                            .arg(lastWrite->pe)
                            .arg(item.first->pe),
                        false, &conflicts);
            }
        }
    }
    model->conflictCount = conflicts.size();
    model->advisoryCount = advisories.size();
}

QJsonArray integerArray(const QList<int>& values)
{
    QJsonArray array;
    for (int value : values)
        array.append(value);
    return array;
}

QJsonArray stringArray(const QStringList& values)
{
    QJsonArray array;
    for (const auto& value : values)
        array.append(value);
    return array;
}

}

bool Analyzer::analyze(const QString& text, const QString& sourceFile, const Options& options, Model* model,
                       QString* error)
{
    if (!model || !error)
        return false;
    *error = QString();
    *model = Model();
    if (options.bankCount <= 0) {
        *error = QStringLiteral("bank count must be positive");
        return false;
    }
    if (options.pea64BankLayout && options.bankCount != 64) {
        *error = QStringLiteral("PEA64 bank layout requires 64 banks");
        return false;
    }
    QList<Node> roots;
    Parser parser(text);
    if (!parser.parse(&roots, error))
        return false;
    QList<const Node*> peaTops;
    findPeaTops(roots, &peaTops);
    if (peaTops.isEmpty()) {
        *error = QStringLiteral("no \\PeaTop block found");
        return false;
    }
    const Node* selected = nullptr;
    int selectedIndex = 0;
    for (int index = 0; index < peaTops.size(); ++index) {
        int peaValue = index;
        if (!peaTops.at(index)->args.isEmpty()
            && !integerArgument(peaTops.at(index)->args.first(), peaTops.at(index)->line, QStringLiteral("PeaTop"),
                                &peaValue, error))
            return false;
        if (peaValue == options.pea || (!selected && index == options.pea)) {
            selected = peaTops.at(index);
            selectedIndex = peaValue;
            break;
        }
    }
    if (!selected) {
        *error = QStringLiteral("PEA %1 not found").arg(options.pea);
        return false;
    }
    if (selected->args.size() < 2) {
        *error = QStringLiteral("line %1: \\PeaTop needs index and body").arg(selected->line);
        return false;
    }
    QList<const Node*> peTops;
    for (const auto& node : selected->args.constLast()) {
        if (node.name == QLatin1String("PeTop"))
            peTops << &node;
    }
    ScheduleBuilder builder(options);
    model->title = sourceFile.section(QLatin1Char('/'), -1).section(QLatin1Char('\\'), -1);
    model->sourceFile = sourceFile;
    model->pea = selectedIndex;
    model->bankCount = options.bankCount;
    model->clockNs = options.clockNs;
    model->bankLayout = options.pea64BankLayout ? QStringLiteral("pea64") : QStringLiteral("modulo");
    for (int pe = 0; pe < std::min(64, static_cast<int>(peTops.size())); ++pe) {
        Track track;
        if (!builder.build(*peTops.at(pe), pe, &track, error))
            return false;
        model->tracks.append(std::move(track));
    }
    while (model->tracks.size() < 64) {
        Track track;
        track.pe = model->tracks.size();
        model->tracks.append(track);
    }
    model->warnings = builder.warnings();
    if (peTops.size() > 64)
        model->warnings
            << QStringLiteral("PEA contains %1 PeTop blocks; only the first 64 are shown").arg(peTops.size());
    for (const auto& track : std::as_const(model->tracks)) {
        model->maxCycle = std::max(model->maxCycle, track.cycles);
        model->eventCount += track.events.size();
    }
    detectConflicts(model, options);
    model->warnings.removeDuplicates();
    std::sort(model->warnings.begin(), model->warnings.end());
    return true;
}

QByteArray Analyzer::toJson(const Model& model)
{
    QJsonObject root;
    root[QStringLiteral("title")] = model.title;
    root[QStringLiteral("source_file")] = model.sourceFile;
    root[QStringLiteral("pea")] = model.pea;
    root[QStringLiteral("max_cycle")] = static_cast<double>(model.maxCycle);
    root[QStringLiteral("event_count")] = model.eventCount;
    root[QStringLiteral("bank_count")] = model.bankCount;
    root[QStringLiteral("clock_ns")] = model.clockNs;
    root[QStringLiteral("warnings")] = stringArray(model.warnings);
    root[QStringLiteral("conflict_count")] = model.conflictCount;
    root[QStringLiteral("advisory_count")] = model.advisoryCount;
    root[QStringLiteral("bank_layout")] = model.bankLayout;
    root[QStringLiteral("timing_model")] =
        QStringLiteral("Static issue schedule: operations=1 cycle, For{N}=N+1, Wait{N}=N cycles.");
    QJsonArray tracks;
    for (const auto& track : model.tracks) {
        QJsonObject trackObject;
        trackObject[QStringLiteral("pe")] = track.pe;
        trackObject[QStringLiteral("cycles")] = static_cast<double>(track.cycles);
        trackObject[QStringLiteral("header")] = QJsonObject{{QStringLiteral("iter_pe"), track.iterPe},
                                                            {QStringLiteral("iter_num"), track.iterNum},
                                                            {QStringLiteral("repeat"), track.repeat}};
        QJsonArray events;
        for (const auto& event : track.events) {
            events.append(QJsonObject{{QStringLiteral("id"), event.id},
                                      {QStringLiteral("pe"), event.pe},
                                      {QStringLiteral("start"), static_cast<double>(event.start)},
                                      {QStringLiteral("duration"), static_cast<double>(event.duration)},
                                      {QStringLiteral("name"), event.name},
                                      {QStringLiteral("category"), event.category},
                                      {QStringLiteral("source"), event.source},
                                      {QStringLiteral("line"), event.line},
                                      {QStringLiteral("loop"), event.loop},
                                      {QStringLiteral("reads"), integerArray(event.reads)},
                                      {QStringLiteral("writes"), integerArray(event.writes)},
                                      {QStringLiteral("conflicts"), stringArray(event.conflicts)},
                                      {QStringLiteral("advisories"), stringArray(event.advisories)}});
        }
        trackObject[QStringLiteral("events")] = events;
        tracks.append(trackObject);
    }
    root[QStringLiteral("tracks")] = tracks;
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

}
