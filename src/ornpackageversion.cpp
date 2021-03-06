#include "ornpackageversion.h"

//#include <QVariantList>
#include <QRegularExpression>


OrnPackageVersion::OrnPackageVersion()
    : downloadSize(0)
    , installSize(0)
{}

OrnPackageVersion::OrnPackageVersion(const quint64 &dsize, const quint64 &isize,
                                     const QString &version, const QString &arch, const QString &alias)
    : downloadSize(dsize)
    , installSize(isize)
    , version(version)
    , arch(arch)
    , repoAlias(alias)
{
    static QRegularExpression sepRe(QStringLiteral("[.+~-]"));
    bool ok;
    for (const QString &s : version.split(sepRe))
    {
        auto v = s.toInt(&ok);
        versionParts << (ok ? QVariant(v) : QVariant(s));
    }
}

QString OrnPackageVersion::packageId(const QString &name) const
{
    QString id(name);
    QChar sep(';');
    id.reserve(name.size() + version.size() + arch.size() + repoAlias.size() + 3);
    id.append(sep).append(version).append(sep).append(arch).append(sep).append(repoAlias);
    return id;
}

bool OrnPackageVersion::operator ==(const OrnPackageVersion &other) const
{
    return downloadSize == other.downloadSize &&
           installSize  == other.installSize  &&
           version      == other.version      &&
           arch         == other.arch         &&
           repoAlias    == other.repoAlias;
}

bool OrnPackageVersion::operator <(const OrnPackageVersion &other) const
{
    if (versionParts == other.versionParts)
    {
        return false;
    }

    auto leftSize  = versionParts.size();
    auto rightSize = other.versionParts.size();
    auto leftIsShorter = leftSize < rightSize;
    auto shorterLength = leftIsShorter ? leftSize : rightSize;

    // Compare the same parts
    for (int i = 0; i < shorterLength; ++i)
    {
        const auto &lp = versionParts[i];
        const auto &rp = other.versionParts[i];
        if (lp != rp)
        {
            return lp < rp;
        }
    }

    // If the same parts are equal then the shorter version is lesser
    return leftIsShorter;
}
