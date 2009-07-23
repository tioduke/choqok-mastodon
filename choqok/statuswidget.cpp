/*
    This file is part of Choqok, the KDE micro-blogging client

    Copyright (C) 2008-2009 Mehrdad Momeny <mehrdad.momeny@gmail.com>

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
#include "statuswidget.h"
#include "settings.h"
#include "mediamanager.h"
#include "backend.h"
#include "accountmanager.h"
#include <KNotification>
#include <KProcess>

#include "mainwindow.h"

#include <KMenu>

#include <KDE/KLocale>
#include <KToolInvocation>
#include "identicasearch.h"
#include "twittersearch.h"
#include <KAction>
#include <KTemporaryFile>
#include "userinfowidget.h"
#include <kio/job.h>

#include "showthread.h"

static const int _15SECS = 15000;
static const int _MINUTE = 60000;
static const int _HOUR = 60*_MINUTE;

const QString StatusWidget::baseText("<table width=\"100%\"><tr><td rowspan=\"2\"\
 width=\"48\">%1</td><td><p>%2</p></td></tr><tr><td style=\"font-size:small;\" align=\"right\">%3</td></tr></table>");
const QString StatusWidget::baseStyle("QFrame.StatusWidget {border: 1px solid rgb(150,150,150);\
border-radius:5px;}\
QFrame.StatusWidget[read=false] {color: %1; background-color: %2}\
QFrame.StatusWidget[read=true] {color: %3; background-color: %4}");

QString StatusWidget::style;

const QRegExp StatusWidget::mUrlRegExp("((ftps?|https?)://[^\\s<>\"]+[^!,\\.\\s<>'\"\\)\\]])"); // "borrowed" from microblog plasmoid
const QRegExp StatusWidget::mUserRegExp("([\\s]|^)@([^\\s\\W]+)");
const QRegExp StatusWidget::mHashtagRegExp("([\\s]|^)#([^\\s\\W]+)");
const QRegExp StatusWidget::mGroupRegExp("([\\s]|^)!([^\\s\\W]+)");

void StatusWidget::setStyle(const QColor& color, const QColor& back, const QColor& read, const QColor& readBack)
{
  style = baseStyle.arg(getColorString(color),getColorString(back),getColorString(read),getColorString(readBack));
}

StatusWidget::StatusWidget( const Account *account, QWidget *parent )
        : KTextBrowser( parent ),mIsRead(true),mCurrentAccount(account),isBaseStatusShowed(false),
        isMissingStatusRequested(false)
{
    setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    setupUi();
    setOpenLinks(false);

    timer.start( _MINUTE );
    connect( &timer, SIGNAL( timeout() ), this, SLOT( updateSign() ) );
    connect(this,SIGNAL(anchorClicked(QUrl)),this,SLOT(checkAnchor(QUrl)));
}

void StatusWidget::checkAnchor(const QUrl & url)
{
    QString scheme = url.scheme();
    Account::Service s = mCurrentAccount->serviceType();
    int type = 0;
    if( scheme == "group" && ( s == Account::Identica || s == Account::Laconica ) ) {
        type = IdenticaSearch::ReferenceGroup;
    } else if(scheme == "tag") {
        switch(s) {
            case Account::Identica:
            case Account::Laconica:
                type = IdenticaSearch::ReferenceHashtag;
                break;
            case Account::Twitter:
                type = TwitterSearch::ReferenceHashtag;
                break;
        }
    } else if(scheme == "user") {
        KMenu menu;
        KAction * info = new KAction( KIcon("user-identity"), i18n("Who is %1", url.host()), &menu );
        KAction * from = new KAction(KIcon("edit-find-user"), i18n("From %1",url.host()),&menu);
        KAction * to = new KAction(KIcon("meeting-attending"), i18n("Replies to %1",url.host()),&menu);
        if(url.host().toLower() == mCurrentStatus.user.screenName.toLower())
            menu.addAction(info);
        menu.addAction(from);
        menu.addAction(to);
        QAction * ret;
        KAction *cont;
        switch(s) {
            case Account::Identica:
            case Account::Laconica:
                from->setData(IdenticaSearch::FromUser);
                to->setData(IdenticaSearch::ToUser);
                break;
            case Account::Twitter:
                cont = new KAction(KIcon("user-properties"),i18n("Including %1",url.host()),&menu);
                menu.addAction(cont);
                from->setData(TwitterSearch::FromUser);
                to->setData(TwitterSearch::ToUser);
                cont->setData(TwitterSearch::ReferenceUser);
                break;
        }
        ret = menu.exec(QCursor::pos());
        if(ret == 0) return;
        if(ret == info) {
            mCurrentStatus.user.isFriend = AccountManager::self()->listFriends(mCurrentAccount->alias()).contains(mCurrentStatus.user.screenName);
            showUserInformation(mCurrentStatus.user);
            return;
        }
        type = ret->data().toInt();
    } else if( scheme == "status" ) {
        if(isBaseStatusShowed) {
            updateUi();
            isBaseStatusShowed = false;
            return;
        }
        Backend *b = new Backend(new Account(*mCurrentAccount), this);
        connect( b, SIGNAL( singleStatusReceived( Status ) ),
                this, SLOT( baseStatusReceived(Status) ) );
        b->requestSingleStatus( url.host().toULongLong() );
        return;
    } else if (scheme == "thread") {
	    qulonglong status = url.host().toULongLong();
	    ShowThread *st = new ShowThread(new Account(*mCurrentAccount), status, NULL);

            connect( st, SIGNAL( forwardReply( const QString&, qulonglong, bool ) ),
                     this, SIGNAL(sigReply( const QString&, qulonglong, bool ) ) );
            connect( st, SIGNAL(forwardReTweet(const QString&)), SIGNAL(sigReTweet(const QString&)));
            connect( st, SIGNAL( forwardFavorited( qulonglong, bool ) ),
                    this, SIGNAL( sigFavorite( qulonglong, bool ) ) );
            connect (st,SIGNAL(forwardSigSearch(int,QString)),this,SIGNAL(sigSearch(int,QString)));

	    st->startPopulate();
	    st->show();
	    return;
    } else {
        if( Settings::useCustomBrowser() ) {
            QStringList args = Settings::customBrowser().split(' ');
            args.append(url.toString());
            if( KProcess::startDetached( args ) == 0 ) {
                KNotification *notif = new KNotification( "notify", this );
                notif->setText( i18n("Could not launch custom browser.\nUsing KDE default browser.") );
                notif->sendEvent();
                KToolInvocation::invokeBrowser(url.toString());
            }
        } else {
                KToolInvocation::invokeBrowser(url.toString());
        }
        return;
    }
    emit sigSearch(type,url.host());
}

void StatusWidget::setupUi()
{
    QGridLayout * buttonGrid = new QGridLayout;

    btnReply = getButton( "btnReply",i18nc( "@info:tooltip", "Reply" ), "edit-undo" );
    btnRemove = getButton( "btnRemove",i18nc( "@info:tooltip", "Remove" ), "edit-delete" );
    btnFavorite = getButton( "btnFavorite",i18nc( "@info:tooltip", "Favorite" ), "rating" );
    btnReTweet = getButton( "btnReTweet", i18nc( "@info:tooltip", "ReTweet" ), "retweet" );
    btnFavorite->setCheckable(true);

    buttonGrid->setRowStretch(0,100);
    buttonGrid->setColumnStretch(5,100);
    buttonGrid->setMargin(0);
    buttonGrid->setSpacing(0);

    buttonGrid->addWidget( btnReply, 1, 0 );
    buttonGrid->addWidget( btnRemove, 1, 1 );
    buttonGrid->addWidget( btnFavorite, 1, 2 );
    buttonGrid->addWidget( btnReTweet, 1, 3 );

    document()->addResource( QTextDocument::ImageResource, QUrl("icon://web"),
                             KIcon("applications-internet").pixmap(8) );
    document()->addResource( QTextDocument::ImageResource, QUrl("icon://thread"),
                             KIcon("go-top").pixmap(8) );
    document()->addResource( QTextDocument::ImageResource, QUrl("img://profileImage"),
                             MediaManager::self()->defaultImage() );
    mImage = "<img src=\"img://profileImage\" title=\""+ mCurrentStatus.user.name +"\" width=\"48\" height=\"48\" />";

    setLayout(buttonGrid);

    connect( btnReply, SIGNAL( clicked( bool ) ), this, SLOT( requestReply() ) );
    connect( btnFavorite, SIGNAL( clicked( bool ) ), this, SLOT( setFavorite( bool ) ) );
    connect( btnRemove, SIGNAL( clicked( bool ) ), this, SLOT( requestDestroy() ) );
    connect( btnReTweet, SIGNAL( clicked( bool ) ), this, SLOT( requestReTweet() ) );

    connect(this,SIGNAL(textChanged()),this,SLOT(setHeight()));
}

void StatusWidget::enterEvent(QEvent* event)
{
  if ( !mCurrentStatus.isDMessage )
      btnFavorite->setVisible( true );
  if ( mCurrentStatus.user.userId != mCurrentAccount->userId() )
      btnReply->setVisible( true );
  else
      btnRemove->setVisible( true );
  btnReTweet->setVisible( true );
  KTextBrowser::enterEvent(event);
}

void StatusWidget::leaveEvent(QEvent* event)
{
  btnRemove->setVisible( false );
  btnFavorite->setVisible( false );
  btnReply->setVisible( false );
  btnReTweet->setVisible( false );

  KTextBrowser::leaveEvent(event);
}

KPushButton * StatusWidget::getButton(const QString & objName, const QString & toolTip, const QString & icon)
{
    KPushButton * button = new KPushButton(KIcon(icon),QString());
    button->setObjectName(objName);
    button->setToolTip(toolTip);
    button->setIconSize(QSize(8,8));
    button->setMinimumSize(QSize(20, 20));
    button->setMaximumSize(QSize(20, 20));
    button->setFlat(true);
    button->setVisible(false);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

StatusWidget::~StatusWidget()
{
}

void StatusWidget::setFavorite( bool isFavorite )
{
    emit sigFavorite( mCurrentStatus.statusId, isFavorite );
}

Status StatusWidget::currentStatus() const
{
    return mCurrentStatus;
}

void StatusWidget::setCurrentStatus( const Status newStatus )
{
    mCurrentStatus = newStatus;
    updateUi();
}

void StatusWidget::updateUi()
{
    if ( mCurrentStatus.isDMessage ) {
        btnFavorite->setVisible( false );
    } else if ( mCurrentStatus.user.userId == mCurrentAccount->userId() ) {
        btnReply->setVisible( false );
    } else {
        btnRemove->setVisible( false );
    }
    mStatus = prepareStatus(mCurrentStatus.content);
    checkForTwitPicImages(mCurrentStatus.content);
    mSign = generateSign();
    setDirection();
    setUserImage();
    setUiStyle();
    updateSign();
    updateFavoriteUi();
}

void StatusWidget::setDirection()
{
    QString txt = mCurrentStatus.content;
    if(txt.startsWith('@'))
        txt.remove(QRegExp("(^)@([^\\s\\W]+)"));
    if(txt.startsWith('#'))
        txt.remove(QRegExp("(^)#([^\\s\\W]+)"));
    if(txt.startsWith('!'))
        txt.remove(QRegExp("(^)!([^\\s\\W]+)"));
    txt.prepend(' ');
    if( txt.isRightToLeft() ) {
        QTextOption options(document()->defaultTextOption());
        options.setTextDirection( Qt::RightToLeft );
        document()->setDefaultTextOption(options);
    }
}

void StatusWidget::setHeight()
{
    document()->setTextWidth(width()-2);
    int h = document()->size().toSize().height()+2;
    setMinimumHeight(h);
    setMaximumHeight(h);
}

QString StatusWidget::formatDateTime( const QDateTime &time )
{
    int seconds = time.secsTo( QDateTime::currentDateTime() );
    if ( seconds <= 15 ) {
        timer.setInterval( _15SECS );
        return i18n( "Just now" );
    }

    if ( seconds <= 45 ) {
        timer.setInterval( _15SECS );
        return i18np( "1 sec ago", "%1 secs ago", seconds );
    }

    int minutes = ( seconds - 45 + 59 ) / 60;
    if ( minutes <= 45 ) {
        timer.setInterval( _MINUTE );
        return i18np( "1 min ago", "%1 mins ago", minutes );
    }

    int hours = ( seconds - 45 * 60 + 3599 ) / 3600;
    if ( hours <= 18 ) {
        timer.setInterval( _MINUTE * 15 );
        return i18np( "1 hour ago", "%1 hours ago", hours );
    }

    timer.setInterval( _HOUR );
    int days = ( seconds - 18 * 3600 + 24 * 3600 - 1 ) / ( 24 * 3600 );
    return i18np( "1 day ago", "%1 days ago", days );
}

void StatusWidget::requestReply()
{
    kDebug();
    emit sigReply( mCurrentStatus.user.screenName, mCurrentStatus.statusId, currentStatus().isDMessage );
}

QString StatusWidget::generateSign()
{
    QString sign;
    sign = "<b><a href='user://"+mCurrentStatus.user.screenName+"' title=\"" +
    mCurrentStatus.user.description + "\">" + mCurrentStatus.user.screenName +
    "</a> <a href=\"" + mCurrentAccount->homepage() + mCurrentStatus.user.screenName + "\" title=\"" +
                    mCurrentStatus.user.description + "\"><img src=\"icon://web\" /></a> - </b>";
    sign += "<a href=\"" + mCurrentAccount->statusUrl( mCurrentStatus.statusId, mCurrentStatus.user.screenName ) +
    "\" title=\"" + mCurrentStatus.creationDateTime.toString() + "\">%1</a>";
    if ( mCurrentStatus.isDMessage ) {
        if( mCurrentStatus.replyToUserId == mCurrentAccount->userId() ) {
            sign.prepend( "From " );
        } else {
            sign.prepend( "To " );
        }
    } else {
        if( !mCurrentStatus.source.isNull() )
            sign += " - " + mCurrentStatus.source;
        if ( mCurrentStatus.replyToStatusId > 0 ) {
            QString showConMsg = i18n("Show Conversation");
            QString link = mCurrentAccount->statusUrl( mCurrentStatus.replyToStatusId,
                                                       mCurrentStatus.replyToUserScreenName );
            QString threadlink = "thread://" + QString::number(mCurrentStatus.statusId);
            sign += " - <a href='status://" + QString::number( mCurrentStatus.replyToStatusId ) + "'>" +
            i18n("in reply to")+ "</a>&nbsp;<a href=\"" + link + "\"><img src=\"icon://web\" /></a> " +
            "<a title=\""+ showConMsg +"\" href=\"" + threadlink + "\"><img src=\"icon://thread\" /></a>";
        }
    }
    return sign;
}

void StatusWidget::updateSign()
{
    setHtml( baseText.arg( mImage, mStatus, mSign.arg( formatDateTime( mCurrentStatus.creationDateTime ) ) ) );
}

void StatusWidget::requestDestroy()
{
    emit sigDestroy( mCurrentStatus.statusId );
}

void StatusWidget::requestReTweet()
{
    QChar re(0x267B);
    QString text =  QString(re) + " @" + mCurrentStatus.user.screenName + ": " + mCurrentStatus.content;
    emit sigReTweet( text );
}

void StatusWidget::checkForTwitPicImages(const QString &status)
{
    ///Check for twitpic images
    if(Settings::loadTwitpicImages()) {
        QRegExp twitPicUrlRegExp("(http://twitpic.com/[^\\s<>\"]+[^!,\\.\\s<>'\"\\]])");
        if( status.indexOf(twitPicUrlRegExp) != -1 ) {
            twitpicPageUrl = twitPicUrlRegExp.cap(0);
            KUrl tempUrl( twitpicPageUrl );
            if( tempUrl.isValid() ) {
                kDebug()<<"Twitpic detected! "<<tempUrl.prettyUrl();
                twitpicImageUrl = QString( "http://twitpic.com/show/mini%1" ).arg(tempUrl.path(KUrl::RemoveTrailingSlash));
                connect( MediaManager::self(), SIGNAL( avatarFetched( const QString &, const QPixmap & ) ),
                         this, SLOT(twitpicImageFetched( const QString&, const QPixmap&)) );
                connect( MediaManager::self(), SIGNAL(fetchError( const QString&, const QString&)),
                        this, SLOT(twitpicImageFailed( const QString&, const QString&)) );
                MediaManager::self()->getAvatarDownloadAsyncIfNotExist( twitpicImageUrl );
            }
        }
    }
}

QString StatusWidget::prepareStatus( const QString &text )
{
    if( !isMissingStatusRequested && text.isEmpty() && ( mCurrentAccount->serviceType() == Account::Identica ||
        mCurrentAccount->serviceType() == Account::Laconica ) ){
        Backend *b = new Backend(new Account(*mCurrentAccount), this);
        connect(b, SIGNAL(singleStatusReceived( Status )),
                 this, SLOT(missingStatusReceived( Status )));
        b->requestSingleStatus(mCurrentStatus.statusId);
        isMissingStatusRequested = true;
        return text;
    }
    QString status = text;

    status.replace( '<', "&lt;" );
    status.replace( '>', "&gt;" );
    status.replace( " www.", " http://www." );
    if ( status.startsWith( QLatin1String("www.") ) ) 
        status.prepend( "http://" );

    // This next block replaces 301 redirects with an appropriate title
    int pos = 0;
    QStringList redirectList;
    while ((pos = mUrlRegExp.indexIn(mCurrentStatus.content, pos)) != -1) {
        pos += mUrlRegExp.matchedLength();
        if( mUrlRegExp.matchedLength() < 31 )//Most of shortenned URLs have less than 30 Chars!
            redirectList << mUrlRegExp.cap(0);
    }

    status.replace(mUrlRegExp,"<a href='\\1' title='\\1'>\\1</a>");

    foreach(QString url, redirectList) {
        KIO::TransferJob *job = KIO::mimetype( url, KIO::HideProgressInfo );
        if ( !job ) {
            kDebug() << "Cannot create a http header request!";
            break;
        }
        connect( job, SIGNAL( permanentRedirection( KIO::Job*, KUrl, KUrl ) ), this, SLOT( slot301Redirected ( KIO::Job *, KUrl, KUrl ) ) );
        job->start();
    }


    if(Settings::isSmiliesEnabled())
        status = MediaManager::self()->parseEmoticons(status);

    status.replace(mUserRegExp,"\\1@<a href='user://\\2'>\\2</a> <a href='"+ mCurrentAccount->homepage() + 
    "\\2'><img src=\"icon://web\" /></a>");
    if ( mCurrentAccount->serviceType() == Account::Identica ||
        mCurrentAccount->serviceType() == Account::Laconica ) {
        status.replace(mGroupRegExp,"\\1!<a href='group://\\2'>\\2</a> <a href='"+ mCurrentAccount->homepage() + 
        "group/\\2'><img src=\"icon://web\" /></a>");
        status.replace(mHashtagRegExp,"\\1#<a href='tag://\\2'>\\2</a> <a href='"+ mCurrentAccount->homepage() + 
        "tag/\\1'><img src=\"icon://web\" /></a>");
      } else {
          status.replace(mHashtagRegExp,"\\1#<a href='tag://\\2'>\\2</a>");
    }
    return status;
}

QString StatusWidget::getColorString(const QColor& color)
{
  return "rgb(" + QString::number(color.red()) + ',' + QString::number(color.green()) + ',' +
  QString::number(color.blue()) + ')';
}

void StatusWidget::setUnread( Notify notifyType )
{
    mIsRead = false;

    if ( notifyType == WithNotify ) {
        QString name = mCurrentStatus.user.screenName;
        QString msg = mCurrentStatus.content;
	QPixmap icon = document()->resource(QTextDocument::ImageResource,QUrl("img://profileImage")).value<QPixmap>();
//         QPixmap * iconUrl = MediaManager::self()->getImageLocalPathIfExist( mCurrentStatus.user.profileImageUrl );
    if ( Settings::notifyType() == SettingsBase::KNotify ) {
        KNotification *notify = new KNotification( "new-status-arrived", parentWidget() );
        notify->setText( QString( "<qt><b>" + name + ":</b><br/>" + msg + "</qt>" ) );
        if(!icon.isNull()) notify->setPixmap( icon );
        notify->setFlags( KNotification::RaiseWidgetOnActivation | KNotification::Persistent );
        notify->setActions( i18n( "Reply" ).split( ',' ) );
        connect( notify, SIGNAL( action1Activated() ), this , SLOT( requestReply() ) );
        notify->sendEvent();
        QTimer::singleShot( Settings::notifyInterval()*1000, notify, SLOT( close() ) );
    } else if ( Settings::notifyType() == SettingsBase::LibNotify ) {
    QString iconArg;
    KTemporaryFile tmp;
    if(!icon.isNull()) {
        tmp.setSuffix(".png");
        if(tmp.open()) {
            icon.save(&tmp,"PNG");
            iconArg = " -i "+tmp.fileName();
        }
        }
            QString libnotifyCmd = QString( "notify-send -t " ) +
            QString::number( Settings::notifyInterval() * 1000 ) + iconArg + QString( " -u low \"" ) +
            name + QString( "\" \"" ) + msg + QString( "\"" );
            KProcess::execute( libnotifyCmd );
        }
    }
}

void StatusWidget::setRead(bool read)
{
    mIsRead = read;
    setUiStyle();
}

void StatusWidget::setUiStyle()
{
    setStyleSheet( style );
}

void StatusWidget::updateFavoriteUi()
{
  btnFavorite->setChecked(mCurrentStatus.isFavorited);
}

bool StatusWidget::isRead() const
{
    return mIsRead;
}

void StatusWidget::setUserImage()
{
    connect( MediaManager::self(), SIGNAL( avatarFetched( const QString &, const QPixmap & ) ),
             this, SLOT(userAvatarFetched(const QString&, const QPixmap&)) );
    connect( MediaManager::self(), SIGNAL(fetchError( const QString&, const QString&)),
             this, SLOT(fetchAvatarError( const QString&, const QString&)) );
    MediaManager::self()->getAvatarDownloadAsyncIfNotExist( mCurrentStatus.user.profileImageUrl );
}

void StatusWidget::userAvatarFetched( const QString & remotePath, const QPixmap & pixmap )
{
    if ( remotePath == KUrl(mCurrentStatus.user.profileImageUrl).url(KUrl::RemoveTrailingSlash) ) {
        QString url = "img://profileImage";
        document()->addResource( QTextDocument::ImageResource, url, pixmap );
        updateSign();
        disconnect( MediaManager::self(), SIGNAL( avatarFetched( const QString &, const QPixmap & ) ),
                    this, SLOT( userAvatarFetched( const QString&, const QPixmap& ) ) );
        disconnect( MediaManager::self(), SIGNAL(fetchError( const QString&, const QString&)),
                 this, SLOT(fetchAvatarError( const QString&, const QString&)) );
    }
}

void StatusWidget::fetchAvatarError( const QString & avatarUrl, const QString &errMsg )
{
    Q_UNUSED(errMsg);
    if( avatarUrl == KUrl(mCurrentStatus.user.profileImageUrl).url(KUrl::RemoveTrailingSlash) ){
        ///Avatar fetching is failed! but will not disconnect to get the img if it fetches later!
        QString url = "img://profileImage";
        document()->addResource( QTextDocument::ImageResource, url, KIcon("image-missing").pixmap(48) );
        updateSign();
    }
}

void StatusWidget::missingStatusReceived( Status status )
{
    if( mCurrentStatus.statusId == mCurrentStatus.statusId ){
        mCurrentStatus = status;
        updateUi();
        sender()->deleteLater();
    }
}

void StatusWidget::resizeEvent(QResizeEvent* event)
{
  setHeight();
  KTextBrowser::resizeEvent(event);
}

void StatusWidget::baseStatusReceived( Status status )
{
    if(isBaseStatusShowed)
        return;
    isBaseStatusShowed = true;
    QString color;
    if( Settings::isCustomUi() ) {
        color = Settings::defaultForeColor().lighter().name();
    } else {
        color = this->palette().dark().color().name();
    }
    QString baseStatusText = "<p style=\"margin-top:10px; margin-bottom:10px; margin-left:20px;\
    margin-right:20px; -qt-block-indent:0; text-indent:0px\"><span style=\" color:" + color + ";\">";
    baseStatusText += "<b><a href='user://"+ status.user.screenName +"'>" + status.user.screenName + "</a> :</b> ";
    baseStatusText += prepareStatus( status.content ) + "</p>";
    mStatus.prepend( baseStatusText );
    updateSign();
}

void StatusWidget::twitpicImageFetched( const QString &imageUrl, const QPixmap & pixmap )
{
    if(imageUrl == twitpicImageUrl) {
        kDebug();
        disconnect( MediaManager::self(), SIGNAL( avatarFetched( const QString &, const QPixmap & ) ),
                 this, SLOT(twitpicImageFetched( const QString&, const QPixmap&)) );
        disconnect( MediaManager::self(), SIGNAL(fetchError( const QString&, const QString&)),
                this, SLOT(twitpicImageFailed( const QString&, const QString&)) );
        QString url = "img://twitpicImage";
        document()->addResource( QTextDocument::ImageResource, url, pixmap );
        QRegExp rx( '>' + twitpicPageUrl + '<');
        mStatus.replace(rx, "><img src=\"img://twitpicImage\" /><");
        updateSign();
    }
}

void StatusWidget::twitpicImageFailed( const QString &imageUrl, const QString &errMsg )
{
    if(imageUrl == twitpicImageUrl) {
        kDebug()<<errMsg;
        disconnect( MediaManager::self(), SIGNAL( avatarFetched( const QString &, const QPixmap & ) ),
                 this, SLOT(twitpicImageFetched( const QString&, const QPixmap&)) );
        disconnect( MediaManager::self(), SIGNAL(fetchError( const QString&, const QString&)),
                this, SLOT(twitpicImageFailed( const QString&, const QString&)) );
    }
}

void StatusWidget::showUserInformation(const User& user)
{
    Backend *b = new Backend(new Account(*mCurrentAccount), this);
    connect( b, SIGNAL( singleStatusReceived( Status ) ),
             this, SLOT( baseStatusReceived(Status) ) );

    UserInfoWidget *widget = new UserInfoWidget(user, this);

    connect(widget, SIGNAL(sigFollowUser(QString)), b, SLOT(slotAddFriend(QString)));
    connect(b, SIGNAL(friendAdded(QString)), this, SLOT(slotFriendAdded(QString)));
    widget->show(QCursor::pos());
}

void StatusWidget::slot301Redirected(KIO::Job *job, const KUrl &fromUrl, const KUrl &toUrl)
{
    job->kill();
    kDebug()<<"Got redirect: "<<fromUrl<<toUrl;
    mStatus.replace(QRegExp("title='" + fromUrl.url() + "'"), "title='" + toUrl.url() + "'");
    updateSign();
}

void StatusWidget::slotFriendAdded(const QString &username)
{
    QString message = "You are now following " + username;
    kDebug()<<message;

    if ( Settings::notifyType() == SettingsBase::LibNotify ) {//Libnotify!
        QString msg = message;
        msg = msg.replace( "<br/>", "\n" );
        QString libnotifyCmd = QString( "notify-send -t " ) + QString::number( Settings::notifyInterval() * 1000 )
                               + QString( " -u low \"" ) + "notification" + QString( "\" \"" ) + msg + QString( "\"" );
        QProcess::execute( libnotifyCmd );
    } else {//KNotify
        KNotification *notif = new KNotification( "notify", this );
        notif->setText( message );
        notif->setFlags( KNotification::RaiseWidgetOnActivation | KNotification::Persistent );
        notif->sendEvent();
        QTimer::singleShot( Settings::notifyInterval()*1000, notif, SLOT( close() ) );
    }
}

#include "statuswidget.moc"