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
#include <QSettings>
#include <QSystemTrayIcon>
#include <QPrinter>
#include <QPrintDialog>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include "forms/mainwin.h"
#include "forms/eventOrderWin.h"
#include "forms/settingsPanel.h"
#include "forms/signScriptPanel.h"
#include "forms/graphSettingPanel.h"
#include "comReader.h"
#include "SVAuxFunc/mt_log.h"
#include "SVAuxFunc/serverTCP.h"
#include "SVGraphPanel/SVGraphPanel.h"
#include "SVExportPanel/SVExportPanel.h"
#include "SVTriggerPanel/SVTriggerPanel.h"
#include "SVScriptPanel/SVScriptPanel.h"
#include "SVServer/SVServer.h"
#include "SVWebServer/SVWebServer.h"
#include "SVZabbix/SVZabbix.h"
#include "serverAPI.h"

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
// const QString VERSION = "1.1.2";
// patch for prev release: repair view graph
//const QString VERSION = "1.1.1";
// SVExportPanel
// -fix select module
// const QString VERSION = "1.1.0";
// -add dark theme
// const QString VERSION = "1.0.10";
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

void statusMess(const QString& mess){

    QMetaObject::invokeMethod(mainWin->ui.txtStatusMess, "append", Qt::AutoConnection,
        Q_ARG(QString, QString::fromStdString(SV_Aux::CurrDateTime()) + " " + mess));

    mainWin->lg.WriteLine(qPrintable(mess));
}

void MainWin::load(){
		
    ui.treeSignals->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui.treeSignals->setIconSize(QSize(40, 20));	

  	orderWin_ = new eventOrderWin(this); orderWin_->setWindowFlags(Qt::Window);
	settPanel_ = new settingsPanel(this); settPanel_->setWindowFlags(Qt::Window);
    graphSettPanel_ = new graphSettingPanel(this, cng.graphSett); graphSettPanel_->setWindowFlags(Qt::Window);
       
    SV_Graph::setLoadSignalData(graphPanels_[this], loadSignalDataSrv);
    SV_Graph::setGetCopySignalRef(graphPanels_[this], getCopySignalRefSrv);
    SV_Graph::setGetSignalData(graphPanels_[this], getSignalDataSrv);
    SV_Graph::setGetSignalAttr(graphPanels_[this], [](const QString& sname, SV_Graph::signalAttr& out){

        if (mainWin->signAttr_.contains(sname)){
            out.color = mainWin->signAttr_[sname].color;
            return true;
        }
        return false;
    });
    SV_Graph::setGraphSetting(graphPanels_[this], cng.graphSett);

    triggerPanel_ = SV_Trigger::createTriggerPanel(this, SV_Trigger::config(cng.cycleRecMs, cng.packetSz));
    triggerPanel_->setWindowFlags(Qt::Window);
    SV_Trigger::setGetCopySignalRef(triggerPanel_, getCopySignalRefSrv);
    SV_Trigger::setGetSignalData(triggerPanel_, getSignalDataSrv);
    SV_Trigger::setGetCopyModuleRef(triggerPanel_, getCopyModuleRefSrv);
    SV_Trigger::setGetModuleData(triggerPanel_, getModuleDataSrv);
    SV_Trigger::setOnTriggerCBack(triggerPanel_, [](const QString& name){
        mainWin->onTrigger(name);
    });

    scriptPanel_ = SV_Script::createScriptPanel(this, SV_Script::config(cng.cycleRecMs, cng.packetSz), SV_Script::modeGr::player);
    scriptPanel_->setWindowFlags(Qt::Window);
    SV_Script::setLoadSignalData(scriptPanel_, loadSignalDataSrv);
    SV_Script::setGetCopySignalRef(scriptPanel_, getCopySignalRefSrv);
    SV_Script::setGetSignalData(scriptPanel_, getSignalDataSrv);
    SV_Script::setGetModuleData(scriptPanel_, getModuleDataSrv);
    SV_Script::setAddSignal(scriptPanel_, addSignalSrv);
    SV_Script::setAddModule(scriptPanel_, addModuleSrv);
    SV_Script::setAddSignalsCBack(scriptPanel_, [](){
      QMetaObject::invokeMethod(mainWin, "updateTblSignal", Qt::AutoConnection);
    });
    SV_Script::setUpdateSignalsCBack(scriptPanel_, [](){
      QMetaObject::invokeMethod(mainWin, "updateSignals", Qt::AutoConnection);
    });
    SV_Script::setModuleConnectCBack(scriptPanel_, [](const QString& module){
      QMetaObject::invokeMethod(mainWin, "moduleConnect", Qt::AutoConnection, Q_ARG(QString, module));
    });
    SV_Script::setChangeSignColor(scriptPanel_, [](const QString& module, const QString& name, const QColor& clr){
        QMetaObject::invokeMethod(mainWin, "changeSignColor", Qt::AutoConnection, Q_ARG(QString, module), Q_ARG(QString, name), Q_ARG(QColor, clr));
    });
   
    exportPanel_ = SV_Exp::createExpPanel(this, SV_Exp::config(cng.cycleRecMs, cng.packetSz));
    exportPanel_->setWindowFlags(Qt::Window);
    SV_Exp::setLoadSignalData(exportPanel_, loadSignalDataSrv);
    SV_Exp::setGetCopySignalRef(exportPanel_, getCopySignalRefSrv);
    SV_Exp::setGetCopyModuleRef(exportPanel_, getCopyModuleRefSrv);
    SV_Exp::setGetSignalData(exportPanel_, getSignalDataSrv);

	SV_Srv::setStatusCBack([](const std::string& mess){
	    statusMess(mess.c_str());
	});
	SV_Srv::setOnUpdateSignalsCBack([](){
        QMetaObject::invokeMethod(mainWin, "updateSignals", Qt::AutoConnection);
	});
	SV_Srv::setOnAddSignalsCBack([](){
        QMetaObject::invokeMethod(mainWin, "updateTblSignal", Qt::AutoConnection);
	});
	SV_Srv::setOnModuleConnectCBack([](const std::string& module){
        QMetaObject::invokeMethod(mainWin, "moduleConnect", Qt::AutoConnection, Q_ARG(QString, QString::fromStdString(module)));
	});
	SV_Srv::setOnModuleDisconnectCBack([](const std::string& module){
        QMetaObject::invokeMethod(mainWin, "moduleDisconnect", Qt::AutoConnection, Q_ARG(QString, QString::fromStdString(module)));
	});
      
    SV_Web::setGetCopySignalRef(getCopySignalRefSrv);
    SV_Web::setGetSignalData(getSignalDataSrv);
    SV_Web::setGetCopyModuleRef(getCopyModuleRefSrv);

    SV_Zbx::setGetSignalData(getSignalDataSrv);

	db_ = new dbProvider(qUtf8Printable(cng.dbPath));

    if (db_->isConnect()){
        statusMess(tr("Подключение БД успешно"));
    }else{
		statusMess(tr("Подключение БД ошибка: ") + cng.dbPath);
		
        delete db_;
        db_ = nullptr;
	}

	/////////////////////
	initTrayIcon();
       
}

void MainWin::Connect(){
		
    connect(ui.treeSignals, &QTreeWidget::itemClicked, [this](QTreeWidgetItem* item, int){
        auto mref = SV_Srv::getCopyModuleRef();

        if (mref.find(item->text(0).toStdString()) != mref.end()){

            ui.lbSignCnt->setText(QString::number(mref[item->text(0).toStdString()]->signls.size()));
        }
    });
    connect(ui.treeSignals, &QTreeWidget::itemDoubleClicked, [this](QTreeWidgetItem* item, int column){
        auto mref = SV_Srv::getCopyModuleRef();

        if (mref.find(item->text(0).toStdString()) != mref.end())
            return;

        auto sign = item->text(5);
        if (column == 0){
            SV_Graph::addSignal(graphPanels_[this], sign);
        }
        else {
            if (column == 2){

                auto clr = QColorDialog::getColor();

                item->setBackgroundColor(2, clr);

                auto sd = getSignalDataSrv(sign);

                if (!sd) return;

                signAttr_[sign] = signalAttr{ QString::fromStdString(sd->name),
                    QString::fromStdString(sd->module),
                    clr };
                for (auto gp : graphPanels_)
                    SV_Graph::setSignalAttr(gp, sign, SV_Graph::signalAttr{ clr });
            }
            else
                ui.treeSignals->editItem(item, column);
        }
    });
    connect(ui.treeSignals, &QTreeWidget::itemChanged, [this](QTreeWidgetItem* item, int column){
        std::string sign = item->text(5).toStdString();
        SV_Cng::signalData* sd = SV_Srv::getSignalData(sign);

        if (!sd) return;

        switch (column){
          case 3: sd->comment = item->text(3).toStdString(); break;
          case 4: sd->group = item->text(4).toStdString(); break;
        }
    });

	connect(ui.actionExit, &QAction::triggered, [this]() { 
		this->close();
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
	connect(ui.actionTrgPanel, &QAction::triggered, [this]() {
        if (triggerPanel_) triggerPanel_->showNormal();
	});
	connect(ui.actionEventOrder, &QAction::triggered, [this]() {
        if (orderWin_) orderWin_->showNormal();
	});
    connect(ui.actionExport, &QAction::triggered, [this]() {
        if (exportPanel_) exportPanel_->showNormal();
    });
    connect(ui.actionNewWin, &QAction::triggered, [this]() {
        
        addNewWindow(QRect());
    });
    connect(ui.actionScript, &QAction::triggered, [this]() {
            
        SV_Script::startUpdateThread(scriptPanel_);
        
        scriptPanel_->showNormal();
    
    });
	connect(ui.actionSettings, &QAction::triggered, [this]() {
        if (settPanel_) settPanel_->showNormal();
	});
    connect(ui.actionGraphSett, &QAction::triggered, [this]() {
        if (graphSettPanel_) graphSettPanel_->showNormal();
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
    connect(ui.actionCheckUpdate, &QAction::triggered, [this]() {
        
        if (!netManager_){
            netManager_ = new QNetworkAccessManager(this);
            connect(netManager_, &QNetworkAccessManager::finished, [](QNetworkReply* reply){
                              
                QByteArray response_data = reply->readAll();
                QJsonDocument jsDoc = QJsonDocument::fromJson(response_data);
                
                if (jsDoc.isObject()){
                    QJsonObject	jsObj = jsDoc.object();

                    QMessageBox msgBox;
                    msgBox.setTextFormat(Qt::RichText); 
                    msgBox.setStandardButtons(QMessageBox::Ok);
                    
                    QString lastVer = jsObj["tag_name"].toString();

                    if (VERSION != lastVer)
                        msgBox.setText(tr("Доступна новая версия: ") + "<a href='" + jsObj["html_url"].toString() + "'>" + lastVer + "</a>");
                    else
                        msgBox.setText(tr("У вас самая новая версия"));

                    msgBox.exec();
                }
            });
        }

        QNetworkRequest request(QUrl("https://api.github.com/repos/Tyill/SVisual/releases/latest"));
    
        netManager_->get(request);
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

            auto tmIntl = SV_Graph::getTimeInterval(graphPanels_[w]);
            txtStream << "tmDiap = " << (tmIntl.second - tmIntl.first) << endl;

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

        statusMess(tr("Состояние успешно сохранено"));
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

             QString tmDiap = settings.value("tmDiap", "10000").toString();

             auto ctmIntl = SV_Graph::getTimeInterval(graphPanels_[win]);
             ctmIntl.second = ctmIntl.first + tmDiap.toLongLong();

             SV_Graph::setTimeInterval(graphPanels_[win], ctmIntl.first, ctmIntl.second);

            settings.endGroup();
        }
        
        statusMess(tr("Состояние успешно загружено"));
    });
    connect(ui.actionManual, &QAction::triggered, [this]() {

#ifdef SV_EN
        QDesktopServices::openUrl(QUrl::fromLocalFile(cng.dirPath + "/SVManualEN.pdf"));
#else
        QDesktopServices::openUrl(QUrl::fromLocalFile(cng.dirPath + "/SVManualRU.pdf"));
#endif
    });
    connect(ui.actionProgram, &QAction::triggered, [this]() {

        QString mess = "<h2>SVMonitor </h2>"
            "<p>Программное обеспечение предназначенное"
            "<p>для анализа сигналов с устройст."
			"<p>2017";

		QMessageBox::about(this, tr("About SVisual"), mess);
	});		
	connect(ui.btnSlowPlay, &QPushButton::clicked, [this]() {
		slowMode();
	});
}

bool MainWin::writeSettings(QString pathIni){

    QFile file(pathIni);

    QTextStream txtStream(&file);
    txtStream.setCodec(QTextCodec::codecForName("UTF-8"));

    // запись новых данных
    file.open(QIODevice::WriteOnly);
    txtStream << "[Param]" << endl;
    txtStream << endl;
    txtStream << "tcp_addr = " << cng.tcp_addr << endl;
    txtStream << "tcp_port = " << cng.tcp_port << endl;
    txtStream << endl;
    txtStream << "web_ena = " << (cng.web_ena ? "1" : "0") << endl;
    txtStream << "web_addr = " << cng.web_addr << endl;
    txtStream << "web_port = " << cng.web_port << endl;
    txtStream << endl;
    txtStream << "zabbix_ena = " << (cng.zabbix_ena ? "1" : "0") << endl;
    txtStream << "zabbix_addr = " << cng.zabbix_addr << endl;
    txtStream << "zabbix_port = " << cng.zabbix_port << endl;
    txtStream << endl;
    txtStream << "com_ena = " << (cng.com_ena ? "1" : "0") << endl;
    
    for (int i = 0; i < cng.com_ports.size(); ++i){
        txtStream << "com" << i << "_name = " << cng.com_ports[i].first << endl;
        txtStream << "com" << i << "_speed = " << cng.com_ports[i].second << endl;
    }

    txtStream << endl;
    txtStream << "dbPath = " << cng.dbPath << endl;
    txtStream << endl;
    txtStream << "cycleRecMs = " << cng.cycleRecMs << endl;
    txtStream << "packetSz = " << cng.packetSz << endl;
    txtStream << endl;
    txtStream << "; save on disk" << endl;
    txtStream << "outArchiveEna = " << (cng.outArchiveEna ? "1" : "0") << endl;
    txtStream << "outArchivePath = " << cng.outArchivePath << endl;
    txtStream << "outArchiveName = " << cng.outArchiveName << endl;
    txtStream << endl;
    txtStream << "selOpenDir = " << cng.selOpenDir << endl;
    txtStream << "fontSz = " << this->font().pointSize() << endl;
    txtStream << "transparent = " << cng.graphSett.transparent << endl;
    txtStream << "lineWidth = " << cng.graphSett.lineWidth << endl;
    txtStream << "darkTheme = " << (cng.graphSett.darkTheme ? "1" : "0") << endl;
    txtStream << endl;
    txtStream << "toutLoadWinStateSec = " << cng.toutLoadWinStateSec << endl;
    txtStream << endl;

    // состояние окон
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

        auto tmIntl = SV_Graph::getTimeInterval(graphPanels_[w]);
        txtStream << "tmDiap = " << (tmIntl.second - tmIntl.first) << endl;

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

    return true;
}

bool MainWin::init(QString initPath){

    QString logPath = cng.dirPath + "/log/";
    lg.SetPath(qPrintable(logPath));
    lg.SetName("svm");

    QSettings settings(initPath, QSettings::IniFormat);
    settings.beginGroup("Param");

    cng.cycleRecMs =  settings.value("cycleRecMs", 100).toInt();
    cng.cycleRecMs = qMax(cng.cycleRecMs, 1);
    cng.packetSz = settings.value("packetSz", 10).toInt();
    cng.packetSz = qMax(cng.packetSz, 1);

  
    cng.selOpenDir = settings.value("selOpenDir", "").toString();

    QFont ft = QApplication::font();
    int fsz = settings.value("fontSz", ft.pointSize()).toInt();
    ft.setPointSize(fsz);
    QApplication::setFont(ft);
    
    // связь по usb
    cng.com_ena = settings.value("com_ena", 0).toInt() == 1;
    QString com_name = settings.value("com0_name", "COM4").toString();
    QString com_speed = settings.value("com0_speed", "9600").toString();

    cng.com_ports.push_back(qMakePair(com_name, com_speed));

    // доп порты
    int comCnt = 1;
    while (true){

        QString com_name = settings.value("com" + QString::number(comCnt) + "_name", "").toString();
        QString com_speed = settings.value("com" + QString::number(comCnt) + "_speed", "").toString();

        if (com_name.isEmpty() || com_speed.isEmpty())
            break;

        cng.com_ports.push_back(qMakePair(com_name, com_speed));
        ++comCnt;
    }
    
    cng.dbPath = settings.value("dbPath", "").toString();
    if (cng.dbPath.isEmpty())cng.dbPath = cng.dirPath + "/svm.db";

    // связь по TCP
    cng.tcp_addr = settings.value("tcp_addr", "127.0.0.1").toString();
    cng.tcp_port = settings.value("tcp_port", "2144").toInt();

    // web
    cng.web_ena = settings.value("web_ena", "0").toInt() == 1;
    cng.web_addr = settings.value("web_addr", "127.0.0.1").toString();
    cng.web_port = settings.value("web_port", "2145").toInt();

    // zabbix
    cng.zabbix_ena = settings.value( "zabbix_ena", "0").toInt() == 1;
    cng.zabbix_addr = settings.value("zabbix_addr", "127.0.0.1").toString();
    cng.zabbix_port = settings.value("zabbix_port", "2146").toInt();

    // копир на диск
    cng.outArchiveEna = settings.value("outArchiveEna", "1").toInt() == 1;
    cng.outArchivePath = settings.value("outArchivePath", "").toString();
    if (cng.outArchivePath.isEmpty()) cng.outArchivePath = cng.dirPath + "/";
    cng.outArchivePath.replace("\\", "/");
    cng.outArchiveName = settings.value("outFileName", "svrec").toString();;
    cng.outArchiveHourCnt = settings.value("outFileHourCnt", 2).toInt();
    cng.outArchiveHourCnt = qBound(1, cng.outArchiveHourCnt, 12);

    cng.graphSett.lineWidth = settings.value("lineWidth", "2").toInt();
    cng.graphSett.transparent = settings.value("transparent", "100").toInt();
    cng.graphSett.darkTheme = settings.value("darkTheme", "0").toInt() == 1;

    cng.toutLoadWinStateSec = settings.value("toutLoadWinStateSec", "10").toInt();

        
    settings.endGroup();

    QTimer* tmWinSetts = new QTimer(this);
    connect(tmWinSetts, &QTimer::timeout, [this, initPath, tmWinSetts]() {
   
        QSettings settings(initPath, QSettings::IniFormat);
        settings.setIniCodec(QTextCodec::codecForName("UTF-8"));
        auto grps = settings.childGroups();
        for (auto& g : grps){

            if (!g.startsWith("graphWin"))
                continue;

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
                for (auto& s : signs){
                    SV_Graph::addSignal(graphPanels_[win], s, sect);
                }
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

            QString tmDiap = settings.value("tmDiap", "10000").toString();

            auto ctmIntl = SV_Graph::getTimeInterval(graphPanels_[win]);
            ctmIntl.second = ctmIntl.first + tmDiap.toLongLong();

            SV_Graph::setTimeInterval(graphPanels_[win], ctmIntl.first, ctmIntl.second);

            settings.endGroup();
        }
        tmWinSetts->deleteLater();
    });
    tmWinSetts->start(cng.toutLoadWinStateSec * 1000);
    
    if (!QFile(initPath).exists())
        writeSettings(initPath);

	srvCng.cycleRecMs = cng.cycleRecMs;
	srvCng.packetSz = cng.packetSz;
	srvCng.outArchiveEna = cng.outArchiveEna;
	srvCng.outArchiveHourCnt = cng.outArchiveHourCnt;
	srvCng.outArchiveName = cng.outArchiveName.toStdString();
	srvCng.outArchivePath = cng.outArchivePath.toStdString();

	return true;
}

MainWin::MainWin(QWidget *parent)
	: QMainWindow(parent){

	ui.setupUi(this);

	mainWin = this;

	this->setWindowTitle(QString("SVMonitor ") + VERSION);
   
	QStringList args = QApplication::arguments();
		
	cng.dirPath = QApplication::applicationDirPath();
	cng.initPath = cng.dirPath + "/svmonitor.ini"; if (args.size() == 2) cng.initPath = args[1];

	init(cng.initPath);
	statusMess(tr("Инициализация параметров успешно"));
	
    auto gp = SV_Graph::createGraphPanel(this, SV_Graph::config(cng.cycleRecMs, cng.packetSz));
    graphPanels_[this] = gp;
    ui.splitter->addWidget(gp);
    
    Connect();

	load();

	sortSignalByModule();

	// запуск получения данных
	if (cng.com_ena){

        for (auto& port : cng.com_ports){

            if (port.first.isEmpty() || port.second.isEmpty()) continue;

            SerialPortReader::config ccng(port.first, port.second.toInt(), cng.cycleRecMs, cng.packetSz);

            auto comReader = new SerialPortReader(ccng);

            if (comReader->startServer()) {

                statusMess(QString(tr("Прослушивание %1 порта запущено")).arg(port.first));
                                
                SV_Srv::startServer(srvCng);

                comReader->setDataCBack(SV_Srv::receiveData);
            }
            else
                statusMess(QString(tr("%1 порт недоступен")).arg(port.first));

            comReaders_.push_back(comReader);
        }    
	}
	else{

        if (SV_TcpSrv::runServer(cng.tcp_addr.toStdString(), cng.tcp_port, true)){

            statusMess(QString(tr("TCP cервер запущен: адрес %1 порт %2").arg(cng.tcp_addr).arg(cng.tcp_port)));

			SV_Srv::startServer(srvCng);

			SV_TcpSrv::setDataCBack(SV_Srv::receiveData);
		}
        else
            statusMess(QString(tr("Не удалось запустить TCP сервер: адрес %1 порт %2").arg(cng.tcp_addr).arg(cng.tcp_port)));
    }

    // web
    if (cng.web_ena){        

        if (SV_Web::startServer(cng.web_addr, cng.web_port, SV_Web::config(SV_CYCLEREC_MS, SV_PACKETSZ))){

            statusMess(QString(tr("WEB cервер запущен: адрес %1 порт %2").arg(cng.web_addr).arg(cng.web_port)));
        }
        else
            statusMess(QString(tr("Не удалось запустить WEB сервер: адрес %1 порт %2").arg(cng.web_addr).arg(cng.web_port)));
    }

    // zabbix
    if (cng.zabbix_ena){

        if (SV_Zbx::startAgent(cng.zabbix_addr, cng.zabbix_port, SV_Zbx::config(SV_CYCLEREC_MS, SV_PACKETSZ))){

            statusMess(QString(tr("Zabbix агент запущен: адрес %1 порт %2").arg(cng.zabbix_addr).arg(cng.zabbix_port)));
        }
        else
            statusMess(QString(tr("Не удалось запустить Zabbix агент: адрес %1 порт %2").arg(cng.zabbix_addr).arg(cng.zabbix_port)));
    }
    
    SV_Trigger::startUpdateThread(triggerPanel_);
}

MainWin::~MainWin(){

    SV_Srv::stopServer();

    for (auto comReader : comReaders_){
        if (comReader && comReader->isRunning())
            comReader->stopServer();
    }

    if (cng.web_ena)
        SV_Web::stopServer();

    if (cng.zabbix_ena)
        SV_Zbx::stopAgent();

	if (db_){
		if (!db_->saveSignals(SV_Srv::getCopySignalRef()))
			statusMess(tr("Ошибка сохранения сигналов в БД"));
        if (!db_->saveAttrSignals(signAttr_))
            statusMess(tr("Ошибка сохранения атрибутов в БД"));
		if (!db_->saveTriggers(SV_Trigger::getCopyTriggerRef(triggerPanel_)))
			statusMess(tr("Ошибка сохранения триггеров в БД"));
	}

	writeSettings(cng.initPath);

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

void MainWin::sortSignalByModule(){
	 
    int itsz = ui.treeSignals->topLevelItemCount();
    QMap<QString, bool> isExpanded;
    for (int i = 0; i < itsz; ++i)
        isExpanded[ui.treeSignals->topLevelItem(i)->text(0)] = ui.treeSignals->topLevelItem(i)->isExpanded();

	ui.treeSignals->clear();
  
	auto mref = SV_Srv::getCopyModuleRef();

    for (auto& it : mref){

		auto md = it.second;

        if (md->isDelete) continue;

		QTreeWidgetItem* root = new QTreeWidgetItem(ui.treeSignals);
		
        if (isExpanded.contains(md->module.c_str()))
           root->setExpanded(isExpanded[md->module.c_str()]);
		root->setFlags(root->flags() | Qt::ItemFlag::ItemIsEditable);
		root->setText(0, md->module.c_str());
        
		md->isEnable ? root->setIcon(0, QIcon(":/SVMonitor/images/trafficlight-green.png")):
			root->setIcon(0, QIcon(":/SVMonitor/images/trafficlight-yel.png"));
	
		if (!md->isActive) root->setIcon(0, QIcon(":/SVMonitor/images/trafficlight-red.png"));
        		
        auto msigns = getModuleSignalsSrv(md->module.c_str());
        for (auto& sign : msigns){
			SV_Cng::signalData* sd = SV_Srv::getSignalData(sign.toStdString());

			if (!sd || sd->isDelete) continue;

			QTreeWidgetItem* item = new QTreeWidgetItem(root);				
			item->setFlags(item->flags() | Qt::ItemFlag::ItemIsEditable);
			item->setText(0, sd->name.c_str());
			item->setText(1, SV_Cng::getSVTypeStr(sd->type).c_str());

            if (signAttr_.contains(sign))
                item->setBackgroundColor(2, signAttr_[sign].color);
            else
                item->setBackgroundColor(2, QColor(255, 255, 255));

			item->setText(3, sd->comment.c_str());
			item->setText(4, sd->group.c_str());
                       
			item->setText(5, sign);			
		}
	}
	
	ui.treeSignals->sortByColumn(1);

	ui.lbAllSignCnt->setText(QString::number(SV_Srv::getCopySignalRef().size()));
        
}

void MainWin::contextMenuEvent(QContextMenuEvent * event){

	QString root = ui.treeSignals->currentItem() ? ui.treeSignals->currentItem()->text(0) : "";
	
	if (root.isEmpty() || !qobject_cast<treeWidgetExt*>(focusWidget())) return;

	QMenu* menu = new QMenu(this);

	auto mref = getCopyModuleRefSrv();

	if (mref.contains(root)){
		
        if (mref[root]->isEnable && mref[root]->isActive){
            menu->addAction(tr("Показать все"));
            menu->addAction(tr("Отключить"));
        }
		else{
			menu->addAction(tr("Включить"));
			menu->addAction(tr("Удалить"));
		}				
	}
	else{
        QString module = ui.treeSignals->currentItem()->parent()->text(0);
        if (module != "Virtual"){
            menu->addAction(tr("Скрипт"));
        }
        menu->addAction(tr("Сбросить цвет"));
        menu->addAction(tr("Удалить"));
	}

	connect(menu,
		SIGNAL(triggered(QAction*)),
		this,
		SLOT(contextMenuClick(QAction*))
		);

	menu->exec(event->globalPos());
}

void MainWin::contextMenuClick(QAction* act){

	QString root = ui.treeSignals->currentItem() ? ui.treeSignals->currentItem()->text(0) : "";

	if (root.isEmpty()) return;

	auto sref = getCopySignalRefSrv();
	auto mref = getCopyModuleRefSrv();

	// signal
	if (mref.find(root) == mref.end()){

        if (act->text() == tr("Скрипт")){

            SV_Script::startUpdateThread(scriptPanel_);

            signScriptPanel* scr = new signScriptPanel(this, scriptPanel_);
            scr->setWindowFlags(Qt::Window);

            QString module = ui.treeSignals->currentItem()->parent()->text(0);

            auto sd = getSignalDataSrv(root + module);

            scr->showSignScript(root, module, sd->type);

        }
        else if (act->text() == tr("Сбросить цвет")){

            QString module = ui.treeSignals->currentItem()->parent()->text(0);
            QString sign = root + module;

            if (signAttr_.contains(sign)){
                signAttr_.remove(sign);
                for (auto gp : graphPanels_){
                    SV_Graph::update(gp);
                }
                db_->delAttrSignal(root, module);
                sortSignalByModule();
            }
        }
        else if(act->text() == tr("Удалить")){

            QString module = ui.treeSignals->currentItem()->parent()->text(0);
            QString sign = root + module;
            if (sref.contains(sign)){
                sref[sign]->isDelete = true;

                statusMess(tr("Сигнал удален ") + module + ":" + root);

                sortSignalByModule();
            }
        }
	}
	// module
	else{
        if (act->text() == tr("Показать все")){
            for (auto& s : mref[root]->signls){
                if (sref.contains(s.c_str()) && sref[s.c_str()]->isActive){
                    SV_Graph::addSignal(graphPanels_[this], QString::fromStdString(s));
                }
            }
        }
		else if (act->text() ==  tr("Включить")){

			mref[root]->isEnable = true;
			sortSignalByModule();
		}
		else if (act->text() ==  tr("Отключить")){

			mref[root]->isEnable = false;
			sortSignalByModule();
		}
		else if (act->text() ==  tr("Удалить")){
				
			QMessageBox mb;
			mb.setText(tr("Удалить модуль со всеми сигналами?"));
			mb.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
			mb.setDefaultButton(QMessageBox::No);

			switch (mb.exec()) {
			case QMessageBox::Yes:
				{
					statusMess(tr("Модуль удален ") + root);

					mref[root]->isDelete = true;
					
                    for (auto s : sref){
                        if (QString::fromStdString(s->module) == root){
                            s->isDelete = true;
                        }
                    }

					sortSignalByModule();
				}
				break;
			case QMessageBox::No:
				break;
			}
		}
	}
		
}

void MainWin::updateTblSignal(){
	
	auto sref = SV_Srv::getCopySignalRef();

	if (sref.size() > SV_VALUE_MAX_CNT)
           statusMess(tr("Превышен лимит количества сигналов: %1. Стабильная работа не гарантирована.").
                    arg(SV_VALUE_MAX_CNT));

	// посмотрим в БД что есть
	if (db_){
		
		for (auto& s : sref){

			// только тех, которые еще не видел
			if (!signExist_.contains(s.first.c_str())){

				signalData sd = db_->getSignal(s.second->name.c_str(), s.second->module.c_str());
				if (!sd.name.empty()){
					s.second->group = sd.group;
					s.second->comment = sd.comment;
				}

                signalAttr as = db_->getAttrSignal(s.second->name.c_str(), s.second->module.c_str());
                if (as.color.isValid()){
                    signAttr_[QString::fromStdString(s.first)] = as;
                }

				auto trg = db_->getTrigger(s.second->name.c_str(), s.second->module.c_str());
				int sz = trg.size();
				for (int i = 0; i < sz; ++i)
					SV_Trigger::addTrigger(triggerPanel_, trg[i]->name, trg[i]);
					
				signExist_.insert(s.first.c_str());
			}
		}
	}
    
	if (!isSlowMode_)
		sortSignalByModule();
}

void MainWin::updateSignals(){

    for (auto gp : graphPanels_)
        SV_Graph::update(gp);
}

void MainWin::moduleConnect(QString module){
	
	statusMess(tr("Подключен модуль: ") + module);

	auto mref = SV_Srv::getCopyModuleRef();
    
	// только тех, которые еще не видел
    if (!signExist_.contains(module)){
				            				
        auto trgOn = db_ ? db_->getTrigger(module + "On") : nullptr;
		if (!trgOn)	{				
            trgOn = new SV_Trigger::triggerData();
            trgOn->name = module + "On";
            trgOn->signal = "";
            trgOn->module = module;
            trgOn->condType = SV_Trigger::eventType::connectModule;
            trgOn->isActive = false;
            trgOn->condValue = 0;
            trgOn->condTOut = 0;
        }
        SV_Trigger::addTrigger(triggerPanel_, module + "On", trgOn);
							
        auto trgOff = db_ ? db_->getTrigger(module + "Off") : nullptr;
        if (!trgOff){
            trgOff = new  SV_Trigger::triggerData();
            trgOff->name = module + "Off";
            trgOff->signal = "";
            trgOff->module = module;
            trgOff->condType = SV_Trigger::eventType::disconnectModule;
            trgOff->isActive = false;
            trgOff->condValue = 0;
            trgOff->condTOut = 0;
        }   
        SV_Trigger::addTrigger(triggerPanel_, module + "Off", trgOff);

        signExist_.insert(module);
	}
	
	sortSignalByModule();
    	
    auto tr = SV_Trigger::getTriggerData(triggerPanel_, module + "On");
    if (tr->isActive)
        tr->condValue = 1;

    tr = SV_Trigger::getTriggerData(triggerPanel_, module + "Off");
    if (tr->isActive)
        tr->condValue = 0;
}

void MainWin::moduleDisconnect(QString module){

    statusMess(tr("Отключен модуль: ") + module);
		
    sortSignalByModule();

    auto tr = SV_Trigger::getTriggerData(triggerPanel_, module + "On");
    if (tr->isActive)
        tr->condValue = 0;

    tr = SV_Trigger::getTriggerData(triggerPanel_, module + "Off");
    if (tr->isActive)
        tr->condValue = 1;
}

void MainWin::onTrigger(QString trigger){
    	
    SV_Trigger::triggerData* td = SV_Trigger::getTriggerData(triggerPanel_, trigger);
		
	QString name = td->module + QString(":") + td->signal + ":" + td->name;

	statusMess(QObject::tr("Событие: ") + name);

    if (db_){
        db_->saveEvent(trigger, QDateTime::currentDateTime());
    }

    if (!td->userProcPath.isEmpty()){
        QFile f(td->userProcPath);
		if (f.exists()){

            QStringList args = td->userProcArgs.split('\t');
            for (auto& ar : args) ar = ar.trimmed();

            QProcess::startDetached(td->userProcPath, args);

            statusMess(name + QObject::tr(" Процесс запущен: ") + td->userProcPath + " args: " + td->userProcArgs);
		}
		else
            statusMess(name + QObject::tr(" Путь не найден: ") + td->userProcPath);
	}
}

void MainWin::initTrayIcon(){

	trayIcon_ = new QSystemTrayIcon(this);
	QIcon trayImage(":/SVMonitor/images/logo.png");
	trayIcon_->setIcon(trayImage);

	// Setting system tray's icon menu...
	QMenu* trayIconMenu = new QMenu(this);

	QAction* restoreAction = new QAction("Восстановить", trayIcon_);
	QAction* quitAction = new QAction("Выход", trayIcon_);

	connect(restoreAction, SIGNAL(triggered()), this, SLOT(showMaximized()));
	connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));

	trayIconMenu->addAction(restoreAction);
	trayIconMenu->addAction(quitAction);

	trayIcon_->setContextMenu(trayIconMenu);

	connect(trayIcon_, &QSystemTrayIcon::activated, [this](){
		trayIcon_->hide();
		this->showMaximized();

		isSlowMode_ = false;

		sortSignalByModule();
	});
}

void MainWin::slowMode(){

	isSlowMode_ = true;
		
	ui.treeSignals->clear();

	this->trayIcon_->show();
	this->hide();

}

MainWin::config MainWin::getConfig(){

	return cng;
}

void MainWin::updateConfig(MainWin::config cng_){

    cng = cng_;

	srvCng.outArchiveEna  = cng.outArchiveEna;
	srvCng.outArchiveHourCnt = cng.outArchiveHourCnt;
	srvCng.outArchiveName = cng.outArchiveName.toStdString();
	srvCng.outArchivePath = cng.outArchivePath.toStdString();

	SV_Srv::setConfig(srvCng);
}

QVector<uEvent> MainWin::getEvents(QDateTime bg, QDateTime en){

	return db_ ? db_->getEvents(bg, en) : QVector<uEvent>();
}

QDialog* MainWin::addNewWindow(const QRect& pos){

    QDialog* graphWin = new QDialog(this, Qt::Window);
    graphWin->setObjectName("graphWin");
    graphWin->installEventFilter(this);

    QVBoxLayout* vertLayout = new QVBoxLayout(graphWin);
    vertLayout->setSpacing(0);
    vertLayout->setContentsMargins(5, 5, 5, 5);

    SV_Graph::config config(cng.cycleRecMs, cng.packetSz);
    config.isShowTable = false;

    auto gp = SV_Graph::createGraphPanel(graphWin, config);
    SV_Graph::setLoadSignalData(gp, [](const QString& sign){
        return SV_Srv::signalBufferEna(sign.toUtf8().data());
    });
    SV_Graph::setGetCopySignalRef(gp, getCopySignalRefSrv);
    SV_Graph::setGetSignalData(gp, getSignalDataSrv);
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