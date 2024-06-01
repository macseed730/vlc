/*****************************************************************************
 * customwidgets.cpp: Custom widgets
 ****************************************************************************
 * Copyright (C) 2006-2011 the VideoLAN team
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "customwidgets.hpp"

#include <QtMath>  // for wheel deadzone calculation
#include <QPainter>
#include <QRect>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QApplication>
#include <vlc_actions.h>

#define SPINNER_SIZE 32

QFramelessButton::QFramelessButton( QWidget *parent )
                    : QPushButton( parent )
{
    setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Preferred );
}

void QFramelessButton::paintEvent( QPaintEvent * )
{
    QPainter painter( this );
    icon().paint(&painter, QRect( 0, 0, width(), height()));

}

QString QVLCDebugLevelSpinBox::textFromValue( int v ) const
{
    QString const texts[] = {
    /* Note that min level 0 is 'errors' in Qt Ui
       FIXME: fix debug levels accordingly to documentation */
    /*  qtr("infos"),*/
        qtr("errors"),
        qtr("warnings"),
        qtr("debug")
    };
    if ( v < 0 ) v = 0;
    if ( v >= 2 ) v = 2;

    return QString( "%1 (%2)" ).arg( v ).arg( texts[v] );
}

VLCQDial::VLCQDial( QWidget *parent ) : QDial( parent )
{

}

void VLCQDial::paintEvent( QPaintEvent *event )
{
    QDial::paintEvent( event );
    QPainter painter( this );
    painter.setPen( QPen( palette().color( QPalette::WindowText ) ) );
    float radius = 0.5 * 0.707106 * qMin( size().width(), size().height() );
    painter.drawText( QRectF( rect().center().x() + radius,
                              rect().center().y() + radius,
                              size().width(),
                              size().height() ),
                      0, QString::number( value() ), 0 );
    painter.end();
}

/***************************************************************************
 * Hotkeys converters
 ***************************************************************************/
int qtKeyModifiersToVLC( const QInputEvent& e )
{
    int i_keyModifiers = 0;
    if( e.modifiers() & Qt::ShiftModifier ) i_keyModifiers |= KEY_MODIFIER_SHIFT;
    if( e.modifiers() & Qt::AltModifier ) i_keyModifiers |= KEY_MODIFIER_ALT;
    if( e.modifiers() & Qt::ControlModifier ) i_keyModifiers |= KEY_MODIFIER_CTRL;
    if( e.modifiers() & Qt::MetaModifier ) i_keyModifiers |= KEY_MODIFIER_META;
    return i_keyModifiers;
}

typedef struct
{
    int      qt;
    uint32_t vlc;
} vlc_qt_key_t;

static const vlc_qt_key_t keys[] =
{
    { Qt::Key_Escape,                KEY_ESC },
    { Qt::Key_Tab,                   '\t', },
    // Qt::Key_Backtab
    { Qt::Key_Backspace,             '\b' },
    { Qt::Key_Return,                '\r' },
    { Qt::Key_Enter,                 '\r' }, // numeric pad
    { Qt::Key_Insert,                KEY_INSERT },
    { Qt::Key_Delete,                KEY_DELETE },
    { Qt::Key_Pause,                 KEY_PAUSE },
    { Qt::Key_Print,                 KEY_PRINT },
    // Qt::Key_SysReq
    // Qt::Key_Clear
    { Qt::Key_Home,                  KEY_HOME },
    { Qt::Key_End,                   KEY_END },
    { Qt::Key_Left,                  KEY_LEFT },
    { Qt::Key_Up,                    KEY_UP },
    { Qt::Key_Right,                 KEY_RIGHT },
    { Qt::Key_Down,                  KEY_DOWN },
    { Qt::Key_PageUp,                KEY_PAGEUP },
    { Qt::Key_PageDown,              KEY_PAGEDOWN },
    // Qt::Key_Shift
    // Qt::Key_Control
    // Qt::Key_Meta
    // Qt::Key_Alt
    // Qt::Key_CapsLock
    // Qt::Key_NumLock
    // Qt::Key_ScrollLock
    /* F1 - F35 - Qt goes to F35, VLC stops at F12 */
    { Qt::Key_F1,                    KEY_F1 },
    { Qt::Key_F2,                    KEY_F2 },
    { Qt::Key_F3,                    KEY_F3 },
    { Qt::Key_F4,                    KEY_F4 },
    { Qt::Key_F5,                    KEY_F5 },
    { Qt::Key_F6,                    KEY_F6 },
    { Qt::Key_F7,                    KEY_F7 },
    { Qt::Key_F8,                    KEY_F8 },
    { Qt::Key_F9,                    KEY_F9 },
    { Qt::Key_F10,                   KEY_F10 },
    { Qt::Key_F11,                   KEY_F11 },
    { Qt::Key_F12,                   KEY_F12 },
    { Qt::Key_F13,                   KEY_F(13) },
    { Qt::Key_F14,                   KEY_F(14) },
    { Qt::Key_F15,                   KEY_F(15) },
    { Qt::Key_F16,                   KEY_F(16) },
    { Qt::Key_F17,                   KEY_F(17) },
    { Qt::Key_F18,                   KEY_F(18) },
    { Qt::Key_F19,                   KEY_F(19) },
    { Qt::Key_F20,                   KEY_F(20) },
    { Qt::Key_F21,                   KEY_F(21) },
    { Qt::Key_F22,                   KEY_F(22) },
    { Qt::Key_F23,                   KEY_F(23) },
    { Qt::Key_F24,                   KEY_F(24) },
    { Qt::Key_F25,                   KEY_F(25) },
    { Qt::Key_F26,                   KEY_F(26) },
    { Qt::Key_F27,                   KEY_F(27) },
    { Qt::Key_F28,                   KEY_F(28) },
    { Qt::Key_F29,                   KEY_F(29) },
    { Qt::Key_F30,                   KEY_F(30) },
    { Qt::Key_F31,                   KEY_F(31) },
    { Qt::Key_F32,                   KEY_F(32) },
    { Qt::Key_F33,                   KEY_F(33) },
    { Qt::Key_F34,                   KEY_F(34) },
    { Qt::Key_F35,                   KEY_F(35) },

    // Qt::Key_Super_L
    // Qt::Key_Super_R
    { Qt::Key_Menu,                  KEY_MENU },
    // Qt::Key_Hyper_L
    // Qt::Key_Hyper_R
    // Qt::Key_Help
    // Qt::Key_Direction_L
    // Qt::Key_Direction_R

    // Qt::Key_Multi_key
    // Qt::Key_Codeinput
    // Qt::Key_SingleCandidate
    // Qt::Key_MultipleCandidate
    // Qt::Key_PreviousCandidate
    // Qt::Key_Mode_switch
    // Qt::Key_Kanji
    // Qt::Key_Muhenkan
    // Qt::Key_Henkan
    // Qt::Key_Romaji
    // Qt::Key_Hiragana
    // Qt::Key_Katakana
    // Qt::Key_Hiragana_Katakana
    // Qt::Key_Zenkaku
    // Qt::Key_Hankaku
    // Qt::Key_Zenkaku_Hankaku
    // Qt::Key_Touroku
    // Qt::Key_Massyo
    // Qt::Key_Kana_Lock
    // Qt::Key_Kana_Shift
    // Qt::Key_Eisu_Shift
    // Qt::Key_Eisu_toggle
    // Qt::Key_Hangul
    // Qt::Key_Hangul_Start
    // Qt::Key_Hangul_End
    // Qt::Key_Hangul_Hanja
    // Qt::Key_Hangul_Jamo
    // Qt::Key_Hangul_Romaja
    // Qt::Key_Hangul_Jeonja
    // Qt::Key_Hangul_Banja
    // Qt::Key_Hangul_PreHanja
    // Qt::Key_Hangul_PostHanja
    // Qt::Key_Hangul_Special
    // Qt::Key_Dead_Grave
    // Qt::Key_Dead_Acute
    // Qt::Key_Dead_Circumflex
    // Qt::Key_Dead_Tilde
    // Qt::Key_Dead_Macron
    // Qt::Key_Dead_Breve
    // Qt::Key_Dead_Abovedot
    // Qt::Key_Dead_Diaeresis
    // Qt::Key_Dead_Abovering
    // Qt::Key_Dead_Doubleacute
    // Qt::Key_Dead_Caron
    // Qt::Key_Dead_Cedilla
    // Qt::Key_Dead_Ogonek
    // Qt::Key_Dead_Iota
    // Qt::Key_Dead_Voiced_Sound
    // Qt::Key_Dead_Semivoiced_Sound
    // Qt::Key_Dead_Belowdot
    // Qt::Key_Dead_Hook
    // Qt::Key_Dead_Horn
    { Qt::Key_Back,                  KEY_BROWSER_BACK },
    { Qt::Key_Forward,               KEY_BROWSER_FORWARD },
    { Qt::Key_Stop,                  KEY_BROWSER_STOP },
    { Qt::Key_Refresh,               KEY_BROWSER_REFRESH },
    { Qt::Key_VolumeDown,            KEY_VOLUME_DOWN },
    { Qt::Key_VolumeMute,            KEY_VOLUME_MUTE },
    { Qt::Key_VolumeUp,              KEY_VOLUME_UP },
    // Qt::Key_BassBoost
    // Qt::Key_BassUp
    // Qt::Key_BassDown
    // Qt::Key_TrebleUp
    // Qt::Key_TrebleDown
    { Qt::Key_MediaPlay,             KEY_MEDIA_PLAY_PAUSE },
    { Qt::Key_MediaStop,             KEY_MEDIA_STOP },
    { Qt::Key_MediaPrevious,         KEY_MEDIA_PREV_TRACK },
    { Qt::Key_MediaNext,             KEY_MEDIA_NEXT_TRACK },
    // Qt::Key_MediaRecord
    { Qt::Key_HomePage,              KEY_BROWSER_HOME },
    { Qt::Key_Favorites,             KEY_BROWSER_FAVORITES },
    { Qt::Key_Search,                KEY_BROWSER_SEARCH },
    // Qt::Key_Standby
    // Qt::Key_OpenUrl
    // Qt::Key_LaunchMail
    // Qt::Key_LaunchMedia
    /* Qt::Key_Launch0 through Qt::Key_LaunchF */
    /* ... */
    { Qt::Key_Reload,                KEY_BROWSER_REFRESH },
};

static int keycmp( const void *a, const void *b )
{
    const int *q = (const int *)a;
    const vlc_qt_key_t *m = (const vlc_qt_key_t *)b;

    return *q - m->qt;
}

int qtEventToVLCKey( QKeyEvent *e )
{
    int qtk = e->key();
    uint32_t i_vlck = 0;

    if( qtk <= 0xff )
    {
        /* VLC and X11 use lowercase whereas Qt uses uppercase, this
         * method should be equal to towlower in case of latin1 */
        if( qtk >= 'A' && qtk <= 'Z' ) i_vlck = qtk+32;
        else if( qtk >= 0xC0 && qtk <= 0xDE && qtk != 0xD7) i_vlck = qtk+32;
        else i_vlck = qtk;
    }
    else
    {
        const vlc_qt_key_t *map;

        map = (const vlc_qt_key_t *)
              bsearch( &qtk, (const void *)keys, sizeof(keys)/sizeof(keys[0]),
                       sizeof(*keys), keycmp );
        if( map != NULL )
            i_vlck = map->vlc;
    }

    /* Handle modifiers */
    i_vlck |= qtKeyModifiersToVLC( *e );
    return i_vlck;
}

int qtWheelEventToVLCKey( const QWheelEvent& e )
{
    const qreal v_cos_deadzone = 0.45; // ~63 degrees
    const qreal h_cos_deadzone = 0.95; // ~15 degrees

    int i_vlck = qtKeyModifiersToVLC(e);  // Handle modifiers

    QPoint p = e.angleDelta();
    if (!p.isNull())
    {
        qreal cos = qFabs(p.x())/qSqrt(qPow(p.x(), 2) + qPow(p.y(), 2));

        if (cos < v_cos_deadzone)
        {
            if (p.y() > 0) i_vlck |= KEY_MOUSEWHEELUP;
            else           i_vlck |= KEY_MOUSEWHEELDOWN;
        }
        else if (cos > h_cos_deadzone)
        {
            if (p.x() > 0) i_vlck |= KEY_MOUSEWHEELLEFT;
            else           i_vlck |= KEY_MOUSEWHEELRIGHT;
        }
    }

    return i_vlck;
}

QString VLCKeyToString( unsigned val, bool locale )
{
    char *base = vlc_keycode2str (val, locale);
    if (base == NULL)
        return qfu( "" );

    QString r = qfu( base );

    free( base );
    return r;
}

/* Animated Icon implementation */
SpinningIcon::SpinningIcon( QWidget *parent ) : QLabel( parent )
{
    QList<QString> frames;
    frames << ":/misc/wait1.svg";
    frames << ":/misc/wait2.svg";
    frames << ":/misc/wait3.svg";
    frames << ":/misc/wait4.svg";
    animator = new PixmapAnimator( this, frames, SPINNER_SIZE, SPINNER_SIZE );
    connect( animator, &PixmapAnimator::pixmapReady, this, [=]( const QPixmap &pixmap ) {
        this->setPixmap( pixmap );
        this->repaint();
    } );
    setScaledContents( true );
    setFixedSize( 16, 16 );
    animator->setCurrentTime( 0 );
}

QToolButtonExt::QToolButtonExt(QWidget *parent, int ms )
    : QToolButton( parent ),
      shortClick( false ),
      longClick( false )
{
    setAutoRepeat( true );
    /* default to twice the doubleclick delay */
    setAutoRepeatDelay( ( ms > 0 )? ms : 2 * QApplication::doubleClickInterval() );
    setAutoRepeatInterval( 100 );
    connect( this, &QToolButtonExt::released, this, &QToolButtonExt::releasedSlot );
    connect( this, &QToolButtonExt::clicked, this, &QToolButtonExt::clickedSlot );
}

/* table illustrating the different scenarios and the events generated
 * ====================
 *
 *  event     isDown()
 *
 *  released  false   }
 *  clicked   false   }= short click
 *
 *  released  false    = cancelled click (mouse released outside of button area,
 *                                        before long click delay kicks in)
 *
 *  released  true    }
 *  clicked   true    }= long click (multiple of these generated)
 *  released  false    = stop long click (mouse released / moved outside of
 *                                        button area)
 * (clicked   false)   = stop long click (additional event if mouse released
 *                                        inside of button area)
 */

void QToolButtonExt::releasedSlot()
{
    if( isDown() )
    {
        // we are beginning a long click
        longClick = true;
        shortClick = false;
    }
    else
    {
        if( longClick )
        {
            // we are stopping a long click
            longClick = false;
            shortClick = false;
        }
        else
        {
            // we are generating a short click
            longClick = false;
            shortClick = true;
        }
    }
}

void QToolButtonExt::clickedSlot()
{
    if( longClick )
        emit longClicked();
    else if( shortClick )
        emit shortClicked();
}

YesNoCheckBox::YesNoCheckBox( QWidget *parent ) : QCheckBox( parent )
{
    setEnabled( false );
    setStyleSheet("\
                  QCheckBox::indicator:unchecked:hover,\
                  QCheckBox::indicator:unchecked {\
                      image: url(:/menu/clear.svg);\
                  }\
                  QCheckBox::indicator:checked:hover,\
                  QCheckBox::indicator:checked {\
                      image: url(:/menu/valid.svg);\
                  }\
        ");
}
