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
package com.csipsimple.ui.prefs;

import com.csipsimple.R;
import com.csipsimple.utils.PreferencesWrapper;


public class PrefsUI extends GenericPrefs {


	@Override
	protected int getXmlPreferences() {
		return R.xml.prefs_ui;
	}

	@Override
	protected void afterBuildPrefs() {
		super.afterBuildPrefs();
		PreferencesWrapper pfw = new PreferencesWrapper(this);
		if(!pfw.isAdvancedUser()) {
			hidePreference(null, "advanced_ui");
			hidePreference("android_integration", "gsm_integration_type");
			
		}
	}
	
	
	@Override
	protected void updateDescriptions() {
		// TODO Auto-generated method stub
		
	}


	
}
