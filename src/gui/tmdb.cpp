/*
	Copyright (C) 2015 TangoCash

	License: GPLv2

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation;

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

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <fstream>

#include <set>
#include <string>

#include "system/settings.h"
#include "system/helpers.h"
#include "system/set_threadname.h"
#include "gui/widget/hintbox.h"

#include <driver/screen_max.h>

#include <global.h>
#include <json/json.h>

#include "tmdb.h"

#if LIBCURL_VERSION_NUM < 0x071507
#include <curl/types.h>
#endif

#define URL_TIMEOUT 60
#define TMDB_COVER "/tmp/tmdb.jpg"

cTmdb::cTmdb(std::string epgtitle)
{
	frameBuffer = CFrameBuffer::getInstance();
	minfo.epgtitle = epgtitle;
	curl_handle = curl_easy_init();
	form = NULL;

	ox = frameBuffer->getScreenWidthRel(false);
	oy = frameBuffer->getScreenHeightRel(false);

	int buttonheight = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getHeight() + 6;
	toph = g_Font[SNeutrinoSettings::FONT_TYPE_EPG_TITLE]->getHeight() + 6;

	sx = getScreenStartX(ox);
	sy = getScreenStartY(oy + buttonheight); /* button box is handled separately (why?) */

#ifdef TMDB_API_KEY
	key = TMDB_API_KEY;
#else
	key = g_settings.tmdb_api_key;
#endif

	CHintBox* box = new CHintBox(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_TMDB_READ_DATA));
	box->paint();

	std::string lang = Lang2ISO639_1(g_settings.language);
	GetMovieDetails(lang);
	if ((minfo.result < 1 || minfo.overview.empty()) && lang != "en")
		GetMovieDetails("en");

	if (box != NULL) {
		box->hide();
		delete box;
	}
}

cTmdb::~cTmdb()
{
	curl_easy_cleanup(curl_handle);
	delete form;
}

size_t cTmdb::CurlWriteToString(void *ptr, size_t size, size_t nmemb, void *data)
{
	if (size * nmemb > 0) {
		std::string* pStr = (std::string*) data;
		pStr->append((char*) ptr, nmemb);
	}
	return size*nmemb;
}

std::string cTmdb::decodeUrl(std::string url)
{
	char * str = curl_easy_unescape(curl_handle, url.c_str(), 0, NULL);
	if (str)
		url = str;
	curl_free(str);
	return url;
}

std::string cTmdb::encodeUrl(std::string txt)
{
	char * str = curl_easy_escape(curl_handle, txt.c_str(), txt.length());
	if (str)
		txt = str;
	curl_free(str);
	return txt;
}

bool cTmdb::getUrl(std::string &url, std::string &answer, CURL *_curl_handle)
{
	printf("[TMDB]: %s\n",__func__);
	if (!_curl_handle)
		_curl_handle = curl_handle;

	curl_easy_setopt(_curl_handle, CURLOPT_URL, url.c_str());
	curl_easy_setopt(_curl_handle, CURLOPT_WRITEFUNCTION, &cTmdb::CurlWriteToString);
	curl_easy_setopt(_curl_handle, CURLOPT_FILE, (void *)&answer);
	curl_easy_setopt(_curl_handle, CURLOPT_FAILONERROR, 1);
	curl_easy_setopt(_curl_handle, CURLOPT_TIMEOUT, URL_TIMEOUT);
	curl_easy_setopt(_curl_handle, CURLOPT_NOSIGNAL, (long)1);
	curl_easy_setopt(_curl_handle, CURLOPT_SSL_VERIFYPEER, false);

	if (!g_settings.softupdate_proxyserver.empty()) {
		curl_easy_setopt(_curl_handle, CURLOPT_PROXY, g_settings.softupdate_proxyserver.c_str());
		if (!g_settings.softupdate_proxyusername.empty()) {
			std::string tmp = g_settings.softupdate_proxyusername + ":" + g_settings.softupdate_proxypassword;
			curl_easy_setopt(_curl_handle, CURLOPT_PROXYUSERPWD, tmp.c_str());
		}
	}

	char cerror[CURL_ERROR_SIZE];
	curl_easy_setopt(_curl_handle, CURLOPT_ERRORBUFFER, cerror);

	printf("try to get [%s] ...\n", url.c_str());
	CURLcode httpres = curl_easy_perform(_curl_handle);

	printf("http: res %d size %d\n", httpres, (int)answer.size());

	if (httpres != 0 || answer.empty()) {
		printf("error: %s\n", cerror);
		return false;
	}
	return true;
}

bool cTmdb::DownloadUrl(std::string url, std::string file, CURL *_curl_handle)
{
	if (!_curl_handle)
		_curl_handle = curl_handle;

	FILE * fp = fopen(file.c_str(), "wb");
	if (fp == NULL) {
		perror(file.c_str());
		return false;
	}
	curl_easy_setopt(_curl_handle, CURLOPT_URL, url.c_str());
	curl_easy_setopt(_curl_handle, CURLOPT_WRITEFUNCTION, NULL);
	curl_easy_setopt(_curl_handle, CURLOPT_FILE, fp);
	curl_easy_setopt(_curl_handle, CURLOPT_FAILONERROR, 1);
	curl_easy_setopt(_curl_handle, CURLOPT_TIMEOUT, URL_TIMEOUT);
	curl_easy_setopt(_curl_handle, CURLOPT_NOSIGNAL, (long)1);
	curl_easy_setopt(_curl_handle, CURLOPT_SSL_VERIFYPEER, false);

	if (!g_settings.softupdate_proxyserver.empty()) {
		curl_easy_setopt(_curl_handle, CURLOPT_PROXY, g_settings.softupdate_proxyserver.c_str());
		if (!g_settings.softupdate_proxyusername.empty()) {
			std::string tmp = g_settings.softupdate_proxyusername + ":" + g_settings.softupdate_proxypassword;
			curl_easy_setopt(_curl_handle, CURLOPT_PROXYUSERPWD, tmp.c_str());
		}
	}

	char cerror[CURL_ERROR_SIZE];
	curl_easy_setopt(_curl_handle, CURLOPT_ERRORBUFFER, cerror);

	printf("try to get [%s] ...\n", url.c_str());
	CURLcode httpres = curl_easy_perform(_curl_handle);

	double dsize;
	curl_easy_getinfo(_curl_handle, CURLINFO_SIZE_DOWNLOAD, &dsize);
	fclose(fp);

	printf("http: res %d size %g.\n", httpres, dsize);

	if (httpres != 0) {
		printf("curl error: %s\n", cerror);
		unlink(file.c_str());
		return false;
	}
	return true;
}

bool cTmdb::GetMovieDetails(std::string lang)
{
	printf("[TMDB]: %s\n",__func__);
	std::string url	= "http://api.themoviedb.org/3/search/multi?api_key="+key+"&language="+lang+"&query=" + encodeUrl(minfo.epgtitle);
	std::string answer;
	if (!getUrl(url, answer))
		return false;

	Json::Value root;
	Json::Reader reader;
	bool parsedSuccess = reader.parse(answer, root, false);
	if (!parsedSuccess) {
		printf("Failed to parse JSON\n");
		printf("%s\n", reader.getFormattedErrorMessages().c_str());
		return false;
	}

	minfo.result = root.get("total_results",0).asInt();

	printf("[TMDB]: results: %d\n",minfo.result);

	if (minfo.result > 0) {
		Json::Value elements = root["results"];
		minfo.id = elements[0].get("id",-1).asInt();
		minfo.media_type = elements[0].get("media_type","").asString();
		if (minfo.id > -1) {
			url = "http://api.themoviedb.org/3/"+minfo.media_type+"/"+to_string(minfo.id)+"?api_key="+key+"&language="+lang+"&append_to_response=credits";
			answer.clear();
			if (!getUrl(url, answer))
				return false;
			parsedSuccess = reader.parse(answer, root, false);
			if (!parsedSuccess) {
				printf("Failed to parse JSON\n");
				printf("%s\n", reader.getFormattedErrorMessages().c_str());
				return false;
			}

			minfo.overview = root.get("overview","").asString();
			minfo.poster_path = root.get("poster_path","").asString();
			minfo.original_title = root.get("original_title","").asString();;
			minfo.release_date = root.get("release_date","").asString();;
			minfo.vote_average = root.get("vote_average","").asString();;
			minfo.vote_count = root.get("vote_count",0).asInt();;
			minfo.runtime = root.get("runtime",0).asInt();;
			if (minfo.media_type == "tv") {
				minfo.original_title = root.get("original_name","").asString();;
				minfo.episodes = root.get("number_of_episodes",0).asInt();;
				minfo.seasons = root.get("number_of_seasons",0).asInt();;
				minfo.release_date = root.get("first_air_date","").asString();;
				elements = root["episode_run_time"];
				minfo.runtimes = elements[0].asString();
				for (unsigned int i= 1; i<elements.size();i++) {
					minfo.runtimes +=  + ", "+elements[i].asString();
				}
			}

			elements = root["genres"];
			minfo.genres = elements[0].get("name","").asString();
			for (unsigned int i= 1; i<elements.size();i++) {
				minfo.genres += ", " + elements[i].get("name","").asString();
			}

			elements = root["credits"]["cast"];
			for (unsigned int i= 0; i<elements.size() && i<10;i++) {
				minfo.cast +=  "  "+elements[i].get("character","").asString()+" ("+elements[i].get("name","").asString() + ")\n";
				//printf("test: %s (%s)\n",elements[i].get("character","").asString().c_str(),elements[i].get("name","").asString().c_str());
			}

			unlink(TMDB_COVER);
			if (hasCover())
				getBigCover(TMDB_COVER);
			//printf("[TMDB]: %s (%s) %s\n %s\n %d\n",minfo.epgtitle.c_str(),minfo.original_title.c_str(),minfo.release_date.c_str(),minfo.overview.c_str(),minfo.found);

			return true;
		}
	} else
		return false;

	return false;
}

std::string cTmdb::CreateEPGText()
{
	std::string epgtext;
	epgtext += "\n";
	epgtext += "Vote: "+minfo.vote_average.substr(0,3)+"/10 Votecount: "+to_string(minfo.vote_count)+"\n";
	epgtext += "\n";
	epgtext += minfo.overview+"\n";
	epgtext += "\n";
	if (minfo.media_type == "tv")
		epgtext += (std::string)g_Locale->getText(LOCALE_EPGVIEWER_LENGTH)+": "+minfo.runtimes+"\n";
	else
		epgtext += (std::string)g_Locale->getText(LOCALE_EPGVIEWER_LENGTH)+": "+to_string(minfo.runtime)+"\n";
	epgtext += (std::string)g_Locale->getText(LOCALE_EPGVIEWER_GENRE)+": "+minfo.genres+"\n";
	epgtext += (std::string)g_Locale->getText(LOCALE_EPGEXTENDED_ORIGINAL_TITLE) +" : "+ minfo.original_title+"\n";
	epgtext += (std::string)g_Locale->getText(LOCALE_EPGEXTENDED_YEAR_OF_PRODUCTION)+" : "+ minfo.release_date.substr(0,4) +"\n";
	if (minfo.media_type == "tv")
		epgtext += "Seasons/Episodes: "+to_string(minfo.seasons)+"/"+to_string(minfo.episodes)+"\n";
	if (!minfo.cast.empty())
		epgtext += (std::string)g_Locale->getText(LOCALE_EPGEXTENDED_ACTORS)+":\n"+ minfo.cast+"\n";
	return epgtext;
}

void cTmdb::exec()
{
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;

	if (form == NULL)
		form = new CComponentsForm();
	form->setDimensionsAll(sx, sy, ox, oy);

	CComponentsHeader *header  = new CComponentsHeader(0, 0, ox, toph);
	CComponentsText *headerText = new CComponentsText(15, 0, ox-15, toph, getTitle(), CTextBox::NO_AUTO_LINEBREAK, g_Font[SNeutrinoSettings::FONT_TYPE_EPG_TITLE]);;
	headerText->doPaintBg(false);
	headerText->setTextColor(COL_MENUHEAD_TEXT);
	form->addCCItem(header);
	form->addCCItem(headerText);


	CComponentsPicture *ptmp = new CComponentsPicture(5, toph+5, "/tmp/tmdb.jpg");
	ptmp->setWidth(342);
	ptmp->setHeight(513);
	ptmp->setColorBody(COL_BLUE);
	ptmp->setCorner(RADIUS_MID, CORNER_TOP_LEFT);
	form->addCCItem(ptmp);

	CComponentsText *des = new CComponentsText(ptmp->getWidth()+10, toph+5, form->getWidth()-ptmp->getWidth()-10, form->getHeight(), CreateEPGText(), CTextBox::AUTO_LINEBREAK_NO_BREAKCHARS | CTextBox::TOP);
	des->setCorner(RADIUS_MID, CORNER_BOTTOM_RIGHT);
	des->setTextFont(g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]);
	form->addCCItem(des);

	form->paint();
	frameBuffer->blit();

	while (1) {
		g_RCInput->getMsg(&msg, &data, 100);
		if (msg == CRCInput::RC_home)
			break;
	}

	if (form->isPainted()) {
		form->hide();
		delete form;
		form = NULL;
	}
}
