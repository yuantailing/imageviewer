#include "dialog.h"
#include "ui_dialog.h"

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
    ui->imageView->setPixmap(QPixmap::fromImage(big.scaled(ui->imageView->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation)));
}

void Dialog::setReturn(QString &text, bool &isClear) {
    this->text = &text;
    this->isClear = &isClear;
    ui->lineEdit->setText(*this->text);
    ui->checkBox->setChecked(!*this->isClear);
}

void Dialog::on_lineEdit_textChanged(const QString &)
{
    *this->text = ui->lineEdit->text();
}

void Dialog::on_checkBox_toggled(bool checked)
{
    *this->isClear = !checked;
}
