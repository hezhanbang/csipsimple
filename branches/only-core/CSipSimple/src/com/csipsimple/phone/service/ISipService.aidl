/**
 * Copyright (C) 2010 Regis Montoya (aka r3gis - www.r3gis.fr)
 * This file is part of CSipSimple.
 *
 *  CSipSimple is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  CSipSimple is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with CSipSimple.  If not, see <http://www.gnu.org/licenses/>.
 */
package com.csipsimple.phone.service;

import com.csipsimple.phone.models.AccountInfo;
import com.csipsimple.phone.models.CallInfo;

interface ISipService{
	//Service overall
	void sipStart();
	void sipStop();
	void forceStopService();
	
	//Registrations
	void addAllAccounts();
	void removeAllAccounts();
	void reAddAllAccounts();
	AccountInfo getAccountInfo(int accountId);
	
	//Call control
	void makeCall(in String callee);
	int answer(int callId, int status);
	int hangup(int callId, int status);
	int sendDtmf(int callId, int keyCode);
	
	CallInfo getCallInfo(int callId);
	
	//Misc
	void switchToAutoAnswer();
	
}