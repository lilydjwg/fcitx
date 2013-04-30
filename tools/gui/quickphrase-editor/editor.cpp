/***************************************************************************
 *   Copyright (C) 2012~2012 by CSSlayer                                   *
 *   wengxt@gmail.com                                                      *
 *                                                                         *
 *  This program is free software: you can redistribute it and/or modify   *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation, either version 3 of the License, or      *
 *  (at your option) any later version.                                    *
 *                                                                         *
 *  This program is distributed in the hope that it will be useful,        *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *  GNU General Public License for more details.                           *
 *                                                                         *
 *  You should have received a copy of the GNU General Public License      *
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 *                                                                         *
 ***************************************************************************/

#include <libintl.h>
#include <fcitx-config/xdg.h>

#include <QFileDialog>
#include <QMessageBox>
#include <QCloseEvent>
#include <QDebug>
#include <QInputDialog>
#include <qtconcurrentrun.h>
#include <cassert>

#include "common.h"
#include "editor.h"
#include "editordialog.h"
#include "model.h"
#include "batchdialog.h"
#include "ui_editor.h"

namespace fcitx {

ListEditor::ListEditor(fcitx::QuickPhraseModel* model, QWidget* parent)
    : FcitxQtConfigUIWidget(parent),
      m_ui(new Ui::Editor),
      m_model(model)
{
    m_ui->setupUi(this);
    m_ui->addButton->setText(_("&Add"));
    m_ui->batchEditButton->setText(_("&Batch Edit"));
    m_ui->deleteButton->setText(_("&Delete"));
    m_ui->clearButton->setText(_("De&lete All"));
    m_ui->importButton->setText(_("&Import"));
    m_ui->exportButton->setText(_("E&xport"));
    m_ui->macroTableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_ui->macroTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_ui->macroTableView->setEditTriggers(QAbstractItemView::DoubleClicked);

    connect(m_ui->addButton, SIGNAL(clicked(bool)), this, SLOT(addWord()));
    connect(m_ui->batchEditButton, SIGNAL(clicked(bool)), this, SLOT(batchEditWord()));
    connect(m_ui->deleteButton, SIGNAL(clicked(bool)), this, SLOT(deleteWord()));
    connect(m_ui->clearButton, SIGNAL(clicked(bool)), this, SLOT(deleteAllWord()));
    connect(m_ui->importButton, SIGNAL(clicked(bool)), this, SLOT(importData()));
    connect(m_ui->exportButton, SIGNAL(clicked(bool)), this, SLOT(exportData()));

    m_ui->macroTableView->horizontalHeader()->setStretchLastSection(true);
    m_ui->macroTableView->verticalHeader()->setVisible(false);
    m_ui->macroTableView->setModel(m_model);
    connect(m_ui->macroTableView->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this, SLOT(itemFocusChanged()));
    connect(m_model, SIGNAL(needSaveChanged(bool)), this, SIGNAL(changed(bool)));
    
    size_t pathSize;
    char** path = FcitxXDGGetPathUserWithPrefix(&pathSize,"data/QuickPhrase.mb.d/");
    assert(pathSize>=1);
    quickPhraseDir.setPath(path[0]);
    path = FcitxXDGGetPathUserWithPrefix(&pathSize,"");
    assert(pathSize>=1);
    fcitxDir.setPath(path[0]);
    
    fileListMutex.unlockInline();

    loadFileList();
    load();
    itemFocusChanged();
    
    connect(m_ui->fileListComboBox,SIGNAL(currentIndexChanged(int)),this,SLOT(fileSelected()));
    connect(m_ui->fileOperateCombox,SIGNAL(activated(int)),this,SLOT(fileOperation(int)));
}

void ListEditor::load()
{
    m_model->load(currentFile(), false);
}

void ListEditor::load(const QString& file)
{
    m_model->load(file.isEmpty() ? "data/QuickPhrase.mb" : file, true);
}

void ListEditor::save(const QString& file)
{
    m_model->save(file.isEmpty() ? "data/QuickPhrase.mb" : file);
}

void ListEditor::save()
{
    //QFutureWatcher< bool >* futureWatcher = m_model->save("data/QuickPhrase.mb");
    QFutureWatcher< bool >* futureWatcher = m_model->save(currentFile());
    connect(futureWatcher, SIGNAL(finished()), this, SIGNAL(saveFinished()));
}

QString ListEditor::addon()
{
    return "fcitx-quickphrase";
}

bool ListEditor::asyncSave()
{
    return true;
}


QString ListEditor::title()
{
    return _("Quick Phrase Editor");
}

ListEditor::~ListEditor()
{
    delete m_ui;
}

void ListEditor::itemFocusChanged()
{
    m_ui->deleteButton->setEnabled(m_ui->macroTableView->currentIndex().isValid());
}

void ListEditor::deleteWord()
{
    if (!m_ui->macroTableView->currentIndex().isValid())
        return;
    int row = m_ui->macroTableView->currentIndex().row();
    m_model->deleteItem(row);
}


void ListEditor::deleteAllWord()
{
    m_model->deleteAllItem();
}

void ListEditor::addWord()
{
    EditorDialog* dialog = new EditorDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    dialog->open();
    connect(dialog, SIGNAL(accepted()), this, SLOT(addWordAccepted()));
}

void ListEditor::batchEditWord()
{
    BatchDialog* dialog = new BatchDialog(this);
    QString text;
    QTextStream stream(&text);
    m_model->saveData(stream);
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    dialog->setText(text);
    dialog->open();
    connect(dialog, SIGNAL(accepted()), this, SLOT(batchEditAccepted()));
}

void ListEditor::addWordAccepted()
{
    const EditorDialog* dialog = qobject_cast< const EditorDialog* >(QObject::sender());

    m_model->addItem(dialog->key(), dialog->value());
    QModelIndex last = m_model->index(m_model->rowCount() - 1, 0);
    m_ui->macroTableView->setCurrentIndex(last);
    m_ui->macroTableView->scrollTo(last);
}

void ListEditor::batchEditAccepted()
{
    const BatchDialog* dialog = qobject_cast< const BatchDialog* >(QObject::sender());

    QString s = dialog->text();
    QTextStream stream(&s);

    m_model->loadData(stream);
    QModelIndex last = m_model->index(m_model->rowCount() - 1, 0);
    m_ui->macroTableView->setCurrentIndex(last);
    m_ui->macroTableView->scrollTo(last);
}

void ListEditor::importData()
{
    QFileDialog* dialog = new QFileDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    dialog->setFileMode(QFileDialog::ExistingFile);
    dialog->setAcceptMode(QFileDialog::AcceptOpen);
    dialog->open();
    connect(dialog, SIGNAL(accepted()), this, SLOT(importFileSelected()));
}

void ListEditor::exportData()
{
    QFileDialog* dialog = new QFileDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    dialog->setAcceptMode(QFileDialog::AcceptSave);
    dialog->open();
    connect(dialog, SIGNAL(accepted()), this, SLOT(exportFileSelected()));
}


void ListEditor::importFileSelected()
{
    const QFileDialog* dialog = qobject_cast< const QFileDialog* >(QObject::sender());
    if (dialog->selectedFiles().length() <= 0)
        return;
    QString file = dialog->selectedFiles()[0];
    load(file);
}

void ListEditor::exportFileSelected()
{
    const QFileDialog* dialog = qobject_cast< const QFileDialog* >(QObject::sender());
    if (dialog->selectedFiles().length() <= 0)
        return;
    QString file = dialog->selectedFiles()[0];
    save(file);
}

void ListEditor::loadFileList()
{
    QFutureWatcher<void>* watcher = new QFutureWatcher<void>(this);
    watcher->setFuture(QtConcurrent::run<void>(this,&ListEditor::_loadFileList));
    connect(watcher,SIGNAL(finished()),this,SLOT(showFileList()));
    connect(watcher,SIGNAL(finished()),watcher,SLOT(deleteLater()));
}

void ListEditor::_loadFileList()
{
    fileListMutex.lock();
//    for (int i=1;i<=m_ui->fileListComboBox->count();i++)
//        m_ui->fileListComboBox->removeItem(i);
    fileList = quickPhraseDir.entryInfoList(QDir::Files | QDir::Readable | QDir::Writable);
    fileListMutex.unlockInline();
}

QString ListEditor::currentFile()
{
    if (m_ui->fileListComboBox->currentIndex()!=0)
        return (QDir("data/QuickPhrase.mb.d/").filePath(m_ui->fileListComboBox->currentText()));
    else
        return QString("data/QuickPhrase.mb");
}

void ListEditor::fileSelected()
{
    qDebug() << "file list count : " << m_ui->fileListComboBox->count();
    qDebug() << m_ui->fileListComboBox->currentIndex() << " " << m_ui->fileListComboBox->currentText();
    load();
}

void ListEditor::fileOperation(int type)
{
    FileOperationType m_type = FileOperationType(type);
    if (m_type==AddFile){
        bool ok;
        QString filename = QInputDialog::getText(this
        ,tr("create new file")
        ,tr("Please input a filename for newfile")
        ,QLineEdit::Normal
        ,"newfile",&ok
        );
        QFile file(quickPhraseDir.filePath(filename));
        file.open(QIODevice::ReadWrite);
        file.close();
        loadFileList();
        //m_ui->fileListComboBox->setCurrentIndex(m_ui->fileListComboBox->findText(filename));
    }
    else if (m_type==RemoveFile){
        QString filename = currentFile();
        int ret = QMessageBox::question(this
        ,tr("Confirm deleting")
        ,tr("Are you sure to delete this file :%1 ?").arg(filename)
        ,QMessageBox::Ok | QMessageBox::Cancel);
        qDebug() << ret;
        if (ret==QMessageBox::Ok){
            bool ok = fcitxDir.remove(filename);
            if (!ok){
                QMessageBox::warning(this
                ,"File Operation Failed"
                ,QString("Error while deleting file %1").arg(filename)
                );
            }
        } 
        loadFileList();
        //m_ui->fileListComboBox->setCurrentIndex(0);
    }
    m_ui->fileOperateCombox->setCurrentIndex(0);
    qDebug() << m_ui->fileListComboBox->currentText();
}

void ListEditor::showFileList()
{
    m_ui->fileListComboBox->clear();
    m_ui->fileListComboBox->addItem("Please select a file to edit...");
    qDebug() << "starting adding file item...";
    Q_FOREACH(QFileInfo i,fileList){
        m_ui->fileListComboBox->addItem(i.fileName());
        qDebug() << "added file item : " << i.fileName();
    }
}

}
