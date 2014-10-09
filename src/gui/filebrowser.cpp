/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Copyright (C) 2007-2009, 2011-2012 Stefan Seyfried

	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* include <config.h> before <gui/filebrowser.h> to enable 64 bit file offsets */
#include <gui/filebrowser.h>

#include <gui/components/cc.h>
#include <gui/widget/buttons.h>
#include <gui/widget/icons.h>
#include <gui/widget/messagebox.h>

#include <algorithm>
#include <iostream>
#include <cctype>

#include <global.h>
#include <neutrino.h>
#include <driver/screen_max.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sstream>

#include <sys/stat.h>

#include <curl/curl.h>
#include <curl/easy.h>

#if LIBCURL_VERSION_NUM < 0x071507
#include <curl/types.h>
#endif
#include <system/helpers.h>
#include <xmltree/xmlinterface.h>

#ifndef __USE_FILE_OFFSET64
#error not using 64 bit file offsets
#endif

#define SMSKEY_TIMEOUT 2000

size_t CurlWriteToString(void *ptr, size_t size, size_t nmemb, void *data)
{
	std::string* pStr = (std::string*) data;
	pStr->append((char*) ptr, nmemb); // do only add the correct size, do not go until end of data (chunked transfer)
	return size*nmemb;
}

SMSKeyInput::SMSKeyInput()
{
	resetOldKey();
	m_timeout = SMSKEY_TIMEOUT;
}

unsigned char SMSKeyInput::handleMsg(const neutrino_msg_t msg)
{
	timeval keyTime;
	gettimeofday(&keyTime,NULL);
	bool timeoutNotReached = (keyTime.tv_sec*1000+keyTime.tv_usec/1000
				  <= m_oldKeyTime.tv_sec*1000+m_oldKeyTime.tv_usec/1000 + m_timeout);

// 	printf("act: %ld , old: %ld (diff: %ld ) , timeout: %ld => timout= %d\n",
// 	       keyTime.tv_sec*1000+keyTime.tv_usec/1000,
// 	       m_oldKeyTime.tv_sec*1000+m_oldKeyTime.tv_usec/1000,
// 	       keyTime.tv_sec*1000+keyTime.tv_usec/1000 -
// 	       m_oldKeyTime.tv_sec*1000+m_oldKeyTime.tv_usec/1000,
// 	       m_timeout,!timeoutNotReached);

	const char *key = "";
	if (CRCInput::isNumeric(msg)) {
		int n = CRCInput::getNumericValue(msg);
		const char *Chars[10] = { "0", "1", "abc2", "def3", "ghi4", "jkl5", "mno6", "pqrs7", "tuv8", "wxyz9" };
		if (timeoutNotReached) {
			key = Chars[n];
			while (*key && *key != m_oldKey)
				key++;
			if (*key)
				key++;
		}
		if (!*key)
			key = Chars[n];
	}
	m_oldKeyTime=keyTime;
	m_oldKey=*key;
	return *key;
}

void SMSKeyInput::resetOldKey()
{
	m_oldKeyTime.tv_sec = 0;
	m_oldKeyTime.tv_usec = 0;
	m_oldKey = 0;
}

unsigned char SMSKeyInput::getOldKey() const
{
	return m_oldKey;
}
#if 0
const timeval* SMSKeyInput::getOldKeyTime() const
{
	return &m_oldKeyTime;
}

time_t SMSKeyInput::getOldKeyTimeSec() const
{
	return m_oldKeyTime.tv_sec;
}


int SMSKeyInput::getTimeout() const
{
	return m_timeout;
}
#endif
void SMSKeyInput::setTimeout(int timeout)
{
	m_timeout = timeout;
}

bool comparetolower(const char a, const char b)
{
	return tolower(a) < tolower(b);
}

// sort operators
bool sortByName (const CFile& a, const CFile& b)
{
	if (std::lexicographical_compare(a.Name.begin(), a.Name.end(), b.Name.begin(), b.Name.end(), comparetolower))
		return true;

	if (std::lexicographical_compare(b.Name.begin(), b.Name.end(), a.Name.begin(), a.Name.end(), comparetolower))
		return false;

	return a.Mode < b.Mode;
}

// Sorts alphabetically with Directories first
bool sortByNameDirsFirst(const CFile& a, const CFile& b)
{
	int typea, typeb;
	typea = a.getType();
	typeb = b.getType();

	if (typea == CFile::FILE_DIR)
		if (typeb == CFile::FILE_DIR)
			//both directories
			return sortByName(a, b);
		else
			//only a is directory
			return true;
	else if (typeb == CFile::FILE_DIR)
		//only b is directory
		return false;
	else
		//no directory
		return sortByName(a, b);
}

bool sortByType (const CFile& a, const CFile& b)
{
	if (a.Mode == b.Mode)
		return sortByName(a, b);
	else
		return a.Mode < b.Mode;
}

bool sortByDate (const CFile& a, const CFile& b)
{
	if (a.getFileName() == "..")
		return true;
	if (b.getFileName() == "..")
		return false;
	return a.Time < b.Time ;
}

bool sortBySize (const CFile& a, const CFile& b)
{
	if (a.getFileName() == "..")
		return true;
	if (b.getFileName() == "..")
		return false;
	return a.Size < b.Size;
}

bool (* const sortBy[FILEBROWSER_NUMBER_OF_SORT_VARIANTS])(const CFile& a, const CFile& b) =
{
	&sortByName,
	&sortByNameDirsFirst,
	&sortByType,
	&sortByDate,
	&sortBySize
};

const neutrino_locale_t sortByNames[FILEBROWSER_NUMBER_OF_SORT_VARIANTS] =
{
	LOCALE_FILEBROWSER_SORT_NAME,
	LOCALE_FILEBROWSER_SORT_NAMEDIRSFIRST,
	LOCALE_FILEBROWSER_SORT_TYPE,
	LOCALE_FILEBROWSER_SORT_DATE,
	LOCALE_FILEBROWSER_SORT_SIZE
};

CFileBrowser::CFileBrowser()
{
	commonInit();
	base = "";
	m_Mode = ModeFile;
}

CFileBrowser::CFileBrowser(const char * const _base, const tFileBrowserMode mode)
{
	commonInit();
	base = _base;
	m_Mode = mode;
}

void CFileBrowser::commonInit()
{
	frameBuffer = CFrameBuffer::getInstance();
	//shoutcast
	sc_init_dir = "/legacy/genrelist?k="  + g_settings.shoutcast_dev_id;

	Filter = NULL;
	use_filter = true;
	Multi_Select = false;
	Dirs_Selectable = false;
	Dir_Mode = false;
	Hide_records = false;
	selected = 0;
	m_SMSKeyInput.setTimeout(SMSKEY_TIMEOUT);
	//fontInit();
}

void CFileBrowser::fontInit()
{
	fnt_title = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE];
	fnt_item  = g_Font[SNeutrinoSettings::FONT_TYPE_FILEBROWSER_ITEM];
	fnt_small = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL];
	width = frameBuffer->getScreenWidthRel();
	height = frameBuffer->getScreenHeightRel();
	x = getScreenStartX(width);

	theight = fnt_title->getHeight();
	fheight = fnt_item->getHeight();
	if (fheight == 0)
		fheight = 1; /* avoid div by zero on invalid font */
	//foheight = fnt_small->getHeight()+6; //initial height value for buttonbar; TODO get value from buttonbar
	foheight = paintFoot(false);
	skwidth = 26;

	liststart = 0;
	listmaxshow = std::max(1,(int)(height - theight - foheight)/fheight);

	//recalc height
	height = theight + listmaxshow * fheight + foheight;
	y = getScreenStartY(height);
}

CFileBrowser::~CFileBrowser()
{
}

CFile *CFileBrowser::getSelectedFile()
{
	if ((!(filelist.empty())) && (!(filelist[selected].Name.empty())))
		return &filelist[selected];
	else
		return NULL;
}

void CFileBrowser::ChangeDir(const std::string & filename, int selection)
{
	std::string newpath;
	if ((m_Mode != ModeSC) && (filename == ".."))
	{
		std::string::size_type pos = Path.substr(0,Path.length()-1).rfind('/');

		if (pos == std::string::npos)
			newpath = Path;
		else
			newpath = Path.substr(0, pos + 1);
	}
	else
	{
		newpath=filename;
	}

	if (m_Mode != ModeSC && (newpath.rfind('/') != newpath.length()-1 || newpath.length() == 0))
		newpath += '/';

	filelist.clear();
	Path = newpath;
	name = newpath;
	CFileList allfiles;
	readDir(newpath, &allfiles);
	// filter
	CFileList::iterator file = allfiles.begin();
	for(; file != allfiles.end() ; file++)
	{
		if (Filter != NULL && !file->isDir() && use_filter)
		{
			if (!Filter->matchFilter(file->Name))
				continue;

			if (Hide_records) {
				int ext_pos = file->Name.rfind('.');
				if (ext_pos > 0) {
					std::string extension = file->Name.substr(ext_pos + 1, name.length() - ext_pos);
					if (strcasecmp(extension.c_str(), "ts") == 0) {
						std::string fname = file->Name.substr(0, ext_pos) + ".xml";
						if (access(fname, F_OK) == 0)
							continue;
					}
				}
			}
		}
		if (Dir_Mode && !file->isDir())
			continue;

		filelist.push_back(*file);
	}
	// sort result
	sort(filelist.begin(), filelist.end(), sortBy[g_settings.filebrowser_sortmethod]);

	selected = 0;
	if ((selection != -1) && (selection < (int)filelist.size()))
		selected = selection;

	paintHead();
	paint();
	paintFoot();
}

bool CFileBrowser::readDir(const std::string & dirname, CFileList* flist)
{
#ifdef ENABLE_INTERNETRADIO
	if (m_Mode == ModeSC)
		return readDir_sc(dirname, flist);
	else
#endif
		return readDir_std(dirname, flist);
}

#ifdef ENABLE_INTERNETRADIO
bool CFileBrowser::readDir_sc(const std::string & dirname, CFileList* flist)
{
#define GET_SHOUTCAST_TIMEOUT	60
/* how the shoutcast xml interfaces looks/works:
1st step: get http://www.shoutcast.com/sbin/newxml.phtml
example answer:

<genrelist>
...
<genre name="Trance"/>
<genre name=...
...

2nd step: get http://www.shoutcast.com/sbin/newxml.phtml?genre=Trance
example answer:

<stationlist>
<tunein base="/sbin/tunein-station.pls"/>
<station name="TechnoBase.FM - 24h Techno, Dance, Trance, House and More - 128k MP3" mt="audio/mpeg" id="524" br="128" genre="Techno Trance Dance House" ct="We aRe oNe" lc="4466"/>
<station name=...
...

3rd step: get/decode playlist http://www.shoutcast.com/sbin/tunein-station.pls?id=524
and add to neutrino playlist

4th step: play from neutrio playlist
*/
	//shoutcast
	const std::string sc_get_top500 = "/legacy/Top500?k=" + g_settings.shoutcast_dev_id;
	const std::string sc_get_genre = "/legacy/stationsearch?k=" + g_settings.shoutcast_dev_id + "&search=";
	const std::string sc_tune_in_base = "http://yp.shoutcast.com";

//	printf("readDir_sc %s\n",dirname.c_str());
	std::string answer="";
//	char *dir_escaped = curl_escape(dirname.c_str(), 0);
	std::string url = m_baseurl;
//	url += dir_escaped;
//	curl_free(dir_escaped);
	url += dirname;
	std::cout << "[FileBrowser] SC URL: " << url << std::endl;
	CURL *curl_handle;
	CURLcode httpres;
	/* init the curl session */
	curl_handle = curl_easy_init();
	/* specify URL to get */
	curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());
	/* send all data to this function  */
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, CurlWriteToString);
	/* we pass our 'chunk' struct to the callback function */
	curl_easy_setopt(curl_handle, CURLOPT_FILE, (void *)&answer);
	/* Generate error if http error >= 400 occurs */
	curl_easy_setopt(curl_handle, CURLOPT_FAILONERROR, 1);
	/* set timeout to 30 seconds */
	curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, GET_SHOUTCAST_TIMEOUT);

	/* error handling */
	char error[CURL_ERROR_SIZE];
	curl_easy_setopt(curl_handle, CURLOPT_ERRORBUFFER, error);
	/* get it! */
	httpres = curl_easy_perform(curl_handle);
	/* cleanup curl stuff */
	curl_easy_cleanup(curl_handle);

	//std::cout << "Answer:" << std::endl << "----------------" << std::endl << answer << std::endl;

	if (!answer.empty() && httpres == 0)
	{
printf("CFileBrowser::readDir_sc: read done, size %d\n", (int)answer.size());
		xmlDocPtr answer_parser = parseXml(answer.c_str());

		if (answer_parser != NULL) {
			char *ptr;
			unsigned char xml_decode = 0;
			xmlNodePtr element = xmlDocGetRootElement(answer_parser);

			if (strcmp(xmlGetName(element), "genrelist") == 0)
				xml_decode = 1;
			else if (strcmp(xmlGetName(element), "stationlist") == 0)
				xml_decode = 2;
			element = element->xmlChildrenNode;

			if (element == NULL) {
				printf("[FileBrowser] SC: Directory cannot be read.\n");
				CFile file;
				file.Mode = S_IFDIR + 0777 ;
				file.Name = dirname + "..";
				file.Size = 0;
				file.Time = 0;
				flist->push_back(file);
			} else {
				char * tunein_base = NULL;

				if (xml_decode == 1) {
					CFile file;
					file.Mode = S_IFDIR + 0777 ;
					file.Name = " Top500"; // use space to have it at the beginning of the list
					file.Url = sc_get_top500;
					file.Size = 0;
					file.Time = 0;
					flist->push_back(file);
				} else if (xml_decode == 2) {
					CFile file2;
					file2.Mode = S_IFDIR + 0777 ;
					file2.Name = "..";
					file2.Url = sc_init_dir;
					file2.Size = 0;
					file2.Time = 0;
					flist->push_back(file2);
				}
				while (element) {
					CFile file;
					if (xml_decode == 1) {
						file.Mode = S_IFDIR + 0777 ;
						file.Name = xmlGetAttribute(element, "name");
						file.Url = sc_get_genre + file.Name;
						file.Size = 0;
						file.Time = 0;
						flist->push_back(file);
					}
					else if (xml_decode == 2) {
						ptr = xmlGetName(element);
						if (ptr != NULL) {
							if (strcmp(ptr, "tunein")==0) {
								ptr = xmlGetAttribute(element, "base");
								if (ptr)
									tunein_base = ptr;
							} else if (strcmp(ptr, "station")==0) {
								ptr = xmlGetAttribute(element, "mt");
								if (ptr && (strcmp(ptr, "audio/mpeg")==0)) {
									file.Mode = S_IFREG + 0777 ;
									file.Name = xmlGetAttribute(element, "name");
									file.Url = sc_tune_in_base + tunein_base + (std::string)"?id=" + xmlGetAttribute(element, "id") + (std::string)"&k=" + g_settings.shoutcast_dev_id;
									//printf("adding %s (%s)\n", file.Name.c_str(), file.Url.c_str());
									ptr = xmlGetAttribute(element, "br");
									if (ptr) {
										file.Size = atoi(ptr);
										file.Time = atoi(ptr);
									} else {
										file.Size = 0;
										file.Time = 0;
									}
									flist->push_back(file);
								}
							}
						}
					}
					element = element->xmlNextNode;
				}
			}
			xmlFreeDoc(answer_parser);
			return true;
		}
	}

	/* since all CURL error messages use only US-ASCII characters, when can safely print them as if they were UTF-8 encoded */
	if (httpres == 22) {
	    strcat(error, "\nProbably wrong link.");
	}
printf("CFileBrowser::readDir_sc: httpres %d error, %s\n", httpres, error);
	DisplayErrorMessage(error); // UTF-8
	CFile file;

	file.Name = dirname + "..";
	file.Mode = S_IFDIR + 0777;
	file.Size = 0;
	file.Time = 0;
	flist->push_back(file);

	return false;
}
#endif

bool CFileBrowser::readDir_std(const std::string & dirname, CFileList* flist)
{
//	printf("readDir_std %s\n",dirname.c_str());
	struct stat64 statbuf;
	dirent64 **namelist;
	int n;

	n = scandir64(dirname.c_str(), &namelist, 0, alphasort64);
	if (n < 0)
	{
		perror(("Filebrowser scandir: "+dirname).c_str());
		return false;
	}
	for (int i = 0; i < n; i++)
	{
		CFile file;
		if(strcmp(namelist[i]->d_name,".") != 0)
		{
			file.Name = dirname + namelist[i]->d_name;

//			printf("file.Name: '%s', getFileName: '%s' getPath: '%s'\n",file.Name.c_str(),file.getFileName().c_str(),file.getPath().c_str());
			if(stat64((file.Name).c_str(),&statbuf) != 0)
				perror("stat error");
			else
			{
				file.Mode = statbuf.st_mode;
				file.Size = statbuf.st_size;
				file.Time = statbuf.st_mtime;
				flist->push_back(file);
			}
		}
		free(namelist[i]);
	}

	free(namelist);

	return true;
}

bool CFileBrowser::checkBD(CFile &file)
{
	if (file.isDir()) {
		std::string bdmv = file.Name + "/BDMV/index.bdmv";
		if (access(bdmv.c_str(), F_OK) == 0)
			return true;
	}
	return false;
}

bool CFileBrowser::exec(const char * const dirname)
{
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;

	bool res = false;
	menu_ret = menu_return::RETURN_REPAINT;

#ifdef ENABLE_INTERNETRADIO
	if (m_Mode == ModeSC) {
		m_baseurl = base;
	}
#endif

	name = dirname;
	std::replace(name.begin(), name.end(), '\\', '/');

	fontInit();
	//paintHead();
	int selection = -1;
	if (name == Path)
		selection = selected;

	ChangeDir(name, selection);

	unsigned int oldselected = selected;

	uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_FILEBROWSER]);

	bool loop=true;
	while (loop)
	{
		g_RCInput->getMsgAbsoluteTimeout( &msg, &data, &timeoutEnd );
		neutrino_msg_t msg_repeatok = msg & ~CRCInput::RC_Repeat;

		if (msg <= CRCInput::RC_MaxRC)
			timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_FILEBROWSER]);

		if(!CRCInput::isNumeric(msg))
		{
			m_SMSKeyInput.resetOldKey();
			paintSMSKey();
		}
		if (msg == CRCInput::RC_blue)
		{
			if(Filter != NULL)
			{
				use_filter = !use_filter;
				ChangeDir(Path);
			}
		}
		else if (msg == CRCInput::RC_home)
		{
			loop = false;
		}
		else if (msg == CRCInput::RC_left)
		{
#ifdef ENABLE_INTERNETRADIO
			if (m_Mode == ModeSC)
			{
				for(unsigned int i = 0; i < filelist.size();i++) {
					if (filelist[i].isDir() && filelist[i].getFileName() == "..") {
						ChangeDir(filelist[i].Url);
						break;
					}
				}
			}
			else
#endif
			if (!selections.empty())
			{
				ChangeDir("..",selections.back());
				selections.pop_back();
			} else
			{
				ChangeDir("..");
			}
		}
		else if (msg == NeutrinoMessages::STANDBY_ON ||
				msg == NeutrinoMessages::SHUTDOWN ||
				msg == NeutrinoMessages::SLEEPTIMER)
		{
			menu_ret = menu_return::RETURN_EXIT_ALL;
			loop = false;
			g_RCInput->postMsg(msg, data);
		}
		else if (msg == CRCInput::RC_timeout)
		{
			selected = oldselected;
			loop = false;
		}
		else if (msg > CRCInput::RC_MaxRC)
		{
			if (CNeutrinoApp::getInstance()->handleMsg( msg, data ) & messages_return::cancel_all) {
				menu_ret = menu_return::RETURN_EXIT_ALL;
				loop = false;
			}
		}
		if ((filelist.empty()))
			continue;

		if (msg == CRCInput::RC_yellow || msg == CRCInput::RC_play)
		{
			if ((Multi_Select) && (selected < filelist.size()))
			{
				if(filelist[selected].getFileName() != "..")
				{
					if(!filelist[selected].isDir() || Dirs_Selectable)
					{
						filelist[selected].Marked = !filelist[selected].Marked;
						msg_repeatok = CRCInput::RC_down;	// jump to next item
					}
				}
			}
		}
		if (msg_repeatok == CRCInput::RC_up)
		{
			unsigned int prevselected=selected;
			unsigned int prevliststart = liststart;
			if (selected)
				selected --;
			else
				selected = filelist.size() - 1;
			liststart = (selected/listmaxshow)*listmaxshow;
			if(prevliststart != liststart)
			{
				paint();
			}
			else
			{
				paintItem(prevselected - prevliststart);
				paintItem(selected - liststart);
			}
		}
		else if (msg_repeatok == CRCInput::RC_down)
		{
			unsigned int prevselected=selected;
			unsigned int prevliststart = liststart;
			selected = (selected + 1) % filelist.size();
			liststart = (selected/listmaxshow)*listmaxshow;
			if(prevliststart != liststart)
			{
				paint();
			}
			else
			{
				paintItem(prevselected - prevliststart);
				paintItem(selected - liststart);
			}
		}
		else if (msg == CRCInput::RC_right)
		{
			if (filelist[selected].isDir())
			{
#ifdef ENABLE_INTERNETRADIO
				if (m_Mode == ModeSC) {
					ChangeDir(filelist[selected].Url);
				} else
#endif
				{
					if (filelist[selected].getFileName() != "..") {
						selections.push_back(selected);
						ChangeDir(filelist[selected].Name);
					}
				}
			}
		}
		else if (msg == (neutrino_msg_t) g_settings.key_pagedown)
		{
			unsigned int last = filelist.size() - 1;
			if (selected != last && selected + listmaxshow >= filelist.size()) {
				selected = last;
			} else {
				selected = (selected == last) ? 0 : selected + listmaxshow;
				liststart = (selected / listmaxshow) * listmaxshow;
			}
			paint();
		}
		else if (msg == (neutrino_msg_t) g_settings.key_pageup)
		{
			if (selected && selected < listmaxshow) {
				selected = 0;
			} else {
				selected = selected ? selected - listmaxshow : filelist.size() - 1;
				liststart = (selected/listmaxshow)*listmaxshow;
			}
			paint();
		}
		else if (msg == CRCInput::RC_spkr)
		{
			if(".." != (filelist[selected].getFileName().substr(0,2))) // do not delete that
			{
				std::stringstream _msg;
				_msg << g_Locale->getText(LOCALE_FILEBROWSER_DODELETE1) << " ";
				if (filelist[selected].getFileName().length() > 25)
					_msg << filelist[selected].getFileName().substr(0, 25) << "...";
				else
					_msg << filelist[selected].getFileName();

				_msg << " " << g_Locale->getText(LOCALE_FILEBROWSER_DODELETE2);
				if (ShowMsg(LOCALE_FILEBROWSER_DELETE, _msg.str(), CMessageBox::mbrNo, CMessageBox::mbYes|CMessageBox::mbNo)==CMessageBox::mbrYes)
				{
					std::string n = filelist[selected].Name;
					recursiveDelete(n.c_str());
					if(n.length() > 3 && ".ts" == n.substr(n.length() - 3))
						recursiveDelete((n.substr(0, n.length() - 2) + "xml").c_str());
					ChangeDir(Path);

				}
			}
		}
		else if (msg == CRCInput::RC_ok)
		{
			bool has_selected = false;
			if (Multi_Select) {
				for(unsigned int i = 0; i < filelist.size();i++) {
					if(filelist[i].Marked) {
						has_selected = true;
						break;
					}
				}
			}
			if (has_selected) {
				res = true;
				break;
			}
			if (filelist[selected].getFileName() == "..")
			{
#ifdef ENABLE_INTERNETRADIO
				if (m_Mode == ModeSC)
					ChangeDir(filelist[selected].Url);
				else
#endif
				{
					if (!selections.empty() ) {
						ChangeDir("..",selections.back());
						selections.pop_back();
					} else {
						std::string::size_type pos = Path.substr(0,Path.length()-1).rfind('/');
						if (pos != std::string::npos)
							ChangeDir("..");
					}
				}
			}
			else
			{
				bool return_dir = Hide_records && checkBD(filelist[selected]);
				if(!return_dir && filelist[selected].isDir() && !Dir_Mode)
				{
#ifdef ENABLE_INTERNETRADIO
					if (m_Mode == ModeSC)
						ChangeDir(filelist[selected].Url);
					else
#endif
					{
						selections.push_back(selected);
						ChangeDir(filelist[selected].Name);
					}
				} else {
					filelist[selected].Marked = true;
					loop = false;
					res = true;
				}
			}
		}
		else if (msg == CRCInput::RC_help || msg == CRCInput::RC_red)
		{
			if (++g_settings.filebrowser_sortmethod >= FILEBROWSER_NUMBER_OF_SORT_VARIANTS)
				g_settings.filebrowser_sortmethod = 0;

			sort(filelist.begin(), filelist.end(), sortBy[g_settings.filebrowser_sortmethod]);

			paint();
			paintFoot();
		}
		else if (CRCInput::isNumeric(msg_repeatok))
		{
			SMSInput(msg_repeatok);
		}
	}

	hide();

	selected_filelist.clear();

	if(res && Multi_Select)
	{
		CProgressWindow * progress = NULL;
		for(unsigned int i = 0; i < filelist.size();i++)
			if(filelist[i].Marked)
			{
				bool return_dir = Hide_records && checkBD(filelist[i]);
				if(!return_dir && filelist[i].isDir()) {
					if (!progress) {
						progress = new CProgressWindow();
						progress->setTitle(LOCALE_FILEBROWSER_SCAN);
						progress->exec(NULL,"");
					}
#ifdef ENABLE_INTERNETRADIO
					if (m_Mode == ModeSC)
						addRecursiveDir(&selected_filelist,filelist[i].Url, true, progress);
					else
#endif
						addRecursiveDir(&selected_filelist,filelist[i].Name, true, progress);
				} else
					selected_filelist.push_back(filelist[i]);
			}
		if (progress) {
			progress->hide();
			delete progress;
		}
	}

	return res;
}

void CFileBrowser::addRecursiveDir(CFileList * re_filelist, std::string rpath, bool bRootCall, CProgressWindow * progress)
{
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;

	int n;

	//printf("addRecursiveDir %s\n",rpath.c_str());

	if (bRootCall) bCancel=false;

	g_RCInput->getMsg_us(&msg, &data, 1);
	if (msg==CRCInput::RC_home)
	{
		// home key cancel scan
		bCancel=true;
	}
	else if (msg > CRCInput::RC_MaxRC && msg != CRCInput::RC_timeout)
	{
		// other event, save to low priority queue
		g_RCInput->postMsg( msg, data, false );
	}
	if(bCancel)
		return;

	if ((m_Mode != ModeSC) && ((rpath.empty()) || ((*rpath.rbegin()) != '/')))
		rpath += '/';

	CFileList tmplist;
	if(!readDir(rpath, &tmplist))
	{
		perror(("Recursive scandir: "+rpath).c_str());
	}
	else
	{
		n = tmplist.size();
		if(progress)
			progress->showStatusMessageUTF(FILESYSTEM_ENCODING_TO_UTF8_STRING(rpath));

		for (int i = 0; i < n; i++)
		{
			if(progress)
				progress->showGlobalStatus(100/n*i);

			std::string basename = tmplist[i].Name.substr(tmplist[i].Name.rfind('/')+1);
			if( basename != ".." )
			{
				if(Filter != NULL && (!tmplist[i].isDir()) && use_filter)
				{
					if(!Filter->matchFilter(tmplist[i].Name))
						continue;
				}
				if(!tmplist[i].isDir())
					re_filelist->push_back(tmplist[i]);
				else
					addRecursiveDir(re_filelist,tmplist[i].Name, false, progress);
			}
		}
	}
}

void CFileBrowser::hide()
{
	frameBuffer->paintBackgroundBoxRel(x,y, width,height);
}

void CFileBrowser::paintItem(unsigned int pos)
{
	int colwidth1, colwidth2, colwidth3;
	int c_rad_small = 0;
	fb_pixel_t color;
	fb_pixel_t bgcolor;
	int ypos = y+ theight+0 + pos*fheight;
	CFile * actual_file = NULL;
	std::string fileicon;
	unsigned int curr = liststart + pos;

	frameBuffer->paintBoxRel(x,ypos, width- 15, fheight, COL_MENUCONTENT_PLUS_0/*DARK*/);

	if (curr >= filelist.size())
		return;

	actual_file = &filelist[curr];
	if (curr == selected)
	{
		color   = actual_file->Marked ? COL_MENUCONTENTINACTIVE_TEXT : COL_MENUCONTENTSELECTED_TEXT;
		bgcolor = actual_file->Marked ? COL_MENUCONTENTSELECTED_PLUS_2 : COL_MENUCONTENTSELECTED_PLUS_0;
		c_rad_small = RADIUS_SMALL;
	}
	else if (actual_file->Marked)
	{
		color   = COL_MENUCONTENT_TEXT;
		bgcolor = COL_MENUCONTENT_PLUS_2;
	}
	else
	{
		color   = COL_MENUCONTENT_TEXT;//DARK;
		bgcolor = COL_MENUCONTENT_PLUS_0;//DARK;
	}

	frameBuffer->paintBoxRel(x,ypos, width- 15, fheight, bgcolor, c_rad_small);

	if (g_settings.filebrowser_showrights == 0 && S_ISREG(actual_file->Mode))
		colwidth2 = 0;
	else
		colwidth2 = fnt_item->getRenderWidth("rwxrwxrwx");
	colwidth3 = fnt_item->getRenderWidth("222.222G");
	colwidth1 = width - 35 - colwidth2 - colwidth3 - 10;

	if ( actual_file->Name.length() > 0 )
	{
		if (curr == selected)
			CVFD::getInstance()->showMenuText(0, FILESYSTEM_ENCODING_TO_UTF8_STRING(actual_file->getFileName()).c_str(), -1, true); // UTF-8

		switch(actual_file->getType())
		{
			case CFile::FILE_CDR:
			case CFile::FILE_MP3:
			case CFile::FILE_OGG:
			case CFile::FILE_WAV:
			case CFile::FILE_FLAC:
			case CFile::FILE_AAC:
				fileicon = NEUTRINO_ICON_MP3;
				break;

			case CFile::FILE_DIR:
				fileicon = NEUTRINO_ICON_FOLDER;
				break;

			case CFile::FILE_PICTURE:
			case CFile::FILE_TEXT:
			default:
				fileicon = NEUTRINO_ICON_FILE;
		}
		frameBuffer->paintIcon(fileicon, x+5 , ypos + (fheight-16) / 2 );

		fnt_item->RenderString(x + 35, ypos + fheight, colwidth1 - 10 , FILESYSTEM_ENCODING_TO_UTF8_STRING(actual_file->getFileName()), color);

		if( S_ISREG(actual_file->Mode) )
		{
			if (g_settings.filebrowser_showrights != 0)
			{
				const char * attribute = "xwr";
				char modestring[9 + 1];
				for (int m = 8; m >= 0; m--)
					modestring[8 - m] = (actual_file->Mode & (1 << m)) ? attribute[m % 3] : '-';

				modestring[9] = 0;

				fnt_item->RenderString(x + width - 25 - colwidth3 - colwidth2 , ypos+ fheight, colwidth2, modestring, color);
			}

#define GIGABYTE 1073741824LL
#define MEGABYTE 1048576LL
#define KILOBYTE 1024LL
			char tmpstr[256];
			const char *unit = "";
			int64_t factor = 0;
			if (actual_file->Size >= GIGABYTE)
			{
				factor = GIGABYTE;
				unit = "G";
			}
			else if (actual_file->Size >= MEGABYTE)
			{
				factor = MEGABYTE;
				unit = "M";
			}
			else if (actual_file->Size >= KILOBYTE)
			{
				factor = KILOBYTE;
				unit = "k";
			}
			if (factor)
			{
				int a = actual_file->Size / factor;
				int b = (actual_file->Size - a * factor) * 1000 / factor;
				snprintf(tmpstr, sizeof(tmpstr), "%d.%03d%s", a, b, unit);
			}
			else
				snprintf(tmpstr,sizeof(tmpstr),"%d", (int)actual_file->Size);

			/* right align file size */
			int sz_w = fnt_item->getRenderWidth(tmpstr);
			fnt_item->RenderString(x + width - sz_w - 25, ypos+ fheight, sz_w, tmpstr, color);
		}

		if(actual_file->isDir())
		{
			char timestring[18];
			time_t rawtime;
			rawtime = actual_file->Time;
			strftime(timestring, 18, "%d-%m-%Y %H:%M", gmtime(&rawtime));
			/* right align directory time */
			int time_w = fnt_item->getRenderWidth(timestring);
			fnt_item->RenderString(x + width - time_w - 25, ypos+ fheight, time_w, timestring, color);
		}
	}
}

void CFileBrowser::paintHead()
{
	char *l_name;
	int i = 0;
	int l;
#ifdef ENABLE_INTERNETRADIO
	if(m_Mode == ModeSC)
		l = asprintf(&l_name, "%s %s", g_Locale->getText(LOCALE_AUDIOPLAYER_ADD_SC), FILESYSTEM_ENCODING_TO_UTF8_STRING(name).c_str());
	else
#endif
		l = asprintf(&l_name, "%s %s", g_Locale->getText(LOCALE_FILEBROWSER_HEAD), FILESYSTEM_ENCODING_TO_UTF8_STRING(name).c_str());

	if (l < 1) /* at least 1 for the " " space */
	{
		perror("CFileBrowser::paintHead asprintf");
		return;
	}

	/* too long? Leave out the "Filebrowser" or "Shoutcast" prefix
	 * the allocated space is sufficient since it is surely shorter than before */
	if (fnt_title->getRenderWidth(l_name) > width - 11)
		l = sprintf(l_name, "%s", FILESYSTEM_ENCODING_TO_UTF8_STRING(name).c_str());
	if (l_name[l - 1] == '/')
		l_name[--l] = '\0';

	/* still too long? the last part is probably more interesting than the first part... */
	while ((fnt_title->getRenderWidth(&l_name[i]) > width - 20) && (i < l))
		i++;

	CComponentsHeader header(x, y, width, theight, &l_name[i]);
	header.paint(CC_SAVE_SCREEN_NO);

	free(l_name);
}

bool chooserDir(char *setting_dir, bool test_dir, const char *action_str, size_t str_leng, bool allow_tmp)
{
	std::string tmp_setting_dir = setting_dir;
	if(chooserDir(tmp_setting_dir, test_dir, action_str,allow_tmp)) {
		strncpy(setting_dir,tmp_setting_dir.c_str(), str_leng);
		return true;
	}
	return false;
}

bool chooserDir(std::string &setting_dir, bool test_dir, const char *action_str, bool allow_tmp)
{
	const char *wrong_str = "Wrong/unsupported";
	CFileBrowser b;
	b.Dir_Mode=true;
	if (b.exec(setting_dir.c_str())) {
		const char * newdir = b.getSelectedFile()->Name.c_str();
		if(test_dir && check_dir(newdir,allow_tmp)) {
			printf("%s %s dir %s\n",wrong_str ,action_str, newdir);
			return false;
		} else {
			setting_dir = b.getSelectedFile()->Name;
			return true;
		}
	}
	return false;
}

const struct button_label FileBrowserFilterButton[2] =
{
	{ NEUTRINO_ICON_BUTTON_BLUE  , LOCALE_FILEBROWSER_FILTER_INACTIVE },
	{ NEUTRINO_ICON_BUTTON_BLUE  , LOCALE_FILEBROWSER_FILTER_ACTIVE   },
};

int CFileBrowser::paintFoot(bool show)
{
	std::string sort_text = g_Locale->getText(LOCALE_MOVIEBROWSER_FOOT_SORT);
	sort_text += g_Locale->getText(sortByNames[g_settings.filebrowser_sortmethod]);

	int sort_text_len = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getRenderWidth(g_Locale->getText(LOCALE_MOVIEBROWSER_FOOT_SORT));
	int len = 0;
	for (int i = 0; i < FILEBROWSER_NUMBER_OF_SORT_VARIANTS; i++)
		len = std::max(len, g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getRenderWidth(g_Locale->getText(sortByNames[i])));

	sort_text_len += len;

	neutrino_locale_t f_loc = LOCALE_FILEBROWSER_FILTER_INACTIVE;
	if (Filter != NULL && use_filter)
		f_loc = LOCALE_FILEBROWSER_FILTER_ACTIVE;

	button_label_ext footerButtons[] = {
		{ NEUTRINO_ICON_BUTTON_RED,		NONEXISTANT_LOCALE,		sort_text.c_str(),	sort_text_len,	false },
		{ NEUTRINO_ICON_BUTTON_OKAY,		LOCALE_FILEBROWSER_SELECT,	NULL,			0,		false },
		{ NEUTRINO_ICON_BUTTON_MUTE_SMALL,	LOCALE_FILEBROWSER_DELETE,	NULL,			0,		false },
		{ NEUTRINO_ICON_BUTTON_PLAY,		LOCALE_FILEBROWSER_MARK,	NULL,			0,		false },
		{ NEUTRINO_ICON_BUTTON_BLUE,		f_loc,				NULL,			0,		false },
	};
	int cnt = sizeof(footerButtons) / sizeof(button_label_ext);
	int fowidth = width - skwidth;

	if (!show)
		return paintButtons(footerButtons, cnt, 0, 0, 0, 0, 0, false, NULL, NULL);

	if (filelist.empty()) {
		frameBuffer->paintBoxRel(x, y + height - foheight, width, foheight, COL_INFOBAR_SHADOW_PLUS_1, RADIUS_MID, CORNER_BOTTOM);
		return foheight;
	}
	int res = paintButtons(footerButtons, Filter ? cnt : cnt - 1, x, y + height - foheight, width, foheight, fowidth);
	paintSMSKey();
	return res;
}

void CFileBrowser::paintSMSKey()
{
	int skheight = fnt_small->getHeight();

	//background
	frameBuffer->paintBoxRel(x + width - skwidth, y + height - foheight, skwidth, foheight, COL_INFOBAR_SHADOW_PLUS_1, RADIUS_MID, CORNER_BOTTOM_RIGHT);

	if(m_SMSKeyInput.getOldKey()!=0)
	{
		char cKey[2] = {m_SMSKeyInput.getOldKey(), 0};
		cKey[0] = toupper(cKey[0]);
		int len = fnt_small->getRenderWidth(cKey);
		fnt_small->RenderString(x + width - skwidth, y + height - foheight + foheight/2 + skheight/2, len, cKey, COL_MENUHEAD_TEXT);
	}
}

void CFileBrowser::paint()
{
	liststart = (selected/listmaxshow)*listmaxshow;

	CVFD::getInstance()->setMode(CVFD::MODE_MENU_UTF8, g_Locale->getText(LOCALE_FILEBROWSER_HEAD));

	for(unsigned int count=0;count<listmaxshow;count++)
		paintItem(count);

	//scrollbar
	int ypos = y+ theight;
	int sb = fheight* listmaxshow;
	frameBuffer->paintBoxRel(x+ width- 15,ypos, 15, sb,  COL_MENUCONTENT_PLUS_1);

	int sbc= ((filelist.size()- 1)/ listmaxshow)+ 1;
	int sbs= (selected/listmaxshow);
	if (sbc < 1)
		sbc = 1;

	frameBuffer->paintBoxRel(x+ width- 13, ypos+ 2+ sbs*(sb-4)/sbc, 11, (sb-4)/sbc, COL_MENUCONTENT_PLUS_3, RADIUS_SMALL);
}

void CFileBrowser::SMSInput(const neutrino_msg_t msg)
{
	unsigned char key = m_SMSKeyInput.handleMsg(msg);

	unsigned int i;
	for(i=(selected+1) % filelist.size(); i != selected ; i= (i+1) % filelist.size())
	{
		if(tolower(filelist[i].getFileName()[0]) == key)
			break;
	}
	int prevselected=selected;
	selected=i;
	paintItem(prevselected - liststart);
	unsigned int oldliststart = liststart;
	liststart = (selected/listmaxshow)*listmaxshow;

	if(oldliststart!=liststart)
		paint();
	else
		paintItem(selected - liststart);
	paintSMSKey();
}

void CFileBrowser::recursiveDelete(const char* file)
{
	struct stat64 statbuf;
	dirent64 **namelist;
	int n;
	printf("Delete %s\n", file);
	if(lstat64(file,&statbuf) == 0)
	{
		if(S_ISDIR(statbuf.st_mode))
		{
			n = scandir64(file, &namelist, 0, alphasort64);
			while(n--)
			{
				if(strcmp(namelist[n]->d_name, ".")!=0 && strcmp(namelist[n]->d_name, "..")!=0)
				{
					std::string fullname = (std::string)file + "/" + namelist[n]->d_name;
					recursiveDelete(fullname.c_str());
				}
				free(namelist[n]);
			}
			free(namelist);
			rmdir(file);
		}
		else
		{
			unlink(file);
		}
	}
	else
		perror(file);
}
