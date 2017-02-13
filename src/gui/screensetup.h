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


#ifndef __screensetup__
#define __screensetup__

#include <gui/widget/menue.h>

#include <string>

class CFrameBuffer;
class CScreenSetup : public CMenuTarget
{
 private:
    CFrameBuffer * frameBuffer;
    CMenuWidget *m;
    int	selected;
    int x_box;
    int y_box;
    int BoxHeight;
    int BoxWidth;
    int x_coord[2];
    int y_coord[2];
#if HAVE_SPARK_HARDWARE || HAVE_DUCKBOX_HARDWARE
    std::string coord[2];
    int x_coord_bak[2];
    int y_coord_bak[2];
    int startX;
    int startY;
    int endX;
    int endY;
    int screenwidth;
    int screenheight;
    bool coord_abs;
    fb_pixel_t color_bak;
    t_channel_id channel_id;
#endif

    void paint();
    void paintBorderUL();
    void paintBorderLR();
#if HAVE_SPARK_HARDWARE || HAVE_DUCKBOX_HARDWARE
    void updateCoords();
#else
    void paintCoords();
#endif
    void paintBorder(int selected);
    void unpaintBorder(int selected);
    void paintIcons(int pselected);
	
 public:
    CScreenSetup();
#if HAVE_SPARK_HARDWARE || HAVE_DUCKBOX_HARDWARE
    void showBorder(t_channel_id cid);
    void hideBorder();
    void resetBorder(t_channel_id cid);
    bool loadBorder(t_channel_id cid);
    void loadBorders(void);
    void saveBorders(void);
#endif
    void hide();
    int exec(CMenuTarget* parent, const std::string & actionKey);

};


#endif
