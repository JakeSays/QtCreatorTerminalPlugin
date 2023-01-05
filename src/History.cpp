/*
    This file is part of terminal, an X terminal.
    Copyright 1997,1998 by Lars Doelle <lars.doelle@on-line.de>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301  USA.
*/

// Own
#include "History.h"

#include "TerminalDebug.h"
#include "KonsoleSettings.h"

// System
#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <sys/types.h>
#include <unistd.h>

// KDE
#include <QDir>
#include <qplatformdefs.h>
#include <QStandardPaths>
#include <config/kconfiggroup.h>
#include <config/ksharedconfig.h>

// Reasonable line size
static const int LINE_SIZE = 1024;

using namespace terminal;

Q_GLOBAL_STATIC(QString, historyFileLocation)

/*
   An arbitrary long scroll.

   One can modify the scroll only by adding either cells
   or newlines, but access it randomly.

   The model is that of an arbitrary wide typewriter scroll
   in that the scroll is a series of lines and each line is
   a series of cells with no overwriting permitted.

   The implementation provides arbitrary length and numbers
   of cells and line/column indexed read access to the scroll
   at constant costs.
*/

// History File ///////////////////////////////////////////
HistoryFile::HistoryFile() :
    _length(0),
    _fileMap(nullptr),
    _readWriteBalance(0)
{
    // Determine the temp directory once
    // This class is called 3 times for each "unlimited" scrollback.
    // This has the down-side that users must restart to
    // load changes.
    if (!historyFileLocation.exists()) {
        QString fileLocation;
        KSharedConfigPtr appConfig = KSharedConfig::openConfig();
        if (qApp->applicationName() != QLatin1String("konsole")) {
            // Check if "kpart"rc has "FileLocation" group; AFAIK
            // only possible if user manually added it. If not
            // found, use konsole's config.
            if (!appConfig->hasGroup("FileLocation")) {
                appConfig = KSharedConfig::openConfig(QStringLiteral("konsolerc"));
            }
        }

        KConfigGroup configGroup = appConfig->group("FileLocation");
        if (configGroup.readEntry("scrollbackUseCacheLocation", false)) {
            fileLocation = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        } else if (configGroup.readEntry("scrollbackUseSpecifiedLocation", false)) {
            const QUrl specifiedUrl = KonsoleSettings::scrollbackUseSpecifiedLocationDirectory();
            fileLocation = specifiedUrl.path();
        } else {
            fileLocation = QDir::tempPath();
        }
        // Validate file location
        const QFileInfo fi(fileLocation);
        if (fileLocation.isEmpty() || !fi.exists() || !fi.isDir() || !fi.isWritable()) {
            qCWarning(TerminalDebug)<<"Invalid scrollback folder "<<fileLocation<<"; using " << QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
            // Per Qt docs, this path is never empty; not sure if that
            // means it always exists.
            fileLocation = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
            const QFileInfo fi2(fileLocation);
            if (!fi2.exists()) {
                if (!QDir().mkpath(fileLocation)) {
                    qCWarning(TerminalDebug)<<"Unable to create scrollback folder "<<fileLocation;
                }
            }
        }
        *historyFileLocation() = fileLocation;
    }
    const QString tmpDir = *historyFileLocation();
    const QString tmpFormat = tmpDir + QLatin1Char('/') + QLatin1String("konsole-XXXXXX.history");
    _tmpFile.setFileTemplate(tmpFormat);
    if (_tmpFile.open()) {
#if defined(Q_OS_LINUX)
        qCDebug(TerminalDebug, "HistoryFile: /proc/%lld/fd/%d", qApp->applicationPid(), _tmpFile.handle());
#endif
        // On some systems QTemporaryFile creates unnamed file.
        // Do not interfere in such cases.
        if (_tmpFile.exists()) {
            // Remove file entry from filesystem. Since the file
            // is opened, it will still be available for reading
            // and writing. This guarantees the file won't remain
            // in filesystem after process termination, even when
            // there was a crash.
            unlink(QFile::encodeName(_tmpFile.fileName()).constData());
        }
    }
}

HistoryFile::~HistoryFile()
{
    if (_fileMap != nullptr) {
        unmap();
    }
}

//TODO:  Mapping the entire file in will cause problems if the history file becomes exceedingly large,
//(ie. larger than available memory).  HistoryFile::map() should only map in sections of the file at a time,
//to avoid this.
void HistoryFile::map()
{
    Q_ASSERT(_fileMap == nullptr);

    if (_tmpFile.flush()) {
        Q_ASSERT(_tmpFile.size() >= _length);
        _fileMap = _tmpFile.map(0, _length);
    }

    //if mmap'ing fails, fall back to the read-lseek combination
    if (_fileMap == nullptr) {
        _readWriteBalance = 0;
        qCDebug(TerminalDebug) << "mmap'ing history failed.  errno = " << errno;
    }
}

void HistoryFile::unmap()
{
    Q_ASSERT(_fileMap != nullptr);

    if (_tmpFile.unmap(_fileMap)) {
        _fileMap = nullptr;
    }

    Q_ASSERT(_fileMap == nullptr);

}

void HistoryFile::add(const char *buffer, qint64 count)
{
    if (_fileMap != nullptr) {
        unmap();
    }

    if (_readWriteBalance < INT_MAX) {
        _readWriteBalance++;
    }

    qint64 rc = 0;

    if (!_tmpFile.seek(_length)) {
        perror("HistoryFile::add.seek");
        return;
    }
    rc = _tmpFile.write(buffer, count);
    if (rc < 0) {
        perror("HistoryFile::add.write");
        return;
    }
    _length += rc;
}

void HistoryFile::get(char *buffer, qint64 size, qint64 loc)
{

    if (loc < 0 || size < 0 || loc + size > _length) {
        fprintf(stderr, "getHist(...,%lld,%lld): invalid args.\n", size, loc);
        return;
    }

    //count number of get() calls vs. number of add() calls.
    //If there are many more get() calls compared with add()
    //calls (decided by using MAP_THRESHOLD) then mmap the log
    //file to improve performance.
    if (_readWriteBalance > INT_MIN) {
        _readWriteBalance--;
    }
    if ((_fileMap == nullptr) && _readWriteBalance < MAP_THRESHOLD) {
        map();
    }

    if (_fileMap != nullptr) {
        memcpy(buffer, _fileMap + loc, size);
    } else {
        qint64 rc = 0;

        if (!_tmpFile.seek(loc)) {
            perror("HistoryFile::get.seek");
            return;
        }
        rc = _tmpFile.read(buffer, size);
        if (rc < 0) {
            perror("HistoryFile::get.read");
            return;
        }
    }
}

qint64 HistoryFile::len() const
{
    return _length;
}

// History Scroll abstract base class //////////////////////////////////////

HistoryScroll::HistoryScroll(HistoryType *t) :
    _historyType(t)
{
}

HistoryScroll::~HistoryScroll()
{
    delete _historyType;
}

bool HistoryScroll::hasScroll()
{
    return true;
}

// History Scroll File //////////////////////////////////////

/*
   The history scroll makes a Row(Row(Cell)) from
   two history buffers. The index buffer contains
   start of line positions which refer to the cells
   buffer.

   Note that index[0] addresses the second line
   (line #1), while the first line (line #0) starts
   at 0 in cells.
*/

HistoryScrollFile::HistoryScrollFile() :
    HistoryScroll(new HistoryTypeFile())
{
}

HistoryScrollFile::~HistoryScrollFile() = default;

int HistoryScrollFile::getLines()
{
    return _index.len() / sizeof(qint64);
}

int HistoryScrollFile::getLineLen(int lineno)
{
    return (startOfLine(lineno + 1) - startOfLine(lineno)) / sizeof(Character);
}

bool HistoryScrollFile::isWrappedLine(int lineno)
{
    if (lineno >= 0 && lineno <= getLines()) {
        unsigned char flag = 0;
        _lineflags.get(reinterpret_cast<char *>(&flag), sizeof(unsigned char),
                       (lineno)*sizeof(unsigned char));
        return flag != 0u;
    }
    return false;
}

qint64 HistoryScrollFile::startOfLine(int lineno)
{
    if (lineno <= 0) {
        return 0;
    }
    if (lineno <= getLines()) {
        qint64 res = 0;
        _index.get(reinterpret_cast<char*>(&res), sizeof(qint64), (lineno - 1)*sizeof(qint64));
        return res;
    }
    return _cells.len();
}

void HistoryScrollFile::getCells(int lineno, int colno, int count, Character res[])
{
    _cells.get(reinterpret_cast<char*>(res), count * sizeof(Character), startOfLine(lineno) + colno * sizeof(Character));
}

void HistoryScrollFile::addCells(const Character text[], int count)
{
    _cells.add(reinterpret_cast<const char*>(text), count * sizeof(Character));
}

void HistoryScrollFile::addLine(bool previousWrapped)
{
    qint64 locn = _cells.len();
    _index.add(reinterpret_cast<char *>(&locn), sizeof(qint64));
    unsigned char flags = previousWrapped ? 0x01 : 0x00;
    _lineflags.add(reinterpret_cast<char *>(&flags), sizeof(char));
}

// History Scroll None //////////////////////////////////////

HistoryScrollNone::HistoryScrollNone() :
    HistoryScroll(new HistoryTypeNone())
{
}

HistoryScrollNone::~HistoryScrollNone() = default;

bool HistoryScrollNone::hasScroll()
{
    return false;
}

int HistoryScrollNone::getLines()
{
    return 0;
}

int HistoryScrollNone::getLineLen(int)
{
    return 0;
}

bool HistoryScrollNone::isWrappedLine(int /*lineno*/)
{
    return false;
}

void HistoryScrollNone::getCells(int, int, int, Character [])
{
}

void HistoryScrollNone::addCells(const Character [], int)
{
}

void HistoryScrollNone::addLine(bool)
{
}

////////////////////////////////////////////////////////////////
// Compact History Scroll //////////////////////////////////////
////////////////////////////////////////////////////////////////
void *CompactHistoryBlock::allocate(size_t size)
{
    Q_ASSERT(size > 0);
    if (_tail - _blockStart + size > _blockLength) {
        return nullptr;
    }

    void *block = _tail;
    _tail += size;
    ////qDebug() << "allocated " << length << " bytes at address " << block;
    _allocCount++;
    return block;
}

void CompactHistoryBlock::deallocate()
{
    _allocCount--;
    Q_ASSERT(_allocCount >= 0);
}

void *CompactHistoryBlockList::allocate(size_t size)
{
    CompactHistoryBlock *block;
    if (list.isEmpty() || list.last()->remaining() < size) {
        block = new CompactHistoryBlock();
        list.append(block);
        ////qDebug() << "new block created, remaining " << block->remaining() << "number of blocks=" << list.size();
    } else {
        block = list.last();
        ////qDebug() << "old block used, remaining " << block->remaining();
    }
    return block->allocate(size);
}

void CompactHistoryBlockList::deallocate(void *ptr)
{
    Q_ASSERT(!list.isEmpty());

    int i = 0;
    CompactHistoryBlock *block = list.at(i);
    while (i < list.size() && !block->contains(ptr)) {
        i++;
        block = list.at(i);
    }

    Q_ASSERT(i < list.size());

    block->deallocate();

    if (!block->isInUse()) {
        list.removeAt(i);
        delete block;
        ////qDebug() << "block deleted, new size = " << list.size();
    }
}

CompactHistoryBlockList::~CompactHistoryBlockList()
{
    qDeleteAll(list.begin(), list.end());
    list.clear();
}

void *CompactHistoryLine::operator new(size_t size, CompactHistoryBlockList &blockList)
{
    return blockList.allocate(size);
}

CompactHistoryLine::CompactHistoryLine(const TextLine &line, CompactHistoryBlockList &bList) :
    _blockListRef(bList),
    _formatArray(nullptr),
    _text(nullptr),
    _formatLength(0),
    _wrapped(false)
{
    _length = line.size();

    if (!line.isEmpty()) {
        _formatLength = 1;
        int k = 1;

        // count number of different formats in this text line
        Character c = line[0];
        while (k < _length) {
            if (!(line[k].equalsFormat(c))) {
                _formatLength++; // format change detected
                c = line[k];
            }
            k++;
        }

        ////qDebug() << "number of different formats in string: " << _formatLength;
        _formatArray = static_cast<CharacterFormat *>(_blockListRef.allocate(sizeof(CharacterFormat) * _formatLength));
        Q_ASSERT(_formatArray != nullptr);
        _text = static_cast<uint *>(_blockListRef.allocate(sizeof(uint) * line.size()));
        Q_ASSERT(_text != nullptr);

        _length = line.size();
        _wrapped = false;

        // record formats and their positions in the format array
        c = line[0];
        _formatArray[0].setFormat(c);
        _formatArray[0].startPos = 0;                      // there's always at least 1 format (for the entire line, unless a change happens)

        k = 1;                                            // look for possible format changes
        int j = 1;
        while (k < _length && j < _formatLength) {
            if (!(line[k].equalsFormat(c))) {
                c = line[k];
                _formatArray[j].setFormat(c);
                _formatArray[j].startPos = k;
                ////qDebug() << "format entry " << j << " at pos " << _formatArray[j].startPos << " " << &(_formatArray[j].startPos) ;
                j++;
            }
            k++;
        }

        // copy character values
        for (int i = 0; i < line.size(); i++) {
            _text[i] = line[i].character;
            ////qDebug() << "char " << i << " at mem " << &(text[i]);
        }
    }
    ////qDebug() << "line created, length " << length << " at " << &(length);
}

CompactHistoryLine::~CompactHistoryLine()
{
    if (_length > 0) {
        _blockListRef.deallocate(_text);
        _blockListRef.deallocate(_formatArray);
    }
    _blockListRef.deallocate(this);
}

void CompactHistoryLine::getCharacter(int index, Character &r)
{
    Q_ASSERT(index < _length);
    int formatPos = 0;
    while ((formatPos + 1) < _formatLength && index >= _formatArray[formatPos + 1].startPos) {
        formatPos++;
    }

    r.character = _text[index];
    r.rendition = _formatArray[formatPos].rendition;
    r.foregroundColor = _formatArray[formatPos].fgColor;
    r.backgroundColor = _formatArray[formatPos].bgColor;
    r.isRealCharacter = _formatArray[formatPos].isRealCharacter;
}

void CompactHistoryLine::getCharacters(Character *array, int size, int startColumn)
{
    Q_ASSERT(startColumn >= 0 && size >= 0);
    Q_ASSERT(startColumn + size <= static_cast<int>(getLength()));

    for (int i = startColumn; i < size + startColumn; i++) {
        getCharacter(i, array[i - startColumn]);
    }
}

CompactHistoryScroll::CompactHistoryScroll(unsigned int maxLineCount) :
    HistoryScroll(new CompactHistoryType(maxLineCount)),
    _lines(),
    _blockList()
{
    ////qDebug() << "scroll of length " << maxLineCount << " created";
    setMaxNbLines(maxLineCount);
}

CompactHistoryScroll::~CompactHistoryScroll()
{
    qDeleteAll(_lines.begin(), _lines.end());
    _lines.clear();
}

void CompactHistoryScroll::addCellsVector(const TextLine &cells)
{
    CompactHistoryLine *line;
    line = new(_blockList) CompactHistoryLine(cells, _blockList);

    if (_lines.size() > static_cast<int>(_maxLineCount)) {
        delete _lines.takeAt(0);
    }
    _lines.append(line);
}

void CompactHistoryScroll::addCells(const Character a[], int count)
{
    TextLine newLine(count);
    std::copy(a, a + count, newLine.begin());
    addCellsVector(newLine);
}

void CompactHistoryScroll::addLine(bool previousWrapped)
{
    CompactHistoryLine *line = _lines.last();
    ////qDebug() << "last line at address " << line;
    line->setWrapped(previousWrapped);
}

int CompactHistoryScroll::getLines()
{
    return _lines.size();
}

int CompactHistoryScroll::getLineLen(int lineNumber)
{
    if ((lineNumber < 0) || (lineNumber >= _lines.size())) {
        //qDebug() << "requested line invalid: 0 < " << lineNumber << " < " <<_lines.size();
        //Q_ASSERT(lineNumber >= 0 && lineNumber < _lines.size());
        return 0;
    }
    CompactHistoryLine *line = _lines[lineNumber];
    ////qDebug() << "request for line at address " << line;
    return line->getLength();
}

void CompactHistoryScroll::getCells(int lineNumber, int startColumn, int count, Character buffer[])
{
    if (count == 0) {
        return;
    }
    Q_ASSERT(lineNumber < _lines.size());
    CompactHistoryLine *line = _lines[lineNumber];
    Q_ASSERT(startColumn >= 0);
    Q_ASSERT(static_cast<unsigned int>(startColumn) <= line->getLength() - count);
    line->getCharacters(buffer, count, startColumn);
}

void CompactHistoryScroll::setMaxNbLines(unsigned int lineCount)
{
    _maxLineCount = lineCount;

    while (_lines.size() > static_cast<int>(lineCount)) {
        delete _lines.takeAt(0);
    }
    ////qDebug() << "set max lines to: " << _maxLineCount;
}

bool CompactHistoryScroll::isWrappedLine(int lineNumber)
{
    Q_ASSERT(lineNumber < _lines.size());
    return _lines[lineNumber]->isWrapped();
}

//////////////////////////////////////////////////////////////////////
// History Types
//////////////////////////////////////////////////////////////////////

HistoryType::HistoryType() = default;
HistoryType::~HistoryType() = default;

//////////////////////////////

HistoryTypeNone::HistoryTypeNone() = default;

bool HistoryTypeNone::isEnabled() const
{
    return false;
}

HistoryScroll *HistoryTypeNone::scroll(HistoryScroll *old) const
{
    delete old;
    return new HistoryScrollNone();
}

int HistoryTypeNone::maximumLineCount() const
{
    return 0;
}

//////////////////////////////

HistoryTypeFile::HistoryTypeFile() = default;

bool HistoryTypeFile::isEnabled() const
{
    return true;
}

HistoryScroll *HistoryTypeFile::scroll(HistoryScroll *old) const
{
    if (dynamic_cast<HistoryFile *>(old) != nullptr) {
        return old; // Unchanged.
    }
    HistoryScroll *newScroll = new HistoryScrollFile();

    Character line[LINE_SIZE];
    int lines = (old != nullptr) ? old->getLines() : 0;
    for (int i = 0; i < lines; i++) {
        int size = old->getLineLen(i);
        if (size > LINE_SIZE) {
            auto tmp_line = new Character[size];
            old->getCells(i, 0, size, tmp_line);
            newScroll->addCells(tmp_line, size);
            newScroll->addLine(old->isWrappedLine(i));
            delete [] tmp_line;
        } else {
            old->getCells(i, 0, size, line);
            newScroll->addCells(line, size);
            newScroll->addLine(old->isWrappedLine(i));
        }
    }

    delete old;
    return newScroll;
}

int HistoryTypeFile::maximumLineCount() const
{
    return -1;
}

//////////////////////////////

CompactHistoryType::CompactHistoryType(unsigned int nbLines) :
    _maxLines(nbLines)
{
}

bool CompactHistoryType::isEnabled() const
{
    return true;
}

int CompactHistoryType::maximumLineCount() const
{
    return _maxLines;
}

HistoryScroll *CompactHistoryType::scroll(HistoryScroll *old) const
{
    if (old != nullptr) {
        auto *oldBuffer = dynamic_cast<CompactHistoryScroll *>(old);
        if (oldBuffer != nullptr) {
            oldBuffer->setMaxNbLines(_maxLines);
            return oldBuffer;
        }
        delete old;
    }
    return new CompactHistoryScroll(_maxLines);
}
