/*
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


#ifndef __httptool__
#define __httptool__

#include <gui/widget/progresswindow.h>

#include <string>

class CHTTPTool
{
	private:
		std::string userAgent;
		int	iGlobalProgressEnd;
		int	iGlobalProgressBegin;

		CProgressWindow*	statusViewer;
		static int show_progress( void *clientp, double dltotal, double dlnow, double ultotal, double ulnow);
		static size_t CurlWriteToString(void *ptr, size_t size, size_t nmemb, void *data);

	public:
		CHTTPTool();
		void setStatusViewer( CProgressWindow* statusview );

		bool downloadFile( const std::string & URL, const char * const downloadTarget, int globalProgressEnd=-1 );
		std::string downloadString(const std::string & URL, int globalProgressEnd=-1 );

};


#endif
