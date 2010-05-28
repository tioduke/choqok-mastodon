/*
    This file is part of Choqok, the KDE micro-blogging client

    Copyright (C) 2010 Mehrdad Momeny <mehrdad.momeny@gmail.com>

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

#ifndef FILTERSETTINGS_H
#define FILTERSETTINGS_H

#include <QObject>
#include <QMap>
#include "filter.h"

class Filter;

class FilterSettings : public QObject
{

public:
    static FilterSettings *self();
    virtual ~FilterSettings();

    QList<Filter*> availableFilters() const;
    void setFilters(QList<Filter*> filters);

    QMap<Filter::FilterField, QString> filterFieldName();
    QMap<Filter::FilterType, QString> filterTypeName();
private:
    FilterSettings();
    static FilterSettings *_self;

    void reloadFilters();
    void saveFilters();
    QList<Filter*> _filters;
    QMap<Filter::FilterField, QString> _filterFieldName;
    QMap<Filter::FilterType, QString> _filterTypeName;
};

#endif // FILTERSETTINGS_H