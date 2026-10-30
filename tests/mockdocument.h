#pragma once

#include <ILexer.h>
#include <Scintilla.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace pineapple {

class MockDocument : public Scintilla::IDocument {
public:
    std::string text;
    std::vector<Sci_Position> lineStarts;
    std::vector<Sci_Position> lineEnds;
    std::vector<int> lineStates;
    std::vector<int> lineLevels;
    std::vector<char> styles;
    Sci_Position stylingPos = 0;

    void setText(const std::string &t)
    {
        text = t;
        lineStarts.clear();
        lineEnds.clear();
        lineStarts.push_back(0);
        Sci_Position lineEndPos = 0;
        for (size_t i = 0; i < text.size(); ++i) {
            if (text[i] == '\n') {
                lineEnds.push_back(lineEndPos);
                lineStarts.push_back(static_cast<Sci_Position>(i + 1));
            }
            lineEndPos = static_cast<Sci_Position>(i + 1);
        }
        lineEnds.push_back(lineEndPos);

        lineStates.assign(lineStarts.size(), 0);
        lineLevels.assign(lineStarts.size(), SC_FOLDLEVELBASE);
        styles.assign(text.size(), 0);
    }

    int lineCount() const { return static_cast<int>(lineStarts.size()); }

    int SCI_METHOD Version() const override { return Scintilla::dvRelease4; }
    void SCI_METHOD SetErrorStatus(int) override {}
    Sci_Position SCI_METHOD Length() const override
    {
        return static_cast<Sci_Position>(text.size());
    }
    void SCI_METHOD GetCharRange(char *buffer, Sci_Position position,
                                  Sci_Position lengthRetrieve) const override
    {
        std::memcpy(buffer, text.data() + position, lengthRetrieve);
    }
    char SCI_METHOD StyleAt(Sci_Position position) const override
    {
        if (position < 0 || position >= static_cast<Sci_Position>(styles.size()))
            return 0;
        return styles[static_cast<size_t>(position)];
    }
    Sci_Position SCI_METHOD LineFromPosition(Sci_Position position) const override
    {
        auto it = std::upper_bound(lineStarts.begin(), lineStarts.end(), position);
        return static_cast<Sci_Position>(it - lineStarts.begin() - 1);
    }
    Sci_Position SCI_METHOD LineStart(Sci_Position line) const override
    {
        if (line < 0)
            return 0;
        if (line >= static_cast<Sci_Position>(lineStarts.size()))
            return static_cast<Sci_Position>(text.size());
        return lineStarts[static_cast<size_t>(line)];
    }
    int SCI_METHOD GetLevel(Sci_Position line) const override
    {
        if (line < 0 || line >= static_cast<Sci_Position>(lineLevels.size()))
            return 0;
        return lineLevels[static_cast<size_t>(line)];
    }
    int SCI_METHOD SetLevel(Sci_Position line, int level) override
    {
        if (line >= 0 && line < static_cast<Sci_Position>(lineLevels.size()))
            lineLevels[static_cast<size_t>(line)] = level;
        return 0;
    }
    int SCI_METHOD GetLineState(Sci_Position line) const override
    {
        if (line < 0 || line >= static_cast<Sci_Position>(lineStates.size()))
            return 0;
        return lineStates[static_cast<size_t>(line)];
    }
    int SCI_METHOD SetLineState(Sci_Position line, int state) override
    {
        if (line >= 0 && line < static_cast<Sci_Position>(lineStates.size()))
            lineStates[static_cast<size_t>(line)] = state;
        return 0;
    }
    void SCI_METHOD StartStyling(Sci_Position position) override { stylingPos = position; }
    bool SCI_METHOD SetStyleFor(Sci_Position length, char style) override
    {
        for (Sci_Position i = 0; i < length; ++i)
            if (stylingPos + i < static_cast<Sci_Position>(styles.size()))
                styles[static_cast<size_t>(stylingPos + i)] = style;
        stylingPos += length;
        return true;
    }
    bool SCI_METHOD SetStyles(Sci_Position length, const char *s) override
    {
        for (Sci_Position i = 0; i < length; ++i)
            if (stylingPos + i < static_cast<Sci_Position>(styles.size()))
                styles[static_cast<size_t>(stylingPos + i)] = s[i];
        stylingPos += length;
        return true;
    }
    void SCI_METHOD DecorationSetCurrentIndicator(int) override {}
    void SCI_METHOD DecorationFillRange(Sci_Position, int, Sci_Position) override {}
    void SCI_METHOD ChangeLexerState(Sci_Position, Sci_Position) override {}
    int SCI_METHOD CodePage() const override { return 65001; }
    bool SCI_METHOD IsDBCSLeadByte(char) const override { return false; }
    const char *SCI_METHOD BufferPointer() override { return text.data(); }
    int SCI_METHOD GetLineIndentation(Sci_Position) override { return 0; }
    Sci_Position SCI_METHOD LineEnd(Sci_Position line) const override
    {
        if (line < 0)
            return 0;
        if (line >= static_cast<Sci_Position>(lineEnds.size()))
            return static_cast<Sci_Position>(text.size());
        return lineEnds[static_cast<size_t>(line)];
    }
    Sci_Position SCI_METHOD GetRelativePosition(Sci_Position positionStart,
                                                 Sci_Position characterOffset) const override
    {
        return positionStart + characterOffset;
    }
    int SCI_METHOD GetCharacterAndWidth(Sci_Position position, Sci_Position *pWidth) const override
    {
        if (pWidth)
            *pWidth = 1;
        if (position < 0 || position >= static_cast<Sci_Position>(text.size()))
            return 0;
        return static_cast<unsigned char>(text[static_cast<size_t>(position)]);
    }
};

} // namespace pineapple
