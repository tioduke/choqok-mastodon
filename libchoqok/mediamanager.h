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
#ifndef MEDIAMANAGER_H
#define MEDIAMANAGER_H

#include <QObject>
#include <QMap>
#include <QPixmap>
#include <QUrl>

#include "choqok_export.h"

namespace KIO
{
class Job;
}
class KJob;
namespace Choqok
{
/**
    @brief Media files manager!
    A simple and global way to fetch and cache images

    @author Mehrdad Momeny \<mehrdad.momeny@gmail.com\>
*/
class CHOQOK_EXPORT MediaManager : public QObject
{
    Q_OBJECT
public:
    enum ReturnMode {
        Sync = 0, Async
    };
    ~MediaManager();

    static MediaManager *self();

    /**
     * @brief Fetch an Image and cache it for later use.
     *
     * @param remoteUrl The URL of image to fetch
     * @param mode Return mode, if set to Sync and the image is not available in the cache the null pixmap will be returned.
     * if mode set to @ref Async and image is not available in the cache, the null pixmap will be returned
     * and then @ref MediaManager will fetch the image and emit @ref imageFetched() on success or
     * emit @ref fetchError() on error.
     * And if mode set to @ref Sync and image is not in the cache @ref MediaManager will not fetch it.
     *
     * @return return @ref QPixmap of requested image if exists in cache, otherwise null pixmap
     */
    QPixmap fetchImage(const QString &remoteUrl, ReturnMode mode = Sync);

    /**
     * @return KDE Default image
     */
    QPixmap &defaultImage();

    /**
     * @brief Parse a text for EmotIcons with kde default theme.
     */
    QString parseEmoticons(const QString &text);

    static QPixmap convertToGrayScale(const QPixmap &pic);

    /**
    Upload medium at @p localUrl to @p pluginId service or to last used service when @p pluginId is empty.

    @see mediumUploaded()
    @see mediumUploadFailed()
    */
    void uploadMedium(const QUrl &localUrl, const QString &pluginId = QString());

    /**
    Create and return a byte array containing a multipart/form-data to send with HTTP POST request

    Boundary is AaB03x

    @param formdata are the "form-data" parts of data.
                    This map knows as a list of name/value pairs
    @param mediaFiles are media files attached to form, each file stored in one QMap in list

    @note media file maps should contain these keys:
        name: The name of entry
        filename: the file name on server
        medium: contain the medium data loaded from disk!
        mediumType: type of medium file
    */
    static QByteArray createMultipartFormData(const QMap<QString, QByteArray> &formdata,
            const QList< QMap<QString, QByteArray> > &mediaFiles);

public Q_SLOTS:
    /**
     * @brief Clear image cache
     */
    void clearImageCache();

Q_SIGNALS:
    void fetchError(const QString &remoteUrl, const QString &errMsg);
    void imageFetched(const QString &remoteUrl, const QPixmap &pixmap);

    void mediumUploaded(const QUrl &localUrl, const QString &remoteUrl);
    void mediumUploadFailed(const QUrl &localUrl, const QString &errorMessage);

protected Q_SLOTS:
    void slotImageFetched(KJob *job);

protected:
    MediaManager();

private:
    class Private;
    Private *const d;
    static MediaManager *mSelf;
};

}
#endif
