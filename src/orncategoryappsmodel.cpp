#include "orncategoryappsmodel.h"

OrnCategoryAppsModel::OrnCategoryAppsModel(QObject *parent) :
    OrnAbstractAppsModel(true, parent)
{

}

quint32 OrnCategoryAppsModel::categoryId() const
{
    return mCategoryId;
}

void OrnCategoryAppsModel::setCategoryId(const quint32 &categoryId)
{
    if (mCategoryId != categoryId)
    {
        mCategoryId = categoryId;
        emit this->categoryIdChanged();
        this->reset();
    }
}

void OrnCategoryAppsModel::fetchMore(const QModelIndex &parent)
{
    if (parent.isValid())
    {
        return;
    }
    OrnAbstractListModel::apiCall(QStringLiteral("categories/%0/apps").arg(mCategoryId));
}
