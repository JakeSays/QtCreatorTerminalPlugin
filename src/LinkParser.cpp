#include "LinkParser.h"
#include "Screen.h"
#include <QDir>
#include <filesystem>
#include "konsole_wcwidth.h"
#include <algorithm>

namespace fs = std::filesystem;

namespace cxxtools
{
LinkParser::LinkParser(Screen* screen) : _screen(screen)
{}

bool LinkParser::GetTextAt(int& x, int& y, TextBuffer& text)
{
    text.Clear();

    auto cells = _screen->GetLineAt(y);
    auto lineWidth = cells.count();

    if(cells.empty() || lineWidth == 0)
    {
        return false;
    }

    if(x >= lineWidth)
    {
        //empty
        return true;
    }

    auto cell = cells[x];

    if(cell.character == 0 && konsole_wcwidth(cell.character) > 1)
    {
        x--;
        if(x < 0)
        {
            return false;
        }
        cell = cells[x];
    }

    if(cell.character == 0 || cell.linkId > 0)
    {
        return false;
    }

    text.SetText(cell);

    return true;
}

bool LinkParser::GetPreviousTextAt(int& x, int& y, TextBuffer &text)
{
    QVector<Character> cells;
    Character cell;

    text.Clear();

    x--;
    if(x < 0)
    {
        y--;
        x = _screen->columnCount() - 1;
        cells = _screen->GetLineAt(y);
        auto lineWidth = cells.count();
        if(cells.isEmpty() || lineWidth == 0)
        {
            return false;
        }
        if(x >= lineWidth)
        {
            //empty
            return true;
        }

        cell = cells[x];
        /* Either the cell is in the normal screen and needs to have
         * autowrapped flag or is in the backlog and its length is larger than
         * the screen, spanning multiple lines */

        //        if(((!cell.wrapped) && y >= 0) || (w < ty->w))

        if((!cell.wrapped && y >= 0) || lineWidth < _screen->columnCount())
        {
            //empty
            return true;
        }
    }
    else
    {
        cells = _screen->GetLineAt(y);
        auto lineWidth = cells.count();

        if(cells.empty() || lineWidth == 0)
        {
            //bad
            return false;
        }

        if(x >= lineWidth)
        {
            //empty
            return true;
        }
        //        if(y >= w)
        //            goto empty;

        cell = cells[x];
    }

    if(cell.character == 0 && konsole_wcwidth(cell.character) > 1)
    {
        x--;
        if(x < 0)
        {
            return false;
        }
        cell = cells[x];
    }

    if(cell.character == 0 || cell.linkId > 0)
    {
        //empty
        return true;
    }

    text.SetText(cell);

    return true;
}

bool LinkParser::GetNextTextAt(int& x, int& y, TextBuffer &text)
{
    Character cell;

    text.Clear();

    x++;
    auto cells = _screen->GetLineAt(y);
    auto lineWidth = cells.count();

    if(cells.isEmpty() || lineWidth == 0)
    {
        return false;
    }

    if(x >= lineWidth)
    {
        y++;
        if(x <= lineWidth)
        {
            cell = cells[lineWidth - 1];
            if(!cell.wrapped)
            {
                //empty
                return true;
            }
        }

        x = 0;
        cells = _screen->GetLineAt(y);
        lineWidth = cells.count();

        if(cells.isEmpty() || lineWidth == 0)
        {
            return false;
        }
    }

    cell = cells[x];
    if(cell.character == 0 && konsole_wcwidth(cell.character))
    {
        x++;
        if(x >= lineWidth)
        {
            cell = cells[lineWidth - 1];
            if(!cell.wrapped && lineWidth == _screen->columnCount())
            {
                //empty
                return true;
            }
            y++;
            x = 0;
            cells = _screen->GetLineAt(y);
            lineWidth = cells.count();
            if(cells.isEmpty() || lineWidth == 0)
            {
                return false;
            }
        }
        return false;
    }

    cell = cells[x];
    if(cell.character == 0 || cell.linkId > 0)
    {
        //empty
        return true;
    }

    text.SetText(cell);

    return true;
}

QString LinkParser::FindLink(int cx, int cy, QRect& region)
{
    region = {0, 0, 0, 0};

    auto x1 = 0;
    auto x2 = 0;
    auto y1 = 0;
    auto y2 = 0;

    auto goback = true;
    auto goforward = false;
    auto escaped = false;

    auto was_protocol = false;

    x1 = x2 = cx;
    y1 = y2 = cy;

    auto screenWidth = _screen->columnCount();
    auto screenHeight = _screen->lineCount();

    if(screenWidth <= 0 || screenHeight <= 0)
    {
        return QString();
    }

    auto scrolledLines = _screen->scrolledLines();

    y1 -= scrolledLines;
    y2 -= scrolledLines;

    TextBuffer text;
    if(!GetTextAt(x1, y1, text) || !text)
    {
        return QString();
    }

    StringBuilder linkBuilder;

    linkBuilder.Append(text);

    char endmatch = 0;

    while(goback)
    {
        int new_x1 = x1;
        int new_y1 = y1;

        if (!GetPreviousTextAt(new_x1, new_y1, text) || !text)
        {
            goback = false;
            goforward = true;
            break;
        }

        linkBuilder.Prepend(text);

        if(linkBuilder.AtSpace())
        {
            int old_txtlen = text.Length;
            if (!GetPreviousTextAt(new_x1, new_y1, text) || !text)
            {
                goback = false;
                goforward = true;
                break;
            }

            if(text.First() != '\\')
            {
                linkBuilder.SkipLeft(old_txtlen);
                goback = false;
                goforward = true;
                break;
            }
        }

        switch(*linkBuilder.Position())
        {
            case '"': endmatch = '"'; break;
            case '\'': endmatch = '\''; break;
            case '`': endmatch = '`'; break;
            case '<': endmatch = '>'; break;
            case '[': endmatch = ']'; break;
            case '{': endmatch = '}'; break;
            case '(': endmatch = ')'; break;
            case '|': endmatch = '|'; break;
        }

        if(endmatch != 0)
        {
            linkBuilder.SkipLeft(text.Length);
            goback = false;
            goforward = true;
            break;
        }

        if(!linkBuilder.AtProtocol())
        {
            if(was_protocol)
            {
                if(!linkBuilder.AtSpace())
                {
                    endmatch = *linkBuilder.Position();
                }
                linkBuilder.SkipLeft(text.Length);
                goback = false;
                goforward = true;
                break;
            }
        }
        else
        {
            was_protocol = true;
        }

        x1 = new_x1;
        y1 = new_y1;
    }

    while(goforward)
    {
        int new_x2 = x2;
        int new_y2 = y2;

        /* Check if the previous char is a delimiter */

        if (GetNextTextAt(new_x2, new_y2, text) && !text)
        {
            goforward = false;
            break;
        }

        /* escaping */
        if(text.First() == '\\')
        {
            x2 = new_x2;
            y2 = new_y2;
            escaped = true;
            continue;
        }

        if(escaped)
        {
            x2 = new_x2;
            y2 = new_y2;
            escaped = false;
        }

        if(std::isspace(text.First()) || text.First() == endmatch)
        {
            goforward = false;
            break;
        }

        switch(text.First())
        {
            case '"':
            case '\'':
            case '`':
            case '<':
            case '>':
            case '[':
            case ']':
            case '{':
            case '}':
            case '|': break;
        }

        if(!linkBuilder.Append(text))
        {
            break;
        }

        if(!linkBuilder.AtProtocol())
        {
            if(was_protocol)
            {
                linkBuilder.SkipRight(text.Length);
                goback = false;
            }
        }
        else
        {
            was_protocol = true;
        }

        x2 = new_x2;
        y2 = new_y2;
    }

    if(linkBuilder.NotEmpty())
    {
        auto atFile = linkBuilder.AtFile();

        if(atFile || linkBuilder.AtUrl())
        {
            region.setCoords(x1, y1, x2 + scrolledLines, y2 + scrolledLines);

            if (atFile && *linkBuilder.Position() != '/')
            {
                auto localPath = linkBuilder.AsAbsolutePath();
                return localPath;
            }

            return linkBuilder.AsString();
        }
    }

    return QString();
}

void TextBuffer::SetText(Character &chr)
{
    Length = ToUtf8(chr.character, Text);
}

int TextBuffer::ToUtf8(wchar_t g, char* txt)
{
    if(g < (1 << (7)))
    {
        // 0xxxxxxx
        txt[0] = g & 0x7f;
        txt[1] = 0;
        return 1;
    }
    else if(g < (1 << (5 + 6)))
    {
        // 110xxxxx 10xxxxxx
        txt[0] = 0xc0 | ((g >> 6) & 0x1f);
        txt[1] = 0x80 | ((g) &0x3f);
        txt[2] = 0;
        return 2;
    }
    else if(g < (1 << (4 + 6 + 6)))
    {
        // 1110xxxx 10xxxxxx 10xxxxxx
        txt[0] = 0xe0 | ((g >> 12) & 0x0f);
        txt[1] = 0x80 | ((g >> 6) & 0x3f);
        txt[2] = 0x80 | ((g) &0x3f);
        txt[3] = 0;
        return 3;
    }
    else if(g < (1 << (3 + 6 + 6 + 6)))
    {
        // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
        txt[0] = 0xf0 | ((g >> 18) & 0x07);
        txt[1] = 0x80 | ((g >> 12) & 0x3f);
        txt[2] = 0x80 | ((g >> 6) & 0x3f);
        txt[3] = 0x80 | ((g) &0x3f);
        txt[4] = 0;
        return 4;
    }

    // error - cant encode this in utf8
    txt[0] = 0;
    return 0;
}

StringBuilder::~StringBuilder()
{
    if (_buffer != nullptr)
    {
        free(_buffer);
    }
}

bool StringBuilder::Append(const TextBuffer &text)
{
    if(!text)
    {
        return true;
    }

    auto newLength = _length + text.Length;

    if ((newLength + _gap >= _capacity) || _buffer == nullptr)
    {
        auto newCapacity = ((newLength + _gap + 15) / 16) * 24;

        auto newBuffer = realloc(_buffer, newCapacity);
        if (newBuffer == nullptr)
        {
            return false;
        }

        _buffer = (char*) newBuffer;
        _bufferPtr = _buffer + _gap;
        _capacity = newCapacity;
    }

    memcpy(_bufferPtr + text.Length, text.Text, text.Length);
    _length = newLength;
    _bufferPtr[_length] = '\0';

    return true;
}

bool StringBuilder::Prepend(const TextBuffer &text)
{
    if (!text)
    {
        return true;
    }

    if (text.Length >= _gap)
    {
        auto alignedGap = ((text.Length + 15) / 16) * 24;
        auto oneThirdCapacity = (((_capacity / 3) + 15) / 16) * 16;
        auto newGap = std::max(alignedGap, oneThirdCapacity);
        auto newCapacity = _capacity + newGap;

        auto newBuffer = (char*) calloc(newCapacity, 1);
        if (newBuffer == nullptr)
        {
            return false;
        }

        memcpy(newBuffer + newGap, _bufferPtr, _length);
        _buffer = newBuffer;
        _bufferPtr = _buffer + newGap;
        _gap = newGap;
        _capacity = newCapacity;
    }

    _bufferPtr -= text.Length;
    _gap -= text.Length;
    _length += text.Length;
    memcpy(_bufferPtr, text.Text, text.Length);

    return true;
}

void StringBuilder::RightTrim()
{
    if (_buffer == nullptr)
    {
        return;
    }

    while (_length > 0)
    {
        auto c = _bufferPtr[_length - 1];

        if (c != ' ' && c != '\t' && c != '\f')
        {
            break;
        }

        _length -= 1;
    }

    _bufferPtr[_length] = '\0';
}

void StringBuilder::SkipLeft(int amount)
{
    _length -= amount;
    _gap += amount;
    _bufferPtr += amount;
}

void StringBuilder::SkipRight(int amount)
{
    _length -= amount;
    _bufferPtr[_length] = '\0';
}

static inline bool isValidChar(int c)
{
    return std::isalpha(c) ||
        c == '.' ||
        c == '-' ||
        c == '+';
}

bool StringBuilder::AtProtocol()
{
    if (!std::isalpha(*_bufferPtr))
    {
        return false;
    }

    auto indexPtr = _bufferPtr + 1;
    auto end = _buffer + _capacity;

    int c;
    do
    {
        c = *(indexPtr++);
    } while(indexPtr < end && isValidChar(c));

    if (indexPtr >= end)
    {
        return false;
    }

    return *indexPtr++ == ':' &&
        *indexPtr++ == '/' &&
        *indexPtr++ == '/';
}

inline bool StringBuilder::StartsWith(const char* prefix) const
{
    auto ptr = _bufferPtr;

    if (*prefix++ == std::tolower(*ptr++) &&
        *prefix++ == std::tolower(*ptr++) &&
        *prefix++ == std::tolower(*ptr++) &&
        *ptr == '.')
    {
        return true;
    }

    return false;
}

bool StringBuilder::AtUrl()
{
    if (AtProtocol())
    {
        return true;
    }

    return StartsWith("www") || StartsWith("ftp");
}

bool StringBuilder::AtFile()
{
    switch(*_bufferPtr)
    {
        case '/': return true;
        case '~':
            if(*(_bufferPtr + 1) == '/')
            {
                return true;
            }
            return false;
        case '.':
            if(*(_bufferPtr + 1) == '/')
            {
                return true;
            }
            else if (*(_bufferPtr + 1) == '.' && *(_bufferPtr + 2) == '/')
            {
                return true;
            }

            return false;
        default: return false;
    }
}

QString StringBuilder::AsAbsolutePath()
{
    return QDir::homePath() + "/" + _bufferPtr;
}

//static QString _home_path_get(QString relpath)
//{
//    return QDir::homePath() + "/" + relpath;
//}

//static QString _local_path_get(QString relpath)
//{
//    if(relpath.startsWith('/'))
//    {
//        return relpath;
//    }

//    if(relpath.startsWith("~/"))
//    {
//        return QDir::homePath() + relpath.mid(1);
//    }

//    return QDir::currentPath() + "/" + relpath;
//}

} //namespace cxxtools
