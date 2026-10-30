#pragma once

#include <QString>

class QTextStream;
class SciEdit;

class Exporter
{
public:
    explicit Exporter(SciEdit *editor);
    virtual ~Exporter();

    void convert(QTextStream &stream);
    void convertRange(QTextStream &stream, int start, int end);

protected:
    virtual void writeRange(QTextStream &stream, int start, int end) = 0;
    void ensureDocumentStyled(int start, int end);

    SciEdit *m_editor;
};

class RtfExporter : public Exporter
{
public:
    using Exporter::Exporter;

protected:
    void writeRange(QTextStream &stream, int start, int end) override;
};

class HtmlExporter : public Exporter
{
public:
    HtmlExporter(SciEdit *editor, const QString &documentTitle = QString());

protected:
    void writeRange(QTextStream &stream, int start, int end) override;

private:
    QString m_documentTitle;
};
