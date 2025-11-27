#pragma once

#include <QAbstractListModel>
#include <QDir>
#include <QFileSystemWatcher>
#include <QStringList>

class ImageListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString folder READ folder WRITE setFolder NOTIFY folderChanged)

public:
    enum Roles {
        PathRole = Qt::UserRole + 1,
        FileNameRole
    };

    explicit ImageListModel(QObject* parent = nullptr);

    QString folder() const { return m_folder; }
    void setFolder(const QString& path);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QString pathAt(int index) const;

signals:
    void folderChanged();

private slots:
    void reload();

private:
    QString m_folder;
    QStringList m_paths;
    QFileSystemWatcher m_watcher;

    static bool isImageFile(const QString& fileName);
};
