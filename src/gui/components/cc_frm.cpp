/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2012, 2013, 2014 Thilo Graf 'dbt'
	Copyright (C) 2012, Michael Liebmann 'micha-bbg'

	License: GPL

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>
#include "cc_frm.h"
#include <stdlib.h>
#include <algorithm>
#include <system/debug.h>

using namespace std;

//-------------------------------------------------------------------------------------------------------
//sub class CComponentsForm from CComponentsItem
CComponentsForm::CComponentsForm(	const int x_pos, const int y_pos, const int w, const int h,
					CComponentsForm* parent,
					bool has_shadow,
					fb_pixel_t color_frame,
					fb_pixel_t color_body,
					fb_pixel_t color_shadow)
					:CComponentsItem(parent)
{
	cc_item_type 	= CC_ITEMTYPE_FRM;

	x		= x_pos;
	y 		= y_pos;
	cc_xr 		= x;
	cc_yr 		= y;
	width 		= w;
	height	 	= h;

	shadow		= has_shadow;
	col_frame 	= color_frame;
	col_body	= color_body;
	col_shadow	= color_shadow;

	shadow_w	= SHADOW_OFFSET;
	corner_rad	= RADIUS_LARGE;
	corner_type 	= CORNER_ALL;
	cc_item_index	= 0;

	v_cc_items.clear();

	append_x_offset = 0;
	append_y_offset = 0;
	page_count	= 1;
	cur_page	= 0;
	sb 		= NULL;
	w_sb		= 15;

	page_scroll_mode = PG_SCROLL_M_UP_DOWN_KEY;

	//connect page scroll slot
	sigc::slot3<void, neutrino_msg_t&, neutrino_msg_data_t&, int&> sl = sigc::mem_fun(*this, &CComponentsForm::execPageScroll);
	this->OnExec.connect(sl);
}

CComponentsForm::~CComponentsForm()
{
	clear();
	delete sb;
}

int CComponentsForm::exec()
{
	dprintf(DEBUG_NORMAL, "[CComponentsForm]   [%s - %d] \n", __func__, __LINE__);
	OnBeforeExec();
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;

	int res = menu_return::RETURN_REPAINT;

	uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(-1);

	//required exit keys
	msg_list_t exit_keys[2];
	exit_keys[0].msg = CRCInput::RC_setup;
	exit_keys[1].msg = CRCInput::RC_home;

	bool exit_loop = false;
	while (!exit_loop)
	{
		g_RCInput->getMsgAbsoluteTimeout( &msg, &data, &timeoutEnd );

		//execute connected slots
		OnExec(msg, data, res);

		//exit loop
		execExit(msg, data, res, exit_loop, exit_keys, 2);

		if (CNeutrinoApp::getInstance()->handleMsg(msg, data) & messages_return::cancel_all)
		{
			dprintf(DEBUG_INFO, "[CComponentsForm]   [%s - %d]  messages_return::cancel_all\n", __func__, __LINE__);
			res  = menu_return::RETURN_EXIT_ALL;
			exit_loop = EXIT;
		}
	}

	OnAfterExec();
	return res;
}


void CComponentsForm::execKey(neutrino_msg_t& msg, neutrino_msg_data_t& data, int& res, bool& exit_loop, const struct msg_list_t * const msg_list, const size_t& key_count, bool force_exit)
{
	for(size_t i = 0; i < key_count; i++){
		if (execKey(msg, data, res, exit_loop, msg_list[i].msg, force_exit)){
			break;
		}
	}
}

void CComponentsForm::execKey(neutrino_msg_t& msg, neutrino_msg_data_t& data, int& res, bool& exit_loop, const std::vector<neutrino_msg_t>& v_msg_list, bool force_exit)
{
	for(size_t i = 0; i < v_msg_list.size(); i++){
		if (execKey(msg, data, res, exit_loop, v_msg_list[i], force_exit)){
			break;
		}
	}
}

inline bool CComponentsForm::execKey(neutrino_msg_t& msg, neutrino_msg_data_t& data, int& res, bool& exit_loop, const neutrino_msg_t& msg_val, bool force_exit)
{
	if (msg == msg_val){
		OnExecMsg(msg, data, res);
		if (force_exit)
			exit_loop = EXIT;
		return true;
	}
	return false;
}


void CComponentsForm::execPageScroll(neutrino_msg_t& msg, neutrino_msg_data_t& /*data*/, int& /*res*/)
{
	if (page_scroll_mode == PG_SCROLL_M_OFF)
		return;

	if (page_scroll_mode & PG_SCROLL_M_UP_DOWN_KEY){
		if (msg == CRCInput::RC_page_up)
			ScrollPage(SCROLL_P_DOWN);
		if (msg == CRCInput::RC_page_down)
			ScrollPage(SCROLL_P_UP);
	}

	if (page_scroll_mode & PG_SCROLL_M_LEFT_RIGHT_KEY){
		if (msg == CRCInput::RC_left)
			ScrollPage(SCROLL_P_DOWN);
		if (msg == CRCInput::RC_right)
			ScrollPage(SCROLL_P_UP);
	}
}

void CComponentsForm::execExit(neutrino_msg_t& msg, neutrino_msg_data_t& data, int& res, bool& exit_loop, const struct msg_list_t * const msg_list, const size_t& key_count)
{
	execKey(msg, data, res, exit_loop, msg_list, key_count, true);
}


void CComponentsForm::clear()
{
 	if (v_cc_items.empty())
		return;

	for(size_t i=0; i<v_cc_items.size(); i++) {
		if (v_cc_items[i]){

		dprintf(DEBUG_DEBUG, "[CComponentsForm] %s... delete form cc-item %d of %d (type=%d)\n", __func__, (int)i+1, (int)v_cc_items.size(), v_cc_items[i]->getItemType());

			delete v_cc_items[i];
			v_cc_items[i] = NULL;
		}
	}
	v_cc_items.clear();
}


void CComponentsForm::addCCItem(CComponentsItem* cc_Item)
{
	if (cc_Item){
		dprintf(DEBUG_DEBUG, "[CComponentsForm]  %s-%d try to add cc_Item [type %d] to form [current index=%d] \n", __func__, __LINE__, cc_Item->getItemType(), cc_item_index);

		cc_Item->setParent(this);
		v_cc_items.push_back(cc_Item);

		dprintf(DEBUG_DEBUG, "\tadded cc_Item [type %d] to form [current index=%d] \n", cc_Item->getItemType(), cc_item_index);

		//assign item index
		int new_index = genIndex();
		cc_Item->setIndex(new_index);
		cc_Item->setFocus(true);

		dprintf(DEBUG_DEBUG, "\t%s-%d parent index = %d, assigned index ======> %d\n", __func__, __LINE__, cc_item_index, new_index);

	}
	else
		dprintf(DEBUG_NORMAL, "[CComponentsForm]  %s-%d tried to add an empty or invalide cc_item !!!\n", __func__, __LINE__);
}

void CComponentsForm::addCCItem(const std::vector<CComponentsItem*> &cc_Items)
{
	for (size_t i= 0; i< cc_Items.size(); i++)
		addCCItem(cc_Items[i]);
}

int CComponentsForm::getCCItemId(CComponentsItem* cc_Item)
{
	if (cc_Item){
		for (size_t i= 0; i< v_cc_items.size(); i++)
			if (v_cc_items[i] == cc_Item)
				return i;
	}
	return -1;
}

int CComponentsForm::genIndex()
{
	int count = v_cc_items.size();
	char buf[64];
	snprintf(buf, sizeof(buf), "%d%d", cc_item_index, count);
	buf[63] = '\0';
	int ret = atoi(buf);
	return ret;
}

CComponentsItem* CComponentsForm::getCCItem(const uint& cc_item_id)
{
	if (v_cc_items[cc_item_id])
		return v_cc_items[cc_item_id];
	return NULL;
}

void CComponentsForm::replaceCCItem(const uint& cc_item_id, CComponentsItem* new_cc_Item)
{
	if (!v_cc_items.empty()){
		CComponentsItem* old_Item = v_cc_items[cc_item_id];
		if (old_Item){
			CComponentsForm * old_parent = old_Item->getParent();
			new_cc_Item->setParent(old_parent);
			new_cc_Item->setIndex(old_parent->getIndex());
			delete old_Item;
			old_Item = NULL;
			v_cc_items[cc_item_id] = new_cc_Item;
		}
	}
	else
		dprintf(DEBUG_NORMAL, "[CComponentsForm]  %s replace cc_Item not possible, v_cc_items is empty\n", __func__);
}

void CComponentsForm::replaceCCItem(CComponentsItem* old_cc_Item, CComponentsItem* new_cc_Item)
{
	replaceCCItem(getCCItemId(old_cc_Item), new_cc_Item);
}

void CComponentsForm::insertCCItem(const uint& cc_item_id, CComponentsItem* cc_Item)
{

	if (cc_Item == NULL){
		dprintf(DEBUG_DEBUG, "[CComponentsForm]  %s parameter: cc_Item = %p...\n", __func__, cc_Item);
		return;
	}

	if (v_cc_items.empty()){
		addCCItem(cc_Item);
		dprintf(DEBUG_DEBUG, "[CComponentsForm]  %s insert cc_Item not possible, v_cc_items is empty, cc_Item added\n", __func__);
	}else{
		v_cc_items.insert(v_cc_items.begin()+cc_item_id, cc_Item);
		cc_Item->setParent(this);
		//assign item index
		int index = genIndex();
		cc_Item->setIndex(index);
	}
}

void CComponentsForm::removeCCItem(const uint& cc_item_id)
{
	if (!v_cc_items.empty()){
		if (v_cc_items[cc_item_id]) {
			delete v_cc_items[cc_item_id];
			v_cc_items[cc_item_id] = NULL;
			v_cc_items.erase(v_cc_items.begin()+cc_item_id);
			dprintf(DEBUG_DEBUG, "[CComponentsForm]  %s removing cc_Item [id=%u]...\n", __func__, cc_item_id);
		}
	}
	else
		dprintf(DEBUG_NORMAL, "[CComponentsForm]  %s removing of cc_Item [id=%u] not possible, v_cc_items is empty...\n", __func__, cc_item_id);
}

void CComponentsForm::removeCCItem(CComponentsItem* cc_Item)
{
	uint id = getCCItemId(cc_Item);	
	removeCCItem(id);
}

void CComponentsForm::exchangeCCItem(const uint& cc_item_id_a, const uint& cc_item_id_b)
{
	if (!v_cc_items.empty())
		swap(v_cc_items[cc_item_id_a], v_cc_items[cc_item_id_b]);
}

void CComponentsForm::exchangeCCItem(CComponentsItem* item_a, CComponentsItem* item_b)
{
	exchangeCCItem(getCCItemId(item_a), getCCItemId(item_b));
}

void CComponentsForm::paintForm(bool do_save_bg)
{
	//paint body
	paintInit(do_save_bg);

	//paint
	paintCCItems();
}

void CComponentsForm::paint(bool do_save_bg)
{
	paintForm(do_save_bg);
}

bool CComponentsForm::isPageChanged()
{
	for(size_t i=0; i<v_cc_items.size(); i++){
		if (v_cc_items[i]->getPageNumber() != cur_page)
			return true;
	}
	return false;
}

void CComponentsForm::paintPage(const u_int8_t& page_number, bool do_save_bg)
{
	cur_page = page_number;
	paint(do_save_bg);
}

void CComponentsForm::paintCCItems()
{
	size_t items_count 	= v_cc_items.size();

	//using of real x/y values to paint items if this text object is bound in a parent form
	int this_x = x, auto_x = x, this_y = y, auto_y = y, this_w = width;
	if (cc_parent){
		this_x = auto_x = cc_xr;
		this_y = auto_y = cc_yr;
	}

	//init and handle scrollbar
	getPageCount();
	int y_sb = this_y+1;
	int x_sb = this_x + width - w_sb;
	int h_sb = height-2;
	if (sb == NULL){
		sb = new CComponentsScrollBar(x_sb, y_sb, w_sb, h_sb);
	}else{
		//clean background, if dimension of scrollbar was changed
		if (w_sb != sb->getWidth())
			sb->kill(col_body);

		//set current dimensions and position
		sb->setDimensionsAll(x_sb, y_sb, w_sb, h_sb);
	}

	if(page_count > 1){
		sb->setSegmentCount(page_count);
		sb->setMarkID(cur_page);
		this_w = width - w_sb;
		sb->paint(false);
	}else{
		if (sb->isPainted())
			sb->kill(col_body);
		this_w = width;
	}

	//detect if current page has changed, if true then kill items from screen
	if(isPageChanged()){
		this->killCCItems(col_body, true);
	}

	for(size_t i=0; i<items_count; i++){
		//assign item object
		CComponentsItem *cc_item = v_cc_items[i];

		dprintf(DEBUG_INFO, "[CComponentsForm] %s: page_count = %u, item_page = %u, cur_page = %u\n", __func__, getPageCount(), cc_item->getPageNumber(), this->cur_page);

		//get current dimension of item
		int w_item = cc_item->getWidth();
		int h_item = cc_item->getHeight();

		//get current position of item
		int xpos = cc_item->getXPos();
		int ypos = cc_item->getYPos();

		//check item for corrupt position, skip current item if found problems
		if (ypos > height || xpos > this_w){
			dprintf(DEBUG_INFO, "[CComponentsForm] %s: [form: %d] [item-index %d] [type=%d] WARNING: item position is out of form size:\ndefinied x=%d, defined this_w=%d \ndefinied y=%d, defined height=%d \n",
				__func__, cc_item_index, cc_item->getIndex(), cc_item->getItemType(), xpos, this_w, ypos, height);
			if (this->cc_item_type != CC_ITEMTYPE_FRM_CHAIN)
				continue;
		}

		//set required x-position to item:
		//append vertical
		if (xpos == CC_APPEND){
			auto_x += append_x_offset;
			cc_item->setRealXPos(auto_x + xpos);
			auto_x += w_item;
		}
		//positionize vertical centered
		else if (xpos == CC_CENTERED){
			auto_x =  this_w/2 - w_item/2;
			cc_item->setRealXPos(this_x + auto_x);
		}
		else{
			cc_item->setRealXPos(this_x + xpos);
			auto_x = (cc_item->getRealXPos() + w_item);
		}

		//set required y-position to item
		//append hor
		if (ypos == CC_APPEND){
			auto_y += append_y_offset;
			cc_item->setRealYPos(auto_y + ypos);
			auto_y += h_item;
		}
		//positionize hor centered
		else if (ypos == CC_CENTERED){
			auto_y =  height/2 - h_item/2;
			cc_item->setRealYPos(this_y + auto_y);
		}
		else{
			cc_item->setRealYPos(this_y + ypos);
			auto_y = (cc_item->getRealYPos() + h_item);
		}

		//These steps check whether the element can be painted into the container.
		//Is it too wide or too high, it will be shortened and displayed in the log.
		//This should be avoid!
		//checkwidth and adapt if required
		int right_frm = (cc_parent ? cc_xr : x) + this_w - 2*fr_thickness;
		int right_item = cc_item->getRealXPos() + w_item;
		int w_diff = right_item - right_frm;
		int new_w = w_item - w_diff;
		//avoid of width error due to odd values (1 line only)
		right_item -= (new_w%2);
		w_item -= (new_w%2);
		if (right_item > right_frm){
			dprintf(DEBUG_INFO, "[CComponentsForm] %s: [form: %d] [item-index %d] [type=%d] this_w is too large, definied width=%d, possible width=%d \n",
				__func__, cc_item_index, cc_item->getIndex(), cc_item->getItemType(), w_item, new_w);
			cc_item->setWidth(new_w);
		}

		//check height and adapt if required
		int bottom_frm = (cc_parent ? cc_yr : y) + height - 2*fr_thickness;
		int bottom_item = cc_item->getRealYPos() + h_item;
		int h_diff = bottom_item - bottom_frm;
		int new_h = h_item - h_diff;
		//avoid of height error due to odd values (1 line only)
		bottom_item -= (new_h%2);
		h_item -= (new_h%2);
		if (bottom_item > bottom_frm){
			dprintf(DEBUG_INFO, "[CComponentsForm] %s: [form: %d] [item-index %d] [type=%d] height is too large, definied height=%d, possible height=%d \n",
			       __func__, cc_item_index, cc_item->getIndex(), cc_item->getItemType(), h_item, new_h);
			cc_item->setHeight(new_h);
		}

		//get current visibility mode from item, me must hold it and restore after paint
		bool item_visible = cc_item->paintAllowed();
		//set visibility mode
		if (!this->cc_allow_paint)
			cc_item->allowPaint(false);

		//finally paint current item, but only required contents of page
		if (cc_item->getPageNumber() == cur_page)
			cc_item->paint(CC_SAVE_SCREEN_NO);

		//restore defined old visibility mode of item after paint
		cc_item->allowPaint(item_visible);
	}
}

void CComponentsForm::hide(bool no_restore)
{
	// hack: ensure hiding of minitv during hide of forms and inherited classes,
	// because the handling of minitv items are different to other item types
	// and need an explizit call of hide()
	for(size_t i=0; i<v_cc_items.size(); i++) {
		if (v_cc_items[i]->getItemType() == CC_ITEMTYPE_PIP){
			v_cc_items[i]->hide();
			break;
		}
	}

	//hide body
	hideCCItem(no_restore);
}

//erase or paint over rendered objects
void CComponentsForm::killCCItems(const fb_pixel_t& bg_color, bool ignore_parent)
{
	for(size_t i=0; i<v_cc_items.size(); i++)
		v_cc_items[i]->kill(bg_color, ignore_parent);
}

void CComponentsForm::setPageCount(const u_int8_t& pageCount)
{
	u_int8_t new_val = pageCount;
	if (new_val <  page_count)
		dprintf(DEBUG_NORMAL, "[CComponentsForm] %s:  current count (= %u) of pages higher than page_count (= %u) will be set, smaller value is ignored!\n", __func__, page_count, new_val) ;
	page_count = max(new_val, page_count);
}

u_int8_t CComponentsForm::getPageCount()
{
	u_int8_t num = 0;
	for(size_t i=0; i<v_cc_items.size(); i++){
		u_int8_t item_num = v_cc_items[i]->getPageNumber();
		num = max(item_num, num);
	}

	//convert type, possible -Wconversion warnings!
	page_count = static_cast<u_int8_t>(num + 1);

	return page_count;
}


void CComponentsForm::setSelectedItem(int item_id)
{
	size_t count = v_cc_items.size();
	int id = item_id;

	if (id > (int)(count-1) || id < 0 || (count == 0)){
		dprintf(DEBUG_NORMAL, "[CComponentsForm]   [%s - %d] invalid parameter item_id = %u, available items = %u, allowed values are: 0...%u! \n", 	__func__, 
																				__LINE__, 
																				item_id, 
																				count, 
																				count==0 ? 0:count-1);
		//exit if no item is available
		if (count == 0)
			return;

		//jump to last item
		if (id < 0)
			id = count-1;
		//jump to 1st item, if id is out of range, avoids also possible segfault
		if (id > (int)(count-1))
			id = 0;
	}

	for (size_t i= 0; i< count; i++)
		v_cc_items[i]->setSelected(i == (size_t)id);

	OnSelect();
}

void CComponentsForm::setSelectedItem(CComponentsItem* cc_item)
{
	int id = getCCItemId(cc_item);
	if (id == -1){
		dprintf(DEBUG_NORMAL, "[CComponentsForm]   [%s - %d] invalid item parameter, no object available\n", __func__,__LINE__);
		return;
	}
	setSelectedItem(id);
}

int CComponentsForm::getSelectedItem()
{
	for (size_t i= 0; i< size(); i++)
		if (getCCItem(i)->isSelected())
			return static_cast<int>(i);
	return -1;
}

CComponentsItem* CComponentsForm::getSelectedItemObject()
{
	int sel = getSelectedItem();
	CComponentsItem* ret = NULL;
	if (sel != -1)
		ret = static_cast<CComponentsItem*>(this->getCCItem(sel));

	return ret;
}


void CComponentsForm::ScrollPage(int direction, bool do_paint)
{
	OnBeforeScrollPage();

	int target_page_id = (int)getPageCount() - 1;
	int target_page = (int)cur_page;
	
	if (direction == SCROLL_P_DOWN)
		target_page = target_page+1 > target_page_id ? 0 : target_page+1;	
	else if	(direction == SCROLL_P_UP)
		target_page = target_page-1 < 0 ? target_page_id : target_page-1;

	if (do_paint)
		paintPage((uint8_t)target_page);
	else
		cur_page = (uint8_t)target_page;

	OnAfterScrollPage();
}
