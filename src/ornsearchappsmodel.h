#ifndef ORNSEARCHAPPSMODEL_H
#define ORNSEARCHAPPSMODEL_H

#include "ornabstractappsmodel.h"

class OrnSearchAppsModel : public OrnAbstractAppsModel
{
    Q_OBJECT
    Q_PROPERTY(QString searchKey READ searchKey WRITE setSearchKey NOTIFY searchKeyChanged)

public:
    explicit OrnSearchAppsModel(QObject *parent = 0);

    QString searchKey() const;
    void setSearchKey(const QString &searchKey);

signals:
    void searchKeyChanged();
    void resultsUpdated();

private:
    QString mSearchKey;

    // QAbstractItemModel interface
public:
    void fetchMore(const QModelIndex &parent);
};

#endif // ORNSEARCHAPPSMODEL_H
