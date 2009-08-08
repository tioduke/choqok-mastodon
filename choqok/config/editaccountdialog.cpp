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

#include "editaccountdialog.h"
#include <klocalizedstring.h>
#include <account.h>
#include <accountmanager.h>
#include <KMessageBox>
#include <editaccountwidget.h>

EditAccountDialog::EditAccountDialog(ChoqokEditAccountWidget* editWidget, QWidget* parent, Qt::WFlags flags)
        : KDialog(parent, flags), widget(editWidget)
{
    if(!widget) {
        this->deleteLater();
        return;
    }
    setMainWidget(widget);
    setCaption(i18n("Edit account"));
}

EditAccountDialog::~EditAccountDialog()
{

}

void EditAccountDialog::closeEvent(QCloseEvent* e)
{
    KDialog::closeEvent(e);
}

void EditAccountDialog::slotButtonClicked(int button)
{
    if(button == KDialog::Ok) {
        if( widget->validateData() )
            if( Choqok::Account *acc = widget->apply() ) {
                if( Choqok::AccountManager::self()->removeAccount(widget->previousAlias()) &&
                    Choqok::AccountManager::self()->registerAccount( acc ) ) {
                    accept();
                } else {
                    KMessageBox::detailedError( this, i18n("Account registration failed."),
                                                Choqok::AccountManager::self()->lastError() );
                }
            }
    } else {
        KDialog::slotButtonClicked(button);
    }
}

