/*
    Copyright 2007-2008 by Robert Knight <robertknight@gmail.com>
    Copyright 2018 by Harald Sitter <sitter@kde.org>

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
#include "EditProfileDialog.h"

// Qt
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QIcon>
#include <QPainter>
#include <QPushButton>
#include <QStandardItem>
#include <QTextCodec>
#include <QTimer>
#include <QUrl>
#include <QStandardPaths>

// KDE
//#include <kcodecaction.h>
//#include <KIconDialog>
#include <KLocalizedString>
#include <KMessageBox>
//#include <KNSCore/DownloadManager>
//#include <KWindowSystem>

#include "QContainerAdapters.h"

// Konsole
#include "ui_EditProfileGeneralPage.h"
#include "ui_EditProfileAppearancePage.h"
#include "ui_EditProfileScrollingPage.h"
#include "ui_EditProfileKeyboardPage.h"
#include "ui_EditProfileMousePage.h"
#include "ui_EditProfileAdvancedPage.h"
#include "ColorSchemeManager.h"
#include "KeyBindingEditor.h"
#include "KeyboardTranslator.h"
#include "KeyboardTranslatorManager.h"
#include "ProfileManager.h"
#include "ShellCommand.h"
//#include "WindowSystemInfo.h"
#include "FontDialog.h"

using namespace terminal;

EditProfileDialog::EditProfileDialog(QWidget *parent)
    : KPageDialog(parent)
    , _generalUi(nullptr)
    , _appearanceUi(nullptr)
    , _scrollingUi(nullptr)
    , _keyboardUi(nullptr)
    , _mouseUi(nullptr)
    , _advancedUi(nullptr)
    , _tempProfile(nullptr)
    , _profile(nullptr)
    , _previewedProperties(QHash<int, QVariant>())
    , _delayedPreviewProperties(QHash<int, QVariant>())
    , _delayedPreviewTimer(new QTimer(this))
    , _colorDialog(nullptr)
    , _buttonBox(nullptr)
    , _fontDialog(nullptr)
{
    setWindowTitle(i18n("Edit Profile"));
    setFaceType(KPageDialog::List);

    _buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply);
    setButtonBox(_buttonBox);

    QPushButton *okButton = _buttonBox->button(QDialogButtonBox::Ok);
    okButton->setDefault(true);
    connect(_buttonBox, &QDialogButtonBox::accepted, this, &terminal::EditProfileDialog::accept);
    connect(_buttonBox, &QDialogButtonBox::rejected, this, &terminal::EditProfileDialog::reject);

    // disable the apply button , since no modification has been made
    _buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);

    connect(_buttonBox->button(QDialogButtonBox::Apply),
            &QPushButton::clicked, this,
            &terminal::EditProfileDialog::apply);

    connect(_delayedPreviewTimer, &QTimer::timeout, this,
            &terminal::EditProfileDialog::delayedPreviewActivate);

    // Set a fallback icon for non-plasma desktops as this dialog looks
    // terrible without all the icons on the left sidebar.  On GTK related
    // desktops, this dialog look good enough without installing
    // oxygen-icon-theme, qt5ct and setting export QT_QPA_PLATFORMTHEME=qt5ct
    // Plain Xorg desktops still look terrible as there are no icons
    // visible.
    const auto defaultIcon = QIcon::fromTheme(QStringLiteral("utilities-terminal"));

    // General page

    const QString generalPageName = i18nc("@title:tab Generic, common options", "General");
    auto *generalPageWidget = new QWidget(this);
    _generalUi = new Ui::EditProfileGeneralPage();
    _generalUi->setupUi(generalPageWidget);
    auto *generalPageItem = addPage(generalPageWidget, generalPageName);
    generalPageItem->setHeader(generalPageName);
    generalPageItem->setIcon(QIcon::fromTheme(QStringLiteral("utilities-terminal")));
    _pages[generalPageItem] = Page(&EditProfileDialog::setupGeneralPage);

    // Appearance page

    const QString appearancePageName = i18n("Appearance");
    auto *appearancePageWidget = new QWidget(this);
    _appearanceUi = new Ui::EditProfileAppearancePage();
    _appearanceUi->setupUi(appearancePageWidget);
    auto *appearancePageItem = addPage(appearancePageWidget, appearancePageName);
    appearancePageItem->setHeader(appearancePageName);
    appearancePageItem->setIcon(QIcon::fromTheme(QStringLiteral("kcolorchooser"),
                                defaultIcon));
    _pages[appearancePageItem] = Page(&EditProfileDialog::setupAppearancePage);

    LabelsAligner appearanceAligner(appearancePageWidget);
    appearanceAligner.addLayout(dynamic_cast<QGridLayout *>(_appearanceUi->miscTabLayout));
    appearanceAligner.addLayout(dynamic_cast<QGridLayout *>(_appearanceUi->contentsGroup->layout()));
    appearanceAligner.updateLayouts();
    appearanceAligner.align();

    // Scrolling page

    const QString scrollingPageName = i18n("Scrolling");
    auto *scrollingPageWidget = new QWidget(this);
    _scrollingUi = new Ui::EditProfileScrollingPage();
    _scrollingUi->setupUi(scrollingPageWidget);
    auto *scrollingPageItem = addPage(scrollingPageWidget, scrollingPageName);
    scrollingPageItem->setHeader(scrollingPageName);
    scrollingPageItem->setIcon(QIcon::fromTheme(QStringLiteral("transform-move-vertical"),
                               defaultIcon));
    _pages[scrollingPageItem] = Page(&EditProfileDialog::setupScrollingPage);

    // adjust "history size" label height to match history size widget's first radio button
    _scrollingUi->historySizeLabel->setFixedHeight(_scrollingUi->historySizeWidget->preferredLabelHeight());

    // Keyboard page

    const QString keyboardPageName = i18n("Keyboard");
    const QString keyboardPageTitle = i18n("Key bindings");
    auto *keyboardPageWidget = new QWidget(this);
    _keyboardUi = new Ui::EditProfileKeyboardPage();
    _keyboardUi->setupUi(keyboardPageWidget);
    auto *keyboardPageItem = addPage(keyboardPageWidget, keyboardPageName);
    keyboardPageItem->setHeader(keyboardPageTitle);
    keyboardPageItem->setIcon(QIcon::fromTheme(QStringLiteral("input-keyboard"),
                              defaultIcon));
    _pages[keyboardPageItem] = Page(&EditProfileDialog::setupKeyboardPage);

    // Mouse page

    const QString mousePageName = i18n("Mouse");
    auto *mousePageWidget = new QWidget(this);
    _mouseUi = new Ui::EditProfileMousePage();
    _mouseUi->setupUi(mousePageWidget);
    auto *mousePageItem = addPage(mousePageWidget, mousePageName);
    mousePageItem->setHeader(mousePageName);
    mousePageItem->setIcon(QIcon::fromTheme(QStringLiteral("input-mouse"),
                           defaultIcon));
    _pages[mousePageItem] = Page(&EditProfileDialog::setupMousePage);

    // Advanced page

    const QString advancedPageName = i18nc("@title:tab Complex options", "Advanced");
    auto *advancedPageWidget = new QWidget(this);
    _advancedUi = new Ui::EditProfileAdvancedPage();
    _advancedUi->setupUi(advancedPageWidget);
    auto *advancedPageItem = addPage(advancedPageWidget, advancedPageName);
    advancedPageItem->setHeader(advancedPageName);
    advancedPageItem->setIcon(QIcon::fromTheme(QStringLiteral("configure"),
                              defaultIcon));
    _pages[advancedPageItem] = Page(&EditProfileDialog::setupAdvancedPage);

    // there are various setupXYZPage() methods to load the items
    // for each page and update their states to match the profile
    // being edited.
    //
    // these are only called when needed ( ie. when the user clicks
    // the tab to move to that page ).
    //
    // the _pageNeedsUpdate vector keeps track of the pages that have
    // not been updated since the last profile change and will need
    // to be refreshed when the user switches to them
    connect(this, &KPageDialog::currentPageChanged,
            this, &terminal::EditProfileDialog::preparePage);

    createTempProfile();
}

EditProfileDialog::~EditProfileDialog()
{
    delete _generalUi;
    delete _appearanceUi;
    delete _scrollingUi;
    delete _keyboardUi;
    delete _mouseUi;
    delete _advancedUi;
}

void EditProfileDialog::save()
{
    if (_tempProfile->isEmpty()) {
        return;
    }

    ProfileManager::instance()->changeProfile(_profile, _tempProfile->setProperties());

    // ensure that these settings are not undone by a call
    // to unpreview()
    for (auto [key, _] : QHashAdapter(_tempProfile->setProperties()))
    {
        _previewedProperties.remove(key);
    }

    createTempProfile();

    _buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
}

void EditProfileDialog::reject()
{
    unpreviewAll();
    QDialog::reject();
}

void EditProfileDialog::accept()
{
    // if the Apply button is disabled then no settings were changed
    // or the changes have already been saved by apply()
    if (_buttonBox->button(QDialogButtonBox::Apply)->isEnabled()) {
        if (!isValidProfileName()) {
            return;
        }
        save();
    }

    unpreviewAll();
    QDialog::accept();
}

void EditProfileDialog::apply()
{
    if (!isValidProfileName()) {
        return;
    }
    save();
}

bool EditProfileDialog::isValidProfileName()
{
    Q_ASSERT(_profile);
    Q_ASSERT(_tempProfile);

    // check whether the user has enough permissions to save the profile
    QFileInfo fileInfo(_profile->path());
    if (fileInfo.exists() && !fileInfo.isWritable()) {
        if (!_tempProfile->isPropertySet(Profile::Name)
            || (_tempProfile->name() == _profile->name())) {
                KMessageBox::sorry(this,
                                   i18n("<p>Konsole does not have permission to save this profile to:<br /> \"%1\"</p>"
                                        "<p>To be able to save settings you can either change the permissions "
                                        "of the profile configuration file or change the profile name to save "
                                        "the settings to a new profile.</p>", _profile->path()));
                return false;
        }
    }

    const QList<Profile::Ptr> existingProfiles = ProfileManager::instance()->allProfiles();
    QStringList otherExistingProfileNames;

    foreach(auto existingProfile, existingProfiles) {
        if (existingProfile->name() != _profile->name()) {
            otherExistingProfileNames.append(existingProfile->name());
        }
    }

    if ((_tempProfile->isPropertySet(Profile::Name)
         && _tempProfile->name().isEmpty())
        || (_profile->name().isEmpty() && _tempProfile->name().isEmpty())) {
        KMessageBox::sorry(this,
                           i18n("<p>Each profile must have a name before it can be saved "
                                "into disk.</p>"));
        // revert the name in the dialog
        _generalUi->profileNameEdit->setText(_profile->name());
        selectProfileName();
        return false;
    } else if (!_tempProfile->name().isEmpty() && otherExistingProfileNames.contains(_tempProfile->name())) {
        KMessageBox::sorry(this,
                            i18n("<p>A profile with this name already exists.</p>"));
        // revert the name in the dialog
        _generalUi->profileNameEdit->setText(_profile->name());
        selectProfileName();
        return false;
    } else {
        return true;
    }
}

QString EditProfileDialog::groupProfileNames(const ProfileGroup::Ptr &group, int maxLength)
{
    QString caption;
    int count = group->profiles().count();
    for (int i = 0; i < count; i++) {
        caption += group->profiles()[i]->name();
        if (i < (count - 1)) {
            caption += u',';
            // limit caption length to prevent very long window titles
            if (maxLength > 0 && caption.length() > maxLength) {
                caption += u"..."_qs;
                break;
            }
        }
    }
    return caption;
}

void EditProfileDialog::updateCaption(const Profile::Ptr &profile)
{
    const int MAX_GROUP_CAPTION_LENGTH = 25;
    ProfileGroup::Ptr group = profile->asGroup();
    if (group && group->profiles().count() > 1) {
        QString caption = groupProfileNames(group, MAX_GROUP_CAPTION_LENGTH);

        if (group->profiles().count() > 1)
        {
            setWindowTitle(i18n("Editing profile: %1", caption));
        }
        else
        {
            setWindowTitle(i18n("Editing %1 profiles: %2",
                group->profiles().count(),
                caption));
        }
    } else {
        setWindowTitle(i18n("Edit Profile \"%1\"", profile->name()));
    }
}

void EditProfileDialog::setProfile(const terminal::Profile::Ptr &profile)
{
    Q_ASSERT(profile);

    _profile = profile;

    // update caption
    updateCaption(profile);

    // mark each page of the dialog as out of date
    // and force an update of the currently visible page
    //
    // the other pages will be updated as necessary
    for (Page &page: _pages) {
        page.needsUpdate = true;
    }
    preparePage(currentPage());

    if (_tempProfile) {
        createTempProfile();
    }
}

const Profile::Ptr EditProfileDialog::lookupProfile() const
{
    return _profile;
}

const QString EditProfileDialog::currentColorSchemeName() const
{
    const QString &currentColorSchemeName = lookupProfile()->colorScheme();
    return currentColorSchemeName;
}

void EditProfileDialog::preparePage(KPageWidgetItem *current, KPageWidgetItem *before)
{
    Q_UNUSED(before);
    Q_ASSERT(current);
    Q_ASSERT(_pages.contains(current));

    const Profile::Ptr profile = lookupProfile();
    auto setupPage = _pages[current].setupPage;
    Q_ASSERT(profile);
    Q_ASSERT(setupPage);

    if (_pages[current].needsUpdate) {
        (*this.*setupPage)(profile);
        _pages[current].needsUpdate = false;
    }
}

void terminal::EditProfileDialog::selectProfileName()
{
    _generalUi->profileNameEdit->setFocus();
    _generalUi->profileNameEdit->selectAll();
}

void EditProfileDialog::setupGeneralPage(const Profile::Ptr &profile)
{
    // basic profile options
    {
        ProfileGroup::Ptr group = profile->asGroup();
        if (!group || group->profiles().count() < 2) {
            _generalUi->profileNameEdit->setText(profile->name());
            _generalUi->profileNameEdit->setClearButtonEnabled(true);
        } else {
            _generalUi->profileNameEdit->setText(groupProfileNames(group, -1));
            _generalUi->profileNameEdit->setEnabled(false);
        }
    }

    ShellCommand command(profile->command(), profile->arguments());
    _generalUi->commandEdit->setText(command.fullCommand());
    // If a "completion" is requested, consider changing this to KLineEdit
    // and using KCompletion.
    _generalUi->initialDirEdit->setText(profile->defaultWorkingDirectory());
    _generalUi->initialDirEdit->setClearButtonEnabled(true);
    _generalUi->initialDirEdit->setPlaceholderText(QStandardPaths::standardLocations(QStandardPaths::HomeLocation).value(0));

    _generalUi->dirSelectButton->setIcon(QIcon::fromTheme(QStringLiteral("folder-open")));
    _generalUi->startInSameDirButton->setChecked(profile->startInCurrentSessionDir());

    // signals and slots
    connect(_generalUi->dirSelectButton, &QToolButton::clicked, this,
            &terminal::EditProfileDialog::selectInitialDir);
    connect(_generalUi->startInSameDirButton, &QCheckBox::toggled, this,
            &terminal::EditProfileDialog::startInSameDir);
    connect(_generalUi->profileNameEdit, &QLineEdit::textChanged, this,
            &terminal::EditProfileDialog::profileNameChanged);
    connect(_generalUi->initialDirEdit, &QLineEdit::textChanged, this,
            &terminal::EditProfileDialog::initialDirChanged);
    connect(_generalUi->commandEdit, &QLineEdit::textChanged, this,
            &terminal::EditProfileDialog::commandChanged);
    connect(_generalUi->environmentEditButton, &QPushButton::clicked, this,
            &terminal::EditProfileDialog::showEnvironmentEditor);
}

void EditProfileDialog::showEnvironmentEditor()
{
    bool ok;
    const Profile::Ptr profile = lookupProfile();

    QStringList currentEnvironment;

    // The user could re-open the environment editor before clicking
    // OK/Apply in the parent edit profile dialog, so we make sure
    // to show the new environment vars
    if (_tempProfile->isPropertySet(Profile::Environment)) {
            currentEnvironment  = _tempProfile->environment();
    } else {
        currentEnvironment = profile->environment();
    }

    QString text = QInputDialog::getMultiLineText(this,
                                                  i18n("Edit Environment"),
                                                  i18n("One environment variable per line"),
                                                  currentEnvironment.join(u'
'),
                                                  &ok);

    QStringList newEnvironment;

    if (ok) {
        if(!text.isEmpty()) {
            newEnvironment = text.split(u'
');
            updateTempProfileProperty(Profile::Environment, newEnvironment);
        } else {
            // the user could have removed all entries so we return an empty list
            updateTempProfileProperty(Profile::Environment, newEnvironment);
        }
    }
}

void EditProfileDialog::profileNameChanged(const QString &name)
{
    updateTempProfileProperty(Profile::Name, name);
    updateTempProfileProperty(Profile::UntranslatedName, name);
    updateCaption(_tempProfile);
}

void EditProfileDialog::startInSameDir(bool sameDir)
{
    updateTempProfileProperty(Profile::StartInCurrentSessionDir, sameDir);
}

void EditProfileDialog::initialDirChanged(const QString &dir)
{
    updateTempProfileProperty(Profile::Directory, dir);
}

void EditProfileDialog::commandChanged(const QString &command)
{
    ShellCommand shellCommand(command);

    updateTempProfileProperty(Profile::Command, shellCommand.command());
    updateTempProfileProperty(Profile::Arguments, shellCommand.arguments());
}

void EditProfileDialog::selectInitialDir()
{
    const QUrl url = QFileDialog::getExistingDirectoryUrl(this,
                                                          i18n("Select Initial Directory"),
                                                          QUrl::fromUserInput(_generalUi->initialDirEdit->text()));

    if (!url.isEmpty()) {
        _generalUi->initialDirEdit->setText(url.path());
    }
}

void EditProfileDialog::setupAppearancePage(const Profile::Ptr &profile)
{
    auto delegate = new ColorSchemeViewDelegate(this);
    _appearanceUi->colorSchemeList->setItemDelegate(delegate);

    _appearanceUi->transparencyWarningWidget->setVisible(false);
    _appearanceUi->transparencyWarningWidget->setWordWrap(true);
    _appearanceUi->transparencyWarningWidget->setCloseButtonVisible(false);
    _appearanceUi->transparencyWarningWidget->setMessageType(KMessageWidget::Warning);

    _appearanceUi->colorSchemeMessageWidget->setVisible(false);
    _appearanceUi->colorSchemeMessageWidget->setWordWrap(true);
    _appearanceUi->colorSchemeMessageWidget->setCloseButtonVisible(false);
    _appearanceUi->colorSchemeMessageWidget->setMessageType(KMessageWidget::Warning);

    _appearanceUi->editColorSchemeButton->setEnabled(false);
    _appearanceUi->removeColorSchemeButton->setEnabled(false);
    _appearanceUi->resetColorSchemeButton->setEnabled(false);

    // setup color list
    // select the colorScheme used in the current profile
    updateColorSchemeList(currentColorSchemeName());

    _appearanceUi->colorSchemeList->setMouseTracking(true);
    _appearanceUi->colorSchemeList->installEventFilter(this);
    _appearanceUi->colorSchemeList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

    connect(_appearanceUi->colorSchemeList->selectionModel(),
            &QItemSelectionModel::selectionChanged, this,
            &terminal::EditProfileDialog::colorSchemeSelected);
    connect(_appearanceUi->colorSchemeList, &QListView::entered, this,
            &terminal::EditProfileDialog::previewColorScheme);

    updateColorSchemeButtons();

    connect(_appearanceUi->editColorSchemeButton, &QPushButton::clicked, this,
            &terminal::EditProfileDialog::editColorScheme);
    connect(_appearanceUi->removeColorSchemeButton, &QPushButton::clicked, this,
            &terminal::EditProfileDialog::removeColorScheme);
    connect(_appearanceUi->newColorSchemeButton, &QPushButton::clicked, this,
            &terminal::EditProfileDialog::newColorScheme);

    connect(_appearanceUi->resetColorSchemeButton, &QPushButton::clicked, this,
            &terminal::EditProfileDialog::resetColorScheme);

    connect(_appearanceUi->chooseFontButton, &QAbstractButton::clicked, this,
            &EditProfileDialog::showFontDialog);

    // setup font preview
    const bool antialias = profile->antiAliasFonts();

    QFont profileFont = profile->font();
    profileFont.setStyleStrategy(antialias ? QFont::PreferAntialias : QFont::NoAntialias);

    _appearanceUi->fontPreview->setFont(profileFont);
    _appearanceUi->fontPreview->setText(QStringLiteral("%1 %2pt").arg(profileFont.family()).arg(profileFont.pointSize()));

    // setup font smoothing
    _appearanceUi->antialiasTextButton->setChecked(antialias);
    connect(_appearanceUi->antialiasTextButton, &QCheckBox::toggled, this,
            &terminal::EditProfileDialog::setAntialiasText);

    _appearanceUi->boldIntenseButton->setChecked(profile->boldIntense());
    connect(_appearanceUi->boldIntenseButton, &QCheckBox::toggled, this,
            &terminal::EditProfileDialog::setBoldIntense);

    _appearanceUi->useFontLineCharactersButton->setChecked(profile->useFontLineCharacters());
    connect(_appearanceUi->useFontLineCharactersButton, &QCheckBox::toggled, this,
            &terminal::EditProfileDialog::useFontLineCharacters);

    _mouseUi->enableMouseWheelZoomButton->setChecked(profile->mouseWheelZoomEnabled());
    connect(_mouseUi->enableMouseWheelZoomButton, &QCheckBox::toggled, this,
            &terminal::EditProfileDialog::toggleMouseWheelZoom);

    // cursor options
    const auto options = QVector<BooleanOption>{
        {
            _appearanceUi->enableBlinkingCursorButton, Profile::BlinkingCursorEnabled,
            SLOT(toggleBlinkingCursor(bool))
        },
    };
    setupCheckBoxes(options, profile);

    if (profile->useCustomCursorColor()) {
        _appearanceUi->customCursorColorButton->setChecked(true);
    } else {
        _appearanceUi->autoCursorColorButton->setChecked(true);
    }

    _appearanceUi->customColorSelectButton->setColor(profile->customCursorColor());

    connect(_appearanceUi->customCursorColorButton, &QRadioButton::clicked, this, &terminal::EditProfileDialog::customCursorColor);
    connect(_appearanceUi->autoCursorColorButton, &QRadioButton::clicked, this, &terminal::EditProfileDialog::autoCursorColor);
    connect(_appearanceUi->customColorSelectButton, &KColorButton::changed, this, &terminal::EditProfileDialog::customCursorColorChanged);

    // Make radio buttons height equal
    int cursorColorRadioHeight = qMax(_appearanceUi->autoCursorColorButton->minimumSizeHint().height(),
                                      _appearanceUi->customColorSelectButton->minimumSizeHint().height());
    _appearanceUi->autoCursorColorButton->setMinimumHeight(cursorColorRadioHeight);
    _appearanceUi->customCursorColorButton->setMinimumHeight(cursorColorRadioHeight);

    const ButtonGroupOptions cursorShapeOptions = {
        _appearanceUi->cursorShape, // group
        Profile::CursorShape,       // profileProperty
        true,                       // preview
        {                           // buttons
            {_appearanceUi->cursorShapeBlock,       Enum::BlockCursor},
            {_appearanceUi->cursorShapeIBeam,       Enum::IBeamCursor},
            {_appearanceUi->cursorShapeUnderline,   Enum::UnderlineCursor},
        },
    };
    setupButtonGroup(cursorShapeOptions, profile);

    _appearanceUi->marginsSpinner->setValue(profile->terminalMargin());
    connect(_appearanceUi->marginsSpinner,
            QOverload<int>::of(&QSpinBox::valueChanged), this,
            &terminal::EditProfileDialog::terminalMarginChanged);

    _appearanceUi->lineSpacingSpinner->setValue(profile->lineSpacing());
    connect(_appearanceUi->lineSpacingSpinner,
            QOverload<int>::of(&QSpinBox::valueChanged), this,
            &terminal::EditProfileDialog::lineSpacingChanged);

    _appearanceUi->alignToCenterButton->setChecked(profile->terminalCenter());
    connect(_appearanceUi->alignToCenterButton, &QCheckBox::toggled, this,
            &terminal::EditProfileDialog::setTerminalCenter);
}

void EditProfileDialog::setAntialiasText(bool enable)
{
    preview(Profile::AntiAliasFonts, enable);
    updateTempProfileProperty(Profile::AntiAliasFonts, enable);

    const QFont font = _profile->font();
    updateFontPreview(font);
}

void EditProfileDialog::setBoldIntense(bool enable)
{
    preview(Profile::BoldIntense, enable);
    updateTempProfileProperty(Profile::BoldIntense, enable);
}

void EditProfileDialog::useFontLineCharacters(bool enable)
{
    preview(Profile::UseFontLineCharacters, enable);
    updateTempProfileProperty(Profile::UseFontLineCharacters, enable);
}

void EditProfileDialog::toggleBlinkingCursor(bool enable)
{
    preview(Profile::BlinkingCursorEnabled, enable);
    updateTempProfileProperty(Profile::BlinkingCursorEnabled, enable);
}

void EditProfileDialog::setCursorShape(int index)
{
    preview(Profile::CursorShape, index);
    updateTempProfileProperty(Profile::CursorShape, index);
}

void EditProfileDialog::autoCursorColor()
{
    preview(Profile::UseCustomCursorColor, false);
    updateTempProfileProperty(Profile::UseCustomCursorColor, false);
}

void EditProfileDialog::customCursorColor()
{
    preview(Profile::UseCustomCursorColor, true);
    updateTempProfileProperty(Profile::UseCustomCursorColor, true);
}

void EditProfileDialog::customCursorColorChanged(const QColor &color)
{
    preview(Profile::CustomCursorColor, color);
    updateTempProfileProperty(Profile::CustomCursorColor, color);

    // ensure that custom cursor colors are enabled
    _appearanceUi->customCursorColorButton->click();
}

void EditProfileDialog::terminalMarginChanged(int margin)
{
    preview(Profile::TerminalMargin, margin);
    updateTempProfileProperty(Profile::TerminalMargin, margin);
}

void EditProfileDialog::lineSpacingChanged(int spacing)
{
    preview(Profile::LineSpacing, spacing);
    updateTempProfileProperty(Profile::LineSpacing, spacing);
}

void EditProfileDialog::setTerminalCenter(bool enable)
{
    preview(Profile::TerminalCenter, enable);
    updateTempProfileProperty(Profile::TerminalCenter, enable);
}

void EditProfileDialog::toggleMouseWheelZoom(bool enable)
{
    updateTempProfileProperty(Profile::MouseWheelZoomEnabled, enable);
}

void EditProfileDialog::toggleAlternateScrolling(bool enable)
{
    updateTempProfileProperty(Profile::AlternateScrolling, enable);
}

void EditProfileDialog::updateColorSchemeList(const QString &selectedColorSchemeName)
{
    if (_appearanceUi->colorSchemeList->model() == nullptr) {
        _appearanceUi->colorSchemeList->setModel(new QStandardItemModel(this));
    }

    const ColorScheme *selectedColorScheme = ColorSchemeManager::instance()->findColorScheme(selectedColorSchemeName);

    auto *model = qobject_cast<QStandardItemModel *>(_appearanceUi->colorSchemeList->model());

    Q_ASSERT(model);

    model->clear();

    QStandardItem *selectedItem = nullptr;

    QList<const ColorScheme *> schemeList = ColorSchemeManager::instance()->allColorSchemes();

    foreach (const ColorScheme *scheme, schemeList) {
        QStandardItem *item = new QStandardItem(scheme->description());
        item->setData(QVariant::fromValue(scheme), Qt::UserRole + 1);
        item->setData(QVariant::fromValue(_profile->font()), Qt::UserRole + 2);
        item->setFlags(item->flags());

        // if selectedColorSchemeName is not empty then select that scheme
        // after saving the changes in the colorScheme editor
        if (selectedColorScheme == scheme) {
            selectedItem = item;
        }

        model->appendRow(item);
    }

    model->sort(0);

    if (selectedItem != nullptr) {
        _appearanceUi->colorSchemeList->updateGeometry();
        _appearanceUi->colorSchemeList->selectionModel()->setCurrentIndex(selectedItem->index(),
                                                                QItemSelectionModel::Select);

        // update transparency warning label
        updateTransparencyWarning();
    }
}

void EditProfileDialog::updateKeyBindingsList(const QString &selectKeyBindingsName)
{
    if (_keyboardUi->keyBindingList->model() == nullptr) {
        _keyboardUi->keyBindingList->setModel(new QStandardItemModel(this));
    }

    auto *model = qobject_cast<QStandardItemModel *>(_keyboardUi->keyBindingList->model());

    Q_ASSERT(model);

    model->clear();

    QStandardItem *selectedItem = nullptr;

    const QStringList &translatorNames = _keyManager->allTranslators();
    for (const QString &translatorName : translatorNames) {
        const KeyboardTranslator *translator = _keyManager->findTranslator(translatorName);
        if (translator == nullptr) {
            continue;
        }

        QStandardItem *item = new QStandardItem(translator->description());
        item->setEditable(false);
        item->setData(QVariant::fromValue(translator), Qt::UserRole + 1);
        item->setData(QVariant::fromValue(_keyManager->findTranslatorPath(translatorName)), Qt::ToolTipRole);
        item->setData(QVariant::fromValue(_profile->font()), Qt::UserRole + 2);
        item->setIcon(QIcon::fromTheme(QStringLiteral("preferences-desktop-keyboard")));

        if (selectKeyBindingsName == translatorName) {
            selectedItem = item;
        }

        model->appendRow(item);
    }

    model->sort(0);

    if (selectedItem != nullptr) {
        _keyboardUi->keyBindingList->selectionModel()->setCurrentIndex(selectedItem->index(),
                                                               QItemSelectionModel::Select);
    }
}

bool EditProfileDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == _appearanceUi->colorSchemeList && event->type() == QEvent::Leave) {
        if (_tempProfile->isPropertySet(Profile::ColorScheme)) {
            preview(Profile::ColorScheme, _tempProfile->colorScheme());
        } else {
            unpreview(Profile::ColorScheme);
        }
    }

    return QDialog::eventFilter(watched, event);
}

QSize EditProfileDialog::sizeHint() const
{
    QFontMetrics fm(font());
    const int ch = fm.boundingRect(u'0').width();

    // By default minimum size is used. Increase it to make text inputs
    // on "tabs" page wider and to add some whitespace on right side
    // of other pages. The window will not be wider than 2/3 of
    // the screen width (unless necessary to fit everything)
    return QDialog::sizeHint() + QSize(10*ch, 0);
}

void EditProfileDialog::unpreviewAll()
{
    _delayedPreviewTimer->stop();
    _delayedPreviewProperties.clear();

    QHash<Profile::Property, QVariant> map;

    for (auto [key, value] : QHashAdapter(_previewedProperties))
    {
        map.insert(static_cast<Profile::Property>(key), value);
    }

    // undo any preview changes
    if (!map.isEmpty())
    {
        ProfileManager::instance()->changeProfile(_profile, map, false);
    }
}

void EditProfileDialog::unpreview(int property)
{
    _delayedPreviewProperties.remove(property);

    if (!_previewedProperties.contains(property)) {
        return;
    }

    QHash<Profile::Property, QVariant> map;
    map.insert(static_cast<Profile::Property>(property), _previewedProperties[property]);
    ProfileManager::instance()->changeProfile(_profile, map, false);

    _previewedProperties.remove(property);
}

void EditProfileDialog::delayedPreview(int property, const QVariant &value)
{
    _delayedPreviewProperties.insert(property, value);

    _delayedPreviewTimer->stop();
    _delayedPreviewTimer->start(300);
}

void EditProfileDialog::delayedPreviewActivate()
{
    Q_ASSERT(qobject_cast<QTimer *>(sender()));

    for (auto [key, value] : QHashAdapter(_delayedPreviewProperties))
    {
        preview(key, value);
    }
}

void EditProfileDialog::preview(int property, const QVariant &value)
{
    QHash<Profile::Property, QVariant> map;
    map.insert(static_cast<Profile::Property>(property), value);

    _delayedPreviewProperties.remove(property);

    const Profile::Ptr original = lookupProfile();

    // skip previews for profile groups if the profiles in the group
    // have conflicting original values for the property
    //
    // TODO - Save the original values for each profile and use to unpreview properties
    ProfileGroup::Ptr group = original->asGroup();
    if (group && group->profiles().count() > 1
        && original->property<QVariant>(static_cast<Profile::Property>(property)).isNull()) {
        return;
    }

    if (!_previewedProperties.contains(property)) {
        _previewedProperties.insert(property,
                                    original->property<QVariant>(static_cast<Profile::Property>(property)));
    }

    // temporary change to color scheme
    ProfileManager::instance()->changeProfile(_profile, map, false);
}

void EditProfileDialog::previewColorScheme(const QModelIndex &index)
{
    const QString &name = index.data(Qt::UserRole + 1).value<const ColorScheme *>()->name();
    delayedPreview(Profile::ColorScheme, name);
}

void EditProfileDialog::showFontDialog()
{
    if (_fontDialog == nullptr) {
        _fontDialog = new FontDialog(this);
        connect(_fontDialog, &FontDialog::fontChanged, this, [this](const QFont &font) {
            preview(Profile::Font, font);
            updateFontPreview(font);
        });
        connect(_fontDialog, &FontDialog::accepted, this, [this]() {
            const QFont font = _fontDialog->font();
            preview(Profile::Font, font);
            updateTempProfileProperty(Profile::Font, font);
            updateFontPreview(font);
        });
        connect(_fontDialog, &FontDialog::rejected, this, [this]() {
            unpreview(Profile::Font);
            const QFont font = _profile->font();
            updateFontPreview(font);
        });
    }
    const QFont font = _profile->font();
    _fontDialog->setFont(font);
    _fontDialog->exec();
}

void EditProfileDialog::updateFontPreview(QFont font)
{
    bool aa = _profile->antiAliasFonts();
    font.setStyleStrategy(aa ? QFont::PreferAntialias : QFont::NoAntialias);

    _appearanceUi->fontPreview->setFont(font);
    _appearanceUi->fontPreview->setText(QStringLiteral("%1 %2pt").arg(font.family()).arg(font.pointSize()));
}

void EditProfileDialog::removeColorScheme()
{
    QModelIndexList selected = _appearanceUi->colorSchemeList->selectionModel()->selectedIndexes();
    if (selected.isEmpty()) {
        return;
    }
}

void EditProfileDialog::resetColorScheme()
{

    QModelIndexList selected = _appearanceUi->colorSchemeList->selectionModel()->selectedIndexes();

    if (!selected.isEmpty()) {
        const QString &name = selected.first().data(Qt::UserRole + 1).value<const ColorScheme *>()->name();

        ColorSchemeManager::instance()->deleteColorScheme(name);

        // select the colorScheme used in the current profile
        updateColorSchemeList(currentColorSchemeName());
    }
}

void EditProfileDialog::showColorSchemeEditor(bool isNewScheme)
{
    // Finding selected ColorScheme
    QModelIndexList selected = _appearanceUi->colorSchemeList->selectionModel()->selectedIndexes();
    QAbstractItemModel *model = _appearanceUi->colorSchemeList->model();
    const ColorScheme *colors = nullptr;
    if (!selected.isEmpty()) {
        colors = model->data(selected.first(), Qt::UserRole + 1).value<const ColorScheme *>();
    } else {
        colors = ColorSchemeManager::instance()->defaultColorScheme();
    }

    Q_ASSERT(colors);

    // Setting up ColorSchemeEditor ui
    // close any running ColorSchemeEditor
    if (_colorDialog != nullptr) {
        closeColorSchemeEditor();
    }
    _colorDialog = new ColorSchemeEditor(this);

    connect(_colorDialog, &terminal::ColorSchemeEditor::colorSchemeSaveRequested, this,
            &terminal::EditProfileDialog::saveColorScheme);
    _colorDialog->setup(colors, isNewScheme);

    _colorDialog->show();
}

void EditProfileDialog::closeColorSchemeEditor()
{
    if (_colorDialog != nullptr) {
        _colorDialog->close();
        delete _colorDialog;
    }
}

void EditProfileDialog::newColorScheme()
{
    showColorSchemeEditor(true);
}

void EditProfileDialog::editColorScheme()
{
    showColorSchemeEditor(false);
}

void EditProfileDialog::saveColorScheme(const ColorScheme &scheme, bool isNewScheme)
{
    auto newScheme = new ColorScheme(scheme);

    // if this is a new color scheme, pick a name based on the description
    if (isNewScheme) {
        newScheme->setName(newScheme->description());
    }

    ColorSchemeManager::instance()->addColorScheme(newScheme);

    const QString &selectedColorSchemeName = newScheme->name();

    // select the edited or the new colorScheme after saving the changes
    updateColorSchemeList(selectedColorSchemeName);

    preview(Profile::ColorScheme, newScheme->name());
}

void EditProfileDialog::colorSchemeSelected()
{
    QModelIndexList selected = _appearanceUi->colorSchemeList->selectionModel()->selectedIndexes();

    if (!selected.isEmpty()) {
        QAbstractItemModel *model = _appearanceUi->colorSchemeList->model();
        const auto *colors = model->data(selected.first(), Qt::UserRole + 1).value<const ColorScheme *>();
        if (colors != nullptr) {
            updateTempProfileProperty(Profile::ColorScheme, colors->name());
            previewColorScheme(selected.first());

            updateTransparencyWarning();
        }
    }

    updateColorSchemeButtons();
}

void EditProfileDialog::updateColorSchemeButtons()
{
    enableIfNonEmptySelection(_appearanceUi->editColorSchemeButton, _appearanceUi->colorSchemeList->selectionModel());

    QModelIndexList selected = _appearanceUi->colorSchemeList->selectionModel()->selectedIndexes();

    if (!selected.isEmpty()) {
        const QString &name = selected.first().data(Qt::UserRole + 1).value<const ColorScheme *>()->name();

        bool isResettable = ColorSchemeManager::instance()->canResetColorScheme(name);
        _appearanceUi->resetColorSchemeButton->setEnabled(isResettable);

        bool isDeletable = ColorSchemeManager::instance()->isColorSchemeDeletable(name);
        // if a colorScheme can be restored then it can't be deleted
        _appearanceUi->removeColorSchemeButton->setEnabled(isDeletable && !isResettable);
    } else {
        _appearanceUi->removeColorSchemeButton->setEnabled(false);
        _appearanceUi->resetColorSchemeButton->setEnabled(false);
    }

}

void EditProfileDialog::updateKeyBindingsButtons()
{
    QModelIndexList selected = _keyboardUi->keyBindingList->selectionModel()->selectedIndexes();

    if (!selected.isEmpty()) {
        _keyboardUi->editKeyBindingsButton->setEnabled(true);

        const QString &name = selected.first().data(Qt::UserRole + 1).value<const KeyboardTranslator *>()->name();

        bool isResettable = _keyManager->isTranslatorResettable(name);
        _keyboardUi->resetKeyBindingsButton->setEnabled(isResettable);

        bool isDeletable = _keyManager->isTranslatorDeletable(name);

        // if a key bindings scheme can be reset then it can't be deleted
        _keyboardUi->removeKeyBindingsButton->setEnabled(isDeletable && !isResettable);
    }
}

void EditProfileDialog::enableIfNonEmptySelection(QWidget *widget, QItemSelectionModel *selectionModel)
{
    widget->setEnabled(selectionModel->hasSelection());
}

void EditProfileDialog::updateTransparencyWarning()
{
    // zero or one indexes can be selected
//    foreach (const QModelIndex &index, _appearanceUi->colorSchemeList->selectionModel()->selectedIndexes()) {
//        bool needTransparency = index.data(Qt::UserRole + 1).value<const ColorScheme *>()->opacity() < 1.0;

//        if (!needTransparency) {
//            _appearanceUi->transparencyWarningWidget->setHidden(true);
//        } else if (!KWindowSystem::compositingActive()) {
//            _appearanceUi->transparencyWarningWidget->setText(i18n(
//                                                        "This color scheme uses a transparent background"
//                                                        " which does not appear to be supported on your"
//                                                        " desktop"));
//            _appearanceUi->transparencyWarningWidget->setHidden(false);
//        } else if (!WindowSystemInfo::HAVE_TRANSPARENCY) {
//            _appearanceUi->transparencyWarningWidget->setText(i18n(
//                                                        "Konsole was started before desktop effects were enabled."
//                                                        " You need to restart Konsole to see transparent background."));
//            _appearanceUi->transparencyWarningWidget->setHidden(false);
//        }
//    }
}

void EditProfileDialog::createTempProfile()
{
    _tempProfile = Profile::Ptr(new Profile);
    _tempProfile->setHidden(true);
}

void EditProfileDialog::updateTempProfileProperty(Profile::Property property, const QVariant &value)
{
    _tempProfile->setProperty(property, value);
    updateButtonApply();
}

void EditProfileDialog::updateButtonApply()
{
    bool userModified = false;

    for (auto [property, value] : QHashAdapter(_tempProfile->setProperties()))
    {
        // for previewed property
        if (_previewedProperties.contains(static_cast<int>(property))) {
            if (value != _previewedProperties.value(static_cast<int>(property))) {
                userModified = true;
                break;
            }
        // for not-previewed property
        //
        // for the Profile::KeyBindings property, if it's set in the _tempProfile
        // then the user opened the edit key bindings dialog and clicked
        // OK, and could have add/removed a key bindings rule
        } else if (property == Profile::KeyBindings || (value != _profile->property<QVariant>(property))) {
            userModified = true;
            break;
        }
    }

    _buttonBox->button(QDialogButtonBox::Apply)->setEnabled(userModified);
}

void EditProfileDialog::setupKeyboardPage(const Profile::Ptr &/* profile */)
{
    // setup translator list
    updateKeyBindingsList(lookupProfile()->keyBindings());

    connect(_keyboardUi->keyBindingList->selectionModel(),
            &QItemSelectionModel::selectionChanged, this,
            &terminal::EditProfileDialog::keyBindingSelected);
    connect(_keyboardUi->newKeyBindingsButton, &QPushButton::clicked, this,
            &terminal::EditProfileDialog::newKeyBinding);

    _keyboardUi->editKeyBindingsButton->setEnabled(false);
    _keyboardUi->removeKeyBindingsButton->setEnabled(false);
    _keyboardUi->resetKeyBindingsButton->setEnabled(false);

    updateKeyBindingsButtons();

    connect(_keyboardUi->editKeyBindingsButton, &QPushButton::clicked, this,
            &terminal::EditProfileDialog::editKeyBinding);
    connect(_keyboardUi->removeKeyBindingsButton, &QPushButton::clicked, this,
            &terminal::EditProfileDialog::removeKeyBinding);
    connect(_keyboardUi->resetKeyBindingsButton, &QPushButton::clicked, this,
            &terminal::EditProfileDialog::resetKeyBindings);
}

void EditProfileDialog::keyBindingSelected()
{
    QModelIndexList selected = _keyboardUi->keyBindingList->selectionModel()->selectedIndexes();

    if (!selected.isEmpty()) {
        QAbstractItemModel *model = _keyboardUi->keyBindingList->model();
        const auto *translator = model->data(selected.first(), Qt::UserRole + 1)
                                               .value<const KeyboardTranslator *>();
        if (translator != nullptr) {
            updateTempProfileProperty(Profile::KeyBindings, translator->name());
        }
    }

    updateKeyBindingsButtons();
}

void EditProfileDialog::removeKeyBinding()
{
    QModelIndexList selected = _keyboardUi->keyBindingList->selectionModel()->selectedIndexes();

    if (!selected.isEmpty()) {
        const QString &name = selected.first().data(Qt::UserRole + 1).value<const KeyboardTranslator *>()->name();
        if (KeyboardTranslatorManager::instance()->deleteTranslator(name)) {
            _keyboardUi->keyBindingList->model()->removeRow(selected.first().row());
        }
    }
}

void EditProfileDialog::showKeyBindingEditor(bool isNewTranslator)
{
    QModelIndexList selected = _keyboardUi->keyBindingList->selectionModel()->selectedIndexes();
    QAbstractItemModel *model = _keyboardUi->keyBindingList->model();

    const KeyboardTranslator *translator = nullptr;
    if (!selected.isEmpty()) {
        translator = model->data(selected.first(), Qt::UserRole + 1).value<const KeyboardTranslator *>();
    } else {
        translator = _keyManager->defaultTranslator();
    }

    Q_ASSERT(translator);

    auto editor = new KeyBindingEditor(this);

    if (translator != nullptr) {
        editor->setup(translator, lookupProfile()->keyBindings(), isNewTranslator);
    }

    connect(editor, &terminal::KeyBindingEditor::updateKeyBindingsListRequest,
            this, &terminal::EditProfileDialog::updateKeyBindingsList);

    connect(editor, &terminal::KeyBindingEditor::updateTempProfileKeyBindingsRequest,
            this, &terminal::EditProfileDialog::updateTempProfileProperty);

    editor->exec();
}

void EditProfileDialog::newKeyBinding()
{
    showKeyBindingEditor(true);
}

void EditProfileDialog::editKeyBinding()
{
    showKeyBindingEditor(false);
}

void EditProfileDialog::resetKeyBindings()
{
    QModelIndexList selected = _keyboardUi->keyBindingList->selectionModel()->selectedIndexes();

    if (!selected.isEmpty()) {
        const QString &name = selected.first().data(Qt::UserRole + 1).value<const KeyboardTranslator *>()->name();

        _keyManager->deleteTranslator(name);
        // find and load the translator
        _keyManager->findTranslator(name);

        updateKeyBindingsList(name);
    }
}

void EditProfileDialog::setupCheckBoxes(const QVector<BooleanOption>& options, const Profile::Ptr &profile)
{
    for(const auto& option : options) {
        option.button->setChecked(profile->property<bool>(option.property));
        connect(option.button, SIGNAL(toggled(bool)), this, option.slot);
    }
}

void EditProfileDialog::setupRadio(const QVector<RadioOption>& possibilities, int actual)
{
    for(const auto& possibility : possibilities) {
        possibility.button->setChecked(possibility.value == actual);
        connect(possibility.button, SIGNAL(clicked()), this, possibility.slot);
    }
}

void EditProfileDialog::setupButtonGroup(const ButtonGroupOptions &options, const Profile::Ptr &profile)
{
    auto currentValue = profile->property<int>(options.profileProperty);

    for (auto option: options.buttons) {
        options.group->setId(option.button, option.value);
    }

    Q_ASSERT(options.buttons.count() > 0);
    auto *activeButton = options.group->button(currentValue);
    if (activeButton == nullptr) {
        activeButton = options.buttons[0].button;
    }
    activeButton->setChecked(true);

    connect(options.group, QOverload<int>::of(&QButtonGroup::buttonClicked),
            this, [this, options](int value) {
        if (options.preview) {
            preview(options.profileProperty, value);
        }
        updateTempProfileProperty(options.profileProperty, value);
    });
}

void EditProfileDialog::setupScrollingPage(const Profile::Ptr &profile)
{
    // setup scrollbar radio
    const ButtonGroupOptions scrollBarPositionOptions = {
        _scrollingUi->scrollBarPosition,    // group
        Profile::ScrollBarPosition,         // profileProperty
        false,                              // preview
        {                                   // buttons
            {_scrollingUi->scrollBarRightButton,    Enum::ScrollBarRight},
            {_scrollingUi->scrollBarLeftButton,     Enum::ScrollBarLeft},
            {_scrollingUi->scrollBarHiddenButton,   Enum::ScrollBarHidden},
        },
    };
    setupButtonGroup(scrollBarPositionOptions, profile);

    // setup scrollback type radio
    auto scrollBackType = profile->property<int>(Profile::HistoryMode);
    _scrollingUi->historySizeWidget->setMode(Enum::HistoryModeEnum(scrollBackType));
    connect(_scrollingUi->historySizeWidget, &terminal::HistorySizeWidget::historyModeChanged, this,
            &terminal::EditProfileDialog::historyModeChanged);

    // setup scrollback line count spinner
    const int historySize = profile->historySize();
    _scrollingUi->historySizeWidget->setLineCount(historySize);

    // setup scrollpageamount type radio
    auto scrollFullPage = profile->property<int>(Profile::ScrollFullPage);

    const auto pageamounts = QVector<RadioOption>{
        {_scrollingUi->scrollHalfPage, Enum::ScrollPageHalf, SLOT(scrollHalfPage())},
        {_scrollingUi->scrollFullPage, Enum::ScrollPageFull, SLOT(scrollFullPage())}
    };

    setupRadio(pageamounts, scrollFullPage);

    // signals and slots
    connect(_scrollingUi->historySizeWidget, &terminal::HistorySizeWidget::historySizeChanged, this,
            &terminal::EditProfileDialog::historySizeChanged);
}

void EditProfileDialog::historySizeChanged(int lineCount)
{
    updateTempProfileProperty(Profile::HistorySize, lineCount);
}

void EditProfileDialog::historyModeChanged(Enum::HistoryModeEnum mode)
{
    updateTempProfileProperty(Profile::HistoryMode, mode);
}

void EditProfileDialog::scrollFullPage()
{
    updateTempProfileProperty(Profile::ScrollFullPage, Enum::ScrollPageFull);
}

void EditProfileDialog::scrollHalfPage()
{
    updateTempProfileProperty(Profile::ScrollFullPage, Enum::ScrollPageHalf);
}

void EditProfileDialog::setupMousePage(const Profile::Ptr &profile)
{
    const auto options = QVector<BooleanOption>{
        {
            _mouseUi->underlineLinksButton, Profile::UnderlineLinksEnabled,
            SLOT(toggleUnderlineLinks(bool))
        },
        {
            _mouseUi->underlineFilesButton, Profile::UnderlineFilesEnabled,
            SLOT(toggleUnderlineFiles(bool))
        },
        {
            _mouseUi->ctrlRequiredForDragButton, Profile::CtrlRequiredForDrag,
            SLOT(toggleCtrlRequiredForDrag(bool))
        },
        {
            _mouseUi->copyTextAsHTMLButton, Profile::CopyTextAsHTML,
            SLOT(toggleCopyTextAsHTML(bool))
        },
        {
            _mouseUi->copyTextToClipboardButton, Profile::AutoCopySelectedText,
            SLOT(toggleCopyTextToClipboard(bool))
        },
        {
            _mouseUi->trimLeadingSpacesButton, Profile::TrimLeadingSpacesInSelectedText,
            SLOT(toggleTrimLeadingSpacesInSelectedText(bool))
        },
        {
            _mouseUi->trimTrailingSpacesButton, Profile::TrimTrailingSpacesInSelectedText,
            SLOT(toggleTrimTrailingSpacesInSelectedText(bool))
        },
        {
            _mouseUi->openLinksByDirectClickButton, Profile::OpenLinksByDirectClickEnabled,
            SLOT(toggleOpenLinksByDirectClick(bool))
        },
        {
            _mouseUi->dropUrlsAsText, Profile::DropUrlsAsText,
            SLOT(toggleDropUrlsAsText(bool))
        },
        {
            _mouseUi->enableAlternateScrollingButton, Profile::AlternateScrolling,
            SLOT(toggleAlternateScrolling(bool))
        }
    };
    setupCheckBoxes(options, profile);

    // setup middle click paste mode
    const auto middleClickPasteMode = profile->property<int>(Profile::MiddleClickPasteMode);
    const auto pasteModes = QVector<RadioOption> {
        {_mouseUi->pasteFromX11SelectionButton, Enum::PasteFromX11Selection, SLOT(pasteFromX11Selection())},
        {_mouseUi->pasteFromClipboardButton, Enum::PasteFromClipboard, SLOT(pasteFromClipboard())}    };
    setupRadio(pasteModes, middleClickPasteMode);

    // interaction options
    _mouseUi->wordCharacterEdit->setText(profile->wordCharacters());

    connect(_mouseUi->wordCharacterEdit, &QLineEdit::textChanged, this, &terminal::EditProfileDialog::wordCharactersChanged);

    const ButtonGroupOptions tripleClickModeOptions = {
        _mouseUi->tripleClickMode,  // group
        Profile::TripleClickMode,   // profileProperty
        false,                      // preview
        {                           // buttons
            {_mouseUi->tripleClickSelectsTheWholeLine,      Enum::SelectWholeLine},
            {_mouseUi->tripleClickSelectsFromMousePosition, Enum::SelectForwardsFromCursor},
        },
    };
    setupButtonGroup(tripleClickModeOptions, profile);

    _mouseUi->openLinksByDirectClickButton->setEnabled(_mouseUi->underlineLinksButton->isChecked() || _mouseUi->underlineFilesButton->isChecked());

    _mouseUi->enableMouseWheelZoomButton->setChecked(profile->mouseWheelZoomEnabled());
    connect(_mouseUi->enableMouseWheelZoomButton, &QCheckBox::toggled, this, &terminal::EditProfileDialog::toggleMouseWheelZoom);
}

void EditProfileDialog::setupAdvancedPage(const Profile::Ptr &profile)
{
    const auto options = QVector<BooleanOption>{
        {
            _advancedUi->enableBlinkingTextButton, Profile::BlinkingTextEnabled,
            SLOT(toggleBlinkingText(bool))
        },
        {
            _advancedUi->enableFlowControlButton, Profile::FlowControlEnabled,
            SLOT(toggleFlowControl(bool))
        },
        {
            _appearanceUi->enableBlinkingCursorButton, Profile::BlinkingCursorEnabled,
            SLOT(toggleBlinkingCursor(bool))
        },
        {
            _advancedUi->enableBidiRenderingButton, Profile::BidiRenderingEnabled,
            SLOT(togglebidiRendering(bool))
        },
        {
            _advancedUi->enableReverseUrlHints, Profile::ReverseUrlHints,
            SLOT(toggleReverseUrlHints(bool))
        }
    };
    setupCheckBoxes(options, profile);

    // Setup the URL hints modifier checkboxes
    {
        auto modifiers = profile->property<int>(Profile::UrlHintsModifiers);
        _advancedUi->urlHintsModifierShift->setChecked((modifiers &Qt::ShiftModifier) != 0u);
        _advancedUi->urlHintsModifierCtrl->setChecked((modifiers &Qt::ControlModifier) != 0u);
        _advancedUi->urlHintsModifierAlt->setChecked((modifiers &Qt::AltModifier) != 0u);
        _advancedUi->urlHintsModifierMeta->setChecked((modifiers &Qt::MetaModifier) != 0u);
        connect(_advancedUi->urlHintsModifierShift, &QCheckBox::toggled, this, &EditProfileDialog::updateUrlHintsModifier);
        connect(_advancedUi->urlHintsModifierCtrl, &QCheckBox::toggled, this, &EditProfileDialog::updateUrlHintsModifier);
        connect(_advancedUi->urlHintsModifierAlt, &QCheckBox::toggled, this, &EditProfileDialog::updateUrlHintsModifier);
        connect(_advancedUi->urlHintsModifierMeta, &QCheckBox::toggled, this, &EditProfileDialog::updateUrlHintsModifier);
    }
}

void EditProfileDialog::setDefaultCodec(QTextCodec *codec)
{
    QString name = QString::fromLocal8Bit(codec->name());

    updateTempProfileProperty(Profile::DefaultEncoding, name);
}

void EditProfileDialog::wordCharactersChanged(const QString &text)
{
    updateTempProfileProperty(Profile::WordCharacters, text);
}

void EditProfileDialog::togglebidiRendering(bool enable)
{
    updateTempProfileProperty(Profile::BidiRenderingEnabled, enable);
}

void EditProfileDialog::toggleUnderlineLinks(bool enable)
{
    updateTempProfileProperty(Profile::UnderlineLinksEnabled, enable);

    bool enableClick = _mouseUi->underlineFilesButton->isChecked() || enable;
    _mouseUi->openLinksByDirectClickButton->setEnabled(enableClick);
}

void EditProfileDialog::toggleUnderlineFiles(bool enable)
{
    updateTempProfileProperty(Profile::UnderlineFilesEnabled, enable);

    bool enableClick = _mouseUi->underlineLinksButton->isChecked() || enable;
    _mouseUi->openLinksByDirectClickButton->setEnabled(enableClick);
}

void EditProfileDialog::toggleCtrlRequiredForDrag(bool enable)
{
    updateTempProfileProperty(Profile::CtrlRequiredForDrag, enable);
}

void EditProfileDialog::toggleDropUrlsAsText(bool enable)
{
    updateTempProfileProperty(Profile::DropUrlsAsText, enable);
}

void EditProfileDialog::toggleOpenLinksByDirectClick(bool enable)
{
    updateTempProfileProperty(Profile::OpenLinksByDirectClickEnabled, enable);
}

void EditProfileDialog::toggleCopyTextAsHTML(bool enable)
{
    updateTempProfileProperty(Profile::CopyTextAsHTML, enable);
}

void EditProfileDialog::toggleCopyTextToClipboard(bool enable)
{
    updateTempProfileProperty(Profile::AutoCopySelectedText, enable);
}

void EditProfileDialog::toggleTrimLeadingSpacesInSelectedText(bool enable)
{
    updateTempProfileProperty(Profile::TrimLeadingSpacesInSelectedText, enable);
}

void EditProfileDialog::toggleTrimTrailingSpacesInSelectedText(bool enable)
{
    updateTempProfileProperty(Profile::TrimTrailingSpacesInSelectedText, enable);
}

void EditProfileDialog::pasteFromX11Selection()
{
    updateTempProfileProperty(Profile::MiddleClickPasteMode, Enum::PasteFromX11Selection);
}

void EditProfileDialog::pasteFromClipboard()
{
    updateTempProfileProperty(Profile::MiddleClickPasteMode, Enum::PasteFromClipboard);
}

void EditProfileDialog::TripleClickModeChanged(int newValue)
{
    updateTempProfileProperty(Profile::TripleClickMode, newValue);
}

void EditProfileDialog::updateUrlHintsModifier(bool)
{
    Qt::KeyboardModifiers modifiers;
    if (_advancedUi->urlHintsModifierShift->isChecked()) {
        modifiers |= Qt::ShiftModifier;
    }
    if (_advancedUi->urlHintsModifierCtrl->isChecked()) {
        modifiers |= Qt::ControlModifier;
    }
    if (_advancedUi->urlHintsModifierAlt->isChecked()) {
        modifiers |= Qt::AltModifier;
    }
    if (_advancedUi->urlHintsModifierMeta->isChecked()) {
        modifiers |= Qt::MetaModifier;
    }
    updateTempProfileProperty(Profile::UrlHintsModifiers, int(modifiers));
}

void EditProfileDialog::toggleReverseUrlHints(bool enable)
{
    updateTempProfileProperty(Profile::ReverseUrlHints, enable);
}

void EditProfileDialog::toggleBlinkingText(bool enable)
{
    updateTempProfileProperty(Profile::BlinkingTextEnabled, enable);
}

void EditProfileDialog::toggleFlowControl(bool enable)
{
    updateTempProfileProperty(Profile::FlowControlEnabled, enable);
}

ColorSchemeViewDelegate::ColorSchemeViewDelegate(QObject *parent) :
    QAbstractItemDelegate(parent)
{
}

void ColorSchemeViewDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const auto *scheme = index.data(Qt::UserRole + 1).value<const ColorScheme *>();
    QFont profileFont = index.data(Qt::UserRole + 2).value<QFont>();
    Q_ASSERT(scheme);
    if (scheme == nullptr) {
        return;
    }

    painter->setRenderHint(QPainter::Antialiasing);

    // Draw background
    QStyle *style = option.widget != nullptr ? option.widget->style() : QApplication::style();
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, option.widget);

    // Draw name
    QPalette::ColorRole textColor = ((option.state & QStyle::State_Selected) != 0)
                                    ? QPalette::HighlightedText : QPalette::Text;
    painter->setPen(option.palette.color(textColor));
    painter->setFont(option.font);

    // Determine width of sample text using profile's font
    const QString sampleText = i18n("AaZz09...");
    QFontMetrics profileFontMetrics(profileFont);
    const int sampleTextWidth = profileFontMetrics.boundingRect(sampleText).width();

    painter->drawText(option.rect.adjusted(sampleTextWidth + 15, 0, 0, 0),
                      Qt::AlignLeft | Qt::AlignVCenter,
                      index.data(Qt::DisplayRole).toString());

    // Draw the preview
    const int x = option.rect.left();
    const int y = option.rect.top();

    QRect previewRect(x + 4, y + 4, sampleTextWidth + 8, option.rect.height() - 8);

    bool transparencyAvailable = true;//TODO: compositing detection KWindowSystem::compositingActive();

    if (transparencyAvailable) {
        painter->save();
        QColor color = scheme->backgroundColor();
        color.setAlphaF(scheme->opacity());
        painter->setPen(Qt::NoPen);
        painter->setCompositionMode(QPainter::CompositionMode_Source);
        painter->setBrush(color);
        painter->drawRect(previewRect);
        painter->restore();
    } else {
        painter->setPen(Qt::NoPen);
        painter->setBrush(scheme->backgroundColor());
        painter->drawRect(previewRect);
    }

    // draw color scheme name using scheme's foreground color
    QPen pen(scheme->foregroundColor());
    painter->setPen(pen);

    // TODO: respect antialias setting
    painter->setFont(profileFont);
    painter->drawText(previewRect, Qt::AlignCenter, sampleText);
}

QSize ColorSchemeViewDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex & /*index*/) const
{
    const int width = 200;
    const int margin = 5;
    const int colorWidth = width / TABLE_COLORS;
    const int heightForWidth = (colorWidth * 2) + option.fontMetrics.height() + margin;

    // temporary
    return {width, heightForWidth};
}
