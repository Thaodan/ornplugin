#include "ornbackup.h"
#include "ornzypp.h"
#include "ornversion.h"
#include "ornclient.h"

#include <QFileInfo>
#include <QSettings>
#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusConnection>

#include <QDebug>

#define BR_CREATED       QStringLiteral("created")
#define BR_REPO_ALL      QStringLiteral("repos/all")
#define BR_REPO_DISABLED QStringLiteral("repos/disabled")
#define BR_INSTALLED     QStringLiteral("packages/installed")
#define BR_BOOKMARKS     QStringLiteral("packages/bookmarks")

OrnBackup::OrnBackup(QObject *parent) :
    QObject(parent),
    mZypp(OrnZypp::instance()),
    mStatus(Idle)
{

}

OrnBackup::Status OrnBackup::status() const
{
    return mStatus;
}

void OrnBackup::setStatus(const Status &status)
{
    if (mStatus != status)
    {
        mStatus = status;
        emit this->statusChanged();
    }
}

QVariantMap OrnBackup::details(const QString &path)
{
    Q_ASSERT_X(QFileInfo(path).isFile(), Q_FUNC_INFO, "Backup file does not exist");

    QVariantMap res;
    QSettings file(path, QSettings::IniFormat);

    res.insert(QLatin1String("created"),   file.value(BR_CREATED).toDateTime().toLocalTime());
    res.insert(QLatin1String("repos"),     file.value(BR_REPO_ALL).toStringList().size());
    res.insert(QLatin1String("packages"),  file.value(BR_INSTALLED).toStringList().size());
    res.insert(QLatin1String("bookmarks"), file.value(BR_BOOKMARKS).toStringList().size());

    return res;
}

void OrnBackup::backup(const QString &filePath)
{
    Q_ASSERT_X(!filePath.isEmpty(), Q_FUNC_INFO, "A file path must be provided");
    Q_ASSERT_X(!QFileInfo(filePath).isFile(), Q_FUNC_INFO, "Backup file already exists");

    if (mStatus != Idle)
    {
        qWarning() << this << "is already" << mStatus;
        return;
    }

    mFilePath = filePath;
    auto dir = QFileInfo(mFilePath).dir();
    if (!dir.exists() && !dir.mkpath(QChar('.')))
    {
        qCritical() << "Failed to create directory" << dir.absolutePath();
        emit this->backupError(DirectoryError);
    }
    qDebug() << mFilePath;
    QtConcurrent::run(this, &OrnBackup::pBackup);
}

void OrnBackup::restore(const QString &filePath)
{
    Q_ASSERT_X(!filePath.isEmpty(), Q_FUNC_INFO, "A file path must be set");
    Q_ASSERT_X(QFileInfo(filePath).isFile(), Q_FUNC_INFO, "Backup file does not exist");

    if (mStatus != Idle)
    {
        qWarning() << this << "is already" << mStatus;
        return;
    }

    mFilePath = filePath;
    auto watcher = new QFutureWatcher<void>();
    connect(watcher, &QFutureWatcher<void>::finished, this, &OrnBackup::pRefreshRepos);
    watcher->setFuture(QtConcurrent::run(this, &OrnBackup::pRestore));
}

QStringList OrnBackup::notFound() const
{
    QStringList names;
    for (const auto &name : mPackagesToInstall.keys())
    {
        if (!mNamesToSearch.contains(name))
        {
            names << name;
        }
    }
    return names;
}

bool OrnBackup::removeFile(const QString &filePath)
{
    Q_ASSERT_X(!QFileInfo(filePath).isDir(), Q_FUNC_INFO, "Path must be a file");
    return QFile(filePath).remove();
}

void OrnBackup::pSearchPackages()
{
    qDebug() << "Searching packages";
    this->setStatus(SearchingPackages);

    // Delete future watcher and prepare variables
    this->sender()->deleteLater();
    mPackagesToInstall.clear();
    mSearchIndex = 0;

    auto t = PackageKit::Daemon::searchNames(mNamesToSearch[mSearchIndex]);
    connect(t, &PackageKit::Transaction::errorCode, mZypp, &OrnZypp::pkError);
    connect(t, &PackageKit::Transaction::package, this, &OrnBackup::pAddPackage);
    // It seems that the PackageKit::Transaction::searchNames(QStringList, ...)
    // does searches only for the first name so we use this hack
    connect(t, &PackageKit::Transaction::finished, [this, t]()
    {
        /* ++mSearchIndex;
        if (mSearchIndex < mNamesToSearch.size())
        {
            delete t;
            auto t = PackageKit::Daemon::searchNames(mNamesToSearch[mSearchIndex]);
        }
        else
        {
        */

        t->deleteLater();
        this->pInstallPackages();
        
    });
}

void OrnBackup::pAddPackage(int info, const QString &packageId, const QString &summary)
{
    Q_UNUSED(info)
    Q_UNUSED(summary)
    auto idParts = packageId.split(QChar(';'));
    auto name = idParts.first();
    if (mNamesToSearch.contains(name))
    {
        auto repo = idParts.last();
        // Process only packages from OpenRepos
        if (repo.startsWith(OrnZypp::repoNamePrefix))
        {
            // We will filter the newest versions later
            mPackagesToInstall.insert(name, packageId);
        }
        else if (repo == QStringLiteral("installed"))
        {
            mInstalled.insert(name, idParts[1]);
        }
    }
}

void OrnBackup::pInstallPackages()
{
    qDebug() << "Installing packages";
    this->setStatus(InstallingPackages);

    QStringList ids;
    for (const auto &pname : mPackagesToInstall.uniqueKeys())
    {
        const auto &pids = mPackagesToInstall.values(pname);
        QString newestId;
        OrnVersion newestVersion;
        for (const auto &pid : pids)
        {
            OrnVersion v(PackageKit::Transaction::packageVersion(pid));
            if (v > newestVersion)
            {
                newestVersion = v;
                newestId = pid;
            }
        }
        // Skip packages that are already installed
        if (!mInstalled.contains(pname) || OrnVersion(mInstalled[pname]) < newestVersion)
        {
            ids << newestId;
        }
    }

    if (ids.isEmpty())
    {
        this->pFinishRestore();
    }
    else
    {
        auto t = PackageKit::Daemon::installPackages(ids);
        connect(t, &PackageKit::Transaction::finished, this, &OrnBackup::pFinishRestore);
    }
}

void OrnBackup::pFinishRestore()
{
    qDebug() << "Finished restoring";
    mFilePath.clear();
    this->setStatus(Idle);
    emit this->restored();
}

void OrnBackup::pBackup()
{
    qDebug() << "Starting backing up";
    this->setStatus(BackingUp);
    QSettings file(mFilePath, QSettings::IniFormat);
    QStringList repos;
    QStringList disabled;
    QSet<QString> ornPackages;

    for (auto it = mZypp->mRepos.cbegin(); it != mZypp->mRepos.cend(); ++it)
    {
        auto author = it.key().mid(OrnZypp::repoNamePrefixLength);
        repos << author;
        auto repo = it.value();
        if (!repo.enabled)
        {
            disabled << author;
        }
        for (const auto &package : repo.packages)
        {
            ornPackages.insert(package);
        }
    }

    qDebug() << "Backing up repos";
    file.setValue(BR_REPO_ALL, repos);
    file.setValue(BR_REPO_DISABLED, disabled);

    qDebug() << "Backing up installed packages";
    QStringList installed;
    for (const auto &name :  mZypp->mInstalledPackages.uniqueKeys())
    {
        if (ornPackages.contains(name))
        {
            installed << name;
        }
    }
    file.setValue(BR_INSTALLED, installed);

    qDebug() << "Backing up bookmarks";
    QVariantList bookmarks;
    for (const auto &b : OrnClient::instance()->mBookmarks)
    {
        bookmarks << b;
    }
    file.setValue(BR_BOOKMARKS, bookmarks);

    file.setValue(BR_CREATED, QDateTime::currentDateTime().toUTC());
    qDebug() << "Finished backing up";
    mFilePath.clear();
    this->setStatus(Idle);
    emit this->backedUp();
}

void OrnBackup::pRestore()
{
    QSettings file(mFilePath, QSettings::IniFormat);

    qDebug() << "Restoring bookmarks";
    this->setStatus(RestoringBookmarks);
    auto client = OrnClient::instance();
    auto oldBookmarks = client->mBookmarks;
    for (const auto &b : file.value(BR_BOOKMARKS).toList())
    {
        client->mBookmarks.insert(b.toUInt());
    }
    if (oldBookmarks != client->mBookmarks)
    {
        emit client->bookmarksChanged();
    }

    qDebug() << "Restoring repos";
    this->setStatus(RestoringRepos);

    auto repos = file.value(BR_REPO_ALL).toStringList();
    auto disabled = file.value(BR_REPO_DISABLED).toStringList().toSet();
    mNamesToSearch = file.value(BR_INSTALLED).toStringList();

    for (const auto &author : repos)
    {
        auto alias = OrnZypp::repoNamePrefix + author;
        auto call = QDBusMessage::createMethodCall(OrnZypp::ssuInterface, OrnZypp::ssuPath,
                                                   OrnZypp::ssuInterface, OrnZypp::ssuAddRepo);
        call.setArguments(QVariantList{ alias, OrnZypp::repoBaseUrl.arg(author) });
        QDBusConnection::systemBus().call(call, QDBus::Block);
        mZypp->mRepos.insert(alias, OrnZypp::RepoMeta(!disabled.contains(author)));
    }
}

void OrnBackup::pRefreshRepos()
{
    qDebug() << "Refreshing repos";
    this->setStatus(RefreshingRepos);
    auto t = PackageKit::Daemon::refreshCache(false);
    connect(t, &PackageKit::Transaction::finished, t, &PackageKit::Transaction::deleteLater);
    connect(t, &PackageKit::Transaction::errorCode, mZypp, &OrnZypp::pkError);
    connect(t, &PackageKit::Transaction::finished, this, &OrnBackup::pSearchPackages);
}
