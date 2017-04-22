/*
    This file is part of Choqok, the KDE micro-blogging client

    Copyright (C) 2017 Andrea Scarpino <scarpino@kde.org>

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

#include "mastodonpostwidget.h"

#include <QAction>
#include <QMenu>
#include <QPushButton>

#include <KLocalizedString>

#include "mediamanager.h"
#include "textbrowser.h"

#include "mastodonaccount.h"
#include "mastodondebug.h"
#include "mastodonmicroblog.h"
#include "mastodonpost.h"

const QIcon MastodonPostWidget::unFavIcon(Choqok::MediaManager::convertToGrayScale(QIcon::fromTheme(QLatin1String("rating")).pixmap(16)));

class MastodonPostWidget::Private
{
public:
    QPushButton *btnFavorite;
};

MastodonPostWidget::MastodonPostWidget(Choqok::Account *account, Choqok::Post *post,
                                   QWidget *parent):
    PostWidget(account, post, parent), d(new Private)
{
}

MastodonPostWidget::~MastodonPostWidget()
{
    delete d;
}

QString MastodonPostWidget::generateSign()
{
    QString ss;

    MastodonPost *post = dynamic_cast<MastodonPost * >(currentPost());
    MastodonAccount *account = qobject_cast<MastodonAccount * >(currentAccount());
    MastodonMicroBlog *microblog = qobject_cast<MastodonMicroBlog * >(account->microblog());
    if (post) {
        if (post->author.userName != account->username()) {
            ss += QLatin1String("<b><a href=\"") + microblog->profileUrl(account, post->author).toDisplayString()
                  + QLatin1String("\" title=\"") + post->author.realName + QLatin1String("\">") +
                  post->author.userName + QLatin1String("</a></b> - ");
        }

        ss += QLatin1String("<a href=\"") + microblog->postUrl(account, post->author.userName,
                                                post->postId) + QLatin1String("\" title=\"") +
              post->creationDateTime.toString(Qt::DefaultLocaleLongDate)
              + QLatin1String("\">%1</a>");

        if (!post->source.isEmpty()) {
            ss += QLatin1String(" - ") + post->source;
        }

        if (!post->repeatedFromUsername.isEmpty()) {
            ss += QLatin1String(" - ");
            ss += i18n("Boosted by: ") + microblog->userNameFromAcct(post->repeatedFromUsername);
        }
    } else {
        qCDebug(CHOQOK) << "post is not a MastodonPost!";
    }

    return ss;
}

void MastodonPostWidget::initUi()
{
    Choqok::UI::PostWidget::initUi();

    if (isResendAvailable()) {
        buttons().value(QLatin1String("btnResend"))->setToolTip(i18nc("@info:tooltip", "Boost"));
    }

    d->btnFavorite = addButton(QLatin1String("btnFavorite"), i18nc("@info:tooltip", "Favourite"), QLatin1String("rating"));
    d->btnFavorite->setCheckable(true);
    connect(d->btnFavorite, SIGNAL(clicked(bool)), this, SLOT(toggleFavorite()));
    updateFavStat();
}

void MastodonPostWidget::slotResendPost()
{
    qCDebug(CHOQOK);
    setReadWithSignal();
    MastodonMicroBlog *microBlog = qobject_cast<MastodonMicroBlog *>(currentAccount()->microblog());
    microBlog->toggleReblog(currentAccount(), currentPost());
}

void MastodonPostWidget::toggleFavorite()
{
    qCDebug(CHOQOK);
    setReadWithSignal();
    MastodonMicroBlog *microBlog = qobject_cast<MastodonMicroBlog *>(currentAccount()->microblog());
    connect(microBlog, SIGNAL(favorite(Choqok::Account*,Choqok::Post*)),
            this, SLOT(slotToggleFavorite(Choqok::Account*,Choqok::Post*)));
    microBlog->toggleFavorite(currentAccount(), currentPost());
}

void MastodonPostWidget::slotToggleFavorite(Choqok::Account *, Choqok::Post *)
{
    qCDebug(CHOQOK);
    updateFavStat();
}

void MastodonPostWidget::updateFavStat()
{
    d->btnFavorite->setChecked(currentPost()->isFavorited);
    if (currentPost()->isFavorited) {
        d->btnFavorite->setIcon(QIcon::fromTheme(QLatin1String("rating")));
    } else {
        d->btnFavorite->setIcon(unFavIcon);
    }
}
