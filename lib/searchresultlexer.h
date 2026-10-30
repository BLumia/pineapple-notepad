#pragma once

#include <ILexer.h>

#include <vector>

namespace pineapple {

// Style byte values. When updating this, also update searchResultStyles
// in basestyles.cpp
enum SearchResultStyle {
    SearchResultStyleDefault = 0,
    SearchResultStyleSearchHeader = 1,
    SearchResultStyleFileHeader = 2,
    SearchResultStyleLineNumber = 3,
    SearchResultStyleWordToSearch = 4,
};

// A highlighted match range on one result line. Offsets are UTF-8 bytes,
// relative to the start of the buffer line, as [start, end).
struct SearchResultMarkingSegment {
    int start = 0;
    int end = 0;
};

// Per-line marking data; index N describes buffer line N. One entry must
// exist for every buffer line (header lines carry empty entries).
struct SearchResultMarkingLine {
    std::vector<SearchResultMarkingSegment> segments;
};

// Line classification by first character, exactly like Notepad++'s
// approach: search header lines have no prefix, file header
// lines start with a space, result lines start with a tab.
class SearchResultLexer final : public Scintilla::ILexer5 {
public:
    // The pointed-to container must outlive this lexer or be reset with
    // another setMarkings() call; Lex() only dereferences it synchronously
    // on the GUI thread.
    void setMarkings(const std::vector<SearchResultMarkingLine> *markings);

    int SCI_METHOD Version() const override;
    void SCI_METHOD Release() override;
    const char *SCI_METHOD PropertyNames() override;
    int SCI_METHOD PropertyType(const char *name) override;
    const char *SCI_METHOD DescribeProperty(const char *name) override;
    Sci_Position SCI_METHOD PropertySet(const char *key, const char *val) override;
    const char *SCI_METHOD DescribeWordListSets() override;
    Sci_Position SCI_METHOD WordListSet(int n, const char *wl) override;
    void SCI_METHOD Lex(Sci_PositionU startPos, Sci_Position lengthDoc, int initStyle,
                        Scintilla::IDocument *pAccess) override;
    void SCI_METHOD Fold(Sci_PositionU startPos, Sci_Position lengthDoc, int initStyle,
                         Scintilla::IDocument *pAccess) override;
    void *SCI_METHOD PrivateCall(int operation, void *pointer) override;
    int SCI_METHOD LineEndTypesSupported() override;
    int SCI_METHOD AllocateSubStyles(int styleBase, int numberStyles) override;
    int SCI_METHOD SubStylesStart(int styleBase) override;
    int SCI_METHOD SubStylesLength(int styleBase) override;
    int SCI_METHOD StyleFromSubStyle(int subStyle) override;
    int SCI_METHOD PrimaryStyleFromStyle(int style) override;
    void SCI_METHOD FreeSubStyles() override;
    void SCI_METHOD SetIdentifiers(int style, const char *identifiers) override;
    int SCI_METHOD DistanceToSecondaryStyles() override;
    const char *SCI_METHOD GetSubStyleBases() override;
    int SCI_METHOD NamedStyles() override;
    const char *SCI_METHOD NameOfStyle(int style) override;
    const char *SCI_METHOD TagsOfStyle(int style) override;
    const char *SCI_METHOD DescriptionOfStyle(int style) override;
    const char *SCI_METHOD GetName() override;
    int SCI_METHOD GetIdentifier() override;
    const char *SCI_METHOD PropertyGet(const char *key) override;

private:
    int refCount_ = 1;
    const std::vector<SearchResultMarkingLine> *markings_ = nullptr;
};

// The caller transfers ownership to Scintilla via SCI_SETILEXER (that is,
// SciEdit::setLexer()); Scintilla releases the instance when it is replaced
// or the document is destroyed.
SearchResultLexer *createSearchResultLexer();

} // namespace pineapple
