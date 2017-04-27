#include "dialog.h"
#include "ui_dialog.h"
#include <QKeyEvent>
#include <QDebug>

Dialog::Dialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Dialog)
{
    ui->setupUi(this);
    ui->lineEdit->setFocus();
}

Dialog::~Dialog()
{
    delete ui;
}

void Dialog::setData(QImage const &small, QImage const &big, QString const &originText) {
    ui->label->setText(tr("原标注：%1").arg(originText));
    ui->smallImageLabel->setPixmap(QPixmap::fromImage(small));
    ui->smallImageLabel->adjustSize();
    ui->imageView->setAlignment(Qt::AlignCenter);
    ui->imageView->setPixmap(QPixmap::fromImage(big));
}

void Dialog::setReturn(QString &text, int &clearFlag) {
    this->text = &text;
    this->clearFlag = &clearFlag;
    ui->lineEdit->setText(*this->text);
    if (clearFlag == 0)
        ui->clearFlagButton_0->setChecked(true);
    else if (clearFlag == 1)
        ui->clearFlagButton_1->setChecked(true);
    else if (clearFlag == 2)
        ui->clearFlagButton_2->setChecked(true);
    else // clearFlag == 3
        ui->clearFlagButton_3->setChecked(true);
}

void Dialog::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_F1) {
        ui->clearFlagButton_0->click();
        close();
    } else if (event->key() == Qt::Key_F2) {
        ui->clearFlagButton_1->click();
        close();
    } else if (event->key() == Qt::Key_F3) {
        ui->clearFlagButton_2->click();
        close();
    } else if (event->key() == Qt::Key_F4) {
        ui->clearFlagButton_3->click();
        close();
    }
    QDialog::keyPressEvent(event);
}

void Dialog::on_lineEdit_textChanged(const QString &)
{
    *this->text = ui->lineEdit->text();
}

void Dialog::on_clearFlagButton_0_toggled(bool checked)
{
    if (true == checked)
        *this->clearFlag = 0;
}

void Dialog::on_clearFlagButton_1_toggled(bool checked)
{
    if (true == checked)
        *this->clearFlag = 1;
}

void Dialog::on_clearFlagButton_2_toggled(bool checked)
{
    if (true == checked)
        *this->clearFlag = 2;
}

void Dialog::on_clearFlagButton_3_toggled(bool checked)
{
    if (true == checked)
        *this->clearFlag = 3;
}
