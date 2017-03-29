#ifndef DIALOG_H
#define DIALOG_H

#include <QDialog>
#include <QImage>

namespace Ui {
class Dialog;
}

class Dialog : public QDialog
{
    Q_OBJECT

public:
    explicit Dialog(QWidget *parent = 0);
    ~Dialog();
    void setData(const QImage &small, const QImage &big, const QString &originText);
    void setReturn(QString &text, bool &isClear);

signals:
    void reportResult(QString, bool);

private slots:
    void on_lineEdit_textChanged(const QString &arg1);
    void on_checkBox_toggled(bool checked);

private:
    Ui::Dialog *ui;
    QString *text;
    bool *isClear;
};

#endif // DIALOG_H
