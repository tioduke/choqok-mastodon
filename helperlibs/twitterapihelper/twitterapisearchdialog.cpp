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

#include "twitterapisearchdialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <KLocalizedString>

#include "twitterapiaccount.h"
#include "twitterapidebug.h"
#include "twitterapimicroblog.h"
#include "twitterapisearch.h"

class TwitterApiSearchDialog::Private
{
public:
    Private(TwitterApiAccount *theAccount)
        : account(theAccount)
    {
        qCDebug(CHOQOK);
        mBlog = qobject_cast<TwitterApiMicroBlog *>(account->microblog());
        if (!mBlog) {
            qCCritical(CHOQOK) << "microblog is not a TwitterApiMicroBlog";
        }
        Q_ASSERT(mBlog);
    }
    QComboBox *searchTypes;
    QLineEdit *searchQuery;
    TwitterApiAccount *account;
    TwitterApiMicroBlog *mBlog;
};

TwitterApiSearchDialog::TwitterApiSearchDialog(TwitterApiAccount *theAccount, QWidget *parent)
    : QDialog(parent), d(new Private(theAccount))
{
    qCDebug(CHOQOK);
    setWindowTitle(i18nc("@title:window", "Search"));
    setAttribute(Qt::WA_DeleteOnClose);
    createUi();
    d->searchQuery->setFocus();
    connect(d->searchTypes, SIGNAL(currentIndexChanged(int)), SLOT(slotSearchTypeChanged(int)));
}

TwitterApiSearchDialog::~TwitterApiSearchDialog()
{
    delete d;
}

void TwitterApiSearchDialog::createUi()
{
    qCDebug(CHOQOK);
    QWidget *wd = new QWidget(this);

    QVBoxLayout *layout = new QVBoxLayout;
    setLayout(layout);
    layout->addWidget(wd);

    d->searchTypes = new QComboBox(wd);
    fillSearchTypes();
    qCDebug(CHOQOK);
    layout->addWidget(d->searchTypes);

    QHBoxLayout *queryLayout = new QHBoxLayout;
    layout->addLayout(queryLayout);
    QLabel *lblType = new QLabel(i18nc("Search query", "Query:"), wd);
    lblType->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    queryLayout->addWidget(lblType);

    d->searchQuery = new QLineEdit(this);
    queryLayout->addWidget(d->searchQuery);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QPushButton *okButton = buttonBox->button(QDialogButtonBox::Ok);
    okButton->setDefault(true);
    okButton->setShortcut(Qt::CTRL | Qt::Key_Return);
    okButton->setText(i18nc("@action:button", "Search"));
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
    layout->addWidget(buttonBox);

    adjustSize();
}

void TwitterApiSearchDialog::fillSearchTypes()
{
    qCDebug(CHOQOK);
    QMap<int, QPair<QString, bool> > searchTypes = d->mBlog->searchBackend()->getSearchTypes();
    int count = searchTypes.count();
    for (int i = 0; i < count; ++i) {
        d->searchTypes->insertItem(i, searchTypes[i].first);
    }
}

void TwitterApiSearchDialog::accept()
{
    bool isB = d->mBlog->searchBackend()->getSearchTypes()[d->searchTypes->currentIndex()].second;
    SearchInfo info(d->account, d->searchQuery->text(), d->searchTypes->currentIndex(), isB);
    d->mBlog->searchBackend()->requestSearchResults(info);
    QDialog::accept();
}

void TwitterApiSearchDialog::slotSearchTypeChanged(int)
{
    d->searchQuery->setFocus();
}

