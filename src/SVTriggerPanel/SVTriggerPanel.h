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
#pragma once

#include <map>
#include <string>
#include <QtCore>
#include "SVConfig/SVConfigData.h"

#ifdef _WIN32
#ifdef SVTRIGGERPANEL_EXPORTS
#define SVTRIGGERPANEL_API __declspec(dllexport)
#else
#define SVTRIGGERPANEL_API __declspec(dllimport)
#endif
#else
#define SVTRIGGERPANEL_API
#endif

namespace SV_Trigger {
   
	struct config {

		int cycleRecMs;
		int packetSz;

		config(int cycleRecMs_ = 100, int packetSz_ = 10) :
				cycleRecMs(cycleRecMs_),
				packetSz(packetSz_) {}
	};

    /// ��� �������
    enum class eventType {
        none = -1,            ///< ���
        connectModule = 0,    ///< ������ ���������
        disconnectModule = 1, ///< ������ ��������
        less = 2,             ///< "<"
        equals = 3,           ///< "=="
        more = 4,             ///< ">"
        posFront = 5,         ///< ������������� �����
        negFront = 6,         ///< ������������� �����
    };
       
    /// �������
    struct triggerData {
        bool isActive;               ///< �������

        QString name;                ///< �������� ��������
        QString signal;              ///< ������
        QString module;              ///< ������
        QString userProcPath;        ///< ���� � ����� ��������
        QString userProcArgs;        ///< ��������� � ����� ��������, ����� /t
       
        eventType condType;          ///< ���
        int condValue;               ///< �������� ������� (����� ������������)
        int condTOut;                ///< ������� ������������, �

        triggerData() {
            isActive = false;
            condType = eventType::none;
            condTOut = 0;
        }
    };

    SVTRIGGERPANEL_API QDialog* createTriggerPanel(QWidget* parent, config);

    SVTRIGGERPANEL_API void startUpdateThread(QDialog* panel);
   
    typedef QMap<QString, SV_Cng::signalData*>(*pf_getCopySignalRef)();
	SVTRIGGERPANEL_API void setGetCopySignalRef(QDialog* panel, pf_getCopySignalRef f);

    typedef SV_Cng::signalData *(*pf_getSignalData)(const QString &sign);
    SVTRIGGERPANEL_API void setGetSignalData(QDialog* panel, pf_getSignalData f);

    typedef QMap<QString, SV_Cng::moduleData*>(*pf_getCopyModuleRef)();
    SVTRIGGERPANEL_API void setGetCopyModuleRef(QDialog* panel, pf_getCopyModuleRef f);

    typedef SV_Cng::moduleData *(*pf_getModuleData)(const QString &module);
    SVTRIGGERPANEL_API void setGetModuleData(QDialog* panel, pf_getModuleData f);
      
    typedef bool(*pf_loadSignalData)(const QString& sign);
    SVTRIGGERPANEL_API void setLoadSignalData(QDialog* panel, pf_loadSignalData f);

    typedef void(*pf_onTriggerCBack)(const QString& name);
    SVTRIGGERPANEL_API void setOnTriggerCBack(QDialog* panel, pf_onTriggerCBack f);

    // ������� ��� ��������
    SVTRIGGERPANEL_API QMap<QString, triggerData*> getCopyTriggerRef(QDialog* panel);

    // ������� ������ ��������
    SVTRIGGERPANEL_API triggerData* getTriggerData(QDialog* panel, const QString& name);

    // �������� �������
    SVTRIGGERPANEL_API bool addTrigger(QDialog* panel, const QString& name, triggerData* td);

    // ������� ��� ������� ��� ������
    SVTRIGGERPANEL_API QString getEventTypeStr(eventType type);
}