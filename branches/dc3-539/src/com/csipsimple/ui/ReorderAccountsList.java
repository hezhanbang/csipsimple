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
package com.csipsimple.ui;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;

import android.app.ListActivity;
import android.graphics.Color;
import android.os.Bundle;
import android.view.View;
import android.widget.ImageView;
import android.widget.SimpleAdapter;
import android.widget.SimpleAdapter.ViewBinder;
import android.widget.TextView;

import com.csipsimple.R;
import com.csipsimple.api.SipProfile;
import com.csipsimple.db.DBAdapter;
import com.csipsimple.utils.Log;
import com.csipsimple.widgets.DragnDropListView;
import com.csipsimple.widgets.DragnDropListView.DropListener;

public class ReorderAccountsList extends ListActivity {
	
	private DBAdapter database;
	private SimpleAdapter adapter;
	
	private ArrayList<HashMap<String, Object>> accountsList;
	
	private static final String THIS_FILE = "ReorderAccountList";
	
	private static final String ACCOUNT_NAME = "account_name";
	private static final String ACCOUNT_ID = "account_id";
	private static final String ACCOUNT_PRIORITY = "account_priority";
	

	@Override
	@SuppressWarnings("unchecked")
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.codecs_list);
		initDatas();
		
		adapter = new SimpleAdapter(this, accountsList, R.layout.codecs_list_item, new String []{
			ACCOUNT_NAME,
			ACCOUNT_PRIORITY
		}, new int[] {
			R.id.line1,
			R.id.entiere_line
		});
		
		adapter.setViewBinder(new ViewBinder() {
			@Override
			public boolean setViewValue(View view, Object data, String textRepresentation) {
				if(view.getId() == R.id.entiere_line) {
					Log.d(THIS_FILE, "Entiere line is binded");
					TextView tv = (TextView) view.findViewById(R.id.line1);
					ImageView grabber = (ImageView) view.findViewById(R.id.icon);
					tv.setTextColor(Color.WHITE);
					grabber.setImageResource(R.drawable.ic_mp_move);
					return true;
				}
				return false;
			}
		});
		
		setListAdapter(adapter);
		
		DragnDropListView listView = (DragnDropListView) getListView();
		listView.setOnDropListener(new DropListener() {
			@Override
			public void drop(int from, int to) {
				
				HashMap<String, Object> item = (HashMap<String, Object>) getListAdapter().getItem(from);
				
				Log.d(THIS_FILE, "Dropped "+item.get(ACCOUNT_NAME)+" -> "+to);
				
				
				accountsList.remove(from);
				accountsList.add(to, item);
				
				//Update priorities
				int currentPriority = 130;
				for(HashMap<String, Object> acc : accountsList) {
					if(currentPriority != (Integer) acc.get(ACCOUNT_PRIORITY)) {
						database.open();
						boolean done = database.updateAccountPriority((Integer) acc.get(ACCOUNT_ID), (int) currentPriority);
						database.close();
						Log.d(THIS_FILE, "Acc "+acc.get(ACCOUNT_NAME)+ " done "+done+" prio "+ currentPriority);
						acc.put(ACCOUNT_PRIORITY, currentPriority);
					}
					//Log.d(THIS_FILE, "Reorder : "+codec.toString());
					currentPriority --;
					
				}
				
				
				//Log.d(THIS_FILE, "Data set "+codecs.toString());
				((SimpleAdapter) adapter).notifyDataSetChanged();
			}
		});
	}
	
	private void initDatas() {
		accountsList = new ArrayList<HashMap<String, Object>>();
		
		
		if(database == null) {
    		database = new DBAdapter(this);
    	}
		
    	database.open();
		List<SipProfile> accList = database.getListAccounts();
		database.close();
		
		
		for(SipProfile acc : accList) {
			HashMap<String, Object> accInfo = new HashMap<String, Object>();
			accInfo.put(ACCOUNT_ID, acc.id);
			accInfo.put(ACCOUNT_NAME, acc.display_name);
			accInfo.put(ACCOUNT_PRIORITY, acc.priority);
			accountsList.add(accInfo);
		}
		
		Collections.sort(accountsList, accountComparator);
	}
	
	private Comparator<HashMap<String, Object>> accountComparator = new Comparator<HashMap<String, Object>>() {
		@Override
		public int compare(HashMap<String, Object> infos1, HashMap<String, Object> infos2) {
			if (infos1 != null && infos2 != null) {
				int c1 = (Integer)infos1.get(ACCOUNT_PRIORITY);
				int c2 = (Integer)infos2.get(ACCOUNT_PRIORITY);
				if (c1 > c2) {
					return -1;
				}
				if (c1 < c2) {
					return 1;
				}
			}

			return 0;
		}
	};

}
