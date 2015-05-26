/*
This file is part of Choqok, the KDE micro-blogging client

Copyright (C) 2008-2012 Mehrdad Momeny <mehrdad.momeny@gmail.com>

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

#include "laconicamicroblog.h"

#include <QMimeDatabase>

#include <KIO/StoredTransferJob>
#include <KJobWidgets>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KMessageBox>

#include "account.h"
#include "accountmanager.h"
#include "choqokappearancesettings.h"
#include "composerwidget.h"
#include "editaccountwidget.h"
#include "mediamanager.h"
#include "microblogwidget.h"
#include "postwidget.h"
#include "timelinewidget.h"

#include "twitterapimicroblogwidget.h"
#include "twitterapipostwidget.h"
#include "twitterapitimelinewidget.h"

#include "laconicaaccount.h"
#include "laconicacomposerwidget.h"
#include "laconicadebug.h"
#include "laconicaeditaccount.h"
#include "laconicapostwidget.h"
#include "laconicasearch.h"

K_PLUGIN_FACTORY_WITH_JSON(LaconicaFactory, "choqok_laconica.json",
                           registerPlugin < LaconicaMicroBlog > ();)

LaconicaMicroBlog::LaconicaMicroBlog(QObject *parent, const QVariantList &)
    : TwitterApiMicroBlog(QLatin1String("choqok_laconica"), parent), friendsPage(1)
{
    qCDebug(CHOQOK);
    setServiceName(QLatin1String("GNU social"));
    mTimelineInfos[QLatin1String("ReTweets")]->name = i18nc("Timeline name", "Repeated");
    mTimelineInfos[QLatin1String("ReTweets")]->description = i18nc("Timeline description", "Your posts that were repeated by others");
}

LaconicaMicroBlog::~LaconicaMicroBlog()
{
    qCDebug(CHOQOK);
}

Choqok::Account *LaconicaMicroBlog::createNewAccount(const QString &alias)
{
    LaconicaAccount *acc = qobject_cast<LaconicaAccount *>(Choqok::AccountManager::self()->findAccount(alias));
    if (!acc) {
        return new LaconicaAccount(this, alias);
    } else {
        return 0;
    }
}

ChoqokEditAccountWidget *LaconicaMicroBlog::createEditAccountWidget(Choqok::Account *account, QWidget *parent)
{
    qCDebug(CHOQOK);
    LaconicaAccount *acc = qobject_cast<LaconicaAccount *>(account);
    if (acc || !account) {
        return new LaconicaEditAccountWidget(this, acc, parent);
    } else {
        qCDebug(CHOQOK) << "Account passed here is not a LaconicaAccount!";
        return 0L;
    }
}

Choqok::UI::MicroBlogWidget *LaconicaMicroBlog::createMicroBlogWidget(Choqok::Account *account, QWidget *parent)
{
    return new TwitterApiMicroBlogWidget(account, parent);
}

Choqok::UI::TimelineWidget *LaconicaMicroBlog::createTimelineWidget(Choqok::Account *account,
        const QString &timelineName, QWidget *parent)
{
    return new TwitterApiTimelineWidget(account, timelineName, parent);
}

Choqok::UI::PostWidget *LaconicaMicroBlog::createPostWidget(Choqok::Account *account,
        Choqok::Post *post, QWidget *parent)
{
    return new LaconicaPostWidget(account, post, parent);
}

Choqok::UI::ComposerWidget *LaconicaMicroBlog::createComposerWidget(Choqok::Account *account, QWidget *parent)
{
    return new LaconicaComposerWidget(account, parent);
}

Choqok::Post *LaconicaMicroBlog::readPost(Choqok::Account *account, const QVariantMap &var, Choqok::Post *post)
{
    if (!post) {
        qCCritical(CHOQOK) << "post is NULL!";
        return 0;
    }

    post = TwitterApiMicroBlog::readPost(account, var, post);

    post->link = var[QLatin1String("external_url")].toString();

    return post;
}

QString LaconicaMicroBlog::profileUrl(Choqok::Account *account, const QString &username) const
{
    TwitterApiAccount *acc = qobject_cast<TwitterApiAccount *>(account);
    if (username.contains(QLatin1Char('@'))) {
        QStringList lst = username.split(QLatin1Char('@'), QString::SkipEmptyParts);
        if (lst.count() == 2) {
            if (lst[1].endsWith(QString::fromLatin1(".status.net"))) {
                return QString::fromLatin1("http://").arg(lst[1]);
            } else {
                return QString::fromLatin1("http://%1/%2").arg(lst[1]).arg(lst[0]);
            }
        }
    }
    if (acc) {
        return QString(acc->homepageUrl().toDisplayString() + QLatin1Char('/') + username) ;
    } else {
        return QString();
    }
}

QString LaconicaMicroBlog::postUrl(Choqok::Account *account,  const QString &username,
                                   const QString &postId) const
{
    Q_UNUSED(username)
    TwitterApiAccount *acc = qobject_cast<TwitterApiAccount *>(account);
    if (acc) {
        QUrl url(acc->homepageUrl());
        url.setPath(url.path() + QString::fromLatin1("/notice/%1").arg(postId));
        return QString(url.toDisplayString());
    } else {
        return QString();
    }
}

TwitterApiSearch *LaconicaMicroBlog::searchBackend()
{
    if (!mSearchBackend) {
        mSearchBackend = new LaconicaSearch(this);
    }
    return mSearchBackend;
}

void LaconicaMicroBlog::createPostWithAttachment(Choqok::Account *theAccount, Choqok::Post *post,
        const QString &mediumToAttach)
{
    if (mediumToAttach.isEmpty()) {
        TwitterApiMicroBlog::createPost(theAccount, post);
    } else {
        QString tmp;
        QUrl picUrl(mediumToAttach);
        KIO::StoredTransferJob *picJob = KIO::storedGet(picUrl, KIO::Reload, KIO::HideProgressInfo);
        picJob->exec();
        if (picJob->error()) {
            qCCritical(CHOQOK) << "Job error: " << picJob->errorString();
            KMessageBox::detailedError(Choqok::UI::Global::mainWindow(),
                                       i18n("Uploading medium failed: cannot read the medium file."),
                                       picJob->errorString());
            return;
        }
        const QByteArray picData = picJob->data();
        if (picData.count() == 0) {
            qCCritical(CHOQOK) << "Cannot read the media file, please check if it exists.";
            KMessageBox::error(Choqok::UI::Global::mainWindow(),
                               i18n("Uploading medium failed: cannot read the medium file."));
            return;
        }
        ///Documentation: http://identi.ca/notice/17779990
        TwitterApiAccount *account = qobject_cast<TwitterApiAccount *>(theAccount);
        QUrl url = account->apiUrl();
        url.setPath(url.path() + QLatin1String("/statuses/update.xml"));
        const QMimeDatabase db;
        QByteArray fileContentType = db.mimeTypeForUrl(picUrl).name().toUtf8();

        QMap<QString, QByteArray> formdata;
        formdata[QLatin1String("status")] = post->content.toUtf8();
        formdata[QLatin1String("in_reply_to_status_id")] = post->replyToPostId.toLatin1();
        formdata[QLatin1String("source")] = "choqok";

        QMap<QString, QByteArray> mediafile;
        mediafile[QLatin1String("name")] = "media";
        mediafile[QLatin1String("filename")] = picUrl.fileName().toUtf8();
        mediafile[QLatin1String("mediumType")] = fileContentType;
        mediafile[QLatin1String("medium")] = picData;
        QList< QMap<QString, QByteArray> > listMediafiles;
        listMediafiles.append(mediafile);

        QByteArray data = Choqok::MediaManager::createMultipartFormData(formdata, listMediafiles);

        KIO::StoredTransferJob *job = KIO::storedHttpPost(data, url, KIO::HideProgressInfo) ;
        if (!job) {
            qCCritical(CHOQOK) << "Cannot create a http POST request!";
            return;
        }
        job->addMetaData(QStringLiteral("content-type"),
                         QStringLiteral("Content-Type: multipart/form-data; boundary=AaB03x"));
        job->addMetaData(QStringLiteral("customHTTPHeader"),
                         QStringLiteral("Authorization: ") +
                         QLatin1String(authorizationHeader(account, url, QOAuth::POST)));
        mCreatePostMap[ job ] = post;
        mJobsAccount[job] = theAccount;
        connect(job, SIGNAL(result(KJob*)),
                SLOT(slotCreatePost(KJob*)));
        job->start();
    }
}

QString LaconicaMicroBlog::generateRepeatedByUserTooltip(const QString &username)
{
    if (Choqok::AppearanceSettings::showRetweetsInChoqokWay()) {
        return i18n("Repeat of %1", username);
    } else {
        return i18n("Repeated by %1", username);
    }
}

QString LaconicaMicroBlog::repeatQuestion()
{
    return i18n("Repeat this notice?");
}

void LaconicaMicroBlog::listFriendsUsername(TwitterApiAccount *theAccount, bool active)
{
    Q_UNUSED(active);
    friendsList.clear();
    if (theAccount) {
        doRequestFriendsScreenName(theAccount, 1);
    }
}

void LaconicaMicroBlog::requestFriendsScreenName(TwitterApiAccount *theAccount, bool active)
{
    Q_UNUSED(active);
    doRequestFriendsScreenName(theAccount, 1);
}

void LaconicaMicroBlog::doRequestFriendsScreenName(TwitterApiAccount *theAccount, int page)
{
    qCDebug(CHOQOK);
    TwitterApiAccount *account = qobject_cast<TwitterApiAccount *>(theAccount);
    QUrl url = account->apiUrl();
    url = url.adjusted(QUrl::StripTrailingSlash);
    url.setPath(url.path() + (QString::fromLatin1("/statuses/friends.%1").arg(format)));
    QOAuth::ParamMap params;
    if (page > 1) {
        params.insert("page", QByteArray::number(page));
        QUrlQuery urlQuery;
        urlQuery.addQueryItem(QLatin1String("page"), QString::number(page));
        url.setQuery(urlQuery);
    }

    KIO::StoredTransferJob *job = KIO::storedGet(url, KIO::Reload, KIO::HideProgressInfo) ;
    if (!job) {
        qCDebug(CHOQOK) << "Cannot create an http GET request!";
        return;
    }
    job->addMetaData(QStringLiteral("customHTTPHeader"),
                     QStringLiteral("Authorization: ") +
                     QLatin1String(authorizationHeader(account, url, QOAuth::GET, params)));
    mJobsAccount[job] = theAccount;
    connect(job, SIGNAL(result(KJob*)), this, SLOT(slotRequestFriendsScreenName(KJob*)));
    job->start();
}

void LaconicaMicroBlog::slotRequestFriendsScreenName(KJob *job)
{
    qCDebug(CHOQOK);
    TwitterApiAccount *theAccount = qobject_cast<TwitterApiAccount *>(mJobsAccount.take(job));
    if (job->error()) {
        Q_EMIT error(theAccount, ServerError, i18n("Friends list for account %1 could not be updated:\n%2",
                     theAccount->username(), job->errorString()), Normal);
        return;
    }
    KIO::StoredTransferJob *stJob = qobject_cast<KIO::StoredTransferJob *>(job);
    QStringList newList = readUsersScreenName(theAccount, stJob->data());
    friendsList << newList;
    if (newList.count() == 100) {
        doRequestFriendsScreenName(theAccount, ++friendsPage);
    } else {
        friendsList.removeDuplicates();
        theAccount->setFriendsList(friendsList);
        Q_EMIT friendsUsernameListed(theAccount, friendsList);
    }
}

/*QStringList LaconicaMicroBlog::readUsersScreenNameFromXml(Choqok::Account* theAccount, const QByteArray& buffer)
{
    qCDebug(CHOQOK);
    QStringList list;
    QDomDocument document;
    document.setContent( buffer );
    QDomElement root = document.documentElement();

    if ( root.tagName() != "users" ) {
        QString err = checkXmlForError(buffer);
        if(!err.isEmpty()){
            Q_EMIT error(theAccount, ServerError, err, Critical);
        } else {
            err = i18n( "Retrieving the friends list failed. The data returned from the server is corrupted." );
            qCDebug(CHOQOK) << "there's no users tag in XML\t the XML is: \n" << buffer;
            Q_EMIT error(theAccount, ParsingError, err, Critical);
            list<<QString(' ');
        }
        return list;
    }
    QDomNode node = root.firstChild();
    QString timeStr;
    while ( !node.isNull() ) {
        if ( node.toElement().tagName() != "user" ) {
            qCDebug(CHOQOK) << "there's no user tag in XML!\n"<<buffer;
            return list;
        }
        QDomNode node2 = node.firstChild();
        while ( !node2.isNull() ) {
            if ( node2.toElement().tagName() == "screen_name" ) {
                list.append( node2.toElement().text() );
                break;
            }
            node2 = node2.nextSibling();
        }
        node = node.nextSibling();
    }
    return list;
}*/

void LaconicaMicroBlog::fetchConversation(Choqok::Account *theAccount, const QString &conversationId)
{
    qCDebug(CHOQOK);
    if (conversationId.isEmpty()) {
        return;
    }
    TwitterApiAccount *account = qobject_cast<TwitterApiAccount *>(theAccount);
    QUrl url = account->apiUrl();
    url.setPath(QString::fromLatin1("/statusnet/conversation/%1.%2").arg(conversationId).arg(format));

    KIO::StoredTransferJob *job = KIO::storedGet(url, KIO::Reload, KIO::HideProgressInfo) ;
    if (!job) {
        qCDebug(CHOQOK) << "Cannot create an http GET request!";
        return;
    }
    job->addMetaData(QStringLiteral("customHTTPHeader"),
                     QStringLiteral("Authorization: ") +
                     QLatin1String(authorizationHeader(account, url, QOAuth::GET)));
    mFetchConversationMap[ job ] = conversationId;
    mJobsAccount[ job ] = theAccount;
    connect(job, SIGNAL(result(KJob*)), this, SLOT(slotFetchConversation(KJob*)));
    job->start();
}

void LaconicaMicroBlog::slotFetchConversation(KJob *job)
{
    qCDebug(CHOQOK);
    if (!job) {
        qCWarning(CHOQOK) << "NULL Job returned";
        return;
    }
    QList<Choqok::Post *> posts;
    QString conversationId = mFetchConversationMap.take(job);
    Choqok::Account *theAccount = mJobsAccount.take(job);
    if (job->error()) {
        qCDebug(CHOQOK) << "Job Error: " << job->errorString();
        Q_EMIT error(theAccount, Choqok::MicroBlog::CommunicationError,
                     i18n("Fetching conversation failed. %1", job->errorString()), Normal);
    } else {
        KIO::StoredTransferJob *stj = qobject_cast<KIO::StoredTransferJob *> (job);
        //if(format=="json"){
        posts = readTimeline(theAccount, stj->data());
        //} else {
        //    posts = readTimelineFromXml ( theAccount, stj->data() );
        //}
        if (!posts.isEmpty()) {
            Q_EMIT conversationFetched(theAccount, conversationId, posts);
        }
    }
}

#include "laconicamicroblog.moc"
