#include "ImageListModel.h"

#include <QDirIterator>
#include <QFileInfo>
#include <algorithm>

namespace {
const QStringList kExtensions = {
    "jpg", "jpeg", "png", "bmp", "gif", "tif", "tiff", "webp", "heic", "raw", "nef", "cr2", "dng"
};
}

ImageListModel::ImageListModel(QObject* parent)
    : QAbstractListModel(parent)
{
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &ImageListModel::reload);
}

void ImageListModel::setFolder(const QString& path)
{
    if (path == m_folder) {
        return;
    }
    m_folder = QDir(path).absolutePath();
    emit folderChanged();

    const auto dirs = m_watcher.directories();
    if (!dirs.isEmpty()) {
        m_watcher.removePaths(dirs);
    }
    if (QDir(m_folder).exists()) {
        m_watcher.addPath(m_folder);
    }
    reload();
}

int ImageListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_paths.size();
}

QVariant ImageListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_paths.size()) {
        return QVariant();
    }
    const QString path = m_paths.at(index.row());
    switch (role) {
    case PathRole:
        return path;
    case FileNameRole:
        return QFileInfo(path).fileName();
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> ImageListModel::roleNames() const
{
    return {
        {PathRole, "path"},
        {FileNameRole, "fileName"},
    };
}

QString ImageListModel::pathAt(int index) const
{
    if (index < 0 || index >= m_paths.size()) {
        return QString();
    }
    return m_paths.at(index);
}

void ImageListModel::reload()
{
    QDir dir(m_folder);
    if (!dir.exists()) {
        beginResetModel();
        m_paths.clear();
        endResetModel();
        return;
    }

    QStringList files;
    QDirIterator it(dir.path(), QDir::Files);
    while (it.hasNext()) {
        const QString entry = it.next();
        if (isImageFile(entry)) {
            files << entry;
        }
    }
    std::sort(files.begin(), files.end(), [](const QString& a, const QString& b) {
        return a.toLower() < b.toLower();
    });

    beginResetModel();
    m_paths = files;
    endResetModel();
}

bool ImageListModel::isImageFile(const QString& fileName)
{
    const QString ext = QFileInfo(fileName).suffix().toLower();
    return kExtensions.contains(ext);
}
