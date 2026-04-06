#ifndef PLATFORMBRIDGE_H
#define PLATFORMBRIDGE_H

#include <QObject>
#include <QString>
#include <QVariantMap>

class PlatformBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString activeEditorId READ activeEditorId NOTIFY activeEditorChanged)
    Q_PROPERTY(bool mobile READ isMobile CONSTANT)
    Q_PROPERTY(bool wasm READ isWasm CONSTANT)
    Q_PROPERTY(bool nativeTouchSelectionSupported READ supportsNativeTouchSelection CONSTANT)

public:
    explicit PlatformBridge(QObject *parent = nullptr);

    Q_INVOKABLE bool openTextEditor(const QString &title,
                                    const QString &text,
                                    const QVariantMap &options = QVariantMap());
    Q_INVOKABLE QString readClipboard() const;
    Q_INVOKABLE bool writeClipboard(const QString &text) const;
    Q_INVOKABLE void requestFocus(const QString &id);
    Q_INVOKABLE bool isMobile() const;
    Q_INVOKABLE bool isWasm() const;
    Q_INVOKABLE bool supportsNativeTouchSelection() const;
    Q_INVOKABLE void closeTextEditor(bool accept = false);

    QString activeEditorId() const;

    void handleEditorTextChanged(const QString &id, const QString &text);
    void handleEditorAccepted(const QString &id, const QString &text);
    void handleEditorClosed(const QString &id, const QString &text, bool accepted);

signals:
    void textEditorOpened(const QString &id,
                          const QString &title,
                          const QString &text,
                          const QVariantMap &options);
    void textEditorTextChanged(const QString &id, const QString &text);
    void textEditorAccepted(const QString &id, const QString &text);
    void textEditorClosed(const QString &id, const QString &text, bool accepted);
    void focusRequested(const QString &id);
    void activeEditorChanged();

private:
    QString m_activeEditorId;
    QVariantMap m_activeEditorOptions;
};

#endif