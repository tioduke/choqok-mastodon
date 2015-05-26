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

#include "laconicasearch.h"

#include <QDomElement>

#include <KIO/StoredTransferJob>
#include <KLocalizedString>

#include "choqokbehaviorsettings.h"

#include "twitterapiaccount.h"

#include "laconicadebug.h"

const QRegExp LaconicaSearch::m_rId(QLatin1String("tag:.+,[\\d-]+:(\\d+)"));
const QRegExp LaconicaSearch::mIdRegExp(QLatin1String("(?:user|(?:.*notice))/([0-9]+)"));

LaconicaSearch::LaconicaSearch(QObject *parent): TwitterApiSearch(parent)
{
    qCDebug(CHOQOK);
    mSearchCode[ReferenceGroup] = QLatin1Char('!');
    mSearchCode[ToUser] = QLatin1Char('@');
    mSearchCode[FromUser].clear();
    mSearchCode[ReferenceHashtag] = QLatin1Char('#');

    mSearchTypes[ReferenceHashtag].first = i18nc("Dents are Identica posts", "Dents Including This Hashtag");
    mSearchTypes[ReferenceHashtag].second = true;

    mSearchTypes[ReferenceGroup].first = i18nc("Dents are Identica posts", "Dents Including This Group");
    mSearchTypes[ReferenceGroup].second = false;

    mSearchTypes[FromUser].first = i18nc("Dents are Identica posts", "Dents From This User");
    mSearchTypes[FromUser].second = false;

    mSearchTypes[ToUser].first = i18nc("Dents are Identica posts", "Dents To This User");
    mSearchTypes[ToUser].second = false;

}

LaconicaSearch::~LaconicaSearch()
{

}

QUrl LaconicaSearch::buildUrl(const SearchInfo &searchInfo,
                              QString sinceStatusId, uint count, uint page)
{
    qCDebug(CHOQOK);

    QString formattedQuery;
    switch (searchInfo.option) {
    case ToUser:
        formattedQuery = searchInfo.query + QLatin1String("/replies/rss");
        break;
    case FromUser:
        formattedQuery = searchInfo.query + QLatin1String("/rss");
        break;
    case ReferenceGroup:
        formattedQuery = QLatin1String("group/") + searchInfo.query + QLatin1String("/rss");
        break;
    case ReferenceHashtag:
        formattedQuery = searchInfo.query;
        break;
    default:
        formattedQuery = searchInfo.query + QLatin1String("/rss");
        break;
    };

    QUrl url;
    TwitterApiAccount *theAccount = qobject_cast<TwitterApiAccount *>(searchInfo.account);
    Q_ASSERT(theAccount);
    if (searchInfo.option == ReferenceHashtag) {
        url = theAccount->apiUrl();
        url = url.adjusted(QUrl::StripTrailingSlash);
        url.setPath(url.path() + QLatin1String("/search.atom"));
        QUrlQuery urlQuery;
        urlQuery.addQueryItem(QLatin1String("q"), formattedQuery);
        if (!sinceStatusId.isEmpty()) {
            urlQuery.addQueryItem(QLatin1String("since_id"), sinceStatusId);
        }
        int cntStr = Choqok::BehaviorSettings::countOfPosts();
        if (count && count <= 100) {
            cntStr =  count;
        }
        urlQuery.addQueryItem(QLatin1String("rpp"), QString::number(cntStr));
        if (page > 1) {
            urlQuery.addQueryItem(QLatin1String("page"), QString::number(page));
        }
        url.setQuery(urlQuery);
    } else {
        url = QUrl(theAccount->apiUrl().url().remove(QLatin1String("/api"), Qt::CaseInsensitive));
        url = url.adjusted(QUrl::StripTrailingSlash);
        url.setPath(url.path() + QLatin1Char('/') + (formattedQuery));
    }
    return url;
}

void LaconicaSearch::requestSearchResults(const SearchInfo &searchInfo,
        const QString &sinceStatusId,
        uint count, uint page)
{
    qCDebug(CHOQOK);
    QUrl url = buildUrl(searchInfo, sinceStatusId, count, page);
    qCDebug(CHOQOK) << url;
    KIO::StoredTransferJob *job = KIO::storedGet(url, KIO::Reload, KIO::HideProgressInfo);
    if (!job) {
        qCCritical(CHOQOK) << "Cannot create an http GET request!";
        return;
    }
    mSearchJobs[job] = searchInfo;
    connect(job, SIGNAL(result(KJob*)), this, SLOT(searchResultsReturned(KJob*)));
    job->start();
}

void LaconicaSearch::searchResultsReturned(KJob *job)
{
    qCDebug(CHOQOK);
    if (job == 0) {
        qCDebug(CHOQOK) << "job is a null pointer";
        Q_EMIT error(i18n("Unable to fetch search results."));
        return;
    }

    SearchInfo info = mSearchJobs.take(job);

    if (job->error()) {
        qCCritical(CHOQOK) << "Error: " << job->errorString();
        Q_EMIT error(i18n("Unable to fetch search results: %1", job->errorString()));
        return;
    }
    KIO::StoredTransferJob *jj = qobject_cast<KIO::StoredTransferJob *>(job);
    QList<Choqok::Post *> postsList;
    if (info.option == ReferenceHashtag) {
        postsList = parseAtom(jj->data());
    } else {
        postsList = parseRss(jj->data());
    }

    qCDebug(CHOQOK) << "Emiting searchResultsReceived()";
    Q_EMIT searchResultsReceived(info, postsList);
}

QString LaconicaSearch::optionCode(int option)
{
    return mSearchCode[option];
}

QList< Choqok::Post * > LaconicaSearch::parseAtom(const QByteArray &buffer)
{
    QDomDocument document;
    QList<Choqok::Post *> statusList;

    document.setContent(buffer);

    QDomElement root = document.documentElement();

    if (root.tagName() != QLatin1String("feed")) {
        qCDebug(CHOQOK) << "There is no feed element in Atom feed " << buffer.data();
        return statusList;
    }

    QDomNode node = root.firstChild();
    QString timeStr;
    while (!node.isNull()) {
        if (node.toElement().tagName() != QLatin1String("entry")) {
            node = node.nextSibling();
            continue;
        }

        QDomNode entryNode = node.firstChild();
        Choqok::Post *status = new Choqok::Post;
        status->isPrivate = false;

        while (!entryNode.isNull()) {
            QDomElement elm = entryNode.toElement();
            if (elm.tagName() == QLatin1String("id")) {
                // Fomatting example: "tag:search.twitter.com,2005:1235016836"
                QString id;
                if (m_rId.exactMatch(elm.text())) {
                    id = m_rId.cap(1);
                }
                /*                sscanf( qPrintable( elm.text() ),
                "tag:search.twitter.com,%*d:%d", &id);*/
                status->postId = id;
            } else if (elm.tagName() == QLatin1String("published")) {
                // Formatting example: "2009-02-21T19:42:39Z"
                // Need to extract date in similar fashion to dateFromString
                int year, month, day, hour, minute, second;
                sscanf(qPrintable(elm.text()),
                       "%d-%d-%dT%d:%d:%d%*s", &year, &month, &day, &hour, &minute, &second);
                QDateTime recognized(QDate(year, month, day), QTime(hour, minute, second));
                recognized.setTimeSpec(Qt::UTC);
                status->creationDateTime = recognized;
            } else if (elm.tagName() == QLatin1String("title")) {
                status->content = elm.text();
            } else if (elm.tagName() == QLatin1String("link")) {
                if (elm.attribute(QLatin1String("rel")) == QLatin1String("related")) {
                    status->author.profileImageUrl = elm.attribute(QLatin1String("href"));
                } else if (elm.attribute(QLatin1String("rel")) == QLatin1String("alternate")) {
                    status->link = elm.attribute(QLatin1String("href"));
                }
            } else if (elm.tagName() == QLatin1String("author")) {
                QDomNode userNode = entryNode.firstChild();
                while (!userNode.isNull()) {
                    if (userNode.toElement().tagName() == QLatin1String("name")) {
                        QString fullName = userNode.toElement().text();
                        int bracketPos = fullName.indexOf(QLatin1String(" "), 0);
                        QString screenName = fullName.left(bracketPos);
                        QString name = fullName.right(fullName.size() - bracketPos - 2);
                        name.chop(1);
                        status->author.realName = name;
                        status->author.userName = screenName;
                    }
                    userNode = userNode.nextSibling();
                }
            } else if (elm.tagName() == QLatin1String("twitter:source")) {
                status->source = QUrl::fromPercentEncoding(elm.text().toLatin1());
            }
            entryNode = entryNode.nextSibling();
        }
        status->isFavorited = false;
        statusList.insert(0, status);
        node = node.nextSibling();
    }
    return statusList;
}

QList< Choqok::Post * > LaconicaSearch::parseRss(const QByteArray &buffer)
{
    qCDebug(CHOQOK);
    QDomDocument document;
    QList<Choqok::Post *> statusList;

    document.setContent(buffer);

    QDomElement root = document.documentElement();

    if (root.tagName() != QLatin1String("rdf:RDF")) {
        qCDebug(CHOQOK) << "There is no rdf:RDF element in RSS feed " << buffer.data();
        return statusList;
    }

    QDomNode node = root.firstChild();
    QString timeStr;
    while (!node.isNull()) {
        if (node.toElement().tagName() != QLatin1String("item")) {
            node = node.nextSibling();
            continue;
        }

        Choqok::Post *status = new Choqok::Post;

        QDomAttr statusIdAttr = node.toElement().attributeNode(QLatin1String("rdf:about"));
        QString statusId;
        if (mIdRegExp.exactMatch(statusIdAttr.value())) {
            statusId = mIdRegExp.cap(1);
        }

        status->postId = statusId;

        QDomNode itemNode = node.firstChild();

        while (!itemNode.isNull()) {
            if (itemNode.toElement().tagName() == QLatin1String("title")) {
                QString content = itemNode.toElement().text();

                int nameSep = content.indexOf(QLatin1Char(':'), 0);
                QString screenName = content.left(nameSep);
                QString statusText = content.right(content.size() - nameSep - 2);

                status->author.userName = screenName;
                status->content = statusText;
            } else if (itemNode.toElement().tagName() == QLatin1String("dc:date")) {
                int year, month, day, hour, minute, second;
                sscanf(qPrintable(itemNode.toElement().text()),
                       "%d-%d-%dT%d:%d:%d%*s", &year, &month, &day, &hour, &minute, &second);
                QDateTime recognized(QDate(year, month, day), QTime(hour, minute, second));
                recognized.setTimeSpec(Qt::UTC);
                status->creationDateTime = recognized;
            } else if (itemNode.toElement().tagName() == QLatin1String("dc:creator")) {
                status->author.realName = itemNode.toElement().text();
            } else if (itemNode.toElement().tagName() == QLatin1String("sioc:reply_of")) {
                QDomAttr userIdAttr = itemNode.toElement().attributeNode(QLatin1String("rdf:resource"));
                QString id;
                if (mIdRegExp.exactMatch(userIdAttr.value())) {
                    id = mIdRegExp.cap(1);
                }
                status->replyToPostId = id;
            } else if (itemNode.toElement().tagName() == QLatin1String("statusnet:postIcon")) {
                QDomAttr imageAttr = itemNode.toElement().attributeNode(QLatin1String("rdf:resource"));
                status->author.profileImageUrl = imageAttr.value();
            } else if (itemNode.toElement().tagName() == QLatin1String("link")) {
//                 QDomAttr imageAttr = itemNode.toElement().attributeNode( "rdf:resource" );
                status->link = itemNode.toElement().text();
            } else if (itemNode.toElement().tagName() == QLatin1String("sioc:has_discussion")) {
                status->conversationId = itemNode.toElement().attributeNode(QLatin1String("rdf:resource")).value();
            }

            itemNode = itemNode.nextSibling();
        }

        status->isPrivate = false;
        status->isFavorited = false;
        statusList.insert(0, status);
        node = node.nextSibling();
    }

    return statusList;
}

