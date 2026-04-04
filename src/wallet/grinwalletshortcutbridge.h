#ifndef GRINWALLETSHORTCUTBRIDGE_H
#define GRINWALLETSHORTCUTBRIDGE_H

#include <QObject>
#include <QString>

class GrinWalletShortcutBridge : public QObject
{
    Q_OBJECT
public:
/**
 * @brief Bridges browser keyboard shortcuts into wallet actions.
 */
    explicit GrinWalletShortcutBridge(QObject *parent = nullptr);

/**
 * @brief Processes install.
 */
    void install();
/**
 * @brief Processes handle shortcut key.
 */
    bool handleShortcutKey(int key);
    void updateBrowserShortcutContext(const QString &text,
                                      const QString &selectedText,
                                      bool focused) const;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
};

#endif
