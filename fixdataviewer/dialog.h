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
    void setReturn(QString &text, int &clearFlag);

protected:
    void keyPressEvent(QKeyEvent *event);

signals:
    void reportResult(QString, bool);

private slots:
    void on_lineEdit_textChanged(const QString &arg1);
    void on_clearFlagButton_0_toggled(bool checked);
    void on_clearFlagButton_1_toggled(bool checked);
    void on_clearFlagButton_2_toggled(bool checked);
    void on_clearFlagButton_3_toggled(bool checked);

private:
    Ui::Dialog *ui;
    QString *text;
    int *clearFlag;
};

#endif // DIALOG_H
