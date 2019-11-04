/*
    This source file is part of terminal, a terminal emulator.

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

#ifndef COLORSCHEME_H
#define COLORSCHEME_H

// Qt
#include <QMetaType>
#include <QSharedData>

// terminal
#include "CharacterColor.h"

class KConfig;
class QPixmap;
class QPainter;

namespace terminal {
/**
 * This class holds the wallpaper pixmap associated with a color scheme.
 * The wallpaper object is shared between multiple TerminalDisplay.
 */
class ColorSchemeWallpaper : public QSharedData
{
public:
    typedef QExplicitlySharedDataPointer<ColorSchemeWallpaper> Ptr;

    explicit ColorSchemeWallpaper(const QString &path);
    ~ColorSchemeWallpaper();

    void load();

    /** Returns true if wallpaper available and drawn */
    bool draw(QPainter &painter, const QRect rect, qreal opacity = 1.0);

    bool isNull() const;

    QString path() const;

private:
    Q_DISABLE_COPY(ColorSchemeWallpaper)

    QString _path;
    QPixmap *_picture;
};

enum class ColorSchemeSource
{
    Unknown,
    FileSystem,
    Resource
};

/**
 * Represents a color scheme for a terminal display.
 *
 * The color scheme includes the palette of colors used to draw the text and character backgrounds
 * in the display and the opacity level of the display background.
 */
class ColorScheme
{
public:
    /**
     * Constructs a new color scheme which is initialized to the default color set
     * for terminal.
     */
    ColorScheme();
    ColorScheme(const ColorScheme &other);
    ColorScheme& operator=(const ColorScheme &other) = delete;
    ~ColorScheme();

    /** Sets the descriptive name of the color scheme. */
    void setDescription(const QString &description);
    /** Returns the descriptive name of the color scheme. */
    QString description() const;

    void setSource(ColorSchemeSource source) { _source = source; }
    ColorSchemeSource source() { return _source; }

    /** Sets the name of the color scheme */
    void setName(const QString &name);
    /** Returns the name of the color scheme */
    QString name() const;

    /** Reads the color scheme from the specified configuration source */
    void read(const KConfig &config);
    /** Writes the color scheme to the specified configuration source */
    void write(KConfig &config) const;

    /** Sets a single entry within the color palette. */
    void setColorTableEntry(int index, const ColorEntry &entry);

    /**
     * Copies the color entries which form the palette for this color scheme
     * into @p table.  @p table should be an array with TABLE_COLORS entries.
     *
     * @param table Array into which the color entries for this color scheme
     * are copied.
     * @param randomSeed Color schemes may allow certain colors in their
     * palette to be randomized.  The seed is used to pick the random color.
     */
    void getColorTable(ColorEntry *table, uint randomSeed = 0) const;

    /**
     * Retrieves a single color entry from the table.
     *
     * See getColorTable()
     */
    ColorEntry colorEntry(int index, uint randomSeed = 0) const;

    /**
     * Convenience method.  Returns the
     * foreground color for this scheme,
     * this is the primary color used to draw the
     * text in this scheme.
     */
    QColor foregroundColor() const;
    /**
     * Convenience method.  Returns the background color for
     * this scheme, this is the primary color used to
     * draw the terminal background in this scheme.
     */
    QColor backgroundColor() const;

    /**
     * Returns true if this color scheme has a dark background.
     * The background color is said to be dark if it has a lightness
     * of less than 50% in the HSLuv color space.
     */
    bool hasDarkBackground() const;

    /**
     * Sets the opacity level of the display background. @p opacity ranges
     * between 0 (completely transparent background) and 1 (completely
     * opaque background).
     *
     * Defaults to 1.
     *
     * TODO: More documentation
     */
    void setOpacity(qreal opacity);
    /**
     * Returns the opacity level for this color scheme, see setOpacity()
     * TODO: More documentation
     */
    qreal opacity() const;

    /**
     * Enables blur behind the transparent window
     *
     * Defaults to false.
     */
    void setBlur(bool blur);
    /**
     * Returns whether blur is enabled for this color scheme, see setBlur()
     */
    bool blur() const;

    void setWallpaper(const QString &path);

    ColorSchemeWallpaper::Ptr wallpaper() const;

    /**
     * Enables colors randomization. This will cause the palette
     * returned by getColorTable() and colorEntry() to be adjusted
     * depending on the parameters of color randomization and the
     * random seed parameter passed to them.
     */
    void setColorRandomization(bool randomize);

    /** Returns true if color randomization is enabled. */
    bool isColorRandomizationEnabled() const;

    static const ColorEntry defaultTable[]; // table of default color entries

    static QString colorNameForIndex(int index);
    static QString translatedColorNameForIndex(int index);

private:
    // specifies how much a particular color can be randomized by
    class RandomizationRange
    {
    public:
        RandomizationRange() : hue(0.0),
            saturation(0.0),
            lightness(0.0)
        {
        }

        bool isNull() const
        {
            return qFuzzyIsNull(hue) && qFuzzyIsNull(saturation) && qFuzzyIsNull(lightness);
        }

        double hue;
        double saturation;
        double lightness;
    };

    // returns the active color table.  if none has been set specifically,
    // this is the default color table.
    const ColorEntry *colorTable() const;
    ColorSchemeSource _source = ColorSchemeSource::Unknown;

    // reads a single color entry from a KConfig source
    // and sets the palette entry at 'index' to the entry read.
    void readColorEntry(const KConfig &config, int index);
    // writes a single color entry to a KConfig source
    void writeColorEntry(KConfig &config, int index) const;

    // sets the amount of randomization allowed for a particular color
    // in the palette.  creates the randomization table if
    // it does not already exist
    void setRandomizationRange(int index, double hue, double saturation, double lightness);

    QString _description;
    QString _name;

    // pointer to custom color table, or 0 if the default color table is
    // being used
    ColorEntry *_table;

    // pointer to randomization table, or 0 if no colors in the color
    // scheme support randomization
    RandomizationRange *_randomTable;

    qreal _opacity;

    // enables blur behind the terminal window
    bool _blur;

    bool _colorRandomization;

    ColorSchemeWallpaper::Ptr _wallpaper;

    static const char * const colorNames[TABLE_COLORS];
    static const char * const translatedColorNames[TABLE_COLORS];
};
}

Q_DECLARE_METATYPE(const terminal::ColorScheme *)

#endif //COLORSCHEME_H
