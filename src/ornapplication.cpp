#include "ornapplication.h"
#include "orn.h"
#include "ornversion.h"
#include "orncategorylistitem.h"

#include <QUrl>
#include <QNetworkRequest>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <QTimer>

#include <QDebug>

OrnApplication::OrnApplication(QObject *parent) :
    OrnApiRequest(parent),
    mRepoStatus(OrnZypp::RepoNotInstalled),
    mAppId(0),
    mUserId(0),
    mRatingCount(0),
    mCommentsCount(0),
    mRating(0.0)
{
    connect(this, &OrnApplication::jsonReady, this, &OrnApplication::onJsonReady);

    auto ornZypp = OrnZypp::instance();
    connect(ornZypp, &OrnZypp::endRepoFetching, this, &OrnApplication::onReposChanged);
    connect(ornZypp, &OrnZypp::repoModified, this, &OrnApplication::onReposChanged);
    connect(ornZypp, &OrnZypp::availablePackagesChanged, this, &OrnApplication::onAvailablePackagesChanged);
    connect(ornZypp, &OrnZypp::installedPackagesChanged, this, &OrnApplication::onInstalledPackagesChanged);
    connect(ornZypp, &OrnZypp::updatesChanged, this, &OrnApplication::updateAvailableChanged);
    connect(ornZypp, &OrnZypp::packageInstalled, this, &OrnApplication::onPackageInstalled);
    connect(ornZypp, &OrnZypp::packageRemoved, this, &OrnApplication::onPackageRemoved);
}

quint32 OrnApplication::appId() const
{
    return mAppId;
}

void OrnApplication::setAppId(const quint32 &appId)
{
    if (mAppId != appId)
    {
        mAppId = appId;
        emit this->appIdChanged();
    }
}

bool OrnApplication::updateAvailable() const
{
    return OrnZypp::instance()->hasUpdate(mPackageName);
}

bool OrnApplication::canBeLaunched() const
{
    return !mDesktopFile.isEmpty();
}

QString OrnApplication::category() const
{
    return mCategories.empty() ? QString() :
                mCategories.last().toMap().value("name").toString();
}

void OrnApplication::update()
{
    auto url = OrnApiRequest::apiUrl(QStringLiteral("apps/%0").arg(mAppId));
    auto request = OrnApiRequest::networkRequest();
    request.setUrl(url);
    this->run(request);
}

void OrnApplication::install()
{
    if (!mAvailablePackageId.isEmpty())
    {
        OrnZypp::instance()->installPackage(mAvailablePackageId);
    }
}

void OrnApplication::remove()
{
    if (!mInstalledPackageId.isEmpty())
    {
        OrnZypp::instance()->removePackage(mAvailablePackageId);
    }
}

void OrnApplication::launch()
{
    if (mDesktopFile.isEmpty())
    {
        qDebug() << "Application" << mPackageName << "could not be launched";
        return;
    }
    qDebug() << "Launching" << mDesktopFile;
    QDesktopServices::openUrl(QUrl::fromLocalFile(mDesktopFile));
}

void OrnApplication::onJsonReady(const QJsonDocument &jsonDoc)
{
    auto jsonObject = jsonDoc.object();
    QString urlKey(QStringLiteral("url"));
    QString nameKey(QStringLiteral("name"));

    mCommentsCount = Orn::toUint(jsonObject[QStringLiteral("comments_count")]);
    mDownloadsCount = Orn::toUint(jsonObject[QStringLiteral("downloads")]);
    mTitle = Orn::toString(jsonObject[QStringLiteral("title")]);
    mIconSource = Orn::toString(jsonObject[QStringLiteral("icon")].toObject()[urlKey]);
    mPackageName = Orn::toString(jsonObject[QStringLiteral("package")].toObject()[nameKey]);
    mBody = Orn::toString(jsonObject[QStringLiteral("body")]);
    mChangelog = Orn::toString(jsonObject[QStringLiteral("changelog")]);
    mCreated = Orn::toDateTime(jsonObject[QStringLiteral("created")]);
    mUpdated = Orn::toDateTime(jsonObject[QStringLiteral("updated")]);

    auto userObject = jsonObject[QStringLiteral("user")].toObject();
    mUserId = Orn::toUint(userObject[QStringLiteral("uid")]);
    mUserName = Orn::toString(userObject[nameKey]);
    mUserIconSource = Orn::toString(userObject[QStringLiteral("picture")].toObject()[urlKey]);

    QString ratingKey(QStringLiteral("rating"));
    auto ratingObject = jsonObject[ratingKey].toObject();
    mRatingCount = Orn::toUint(ratingObject[QStringLiteral("count")]);
    mRating = ratingObject[ratingKey].toString().toFloat();

//    mTagsIds = Orn::toIntList(jsonObject[QStringLiteral("tags")]);
    auto catIds = Orn::toIntList(jsonObject[QStringLiteral("category")]);
    mCategories.clear();
    for (const auto &id : catIds)
    {
        mCategories << QVariantMap{
            { "id",   id },
            { "name", OrnCategoryListItem::categoryName(id) }
        };
    }

    QString thumbsKey(QStringLiteral("thumbs"));
    QString largeKey(QStringLiteral("large"));
    auto jsonArray = jsonObject[QStringLiteral("screenshots")].toArray();
    mScreenshots.clear();
    for (const QJsonValue &v: jsonArray)
    {
        auto o = v.toObject();
        mScreenshots << QVariantMap{
            { "url",   Orn::toString(o[urlKey]) },
            { "thumb", Orn::toString(o[thumbsKey].toObject()[largeKey]) }
        };
    }

    if (!mUserName.isEmpty())
    {
        // Generate repository name
        mRepoAlias = OrnZypp::repoNamePrefix + mUserName;
        // Check if repository is enabled
        this->onReposChanged();
    }
    else
    {
        mRepoAlias.clear();
    }

    qDebug() << "Application" << mPackageName << "information updated";
    this->onInstalledPackagesChanged();
    emit this->updated();
}

void OrnApplication::onReposChanged()
{
    if (mRepoAlias.isEmpty())
    {
        return;
    }

    auto repoStatus = OrnZypp::instance()->repoStatus(mRepoAlias);
    if (mRepoStatus != repoStatus)
    {
        qDebug() << mPackageName << "repo" << mRepoAlias << "status is" << repoStatus;
        mRepoStatus = repoStatus;
        emit this->repoStatusChanged();
        this->onAvailablePackagesChanged();
    }
}

void OrnApplication::onAvailablePackagesChanged()
{
    if (mPackageName.isEmpty() || mRepoAlias.isEmpty())
    {
        return;
    }

    auto ornZypp = OrnZypp::instance();
    if (ornZypp->isAvailable(mPackageName))
    {
        auto ids = ornZypp->availablePackages(mPackageName);
        auto newest = mAvailableVersion;
        QString newestId;
        for (const auto &id : ids)
        {
            auto idParts = id.split(QChar(';'));
            auto repo = idParts.last();
            if (repo == mRepoAlias || repo == OrnZypp::installed)
            {
                // Install packages only from current repo
                auto version = idParts[1];
                if (OrnVersion(newest) < OrnVersion(version))
                {
                    newest = version;
                    newestId = id;
                }
            }
        }
        if (mAvailableVersion != newest)
        {
            mAvailableVersion = newest;
            mAvailablePackageId = newestId;
            emit this->availableVersionChanged();
        }
    }
    else if (!mAvailableVersion.isEmpty())
    {
        mAvailableVersion.clear();
        mAvailablePackageId.clear();
        emit this->availableVersionChanged();
        emit this->appNotFound();
    }
}

void OrnApplication::onInstalledPackagesChanged()
{
    if (mPackageName.isEmpty())
    {
        return;
    }

    auto ornZypp = OrnZypp::instance();
    if (ornZypp->isInstalled(mPackageName))
    {
        auto id = ornZypp->installedPackage(mPackageName);
        auto version = PackageKit::Transaction::packageVersion(id);
        if (mInstalledVersion != version)
        {
            mInstalledVersion = version;
            mInstalledPackageId = id;
            emit this->installedVersionChanged();
        }
    }
    else if (!mInstalledVersion.isEmpty())
    {
        mInstalledVersion.clear();
        mInstalledPackageId.clear();
        emit this->installedVersionChanged();
    }
#if 0
    #FIXME disabled for now
    this->checkDesktopFile();
#endif
}

void OrnApplication::onPackageInstalled(const QString &packageId)
{
    if (PackageKit::Transaction::packageName(packageId) == mPackageName)
    {
        emit this->installed();
    }
}

void OrnApplication::onPackageRemoved(const QString &packageId)
{
    if (PackageKit::Transaction::packageName(packageId) == mPackageName)
    {
        emit this->removed();
    }
}
#if 0
void OrnApplication::checkDesktopFile()
{

    auto desktopFile = mDesktopFile;
    if (mPackageName.isEmpty() || mInstalledPackageId.isEmpty())
    {
        desktopFile.clear();
    }
    else
    {
        //FIXME
        auto desktopFiles = PackageKit::Transaction::packageDesktopFiles(mPackageName);
        for (const auto &file : desktopFiles)
        {
            if (QFileInfo(file).isFile())
            {
                desktopFile = file;
                break;
            }
        }
    }
    if (mDesktopFile != desktopFile)
    {
        qDebug() << "Using desktop file" << desktopFile;
        mDesktopFile = desktopFile;
        emit this->canBeLaunchedChanged();
    }

}
#endif
