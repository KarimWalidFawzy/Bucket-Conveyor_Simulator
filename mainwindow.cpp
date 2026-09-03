#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::on_pushButton_clicked()
{
    // Causes trigger to occur
    //1. Entire bucket array shifts by 1
    //2. Whatever was in bucket 8 exits the line i.e. discarded
    //3. A new item - a ball or an empty slot is injected to bucket 1 at a random probabibilty {70% ball generation,30% empty}
    //4. Every ball in the line rolls furtherwhether or not it changes bucket position this trigger — rotation is a function of triggers elapsed, not of bucket index.
    //Empty slots shift forward exactly like filled slots — an empty bucket must visibly move through the sequence the same way a product would; it is never skipped or compressed.
    //
}

