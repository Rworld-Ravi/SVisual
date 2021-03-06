//
// SVisual Project
// Copyright (C) 2018 by Contributors <https://github.com/Tyill/SVisual>
//
// This code is licensed under the MIT License.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
#include "stdafx.h"
#include <QPrinter>
#include <QPrintDialog>
#include "forms/mainWin.h"
#include "forms/graphSettingPanel.h"
#include "forms/signScriptPanel.h"
#include "SVGraphPanel/SVGraphPanel.h"
#include "SVStatPanel/SVStatPanel.h"
#include "SVScriptPanel/SVScriptPanel.h"
#include "SVExportPanel/SVExportPanel.h" 
#include "SVConfig/SVConfigLimits.h"
#include "SVConfig/SVConfigData.h"

const QString VERSION = "1.1.8";
// proposal https://github.com/Tyill/SVisual/issues/31

//const QString VERSION = "1.1.7";
// added zabbix agent
//const QString VERSION = "1.1.6";
// added menu - script for signal
//const QString VERSION = "1.1.5";
// added select color signal
// const QString VERSION = "1.1.4";
// added web browse
//const QString VERSION = "1.1.3";
// reading from additional COM ports
//const QString VERSION = "1.1.2";
// patch for prev release: repair view graph
//const QString VERSION = "1.1.1";
// SVExportPanel
// -fix select module
//const QString VERSION = "1.1.0";
// -add dark theme
//const QString VERSION = "1.0.10";
// -fix graph view
//const QString VERSION = "1.0.9";
// -add setting graph view 
// const QString VERSION = "1.0.8";
// -add script panel
// const QString VERSION = "1.0.7";
// -font change
// const QString VERSION = "1.0.6";
// -save win state
// -small fix's

MainWin* mainWin = nullptr;

using namespace SV_Cng;


bool loadSignalData(const QString& sign);

QMap<QString, signalData*> getCopySignalRef(){
    return mainWin->signalRef_;
}

QMap<QString, moduleData*> getCopyModuleRef(){
    return mainWin->moduleRef_;
}

signalData* getSignalData(const QString& sign){
   return mainWin->signalRef_.contains(sign) ? mainWin->signalRef_[sign] : nullptr;;
}

moduleData* getModuleData(const QString& sign){
   return mainWin->moduleRef_.contains(sign) ? mainWin->moduleRef_[sign] : nullptr;
}

bool addSignal(SV_Cng::signalData* sd){
    QMutexLocker locker(&mainWin->mtx_);

    QString sign = QString::fromStdString(sd->name + sd->module);
    if (!mainWin->signalRef_.contains(sign)) {
        mainWin->signalRef_.insert(sign, sd);
        mainWin->signalRef_[sign]->isActive = true;
        mainWin->moduleRef_[QString::fromStdString(sd->module)]->signls.push_back(sd->name + sd->module);
        return true;
    }
    return false;
}

bool addModule(SV_Cng::moduleData* md){
    QMutexLocker locker(&mainWin->mtx_);

    QString name = QString::fromStdString(md->module);
    if (!mainWin->moduleRef_.contains(name)) {
        mainWin->moduleRef_.insert(name, md);
        mainWin->moduleRef_[name]->isActive = true;

        return true;
    }
    return false;
}

QVector<QString> getModuleSignals(const QString& md){
    QMutexLocker locker(&mainWin->mtx_);

    QVector<QString> res;
    if (mainWin->moduleRef_.contains(md)){
        auto& msign = mainWin->moduleRef_[md]->signls;
        for (auto& s : msign){
           res.push_back(QString::fromStdString(s));
        }
    }    
    return res;
}


bool MainWin::writeSettings(QString pathIni){

    QFile file(pathIni);

    QTextStream txtStream(&file);	QStringList sList;

    file.open(QIODevice::WriteOnly);
    txtStream << "[Param]" << endl;
    txtStream << endl;
    txtStream << "selOpenDir = " << cng.selOpenDir << endl;
    txtStream << endl;
    txtStream << "cycleRecMs = " << cng.cycleRecMs << endl;
    txtStream << "packetSz = " << cng.packetSz << endl;
    txtStream << endl;
    txtStream << "sortByMod = " << (cng.sortByMod ? 1 : 0) << endl;
    txtStream << "fontSz = " << this->font().pointSize() << endl;
    txtStream << "transparent = " << cng.graphSett.transparent << endl;
    txtStream << "lineWidth = " << cng.graphSett.lineWidth << endl;
    txtStream << "darkTheme = " << (cng.graphSett.darkTheme ? "1" : "0") << endl;
    txtStream << endl;
	file.close();

    return true;
}

bool MainWin::readSignals(QString path){

    QFile file(path);

    QTextStream txtStream(&file);

    if (file.open(QIODevice::ReadOnly)){

        while (!txtStream.atEnd()){

            QStringList lst = txtStream.readLine().split("\t");

            if (lst.size() >= 4){

				std::string module = qPrintable(lst[0]);
				std::string sname = qPrintable(lst[1]);
                std::string sign = sname + module;

                if (signalRef_.contains(sign.c_str())) continue;

                if (!moduleRef_.contains(module.c_str()))
                    moduleRef_[module.c_str()] = new moduleData(module);

                moduleRef_[module.c_str()]->signls.push_back(sign);
                moduleRef_[module.c_str()]->isActive = false;

				std::string group = (lst.size() >= 5) ? qPrintable(lst[4]) : "";
				std::string comment = qPrintable(lst[3]);
				std::string stype = qPrintable(lst[2]);

                if (lst.size() >= 6){
                    signAttr_[sign.c_str()].signal = QString::fromStdString(sname);
                    signAttr_[sign.c_str()].module = QString::fromStdString(module);
                    QColor clr;
                    clr.setNamedColor(lst[5]);
                    signAttr_[sign.c_str()].color = clr;
                }

                if (!groupRef_.contains(group.c_str()))
                    groupRef_[group.c_str()] = new groupData(group.c_str());

                groupRef_[group.c_str()]->signls.push_back(sign);
                groupRef_[group.c_str()]->isActive = false;

                auto sd = new signalData();
                sd->name = sname;
                sd->module = module;
                sd->group = group;
                sd->comment = comment;
                sd->type = getSVType(stype);

                signalRef_[sign.c_str()] = sd;
            }
        }

        file.close();

    }

    return true;
}

bool MainWin::writeSignals(QString path){

    QFile file(path);

    QTextStream txtStream(&file);

    file.open(QIODevice::WriteOnly);

    if (file.isOpen()){

        for (auto s : signalRef_){

            if (s->module == "Virtual")
                continue;

            txtStream << s->module.c_str() << '\t'
                << QString::fromLocal8Bit(s->name.c_str()) << '\t'
                << getSVTypeStr(s->type).c_str() << '\t'
                << QString::fromLocal8Bit(s->comment.c_str()) << '\t'
                << QString::fromLocal8Bit(s->group.c_str());
                
            QString sign = QString::fromStdString(s->name + s->module);
            if (signAttr_.contains(sign))
                txtStream << '\t' << signAttr_[sign].color.name(QColor::HexArgb).toUtf8().data() << endl;
            else
                txtStream << endl;
        }

        file.close();
    }

    return true;
}

void MainWin::updateGroup(QString group, QString sign){

	if (!groupRef_.contains(group))
		groupRef_[group] = new groupData(group.toUtf8().data());

	std::string sname = sign.toUtf8().data();

	bool isSingExist = false;
	for (auto& s : groupRef_[group]->signls){
	    if (s == sname){
            isSingExist = true;
            break;
        }
	}

	if (!isSingExist)
		groupRef_[group]->signls.push_back(sname);

}

void MainWin::load(){

	ui.treeSignals->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
	ui.treeSignals->setIconSize(QSize(40, 20));
   
	auto gp = SV_Graph::createGraphPanel(this, SV_Graph::config(cng.cycleRecMs,cng.packetSz, SV_Graph::modeGr::viewer));
    SV_Graph::setGetCopySignalRef(gp, getCopySignalRef);
    SV_Graph::setGetSignalData(gp, getSignalData);
    SV_Graph::setLoadSignalData(gp, loadSignalData);
    SV_Graph::setGetSignalAttr(gp, [](const QString& sname, SV_Graph::signalAttr& out){

        if (mainWin->signAttr_.contains(sname)){
            out.color = mainWin->signAttr_[sname].color;
            return true;
        }
        return false;
    });
    SV_Graph::setGraphSetting(gp, cng.graphSett);
    graphPanels_[this] = gp;

    graphSettPanel_ = new graphSettingPanel(this, cng.graphSett); graphSettPanel_->setWindowFlags(Qt::Window);

    exportPanel_ = SV_Exp::createExpPanel(this, SV_Exp::config(cng.cycleRecMs, cng.packetSz));
    exportPanel_->setWindowFlags(Qt::Window);
    SV_Exp::setLoadSignalData(exportPanel_, loadSignalData);
    SV_Exp::setGetCopySignalRef(exportPanel_, getCopySignalRef);
    SV_Exp::setGetCopyModuleRef(exportPanel_, getCopyModuleRef);
    SV_Exp::setGetSignalData(exportPanel_, getSignalData);

	statPanel_ = SV_Stat::createStatPanel(this, SV_Stat::config(cng.cycleRecMs,cng.packetSz));
	statPanel_->setWindowFlags(Qt::Window);
	SV_Stat::setGetCopySignalRef(statPanel_, getCopySignalRef);
	SV_Stat::setGetSignalData(statPanel_, getSignalData);
	SV_Stat::setLoadSignalData(statPanel_, loadSignalData);
	SV_Stat::setSetTimeInterval(statPanel_, [](qint64 st, qint64 en){
        SV_Graph::setTimeInterval(mainWin->graphPanels_[mainWin], st, en);
	});
	SV_Stat::setGetTimeInterval(statPanel_, [](){
        return SV_Graph::getTimeInterval(mainWin->graphPanels_[mainWin]);
	});

    scriptPanel_ = SV_Script::createScriptPanel(this, SV_Script::config(cng.cycleRecMs, cng.packetSz), SV_Script::modeGr::viewer);
    scriptPanel_->setWindowFlags(Qt::Window);
    SV_Script::setLoadSignalData(scriptPanel_, loadSignalData);
    SV_Script::setGetCopySignalRef(scriptPanel_, getCopySignalRef);
    SV_Script::setGetSignalData(scriptPanel_, getSignalData);
    SV_Script::setGetModuleData(scriptPanel_, getModuleData);
    SV_Script::setAddSignal(scriptPanel_, addSignal);
    SV_Script::setAddModule(scriptPanel_, addModule);
    SV_Script::setAddSignalsCBack(scriptPanel_, [](){
        QMetaObject::invokeMethod(mainWin, "updateTblSignal", Qt::AutoConnection);
    });     
    SV_Script::setUpdateSignalsCBack(scriptPanel_, [](){
        QMetaObject::invokeMethod(mainWin, "updateSignals", Qt::AutoConnection);
    });
    SV_Script::setModuleConnectCBack(scriptPanel_, [](const QString& module){
        QMetaObject::invokeMethod(mainWin, "updateTblSignal", Qt::AutoConnection);
    });
    SV_Script::setChangeSignColor(scriptPanel_, [](const QString& module, const QString& name, const QColor& clr){
        QMetaObject::invokeMethod(mainWin, "changeSignColor", Qt::AutoConnection, Q_ARG(QString, module), Q_ARG(QString, name), Q_ARG(QColor, clr));
    });
      
	ui.splitter->addWidget(gp);
    QList<int> ss; ss.append(150); ss.append(500);
    ui.splitter->setSizes(ss);

	ui.progressBar->setVisible(false);

	ui.btnSortByModule->setChecked(cng.sortByMod);
	ui.btnSortByGroup->setChecked(!cng.sortByMod);

	readSignals(QApplication::applicationDirPath() + "/svsignals.txt");
}

void MainWin::Connect(){

	connect(ui.actionOpen, SIGNAL(triggered()), this, SLOT(actionOpenData()));
	connect(ui.actionStat, SIGNAL(triggered()), this, SLOT(actionOpenStat()));

    connect(ui.treeSignals, &QTreeWidget::itemClicked, [this](QTreeWidgetItem* item, int){
        if (moduleRef_.contains(item->text(0))){

            auto sref = getCopySignalRef();
            std::string module = item->text(0).toUtf8().data();
            int scnt = 0;
            for (auto s : sref)
                if ((s->module == module) && s->isActive) ++scnt;

            ui.lbSignCnt->setText(QString::number(scnt));
        }
    });
    connect(ui.treeSignals, &QTreeWidget::itemDoubleClicked, [this](QTreeWidgetItem* item, int column){
        if (moduleRef_.contains(item->text(0))) return;

        auto sign = item->text(5);

        if (column == 0){
            SV_Graph::addSignal(graphPanels_[this], sign);
        }
        else {
            if (column == 2){

                auto clr = QColorDialog::getColor();

                item->setBackgroundColor(2, clr);

                auto sd = getSignalData(sign);

                if (!sd) return;

                signAttr_[sign] = signalAttr{ QString::fromStdString(sd->name),
                    QString::fromStdString(sd->module),
                    clr };
                for (auto gp : graphPanels_)
                    SV_Graph::setSignalAttr(gp, sign, SV_Graph::signalAttr{ signAttr_[sign].color });
            }
            else
                ui.treeSignals->editItem(item, column);
        }
    });
    connect(ui.treeSignals, &QTreeWidget::itemChanged, [this](QTreeWidgetItem* item, int column){
        QString sign = item->text(5);
        signalData* sd = getSignalData(sign);

        if (!sd) return;

        switch (column){
            case 3:
                if (cng.sortByMod){
                    sd->group = item->text(3).toUtf8().data();
                    updateGroup(item->text(3), sign);
                }
                break;
            case 4: sd->comment = item->text(4).toUtf8().data(); break;
        }
    });
	connect(ui.actionExit, &QAction::triggered, [this]() { 
		this->close();
	});
	connect(ui.actionExport, &QAction::triggered, [this]() {
        if (exportPanel_) exportPanel_->showNormal();
	});
    connect(ui.actionNewWin, &QAction::triggered, [this]() {

        addNewWindow(QRect());
    });		
    connect(ui.actionUpFont, &QAction::triggered, [this]() {

        QFont ft = QApplication::font();

        ft.setPointSize(ft.pointSize() + 1);

        QApplication::setFont(ft);
    });
    connect(ui.actionDnFont, &QAction::triggered, [this]() {

        QFont ft = QApplication::font();

        ft.setPointSize(ft.pointSize() - 1);
        
        QApplication::setFont(ft);
    });
    connect(ui.actionScript, &QAction::triggered, [this]() {
               
        SV_Script::startUpdateThread(scriptPanel_);
            
        scriptPanel_->showNormal();
    });
    connect(ui.actionGraphSett, &QAction::triggered, [this]() {

        if (graphSettPanel_) graphSettPanel_->showNormal();
    });

	connect(ui.btnSortByGroup, &QPushButton::clicked, [this]() {
		this->ui.btnSortByGroup->setChecked(true);
		this->ui.btnSortByModule->setChecked(false);
		cng.sortByMod = false;
		sortSignalByGroupOrModule(false);
	});

	connect(ui.btnSortByModule, &QPushButton::clicked, [this]() {
		this->ui.btnSortByModule->setChecked(true);
		this->ui.btnSortByGroup->setChecked(false);
		cng.sortByMod = true;
		sortSignalByGroupOrModule(true);
	});

	connect(ui.actionPrint, &QAction::triggered, [this]() {

		QPrinter printer(QPrinter::HighResolution);
		printer.setPageMargins(12, 16, 12, 20, QPrinter::Millimeter);
		printer.setFullPage(false);

		QPrintDialog printDialog(&printer, this);
		if (printDialog.exec() == QDialog::Accepted) {

			QPainter painter(&printer);

			double xscale = printer.pageRect().width() / double(graphPanels_[this]->width());
            double yscale = printer.pageRect().height() / double(graphPanels_[this]->height());
			double scale = qMin(xscale, yscale);
			painter.translate(printer.paperRect().x(), printer.paperRect().y());
			painter.scale(scale, scale);

            graphPanels_[this]->render(&painter);
		}
	});

    connect(ui.actionSaveWinState, &QAction::triggered, [this]() {

        QString fname = QFileDialog::getSaveFileName(this,
            tr("Сохранение состояния окон"), cng.selOpenDir,
            "ini files (*.ini)");

        if (fname.isEmpty()) return;
        cng.selOpenDir = fname;

        QFile file(fname);

        QTextStream txtStream(&file);
        txtStream.setCodec(QTextCodec::codecForName("UTF-8"));

        auto wins = graphPanels_.keys();

        int cnt = 0;
        for (auto w : wins){

            file.open(QIODevice::WriteOnly);
            txtStream << "[graphWin" << cnt << "]" << endl;

            if (w == this)
                txtStream << "locate = 0" << endl;
            else{
                auto geom = ((QDialog*)w)->geometry();
                txtStream << "locate = " << geom.x() << " " << geom.y() << " " << geom.width() << " " << geom.height() << endl;
            }
                        
            QVector<QVector<QString>> signs = SV_Graph::getLocateSignals(graphPanels_[w]);
            for (int i = 0; i < signs.size(); ++i){

                txtStream << "section" << i << " = ";
                for (int j = signs[i].size() - 1; j >= 0; --j)
                    txtStream << signs[i][j] << " ";

                txtStream << endl;
            }

            QVector<SV_Graph::axisAttr> axisAttr = SV_Graph::getAxisAttr(graphPanels_[w]);
            for (int i = 0; i < axisAttr.size(); ++i){

                txtStream << "axisAttr" << i << " = ";
                txtStream << (axisAttr[i].isAuto ? "1" : "0") << " ";
                txtStream << axisAttr[i].min << " ";
                txtStream << axisAttr[i].max << " ";
                txtStream << axisAttr[i].step << " ";

                txtStream << endl;
            }

            txtStream << endl;
            ++cnt;
        }

        file.close();

    });

    connect(ui.actionLoadWinState, &QAction::triggered, [this]() {

        QString fname = QFileDialog::getOpenFileName(this,
            tr("Загрузка состояния окон"), cng.selOpenDir,
            "ini files (*.ini)");

        if (fname.isEmpty()) return;
        cng.selOpenDir = fname;

        QSettings settings(fname, QSettings::IniFormat);
        settings.setIniCodec(QTextCodec::codecForName("UTF-8"));

        auto grps = settings.childGroups();
        for (auto& g : grps){
            settings.beginGroup(g);

            QString locate = settings.value("locate").toString();
            QObject* win = this;
            if (locate != "0"){

                auto lt = locate.split(' ');

                win = addNewWindow(QRect(lt[0].toInt(), lt[1].toInt(), lt[2].toInt(), lt[3].toInt()));
            }

            int sect = 0;
            while (true){

                QString str = settings.value("section" + QString::number(sect), "").toString();
                if (str.isEmpty()) break;

                QStringList signs = str.split(' ');
                for (auto& s : signs)
                    SV_Graph::addSignal(graphPanels_[win], s, sect);

                ++sect;
            }

            QVector< SV_Graph::axisAttr> axisAttrs;
            int axisInx = 0;
            while (true){

                QString str = settings.value("axisAttr" + QString::number(axisInx), "").toString();
                if (str.isEmpty()) break;

                QStringList attr = str.split(' ');

                SV_Graph::axisAttr axAttr;
                axAttr.isAuto = attr[0] == "1";
                axAttr.min = attr[1].toDouble();
                axAttr.max = attr[2].toDouble();
                axAttr.step = attr[3].toDouble();

                axisAttrs.push_back(axAttr);

                ++axisInx;
            }

            if (!axisAttrs.empty())
                SV_Graph::setAxisAttr(graphPanels_[win], axisAttrs);
          
            settings.endGroup();
        }

    });
    connect(ui.actionManual, &QAction::triggered, [this]() {

#ifdef SV_EN
        QDesktopServices::openUrl(QUrl::fromLocalFile(QApplication::applicationDirPath() + "/SVManualEN.pdf"));
#else
        QDesktopServices::openUrl(QUrl::fromLocalFile(QApplication::applicationDirPath() + "/SVManualRU.pdf"));
#endif
    });    
	connect(ui.actionProgram, &QAction::triggered, [this]() {
	QMessageBox::about(this, tr("About SVisual"),
			tr("<h2>SVViewer </h2>"
			"<p>Программное обеспечение предназначенное"
			"<p>для анализа сигналов с устройст."
            "<p>2017"));
	});
}

bool MainWin::init(QString initPath){
    
	QSettings settings(initPath, QSettings::IniFormat);
	settings.beginGroup("Param");

	cng.cycleRecMs =  settings.value("cycleRecMs", 100).toInt();
	cng.cycleRecMs = qMax(cng.cycleRecMs, 1);
	cng.packetSz = settings.value("packetSz", 10).toInt();
	cng.packetSz = qMax(cng.packetSz, 1);

	cng.selOpenDir = settings.value("selOpenDir", "").toString();
	cng.sortByMod = settings.value("sortByMod", 1).toInt() == 1;
		
    cng.graphSett.lineWidth = settings.value("lineWidth", "2").toInt();
    cng.graphSett.transparent = settings.value("transparent", "100").toInt();
    cng.graphSett.darkTheme = settings.value("darkTheme", "0").toInt() == 1;

    QFont ft = QApplication::font();
    int fsz = settings.value("fontSz", ft.pointSize()).toInt();
    ft.setPointSize(fsz);
    QApplication::setFont(ft);

	if (!QFile(initPath).exists())
		writeSettings(initPath);

	return true;

}

MainWin::MainWin(QWidget *parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);

	mainWin = this;

	this->setWindowTitle(QString("SVViewer ") + VERSION);

	QStringList args = QApplication::arguments();
	cng.initPath = QApplication::applicationDirPath(); if (args.size() == 2) cng.initPath = args[1];
	init(cng.initPath + "/sviewer.ini");

	Connect();

	load();

}

MainWin::~MainWin()
{	
	writeSettings(cng.initPath + "/sviewer.ini");
	writeSignals(cng.initPath + "/svsignals.txt");
}

void MainWin::updateGraphSetting(const SV_Graph::graphSetting& gs){

    cng.graphSett = gs;

    for (auto o : graphPanels_)
        SV_Graph::setGraphSetting(o, gs);
}

bool MainWin::eventFilter(QObject *target, QEvent *event){

    if ((event->type() == QEvent::Close) && (target->objectName() == "graphWin")){

        graphPanels_.remove(target);
        target->deleteLater();
    }

    return QMainWindow::eventFilter(target, event);
}

void MainWin::updateTblSignal(){

    mainWin->sortSignalByGroupOrModule(mainWin->ui.btnSortByModule->isChecked());
}

void MainWin::updateSignals(){

    for (auto gp : graphPanels_)
        SV_Graph::update(gp);
}

void MainWin::sortSignalByGroupOrModule(bool byModule){

    int itsz = ui.treeSignals->topLevelItemCount();
    QMap<QString, bool> isExpanded;
    for (int i = 0; i < itsz; ++i)
        isExpanded[ui.treeSignals->topLevelItem(i)->text(0)] = ui.treeSignals->topLevelItem(i)->isExpanded();

	ui.treeSignals->clear();
	        	
	int scnt = 0;
	if (byModule){
        QMap<QString, signalData*> sref;
        QMap<QString, moduleData*> mref;
        { QMutexLocker locker(&mainWin->mtx_); 
          mref = getCopyModuleRef();
          sref = getCopySignalRef();
        }        

		ui.treeSignals->headerItem()->setText(3, tr("Группа"));
		
        for (auto md : mref){

            if (!md->isActive) continue;

			QTreeWidgetItem* root = new QTreeWidgetItem(ui.treeSignals);
			
            if (isExpanded.contains(md->module.c_str())){
                root->setExpanded(isExpanded[md->module.c_str()]);
            }
            root->setText(0, md->module.c_str());

            auto signls = getModuleSignals(QString::fromStdString(md->module));
            for (auto& sname : signls){
                				
                if (!sref.contains(sname) || !sref[sname]->isActive) continue;
				++scnt;
				
				QTreeWidgetItem* item = new QTreeWidgetItem(root);
				item->setFlags(item->flags() | Qt::ItemFlag::ItemIsEditable);
				item->setText(0, sref[sname]->name.c_str());
				item->setText(1, SV_Cng::getSVTypeStr(sref[sname]->type).c_str());

                if (signAttr_.contains(sname))
                    item->setBackgroundColor(2, signAttr_[sname].color);
                else
                    item->setBackgroundColor(2, QColor(255, 255, 255));

				item->setText(3, sref[sname]->group.c_str());
				item->setText(4, sref[sname]->comment.c_str());
				item->setText(5, sname);
			}
		}
	}
	else {

        QMap<QString, signalData*> sref;
        { QMutexLocker locker(&mainWin->mtx_);
          sref = getCopySignalRef();
        }

		ui.treeSignals->headerItem()->setText(3, tr("Модуль"));

		for (auto grp : groupRef_){

            if (!grp->isActive) continue;

            if (grp->signls.empty()) continue;

			QTreeWidgetItem* root = new QTreeWidgetItem(ui.treeSignals);

            if (isExpanded.contains(grp->group.c_str()))
                root->setExpanded(isExpanded[grp->group.c_str()]);
            root->setText(0, grp->group.c_str());
					
            for (auto& s : grp->signls){

				QString sname = s.c_str();

                if (!sref.contains(sname) || !sref[sname]->isActive) continue;
				++scnt;

				QTreeWidgetItem* item = new QTreeWidgetItem(root);
				item->setFlags(item->flags() | Qt::ItemFlag::ItemIsEditable);
				item->setText(0, sref[sname]->name.c_str());
				item->setText(1, SV_Cng::getSVTypeStr(sref[sname]->type).c_str());

                if (signAttr_.contains(sname))
                    item->setBackgroundColor(2, signAttr_[sname].color);
                else
                    item->setBackgroundColor(2, QColor(255, 255, 255));

				item->setText(3, sref[sname]->module.c_str());
				item->setText(4, sref[sname]->comment.c_str());
				item->setText(5, sname);
			}
		}
	}

	ui.treeSignals->sortByColumn(1);

	ui.lbAllSignCnt->setText(QString::number(scnt));
	
}

void MainWin::loadDataFinished(bool ok){

	if (ok){
		ui.lbStatusMess->setText(cng.selOpenDir);
				
		sortSignalByGroupOrModule(ui.btnSortByModule->isChecked());
	}
	else
		ui.lbStatusMess->setText(tr("Файл не удалось прочитать"));
			
	ui.progressBar->setVisible(false);

	thrLoadData_->deleteLater();
	
}

void MainWin::actionOpenData(){
	
	QStringList files = QFileDialog::getOpenFileNames(this,
		tr("Добавление файлов данных"), cng.selOpenDir,
		"dat files (*.dat)");

	if (files.isEmpty()) return;

	ui.progressBar->setVisible(true);

	thrLoadData_ = new thrLoadData(files);
	connect(thrLoadData_, SIGNAL(finished(bool)), this, SLOT(loadDataFinished(bool)));
}

void MainWin::actionOpenStat(){
		
    statPanel_->showNormal();
}

void MainWin::contextMenuEvent(QContextMenuEvent * event){
       
    if (!ui.treeSignals->currentItem() || !qobject_cast<treeWidgetExt*>(focusWidget())) return;
       
    QString root = ui.treeSignals->currentItem() ? ui.treeSignals->currentItem()->text(0) : "";

    if (root.isEmpty() || !qobject_cast<treeWidgetExt*>(focusWidget())) return;
        
    auto mref = getCopyModuleRef();

    if (mref.contains(root)){
            
        if (mref[root]->isActive){
            QMenu* menu = new QMenu(this);
            menu->addAction(tr("Показать все"));
            connect(menu,
                SIGNAL(triggered(QAction*)),
                this,
                SLOT(contextMenuClick(QAction*))
                );
            menu->exec(event->globalPos());
        }
    }
    else{
        QString sign = ui.treeSignals->currentItem()->text(5);

        auto sd = getSignalData(sign);

        if (sd){

            QMenu* menu = new QMenu(this);
            if (sd->module != "Virtual"){
                menu->addAction(tr("Скрипт"));
            }
            menu->addAction(tr("Сбросить цвет"));
            
            connect(menu,
                SIGNAL(triggered(QAction*)),
                this,
                SLOT(contextMenuClick(QAction*))
                );

            menu->exec(event->globalPos());
        }
    }
}

void MainWin::contextMenuClick(QAction* act){
         
    QString root = ui.treeSignals->currentItem() ? ui.treeSignals->currentItem()->text(0) : "";

    if (root.isEmpty()) return;
             
    if (act->text() == tr("Показать все")){
        auto mref = getCopyModuleRef();
        auto sref = getCopySignalRef();
        auto signls = getModuleSignals(root);
        for (auto& s : signls){
            if (sref.contains(s) && sref[s]->isActive){
                SV_Graph::addSignal(graphPanels_[this], s);
            }
        }
    }
    else if (act->text() == tr("Сбросить цвет")){

        QString sign = ui.treeSignals->currentItem()->text(5);

        if (signAttr_.contains(sign)){
            signAttr_.remove(sign);
            for (auto gp : graphPanels_){
                SV_Graph::update(gp);
            }
            sortSignalByGroupOrModule(ui.btnSortByModule->isChecked());
        }
    }
    else if (act->text() == tr("Скрипт")){

        SV_Script::startUpdateThread(scriptPanel_);

        signScriptPanel* scr = new signScriptPanel(this, scriptPanel_);
        scr->setWindowFlags(Qt::Window);
    
        QString sign = ui.treeSignals->currentItem()->text(5);

        auto sd = getSignalData(sign);

        scr->showSignScript(QString::fromStdString(sd->name), QString::fromStdString(sd->module), sd->type);
    }
}

QDialog* MainWin::addNewWindow(const QRect& pos){

    QDialog* graphWin = new QDialog(this, Qt::Window);
    graphWin->setObjectName("graphWin");
    graphWin->installEventFilter(this);

    QVBoxLayout* vertLayout = new QVBoxLayout(graphWin);
    vertLayout->setSpacing(0);
    vertLayout->setContentsMargins(5, 5, 5, 5);

    SV_Graph::config config(cng.cycleRecMs, cng.packetSz, SV_Graph::modeGr::viewer);
    config.isShowTable = false;

    auto gp = SV_Graph::createGraphPanel(graphWin, config);
    SV_Graph::setGetCopySignalRef(gp, getCopySignalRef);
    SV_Graph::setGetSignalData(gp, getSignalData);
    SV_Graph::setLoadSignalData(gp, loadSignalData);
    SV_Graph::setGetSignalAttr(gp, [](const QString& sname, SV_Graph::signalAttr& out){

        if (mainWin->signAttr_.contains(sname)){
            out.color = mainWin->signAttr_[sname].color;
            return true;
        }
        return false;
    });
    SV_Graph::setGraphSetting(gp, cng.graphSett);

    graphPanels_[graphWin] = gp;
    vertLayout->addWidget(gp);
      
    graphWin->show();

    if (!pos.isNull()){
        graphWin->setGeometry(pos);
        graphWin->resize(QSize(pos.width(), pos.height()));
    }

    return graphWin;
}

void MainWin::changeSignColor(QString module, QString name, QColor clr){
    for (auto gp : mainWin->graphPanels_){
        SV_Graph::setSignalAttr(gp, name + module, SV_Graph::signalAttr{ clr });
    }
}
