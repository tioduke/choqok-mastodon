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

#include "imagepreview.h"

#include <QTimer>

#include <KPluginFactory>

#include "choqokuiglobal.h"
#include "mediamanager.h"
#include "postwidget.h"
#include "textbrowser.h"

K_PLUGIN_FACTORY_WITH_JSON(ImagePreviewFactory, "choqok_imagepreview.json",
                           registerPlugin < ImagePreview > ();)

const QRegExp ImagePreview::mYFrogRegExp(QLatin1String("(http://yfrog.[^\\s<>\"]+[^!,\\.\\s<>'\\\"\\]])"));
const QRegExp ImagePreview::mImgLyRegExp(QLatin1String("(http://img.ly/[^\\s<>\"]+[^!,\\.\\s<>'\"\\]])"));
const QRegExp ImagePreview::mTwitgooRegExp(QLatin1String("(http://(([a-zA-Z0-9]+\\.)?)twitgoo.com/[^\\s<>\"]+[^!,\\.\\s<>'\"\\]])"));
const QRegExp ImagePreview::mPumpIORegExp(QLatin1String("(https://([a-zA-Z0-9]+\\.)?[a-zA-Z0-9]+\\.[a-zA-Z]+/uploads/\\w+/\\d{4}/\\d{1,2}/\\d{1,2}/\\w+)(\\.[a-zA-Z]{3,4})"));

ImagePreview::ImagePreview(QObject *parent, const QList< QVariant > &)
    : Choqok::Plugin(QLatin1String("choqok_imagepreview"), parent), state(Stopped)
{
    connect(Choqok::UI::Global::SessionManager::self(),
            SIGNAL(newPostWidgetAdded(Choqok::UI::PostWidget*,Choqok::Account*,QString)),
            this,
            SLOT(slotAddNewPostWidget(Choqok::UI::PostWidget*)));
}

ImagePreview::~ImagePreview()
{

}

void ImagePreview::slotAddNewPostWidget(Choqok::UI::PostWidget *newWidget)
{
    postsQueue.enqueue(newWidget);
    if (state == Stopped) {
        state = Running;
        QTimer::singleShot(1000, this, SLOT(startParsing()));
    }
}

void ImagePreview::startParsing()
{
    int i = 8;
    while (!postsQueue.isEmpty() && i > 0) {
        parse(postsQueue.dequeue());
        --i;
    }

    if (postsQueue.isEmpty()) {
        state = Stopped;
    } else {
        QTimer::singleShot(500, this, SLOT(startParsing()));
    }
}

void ImagePreview::parse(Choqok::UI::PostWidget *postToParse)
{
    if (!postToParse) {
        return;
    }
    int pos = 0;
    QStringList yfrogRedirectList;
    QStringList ImgLyRedirectList;
    QStringList TwitgooRedirectList;
    QStringList PumpIORedirectList;
    QString content = postToParse->currentPost()->content;

    //YFrog: http://code.google.com/p/imageshackapi/wiki/YFROGurls
    //       http://code.google.com/p/imageshackapi/wiki/YFROGthumbnails
    pos = 0;
    while ((pos = mYFrogRegExp.indexIn(content, pos)) != -1) {
        pos += mYFrogRegExp.matchedLength();
        yfrogRedirectList << mYFrogRegExp.cap(0);
    }
    for (const QString &url: yfrogRedirectList) {
//         if( url.endsWith('j') || url.endsWith('p') || url.endsWith('g') ) //To check if it's Image or not!
        connect(Choqok::MediaManager::self(),
                SIGNAL(imageFetched(QString,QPixmap)),
                SLOT(slotImageFetched(QString,QPixmap)));
        QString yfrogThumbnailUrl = url + QLatin1String(".th.jpg");
        mParsingList.insert(yfrogThumbnailUrl, postToParse);
        mBaseUrlMap.insert(yfrogThumbnailUrl, url);
        Choqok::MediaManager::self()->fetchImage(yfrogThumbnailUrl, Choqok::MediaManager::Async);
    }

    //Img.ly; http://img.ly/api/docs
    pos = 0;
    while ((pos = mImgLyRegExp.indexIn(content, pos)) != -1) {
        pos += mImgLyRegExp.matchedLength();
        ImgLyRedirectList << mImgLyRegExp.cap(0);
    }
    for (const QString &url: ImgLyRedirectList) {
        connect(Choqok::MediaManager::self(),
                SIGNAL(imageFetched(QString,QPixmap)),
                SLOT(slotImageFetched(QString,QPixmap)));
        QString ImgLyUrl = QStringLiteral("http://img.ly/show/thumb%1").arg(QString(url).remove(QLatin1String("http://img.ly")));
        mParsingList.insert(ImgLyUrl, postToParse);
        mBaseUrlMap.insert(ImgLyUrl, url);
        Choqok::MediaManager::self()->fetchImage(ImgLyUrl, Choqok::MediaManager::Async);
    }

    //Twitgoo; http://twitgoo.com/docs/TwitgooHelp.htm
    pos = 0;
    while ((pos = mTwitgooRegExp.indexIn(content, pos)) != -1) {
        pos += mTwitgooRegExp.matchedLength();
        TwitgooRedirectList << mTwitgooRegExp.cap(0);
    }
    for (const QString &url: TwitgooRedirectList) {
        connect(Choqok::MediaManager::self(),
                SIGNAL(imageFetched(QString,QPixmap)),
                SLOT(slotImageFetched(QString,QPixmap)));
        QString TwitgooUrl = url + QLatin1String("/thumb");
        mParsingList.insert(TwitgooUrl, postToParse);
        mBaseUrlMap.insert(TwitgooUrl, url);
        Choqok::MediaManager::self()->fetchImage(TwitgooUrl, Choqok::MediaManager::Async);
    }

    //PumpIO
    pos = 0;
    QString baseUrl;
    QString imageExtension;
    while ((pos = mPumpIORegExp.indexIn(content, pos)) != -1) {
        pos += mPumpIORegExp.matchedLength();
        PumpIORedirectList << mPumpIORegExp.cap(0);
        baseUrl = mPumpIORegExp.cap(1);
        imageExtension = mPumpIORegExp.cap(mPumpIORegExp.capturedTexts().length() - 1);
    }
    for (const QString &url: PumpIORedirectList) {
        connect(Choqok::MediaManager::self(), SIGNAL(imageFetched(QString,QPixmap)),
                SLOT(slotImageFetched(QString,QPixmap)));
        const QString pumpIOUrl = baseUrl + QLatin1String("_thumb") + imageExtension;
        mParsingList.insert(pumpIOUrl, postToParse);
        mBaseUrlMap.insert(pumpIOUrl, url);
        Choqok::MediaManager::self()->fetchImage(pumpIOUrl, Choqok::MediaManager::Async);
    }
}

void ImagePreview::slotImageFetched(const QString &remoteUrl, const QPixmap &pixmap)
{
    Choqok::UI::PostWidget *postToParse = mParsingList.take(remoteUrl);
    QString baseUrl = mBaseUrlMap.take(remoteUrl);
    if (!postToParse) {
        return;
    }
    QString content = postToParse->content();
    QUrl imgU(remoteUrl);
    imgU.setScheme(QLatin1String("img"));
//     imgUrl.replace("http://","img://");
    QPixmap pix = pixmap;
    if (pixmap.width() > 200) {
        pix = pixmap.scaledToWidth(200);
    } else if (pixmap.height() > 200) {
        pix = pixmap.scaledToHeight(200);
    }
    postToParse->mainWidget()->document()->addResource(QTextDocument::ImageResource, imgU, pix);
    content.replace(QRegExp(QLatin1Char('>') + baseUrl + QLatin1Char('<')), QStringLiteral("><img align='left' src='")
                    + imgU.toDisplayString() + QStringLiteral("' /><"));
    postToParse->setContent(content);
}

#include "imagepreview.moc"
