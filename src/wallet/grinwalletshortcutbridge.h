#ifndef GRINWALLETSHORTCUTBRIDGE_H
#define GRINWALLETSHORTCUTBRIDGE_H

#include <QObject>
#include <QString>

class GrinWalletShortcutBridge : public QObject
{
    Q_OBJECT
public:
    explicit GrinWalletShortcutBridge(QObject *parent = nullptr);

    void install();
    bool handleShortcutKey(int key);
    void updateBrowserShortcutContext(const QString &text,
                                      const QString &selectedText,
                                      bool focused) const;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
};

#endif
