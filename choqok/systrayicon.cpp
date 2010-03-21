/*
    This file is part of Choqok, the KDE micro-blogging client

    Copyright (C) 2008-2010 Mehrdad Momeny <mehrdad.momeny@gmail.com>

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of
    the License or (at your option) version 3 or any later version
    accepted by the membership of KDE e.V. (or its successor approved
    by the membership of KDE e.V.), which shall act as a proxy
    defined in Section 14 of version 3 of the license.


    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, see http://www.gnu.org/licenses/

*/
#include "systrayicon.h"
#include <kaction.h>
#include <kactioncollection.h>
#include <KDE/KLocale>
#include <QPainter>
#include <KColorScheme>
#include <QTimer>
#include "accountmanager.h"
#include <kglobalsettings.h>
#include <mediamanager.h>
#include <QWheelEvent>
#include <KDebug>

SysTrayIcon::SysTrayIcon( Choqok::UI::MainWindow* parent )
: KStatusNotifierItem( parent ), _mainwin(parent)
{
    kDebug();
    unread = 0;
//     setAssociatedWidget(parent);
    setCategory(ApplicationStatus);
    setStandardActionsEnabled(false);
    setStatus(Active);
    m_defaultIcon = _mainwin->windowIcon().pixmap( 22 );
    this->setIconByPixmap( _mainwin->windowIcon() );

    isIconChanged = false;
}

SysTrayIcon::~SysTrayIcon()
{
    kDebug();
}

// bool SysTrayIcon::event(QEvent* event)
// {
//   if(event->type() == QEvent::Wheel) {
//     QWheelEvent * wheel = static_cast<QWheelEvent*>(event);
//     emit wheelEvent(*wheel);
//     return true;
//   }
//   return false;
// }

void SysTrayIcon::resetUnreadCount()
{
    updateUnreadCount(-unread);
}

void SysTrayIcon::updateUnreadCount( int changeOfUnreadPosts )
{
    kDebug();
    unread += changeOfUnreadPosts;

    this->setToolTip( "choqok", i18n("Choqok"), i18np( "1 unread post", "%1 unread posts", unread ) );

    if ( unread <= 0 ) {
        setIconByPixmap( QIcon(m_defaultIcon) );
        isBaseIconChanged = true;
        unread = 0;
    } else {
        // adapted from KMSystemTray::updateCount()
        int oldWidth = m_defaultIcon.size().width();

        if ( oldWidth == 0 )
            return;

        QString countStr = QString::number( unread );
        QFont f = KGlobalSettings::generalFont();
        f.setBold( true );

        float pointSize = f.pointSizeF();
        QFontMetrics fm( f );
        int w = fm.width( countStr );
        if ( w > ( oldWidth - 2 ) ) {
            pointSize *= float( oldWidth - 2 ) / float( w );
            f.setPointSizeF( pointSize );
        }

        // overlay
        QImage overlayImg = m_defaultIcon.toImage().copy();
        QPainter p( &overlayImg );
        p.setFont( f );
        KColorScheme scheme( QPalette::Active, KColorScheme::View );

        fm = QFontMetrics( f );
        QRect boundingRect = fm.tightBoundingRect( countStr );
        boundingRect.adjust( 0, 0, 0, 2 );
        boundingRect.setHeight( qMin( boundingRect.height(), oldWidth ) );
        boundingRect.moveTo(( oldWidth - boundingRect.width() ) / 2,
                            (( oldWidth - boundingRect.height() ) / 2 ) - 1 );
        p.setOpacity( 0.7 );
        p.setBrush( scheme.background( KColorScheme::LinkBackground ) );
        p.setPen( scheme.background( KColorScheme::LinkBackground ).color() );
        p.drawRoundedRect( boundingRect, 2.0, 2.0 );

        p.setBrush( Qt::NoBrush );
        p.setPen( QColor( 0, 0, 0 ) );
        p.setOpacity( 1.0 );
        p.drawText( overlayImg.rect(), Qt::AlignCenter, countStr );

        setIconByPixmap( QIcon( QPixmap::fromImage( overlayImg ) ) );
        isBaseIconChanged = true;
    }
}

void SysTrayIcon::setTimeLineUpdatesEnabled( bool isEnabled )
{
    if ( isEnabled ) {
        setToolTip( "choqok", i18n( "Choqok" ), QString() );
        m_defaultIcon = _mainwin->windowIcon().pixmap( 22 );
    } else {
        setToolTip( "choqok", i18n( "Choqok - Disabled" ), QString() );
        ///Generating new Icon:
        m_defaultIcon = Choqok::MediaManager::convertToGrayScale(m_defaultIcon);
    }
    setIconByPixmap( QIcon( KIcon( m_defaultIcon ) ) );
    updateUnreadCount( 0 );
}

void SysTrayIcon::slotJobDone( Choqok::JobResult result )
{
    kDebug();
    if ( !isIconChanged ) {
        prevIcon = iconPixmap();
        isIconChanged = true;
    }
    isBaseIconChanged = false;
    if ( result == Choqok::Success ) {
        setIconByName( "dialog-ok" );
    } else {
        setIconByName( "dialog-error" );
    }
    QTimer::singleShot( 5000, this, SLOT( slotRestoreIcon() ) );
}

void SysTrayIcon::slotRestoreIcon()
{
    if ( !isBaseIconChanged ) {
        setIconByPixmap( prevIcon );
    }
    isIconChanged = false;
}

int SysTrayIcon::unreadCount() const
{
    return unread;
}

#include "systrayicon.moc"
