#pragma once

#include <QString>
#include <QRect>
#include <QByteArray>

#include "Character.h"

namespace cxxtools
{
class Screen;

class TextBuffer
{
public:
    static constexpr int kBufferSize = 8;

    char Text[kBufferSize] = {};
    int Length = 0;

    void Clear()
    {
        Text[0] = '\0';
        Length = 0;
    }

    void SetText(Character& chr);

    static int ToUtf8(wchar_t g, char* txt);

    operator bool() const { return Length != 0; }

    char First() { return Text[0]; }

    void InsertAt(QByteArray& text, int index) const
    {
        if(Length == 0)
        {
            return;
        }

        text.insert(index, Text, Length);
    }

    void AppendTo(QByteArray& text) const
    {
        if(Length == 0)
        {
            return;
        }

        text.append(text, Length);
    }
};


class StringBuilder
{
    char* _buffer = nullptr;
    char* _bufferPtr = nullptr;
    int _gap = 0;
    int _length = 0;
    int _capacity = 0;

    static constexpr int kInitialReservation = 256;

public:
    StringBuilder()
        : _buffer((char*) malloc(kInitialReservation)),
          _bufferPtr(_buffer),
          _capacity(kInitialReservation)

    {
    }

    ~StringBuilder();

    bool Append(const TextBuffer& text);
    bool Prepend(const TextBuffer& text);

    void RightTrim();
    void SkipLeft(int amount);
    void SkipRight(int amount);

    bool Empty() { return _length == 0; }
    bool NotEmpty() { return _length > 0; }

    const char* Position() const { return _bufferPtr; }
    const char* Buffer() const { return _buffer; }

    bool AtSpace() const { return std::isspace(*_bufferPtr); }
    bool AtProtocol();
    bool AtUrl();
    bool AtFile();

    QString AsAbsolutePath();
    QString AsString() { return QString(QLatin1String(_bufferPtr)); }

private:
    inline bool StartsWith(const char* prefix) const;
};

class LinkParser
{
    Screen* _screen;

public:
    LinkParser(Screen* screen);

    QString FindLink(int cx, int cy, QRect& region);

private:


    bool GetTextAt(int& x, int& y, TextBuffer& text);
    bool GetPreviousTextAt(int& x, int& y, TextBuffer& text);
    bool GetNextTextAt(int& x, int& y, TextBuffer& text);
};

} //namespace cxxtools
