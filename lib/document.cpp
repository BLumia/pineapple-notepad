#include "document.h"

#include "sciedit.h"

#include <QLoggingCategory>
#include <QFile>

Q_LOGGING_CATEGORY(lcDocument, "pnotepad.document")
#include <QFileInfo>
#include <QPair>
#include <QSaveFile>
#include <QStringConverter>
#include <QStringDecoder>
#include <QStringEncoder>

#include <Scintilla.h>

#include <KEncodingProber>

namespace {
struct EolResult {
    int mode = SC_EOL_LF;
    bool mixed = false;
};

struct EolCounter {
    qint64 crlf = 0;
    qint64 lf = 0;
    qint64 cr = 0;
    bool prevEndedWithCr = false;

    void feed(const QString &text)
    {
        for (qsizetype i = 0; i < text.size(); ++i) {
            const QChar c = text.at(i);
            if (c == QLatin1Char('\r')) {
                if (i + 1 < text.size() && text.at(i + 1) == QLatin1Char('\n')) {
                    ++crlf;
                    ++i;
                } else {
                    ++cr;
                    prevEndedWithCr = (i + 1 == text.size());
                }
                continue;
            }
            if (prevEndedWithCr && c == QLatin1Char('\n')) {
                --cr;
                ++crlf;
                prevEndedWithCr = false;
                continue;
            }
            prevEndedWithCr = false;
            if (c == QLatin1Char('\n')) {
                ++lf;
                continue;
            }
        }
    }

    EolResult finish() const
    {
        const int kinds = (crlf > 0 ? 1 : 0) + (lf > 0 ? 1 : 0) + (cr > 0 ? 1 : 0);
        EolResult out;
        out.mixed = kinds > 1;

        if (crlf == 0 && lf == 0 && cr == 0) {
            out.mode = SC_EOL_LF;
            return out;
        }

        if (crlf >= lf && crlf >= cr) {
            out.mode = SC_EOL_CRLF;
        } else if (lf >= cr) {
            out.mode = SC_EOL_LF;
        } else {
            out.mode = SC_EOL_CR;
        }
        return out;
    }
};

QByteArray takeUnicodeBom(QByteArray &view)
{
    if (view.startsWith("\xEF\xBB\xBF")) {
        view = view.mid(3);
        return QByteArray("\xEF\xBB\xBF", 3);
    }
    if (view.size() >= 4 && static_cast<unsigned char>(view[0]) == 0xFF && static_cast<unsigned char>(view[1]) == 0xFE &&
        static_cast<unsigned char>(view[2]) == 0x00 && static_cast<unsigned char>(view[3]) == 0x00) {
        const QByteArray bom = view.left(4);
        view = view.mid(4);
        return bom;
    }
    if (view.size() >= 4 && static_cast<unsigned char>(view[0]) == 0x00 && static_cast<unsigned char>(view[1]) == 0x00 &&
        static_cast<unsigned char>(view[2]) == 0xFE && static_cast<unsigned char>(view[3]) == 0xFF) {
        const QByteArray bom = view.left(4);
        view = view.mid(4);
        return bom;
    }
    if (view.size() >= 2 && static_cast<unsigned char>(view[0]) == 0xFF && static_cast<unsigned char>(view[1]) == 0xFE) {
        const QByteArray bom = view.left(2);
        view = view.mid(2);
        return bom;
    }
    if (view.size() >= 2 && static_cast<unsigned char>(view[0]) == 0xFE && static_cast<unsigned char>(view[1]) == 0xFF) {
        const QByteArray bom = view.left(2);
        view = view.mid(2);
        return bom;
    }
    return QByteArray();
}

QByteArray codecNameForUnicodeBom(const QByteArray &bom)
{
    if (bom.size() == 3 && bom == QByteArray("\xEF\xBB\xBF", 3)) {
        return QByteArrayLiteral("UTF-8");
    }
    if (bom.size() == 2 && static_cast<unsigned char>(bom[0]) == 0xFF && static_cast<unsigned char>(bom[1]) == 0xFE) {
        return QByteArrayLiteral("UTF-16LE");
    }
    if (bom.size() == 2 && static_cast<unsigned char>(bom[0]) == 0xFE && static_cast<unsigned char>(bom[1]) == 0xFF) {
        return QByteArrayLiteral("UTF-16BE");
    }
    if (bom.size() == 4 && static_cast<unsigned char>(bom[0]) == 0xFF && static_cast<unsigned char>(bom[1]) == 0xFE &&
        static_cast<unsigned char>(bom[2]) == 0x00 && static_cast<unsigned char>(bom[3]) == 0x00) {
        return QByteArrayLiteral("UTF-32LE");
    }
    if (bom.size() == 4 && static_cast<unsigned char>(bom[0]) == 0x00 && static_cast<unsigned char>(bom[1]) == 0x00 &&
        static_cast<unsigned char>(bom[2]) == 0xFE && static_cast<unsigned char>(bom[3]) == 0xFF) {
        return QByteArrayLiteral("UTF-32BE");
    }
    return QByteArray();
}

QString displayNameForCodec(const QByteArray &codecName, bool writeBom)
{
    const QString base = codecName.isEmpty() ? QStringLiteral("UTF-8") : QString::fromLatin1(codecName);
    if (!writeBom) {
        return base;
    }

    if (codecName == QByteArrayLiteral("UTF-8")) {
        return QStringLiteral("UTF-8 BOM");
    }
    if (codecName == QByteArrayLiteral("UTF-16LE")) {
        return QStringLiteral("UTF-16 LE BOM");
    }
    if (codecName == QByteArrayLiteral("UTF-16BE")) {
        return QStringLiteral("UTF-16 BE BOM");
    }
    if (codecName == QByteArrayLiteral("UTF-32LE")) {
        return QStringLiteral("UTF-32 LE BOM");
    }
    if (codecName == QByteArrayLiteral("UTF-32BE")) {
        return QStringLiteral("UTF-32 BE BOM");
    }
    return QStringLiteral("%1 BOM").arg(base);
}

QByteArray encodeText(const QString &text, const QByteArray &codecName, bool writeBom)
{
    const QStringConverter::Flags flags = writeBom ? QStringConverter::Flag::WriteBom : QStringConverter::Flag::Default;
    if (!codecName.isEmpty()) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        QStringEncoder encoder(QString::fromLatin1(codecName), flags);
#else
        QStringEncoder encoder(codecName.constData(), flags);
#endif
        if (encoder.isValid()) {
            const QByteArray out = QByteArray(encoder(text));
            if (encoder.hasError()) {
                // It's safe to ignore the error since the target encoding might not support all characters in the input string.
                qCDebug(lcDocument) << "Encoding to" << codecName << "with BOM =" << writeBom 
                                    << "has error likely due to limitations in the target encoding, which can likely be ignored.";
            }
            return out;
        }
    }

    QStringEncoder encoder(QStringConverter::Utf8, flags);
    const QByteArray out = QByteArray(encoder(text));
    return encoder.hasError() ? QByteArray() : out;
}
}

bool Document::loadFromDiskInto(SciEdit *editor)
{
    return loadFromDiskInto(editor, QByteArray(), false, QString());
}

bool Document::loadFromDiskInto(SciEdit *targetEditor, const QByteArray &forcedCodecName, bool forcedWriteBom, const QString &forcedDisplayName)
{
    if (!targetEditor || filePath.isEmpty()) {
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(lcDocument) << "Failed to open file:" << filePath;
        return false;
    }

    const qint64 fileSize = file.size();
    static const qint64 kLargeFileThresholdBytes = 200LL * 1024LL * 1024LL;
    isLargeFile = fileSize >= kLargeFileThresholdBytes;
    isLoading = true;

    const bool oldUrlScanEnabled = targetEditor->urlScanEnabled();
    targetEditor->beginUrlScanBatch();
    targetEditor->beginSpellCheckBatch();
    if (isLargeFile) {
        targetEditor->setUrlScanEnabled(false);
    }

    targetEditor->setUndoCollection(false);
    targetEditor->clearAll();
    targetEditor->setCodePage(SC_CP_UTF8);

    const qint64 extra = qMin<qint64>(1LL << 20, fileSize / 6);
    const qint64 allocSize = qMax<qint64>(0, fileSize + extra);
    targetEditor->allocate(allocSize);

    QByteArray chosenCodec = QByteArrayLiteral("UTF-8");
    bool chosenWriteBom = false;
    QString chosenDisplayName = QStringLiteral("UTF-8");

    QStringDecoder decoder(QStringConverter::Utf8);
    bool decoderInitialized = false;

    auto initDecoder = [&](const QByteArray &codecName, bool writeBom, const QString &displayName) {
        QStringDecoder d(codecName);
        if (!d.isValid()) {
            d = QStringDecoder(QStringConverter::Utf8);
            chosenCodec = QByteArrayLiteral("UTF-8");
            chosenWriteBom = false;
            chosenDisplayName = QStringLiteral("UTF-8");
        } else {
            chosenCodec = codecName.isEmpty() ? QByteArrayLiteral("UTF-8") : codecName;
            chosenWriteBom = writeBom;
            chosenDisplayName = displayName.isEmpty() ? displayNameForCodec(chosenCodec, chosenWriteBom) : displayName;
        }
        decoder = std::move(d);
        decoderInitialized = true;
    };

    if (!forcedCodecName.isEmpty()) {
        initDecoder(forcedCodecName, forcedWriteBom, forcedDisplayName);
    }

    EolCounter eolCounter;
    static const qint64 kProbeSize = 1024LL * 1024LL;
    static const qint64 kChunkSize = 256LL * 1024LL;

    bool first = true;
    while (!file.atEnd()) {
        QByteArray chunk = file.read(first ? qMin(kProbeSize, fileSize) : kChunkSize);
        if (chunk.isEmpty()) {
            break;
        }

        if (first && forcedCodecName.isEmpty()) {
            QByteArray view = chunk;
            const QByteArray bom = takeUnicodeBom(view);
            if (!bom.isEmpty()) {
                const QByteArray bomCodec = codecNameForUnicodeBom(bom);
                initDecoder(bomCodec.isEmpty() ? QByteArrayLiteral("UTF-8") : bomCodec, true, QString());
                chunk = view;
            } else {
                QStringDecoder utf8(QStringConverter::Utf8);
                const QString utf8Text = utf8.decode(view);
                if (!utf8.hasError()) {
                    initDecoder(QByteArrayLiteral("UTF-8"), false, QStringLiteral("UTF-8"));
                    chunk = view;
                } else {
                    KEncodingProber prober(KEncodingProber::Universal);
                    prober.feed(view);
                    const QByteArray probed = prober.encoding();
                    if (!probed.isEmpty() && prober.confidence() >= 0.6f) {
                        initDecoder(probed, false, QString::fromLatin1(probed));
                    } else {
                        initDecoder(QByteArrayLiteral("UTF-8"), false, QStringLiteral("UTF-8"));
                    }
                    chunk = view;
                }
            }
        }

        if (!decoderInitialized) {
            initDecoder(QByteArrayLiteral("UTF-8"), false, QStringLiteral("UTF-8"));
        }

        const QString text = decoder.decode(chunk);
        if (!text.isEmpty()) {
            eolCounter.feed(text);
            const QByteArray utf8Bytes = text.toUtf8();
            if (!utf8Bytes.isEmpty()) {
                targetEditor->appendText(utf8Bytes.size(), utf8Bytes.constData());
            }
        }

        first = false;
    }

    file.close();

    const EolResult eol = eolCounter.finish();
    eolMode = eol.mode;
    eolMixed = eol.mixed;
    codecName = chosenCodec;
    encodingWriteBom = chosenWriteBom;
    encoding = chosenDisplayName;

    targetEditor->setEOLMode(eolMode);
    if (isLargeFile) {
        targetEditor->setWrapMode(SC_WRAP_NONE);
    }
    targetEditor->setIsLargeFile(isLargeFile);
    targetEditor->setUndoCollection(true);
    targetEditor->emptyUndoBuffer();
    targetEditor->setSavePoint();

    if (!isLargeFile) {
        targetEditor->setUrlScanEnabled(oldUrlScanEnabled);
    }
    targetEditor->endUrlScanBatch();
    targetEditor->endSpellCheckBatch();

    content = QByteArray();
    lastKnownDiskModifiedAt = QFileInfo(filePath).lastModified();
    isLoading = false;
    return true;
}

void Document::attachEditor(SciEdit *targetEditor)
{
    if (!targetEditor) {
        return;
    }

    if (hasEditor(targetEditor)) {
        return;
    }

    if (m_editorRefs.isEmpty()) {
        m_editorRefs.append({targetEditor, 0, 0, 0});
        if (!untitled && !filePath.isEmpty()) {
            loadFromDiskInto(targetEditor);
            return;
        }

        targetEditor->setUndoCollection(false);
        targetEditor->clearAll();
        targetEditor->setEOLMode(eolMode);
        targetEditor->setUndoCollection(true);
        targetEditor->emptyUndoBuffer();
        targetEditor->setSavePoint();
    } else {
        SciEdit *primary = m_editorRefs.first().editor;
        if (!primary) {
            m_editorRefs.append({targetEditor, 0, 0, 0});
            if (!untitled && !filePath.isEmpty()) {
                loadFromDiskInto(targetEditor);
            }
            return;
        }

        sptr_t docPtr = primary->docPointer();
        primary->addRefDocument(docPtr);

        sptr_t oldDocPtr = targetEditor->docPointer();
        targetEditor->setDocPointer(docPtr);
        targetEditor->releaseDocument(oldDocPtr);

        m_editorRefs.append({targetEditor, 0, 0, 0});
    }
}

void Document::addEditorReference(SciEdit *editor)
{
    attachEditor(editor);
}

void Document::removeEditorReference(SciEdit *editor)
{
    for (int i = 0; i < m_editorRefs.size(); ++i) {
        if (m_editorRefs[i].editor == editor) {
            m_editorRefs.removeAt(i);
            return;
        }
    }
}

bool Document::hasEditor(SciEdit *editor) const
{
    for (const auto &ref : m_editorRefs) {
        if (ref.editor == editor) {
            return true;
        }
    }
    return false;
}

bool Document::hasAnyEditor() const
{
    for (const auto &ref : m_editorRefs) {
        if (ref.editor) {
            return true;
        }
    }
    return false;
}

SciEdit *Document::firstEditor() const
{
    for (const auto &ref : m_editorRefs) {
        if (ref.editor) {
            return ref.editor;
        }
    }
    return nullptr;
}

const QVector<EditorRef> &Document::editorReferences() const
{
    return m_editorRefs;
}

bool Document::save()
{
    if (filePath.isEmpty()) {
        return false;
    }
    SciEdit *ed = firstEditor();
    if (!ed) {
        return false;
    }

    isSaving = true;

    const QByteArray utf8Bytes = ed->getText(ed->textLength());
    const QString text = QString::fromUtf8(utf8Bytes);
    QByteArray outBytes = encodeText(text, codecName, encodingWriteBom);
    if (outBytes.isEmpty()) {
        outBytes = text.toUtf8();
    }

    QSaveFile file(filePath);
    file.setDirectWriteFallback(true);
    if (!file.open(QIODevice::WriteOnly)) {
        qCWarning(lcDocument) << "Failed to save file:" << filePath << "Error:" << file.errorString();
        isSaving = false;
        return false;
    }
    if (file.write(outBytes) < 0) {
        qCWarning(lcDocument) << "Failed to write file:" << filePath << "Error:" << file.errorString();
        isSaving = false;
        return false;
    }
    if (!file.commit()) {
        qCWarning(lcDocument) << "Failed to commit file:" << filePath << "Error:" << file.errorString();
        isSaving = false;
        return false;
    }

    lastKnownDiskModifiedAt = QFileInfo(filePath).lastModified();
    contentModified = false;
    encodingModified = false;
    untitled = false;
    for (const auto &ref : m_editorRefs) {
        if (ref.editor) {
            ref.editor->setSavePoint();
        }
    }
    isSaving = false;
    return true;
}

bool Document::saveAs(const QString &newPath)
{
    if (newPath.isEmpty()) {
        return false;
    }

    filePath = newPath;
    title = QFileInfo(newPath).fileName();
    untitled = false;
    return save();
}

bool Document::reload(bool preserveViewState)
{
    if (filePath.isEmpty()) {
        return false;
    }
    SciEdit *ed = firstEditor();
    if (!ed) {
        return false;
    }

    QVector<QPair<QPointer<SciEdit>, QPair<sptr_t, sptr_t>>> savedPositions;
    if (preserveViewState) {
        for (const auto &ref : m_editorRefs) {
            if (ref.editor) {
                savedPositions.append({ref.editor,
                    {ref.editor->currentPos(),
                     ref.editor->firstVisibleLine()}});
            }
        }
    }

    if (!loadFromDiskInto(ed)) {
        return false;
    }

    if (preserveViewState) {
        for (const auto &pair : savedPositions) {
            SciEdit *refEditor = pair.first;
            if (refEditor) {
                refEditor->gotoPos(pair.second.first);
                refEditor->setFirstVisibleLine(pair.second.second);
                refEditor->setXOffset(0);
                refEditor->scrollCaret();
            }
        }
    }
    return true;
}
