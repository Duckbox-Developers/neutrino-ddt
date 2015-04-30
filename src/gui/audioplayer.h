/*
  $Id: audioplayer.h,v 1.24 2009/10/03 10:36:29 seife Exp $
  Neutrino-GUI  -   DBoxII-Project

  Copyright (C) 2001 Steffen Hehn 'McClean'
  Homepage: http://dbox.cyberphoria.org/

  Kommentar:

  Diese GUI wurde von Grund auf neu programmiert und sollte nun vom
  Aufbau und auch den Ausbaumoeglichkeiten gut aussehen. Neutrino basiert
  auf der Client-Server Idee, diese GUI ist also von der direkten DBox-
  Steuerung getrennt. Diese wird dann von Daemons uebernommen.


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

#ifndef __audioplayergui__
#define __audioplayergui__


#include <driver/framebuffer.h>
#include <driver/audiofile.h>
#include <gui/filebrowser.h>
#include <gui/components/cc.h>
#include <gui/widget/menue.h>

#include <xmltree/xmlinterface.h>

#include <string>
#include <set>
#include <map>
#include <cstdlib>
#include <ctime>

typedef std::set<long> CPosList;

typedef std::map<unsigned char, CPosList> CTitle2Pos;
typedef std::pair<unsigned char, CPosList> CTitle2PosItem;

class CAudiofileExt : public CAudiofile
{
public:

	CAudiofileExt();

	CAudiofileExt(std::string name, CFile::FileType type);

	CAudiofileExt(const CAudiofileExt& src);

	void operator=(const CAudiofileExt& src);


	char firstChar;
};

typedef std::vector<CAudiofileExt> CAudioPlayList;

class RandomNumber
{
 public:
	RandomNumber()
	{
		srand(time(0));
	}

	int operator()(int n)
	{
		return (n * rand() / RAND_MAX);
	}
};

class CAudioPlayerGui : public CMenuTarget
{
 public:
	enum State
	{
		PLAY=0,
		STOP,
		PAUSE,
		FF,
		REV
	};

	enum DisplayOrder {ARTIST_TITLE = 0, TITLE_ARTIST=1};

 private:
	void Init(void);
	CFrameBuffer * m_frameBuffer;
	unsigned int   m_selected;
	int            m_current;
	unsigned int   m_liststart;
	unsigned int   m_listmaxshow;
	int            m_fheight; // Fonthoehe Playlist-Inhalt
	int            m_theight; // Fonthoehe Playlist-Titel
	int            m_sheight; // Fonthoehe MP Info
	int            m_buttonHeight;
	int            m_title_height;
	int            m_info_height;
	int            m_key_level;
	bool           m_visible;
	State          m_state;
	time_t         m_time_total;
	time_t         m_time_played;
	std::string    m_metainfo;
	bool           m_select_title_by_name;
	bool           m_show_playlist;

	bool           m_playlistHasChanged;

	CAudioPlayList      m_playlist;
	CAudioPlayList      m_radiolist;
	CAudioPlayList      m_filelist;
	CTitle2Pos     m_title2Pos;
	CAudiofileExt  m_curr_audiofile;
	std::string    m_Path;

	int            m_width;
	int            m_height;
	int            m_x;
	int            m_y;
	int            m_title_w;

	int            m_LastMode;
	int            m_idletime;
	bool          m_screensaver;
	bool          m_inetmode;
	CComponentsDetailLine *dline;
	CComponentsInfoBox *ibox;

	SMSKeyInput    m_SMSKeyInput;

	void paintItem(int pos);
	void paint();
	void paintHead();
	void paintFoot();
	void paintInfo();
	void paintCover();
	void paintLCD();
	void hide();

	void get_id3(CAudiofileExt * audiofile);
	void get_mp3info(CAudiofileExt * audiofile);
	CFileFilter audiofilefilter;
	void paintItemID3DetailsLine (int pos);
	void clearItemID3DetailsLine ();
	void ff(unsigned int seconds=0);
	void rev(unsigned int seconds=0);
	int getNext();
	void GetMetaData(CAudiofileExt &File);
	void updateMetaData();
	void updateTimes(const bool force = false);
	void showMetaData();
	void screensaver(bool on);
	bool getNumericInput(neutrino_msg_t& msg,int& val);

	void addToPlaylist(CAudiofileExt &file);
	void removeFromPlaylist(long pos);

	/**
	 * Adds an url (shoutcast, ...) to the to the audioplayer playlist
	 */
	void addUrl2Playlist(const char *url, const char *name = NULL, const time_t bitrate = 0);

	/**
	 * Adds a url which points to an .m3u format (playlist, ...) to the audioplayer playlist
	 */
	void processPlaylistUrl(const char *url, const char *name = NULL, const time_t bitrate = 0);

	/**
	 * Loads a given XML file of internet audiostreams or playlists and processes them
	 */
	void scanXmlFile(std::string filename);

	/**
	 * Processes a loaded XML file/data of internet audiostreams or playlists
	 */
	void scanXmlData(xmlDocPtr answer_parser, const char *nametag, const char *urltag, const char *bitratetag = NULL, bool usechild = false);

	/**
	 * Reads the icecast directory (XML file) and calls scanXmlData
	 */
	void readDir_ic(void);

	void selectTitle(unsigned char selectionChar);
	/**
	 * Appends the file information to the given string.
	 * @param fileInfo a string where the file information will be appended
	 * @param file the file to return the information for
	 */
	void getFileInfoToDisplay(std::string& fileInfo, CAudiofileExt &file);

	void printSearchTree();

	void buildSearchTree();

	unsigned char getFirstChar(CAudiofileExt &file);

	void printTimevalDiff(timeval &start, timeval &end);

	/**
	 * Saves the current playlist into a .m3u playlist file.
	 */
	void savePlaylist();

	/**
	 * Converts an absolute filename to a relative one
	 * as seen from a file in fromDir.
	 * Example:
	 * absFilename: /mnt/audio/A/abc.mp3
	 * fromDir: /mnt/audio/B
	 * => ../A/abc.mp3 will be returned
	 * @param fromDir the directory from where we want to
	 * access the file
	 * @param absFilename the file we want to access in a
	 * relative way from fromDir (given as an absolute path)
	 * @return the location of absFilename as seen from fromDir
	 * (relative path)
	 */
	std::string absPath2Rel(const std::string& fromDir,
				const std::string& absFilename);

	/**
	 * Asks the user if the file filename should be overwritten or not
	 * @param filename the name of the file
	 * @return true if file should be overwritten, false otherwise
	 */
	bool askToOverwriteFile(const std::string& filename);
	bool openFilebrowser(void);
	bool openSCbrowser(void);
	bool clearPlaylist(void);
	bool shufflePlaylist(void);

	bool pictureviewer;

 public:
	CAudioPlayerGui(bool inetmode = false);
	~CAudioPlayerGui();
	int show();
	int exec(CMenuTarget* parent, const std::string & actionKey);

	void wantNextPlay();
	void pause();
	void play(unsigned int pos);
	void stop();
	bool playNext(bool allow_rotate = false);
	bool playPrev(bool allow_rotate = false);
	int getAudioPlayerM_current() {return m_current;}
};


#endif
