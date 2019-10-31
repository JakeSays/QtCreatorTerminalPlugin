/*
    Copyright 2007-2008 by Robert Knight <robertknight@gmail.com>

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

#ifndef FILTER_H
#define FILTER_H

// Qt
#include <QList>
#include <QSet>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QRegularExpression>
#include <QMultiHash>

// terminal
#include "Character.h"

class QAction;

namespace terminal {
class Session;

/**
 * A filter processes blocks of text looking for certain patterns (such as URLs or keywords from a list)
 * and marks the areas which match the filter's patterns as 'hotspots'.
 *
 * Each hotspot has a type identifier associated with it ( such as a link or a highlighted section ),
 * and an action.  When the user performs some activity such as a mouse-click in a hotspot area ( the exact
 * action will depend on what is displaying the block of text which the filter is processing ), the hotspot's
 * activate() method should be called.  Depending on the type of hotspot this will trigger a suitable response.
 *
 * For example, if a hotspot represents a URL then a suitable action would be opening that URL in a web browser.
 * Hotspots may have more than one action, in which case the list of actions can be obtained using the
 * actions() method.
 *
 * Different subclasses of filter will return different types of hotspot.
 * Subclasses must reimplement the process() method to examine a block of text and identify sections of interest.
 * When processing the text they should create instances of Filter::HotSpot subclasses for sections of interest
 * and add them to the filter's list of hotspots using addHotSpot()
 */

enum class SpotType
{
    // the type of the hotspot is not specified
    NotSpecified,
    // this hotspot represents a clickable link
    Link,
    // represents a clickable file link
    FileLink,
    // this hotspot represents a marker
    Marker
};

class Filter
{
public:
    /**
    * Represents an area of text which matched the pattern a particular filter has been looking for.
    *
    * Each hotspot has a type identifier associated with it ( such as a link or a highlighted section ),
    * and an action.  When the user performs some activity such as a mouse-click in a hotspot area ( the exact
    * action will depend on what is displaying the block of text which the filter is processing ), the hotspot's
    * activate() method should be called.  Depending on the type of hotspot this will trigger a suitable response.
    *
    * For example, if a hotspot represents a URL then a suitable action would be opening that URL in a web browser.
    * Hotspots may have more than one action, in which case the list of actions can be obtained using the
    * actions() method.  These actions may then be displayed in a popup menu or toolbar for example.
    */
    class HotSpot
    {
    public:
        /**
         * Constructs a new hotspot which covers the area from (@p startLine,@p startColumn) to (@p endLine,@p endColumn)
         * in a block of text.
         */
        HotSpot(int startLine, int startColumn, int endLine, int endColumn);
        virtual ~HotSpot();

        /** Returns the line when the hotspot area starts */
        int startLine() const;
        /** Returns the line where the hotspot area ends */
        int endLine() const;
        /** Returns the column on startLine() where the hotspot area starts */
        int startColumn() const;
        /** Returns the column on endLine() where the hotspot area ends */
        int endColumn() const;
        /**
         * Returns the type of the hotspot.  This is usually used as a hint for views on how to represent
         * the hotspot graphically.  eg.  Link hotspots are typically underlined when the user mouses over them
         */
        SpotType type() const;

        bool isLink() const { return _type == SpotType::Link || _type == SpotType::FileLink; }

        /**
         * Causes the action associated with a hotspot to be triggered.
         *
         * @param object The object which caused the hotspot to be triggered.  This is
         * typically null ( in which case the default action should be performed ) or
         * one of the objects from the actions() list.  In which case the associated
         * action should be performed.
         */
        virtual void activate(QObject *object = nullptr) = 0;
        /**
         * Returns a list of actions associated with the hotspot which can be used in a
         * menu or toolbar
         */
        virtual QList<QAction *> actions();

    protected:
        /** Sets the type of a hotspot.  This should only be set once */
        void setType(SpotType type);

    private:
        int _startLine;
        int _startColumn;
        int _endLine;
        int _endColumn;
        SpotType _type;
    };

    /** Constructs a new filter. */
    Filter();
    virtual ~Filter();

    /** Causes the filter to process the block of text currently in its internal buffer */
    virtual void process() = 0;

    /**
     * Empties the filters internal buffer and resets the line count back to 0.
     * All hotspots are deleted.
     */
    void reset();

    /** Returns the hotspot which covers the given @p line and @p column, or 0 if no hotspot covers that area */
    HotSpot *hotSpotAt(int line, int column) const;

    /** Returns the list of hotspots identified by the filter */
    QList<HotSpot *> hotSpots() const;

    /** Returns the list of hotspots identified by the filter which occur on a given line */

    /**
     * TODO: Document me
     */
    void setBuffer(const QString *buffer, const QList<int> *linePositions);

protected:
    /** Adds a new hotspot to the list */
    void addHotSpot(HotSpot *);
    /** Returns the internal buffer */
    const QString *buffer();
    /** Converts a character position within buffer() to a line and column */
    void getLineColumn(int position, int &startLine, int &startColumn);

private:
    Q_DISABLE_COPY(Filter)

    QMultiHash<int, HotSpot *> _hotspots;
    QList<HotSpot *> _hotspotList;

    const QList<int> *_linePositions;
    const QString *_buffer;
};

/**
 * A filter which searches for sections of text matching a regular expression and creates a new RegExpFilter::HotSpot
 * instance for them.
 *
 * Subclasses can reimplement newHotSpot() to return custom hotspot types when matches for the regular expression
 * are found.
 */
class RegExpFilter : public Filter
{
public:
    /**
     * Type of hotspot created by RegExpFilter.  The capturedTexts() method can be used to find the text
     * matched by the filter's regular expression.
     */
    class HotSpot : public Filter::HotSpot
    {
    public:
        HotSpot(int startLine, int startColumn, int endLine, int endColumn,
                const QStringList &capturedTexts);
        void activate(QObject *object = nullptr) Q_DECL_OVERRIDE;

        /** Returns the texts found by the filter when matching the filter's regular expression */
        QStringList capturedTexts() const;
    private:
        QStringList _capturedTexts;
    };

    /** Constructs a new regular expression filter */
    RegExpFilter();

    /**
     * Sets the regular expression which the filter searches for in blocks of text.
     *
     * Regular expressions which match the empty string are treated as not matching
     * anything.
     */
    void setRegExp(const QRegularExpression &regExp);
    /** Returns the regular expression which the filter searches for in blocks of text */
    QRegularExpression regExp() const;

    /**
     * Reimplemented to search the filter's text buffer for text matching regExp()
     *
     * If regexp matches the empty string, then process() will return immediately
     * without finding results.
     */
    void process() Q_DECL_OVERRIDE;

protected:
    /**
     * Called when a match for the regular expression is encountered.  Subclasses should reimplement this
     * to return custom hotspot types
     */
    virtual RegExpFilter::HotSpot *newHotSpot(int startLine, int startColumn, int endLine,
                                              int endColumn, const QStringList &capturedTexts);

private:
    QRegularExpression _searchText;
};

class FilterObject;

/** A filter which matches URLs in blocks of text */
class UrlFilter : public RegExpFilter
{
public:
    /**
     * Hotspot type created by UrlFilter instances.  The activate() method opens a web browser
     * at the given URL when called.
     */
    class HotSpot : public RegExpFilter::HotSpot
    {
    public:
        HotSpot(int startLine, int startColumn, int endLine, int endColumn,
                const QStringList &capturedTexts);
        ~HotSpot() Q_DECL_OVERRIDE;

        QList<QAction *> actions() Q_DECL_OVERRIDE;

        /**
         * Open a web browser at the current URL.  The url itself can be determined using
         * the capturedTexts() method.
         */
        void activate(QObject *object = nullptr) Q_DECL_OVERRIDE;

    private:
        enum UrlType {
            StandardUrl,
            Email,
            Unknown
        };
        UrlType urlType() const;

        FilterObject *_urlObject;
    };

    UrlFilter();

protected:
    RegExpFilter::HotSpot *newHotSpot(int, int, int, int, const QStringList &) Q_DECL_OVERRIDE;

private:
    static const QRegularExpression FullUrlRegExp;
    static const QRegularExpression EmailAddressRegExp;

    // combined OR of FullUrlRegExp and EmailAddressRegExp
    static const QRegularExpression CompleteUrlRegExp;
};


class FileDetails;


/**
 * A filter which matches files according to POSIX Portable Filename Character Set
 * http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap03.html#tag_03_267
 */
class FileFilter : public RegExpFilter
{
public:
    /**
     * Hotspot type created by FileFilter instances.
     */
    class HotSpot : public RegExpFilter::HotSpot
    {
    public:
        HotSpot(int startLine, int startColumn, int endLine, int endColumn,
                const QStringList &capturedTexts, const QString &fileName,
            const QString& fullPath,
            int documentLine = -1,
            int documentColumn = -1);
        ~HotSpot() Q_DECL_OVERRIDE;

        QList<QAction *> actions() Q_DECL_OVERRIDE;

        /**
         * Opens kate for editing the file.
         */
        void activate(QObject *object = nullptr) Q_DECL_OVERRIDE;

    private:
        QPointer<FileDetails> _deets;
        QString _fileName;
        QString _fullPath;
    };

    explicit FileFilter(Session *session = nullptr);
    ~FileFilter() override;

    void SetSession(Session* session) { _session = session; }
    void process() Q_DECL_OVERRIDE;

protected:
    RegExpFilter::HotSpot *newHotSpot(int, int, int, int, const QStringList &) Q_DECL_OVERRIDE;

private:
    Session* _session;
    QString _dirPath;
    QSet<QString> _currentFiles;
};


class FilterObject : public QObject
{
    Q_OBJECT
public:
    explicit FilterObject(Filter::HotSpot *filter) : _hotSpot(filter)
    {
    }

public Q_SLOTS:
    void activated();
private:
    Q_DISABLE_COPY(FilterObject)

    Filter::HotSpot *_hotSpot;
};

class FileDetails: public FilterObject
{
    Q_OBJECT

public:
    int DocumentLine;
    int DocumentColumn;
    QString FileName;
    QString FullPath;

    FileDetails(FileFilter::HotSpot* hotSpot, const QString& fileName, const QString& fullPath, int documentLine, int documentColumn)
        : FilterObject(hotSpot),
          DocumentLine(documentLine),
          DocumentColumn(documentColumn),
          FileName(fileName),
          FullPath(fullPath)
    {}
};

/**
 * A chain which allows a group of filters to be processed as one.
 * The chain owns the filters added to it and deletes them when the chain itself is destroyed.
 *
 * Use addFilter() to add a new filter to the chain.
 * When new text to be filtered arrives, use addLine() to add each additional
 * line of text which needs to be processed and then after adding the last line, use
 * process() to cause each filter in the chain to process the text.
 *
 * After processing a block of text, the reset() method can be used to set the filter chain's
 * internal cursor back to the first line.
 *
 * The hotSpotAt() method will return the first hotspot which covers a given position.
 *
 * The hotSpots() method return all of the hotspots in the text and on
 * a given line respectively.
 */
class FilterChain : protected QList<Filter *>
{
public:
    virtual ~FilterChain();

    /** Adds a new filter to the chain.  The chain will delete this filter when it is destroyed */
    void addFilter(Filter *filter);
    /** Removes a filter from the chain.  The chain will no longer delete the filter when destroyed */
    void removeFilter(Filter *filter);
    /** Removes all filters from the chain */
    void clear();

    /** Resets each filter in the chain */
    void reset();
    /**
     * Processes each filter in the chain
     */
    void process();

    /** Sets the buffer for each filter in the chain to process. */
    void setBuffer(const QString *buffer, const QList<int> *linePositions);

    /** Returns the first hotspot which occurs at @p line, @p column or 0 if no hotspot was found */
    Filter::HotSpot *hotSpotAt(int line, int column) const;
    /** Returns a list of all the hotspots in all the chain's filters */
    QList<Filter::HotSpot *> hotSpots() const;
};

/** A filter chain which processes character images from terminal displays */
class TerminalImageFilterChain : public FilterChain
{
public:
    TerminalImageFilterChain();
    ~TerminalImageFilterChain() Q_DECL_OVERRIDE;

    /**
     * Set the current terminal image to @p image.
     *
     * @param image The terminal image
     * @param lines The number of lines in the terminal image
     * @param columns The number of columns in the terminal image
     * @param lineProperties The line properties to set for image
     */
    void setImage(const Character * const image, int lines, int columns,
                  const QVector<LineProperty> &lineProperties);

private:
    Q_DISABLE_COPY(TerminalImageFilterChain)

    QString *_buffer;
    QList<int> *_linePositions;
};
}
#endif //FILTER_H
