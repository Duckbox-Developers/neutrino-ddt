/*
	Based up Neutrino-GUI - Tuxbox-Project 
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2012-2014, Thilo Graf 'dbt'
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

	You should have received a copy of the GNU General Public
	License along with this program; if not, write to the
	Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
	Boston, MA  02110-1301, USA.
*/

#ifndef __CC_ITEM_PICTURE_H__
#define __CC_ITEM_PICTURE_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "cc_base.h"
#include "cc_item.h"
#include <string>
#include <driver/pictureviewer/pictureviewer.h>

#define NO_SCALE false
#define SCALE true

//! Sub class of CComponentsItem. Shows box with image with assigned attributes.
/*!
Picture is usable as an object like each other CCItems.
*/

class CComponentsPicture : public CComponentsItem
{
	protected:
		///possible image formats
		std::vector<std::string> v_ext;

		///option to enable disable cache, default = false
		bool enable_cache;
		///screen cache content for painted image
		fb_pixel_t *image_cache;

		///current original image dimensions
		int dx, dy;

		///property: name of image (without extensionn) full path to image (with extension), icon names to find in /widget/icons.h, icons will paint never scaled
		std::string pic_name, pic_name_old;
 
		///indicate that image was sucessful painted
		bool is_image_painted;

		///sets that image may be painted, default = false
		bool do_paint;

		///set the transparency of pictures (default = CFrameBuffer::TM_NONE)
		int image_transparent;

		///set internel paint mode to allow/disallow scale an image, value is assigned with constructor, if defined dimensions w and h = 0, then image scale is enabled
		bool do_scale;
		///sets internal option for keeping aspect, see also setHeight(), setWidth(), default value = false
		bool keep_dx_aspect;
		bool keep_dy_aspect;

		void init(	const int &x_pos, const int &y_pos, const int &w, const int &h,
				const std::string& image_name,
				CComponentsForm *parent,
				int shadow_mode,
				fb_pixel_t color_frame,
				fb_pixel_t color_background,
				fb_pixel_t color_shadow,
				int transparent,
				bool allow_scale);

		///initialize all required attributes
		void initCCItem();
		///initialize position of picture object dependendly from settings
		void initPosition(int *x_position, int *y_position);
		///paint image
		void paintPicture();

	public:
		/*!
		Constructor for image objects: use this for scaled images.
		Dimensions are defined with parameters w (width) and h (height).
		Note: only dimension values >0 causes scaling of image!
		Note: See also class CComponentsPictureScalable(). That does the same like this, but uses internal value 0 for parameters w (width) and h (height).
		If scaling is not required, you should use overloaded version that comes without dimension parameters.
		If no dimensions are defined (in constructor or e.g. with setWidth() or setHeight(),
		width and height are defined by image itself and are retrievable e.g. with getWidth() or getHeight().
		*/
		CComponentsPicture( 	const int &x_pos, const int &y_pos, const int &w, const int &h,
					const std::string& image_name,
					CComponentsForm *parent = NULL,
					int shadow_mode = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6,
					fb_pixel_t color_background = 0,
					fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0,
					int transparent = CFrameBuffer::TM_NONE);

		/*!
		Constructor for image objects, use this for non scaled images. This is similar with known method paintIcon() from framebuffer class.
		Dimensions are defined by image itself.
		Note: you can use the dimension setters setWidth() or setHeight() too, but this has only effects for item body, not for image!
		If scaling is required, you should use overloaded version above, that comes with dimension parameters or use
		class CComponentsPictureScalable().
		*/
		CComponentsPicture( 	const int &x_pos, const int &y_pos,
					const std::string& image_name,
					CComponentsForm *parent = NULL,
					int shadow_mode = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6,
					fb_pixel_t color_background = 0,
					fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0,
					int transparent = CFrameBuffer::TM_NONE);
		virtual~CComponentsPicture()
		{
			delete[]image_cache;
		}

		///sets an image name (unscaled icons only), full image path or url to an image file
		virtual void setPicture(const std::string& picture_name);
		///sets an image name (unscaled icons only), full image path or url to an image file
		virtual void setPicture(const char* picture_name);

		///handle image size
		virtual void getSize(int* width_image, int *height_image);
		///return width of component
		virtual int getWidth();
		///return height of component
		virtual int getHeight();

		///set width of object and image, value >0 causes scale of image, parameter keep_aspect = true causes scaling of height with same aspect, default = false
		virtual void setWidth(const int& w, bool keep_aspect = false);
		///set height of object and image, value >0 causes scale of image, parameter keep_aspect = true causes scaling of width with same aspect, , default = false
		virtual void setHeight(const int& h, bool keep_aspect = false);
		///set width of object and image related to current screen size, see also CComponentsItem::setWidthP(), parameter as uint8_t
		virtual void setWidthP(const uint8_t& w_percent){CComponentsItem::setWidthP(w_percent), do_scale = true; initCCItem();}
		///set height of object and image related to current screen size, see also CComponentsItem::setHeightP(), parameter as uint8_t
		virtual void setHeightP(const uint8_t& h_percent){CComponentsItem::setHeightP(h_percent), do_scale = true; initCCItem();}

		///return paint mode of internal image, true=image was painted, please do not to confuse with isPainted()! isPainted() is related to item itself.
		virtual inline bool isPicPainted(){return is_image_painted;};

		/**sets transparency mode if icons
		 * @param[in] t 	Transparency mode
		 * 			@li t = CFrameBuffer::TM_BLACK : Transparency when black content ('pseudo' transparency)
		 * 			@li t = CFrameBuffer::TM_NONE : No 'pseudo' transparency
		*/
		void SetTransparent(int t){ image_transparent = t; }

		///paint item
		virtual void paint(bool do_save_bg = CC_SAVE_SCREEN_YES);
		///hide item, see also CComponents::hide();
		virtual void hide();

		virtual bool hasChanges();

		///remove possible cache
		virtual void clearCache();
		///enable/disable image cache
		virtual void enableCache(bool enable = true){if (enable_cache == enable) return; enable_cache = enable; if (!enable_cache) clearCache();}
		///disable image cache, makes clean up too
		virtual void disableCache(){enableCache(false);}
};

class 	CComponentsPictureScalable : public CComponentsPicture
{
	public:
		/*!
		Constructor for image objects: use this for scaled images.
		Does the same like class CComponentsPicture() with assigned value 0 for parameters w (width) and h (height).
		*/
		CComponentsPictureScalable( 	const int &x_pos, const int &y_pos,
						const std::string& image_name,
						CComponentsForm *parent = NULL,
						int shadow_mode = CC_SHADOW_OFF,
						fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6,
						fb_pixel_t color_background = 0,
						fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0,
						int transparent = CFrameBuffer::TM_NONE)
						: CComponentsPicture(x_pos, y_pos, 0, 0, image_name, parent, shadow_mode, color_frame, color_background, color_shadow, transparent){};
};

class CComponentsChannelLogo : public CComponentsPicture
{
	private:
		///channel id
		uint64_t channel_id;
		///channel name
		std::string channel_name;
		
		///alternate image file, if no channel logo is available
		std::string alt_pic_name;
		
		///indicates that logo is available, after paint or new instance, value = false
		bool has_logo;

		void init(const uint64_t& channelId, const std::string& channelName, bool allow_scale);

	public:
		/*!
		Constructor for channel image objects: use this for scaled channel logos.
		Does the same like class CComponentsPicture() with parameters w (width) and h (height), see above!
		Requires parameter channel_name or channelId instead image filename
		NOTE: channel name string is prefered!
		*/
		CComponentsChannelLogo( const int &x_pos, const int &y_pos, const int &w, const int &h,
					const std::string& channelName = "",
					const uint64_t& channelId =0,
					CComponentsForm *parent = NULL,
					int shadow_mode = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6,
					fb_pixel_t color_background = 0,
					fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0,
					int transparent = CFrameBuffer::TM_BLACK);

		/*!
		Constructor for channel image objects: use this for non scaled channel logos.
		Does the same like class CComponentsPicture() without parameters w (width) and h (height), see above!
		Requires parameter channel_name or channelId instead image filename
		NOTE: channel name string is prefered!
		*/
		CComponentsChannelLogo( const int &x_pos, const int &y_pos,
					const std::string& channelName = "",
					const uint64_t& channelId =0,
					CComponentsForm *parent = NULL,
					int shadow_mode = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6,
					fb_pixel_t color_background = 0,
					fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0,
					int transparent = CFrameBuffer::TM_BLACK);

		///set channel id and/or channel name, NOTE: channel name is prefered
		void setChannel(const uint64_t& channelId, const std::string& channelName);
		
		///set an alternate logo if no logo is available NOTE: value of has_logo will set to true
		void setAltLogo(const std::string& picture_name);
		///set an alternate logo if no logo is available, NOTE: value of has_logo will set to true
		void setAltLogo(const char* picture_name);
		
		///returns true, if any logo is available, also if an alternate logo was setted
		bool hasLogo(){return has_logo;};
		
};

class 	CComponentsChannelLogoScalable : public CComponentsChannelLogo
{
	public:
		/*!
		Constructor for channel image objects: use this for scaled channel logos.
		Does the same like class CComponentsPictureScalable(), see above!
		Requires parameter channel_name or channelId instead image filename.
		NOTE: channel name string is prefered!
		*/
		CComponentsChannelLogoScalable( const int &x_pos, const int &y_pos,
						const std::string& channelName = "",
						const uint64_t& channelId =0,
						CComponentsForm *parent = NULL,
						int shadow_mode = CC_SHADOW_OFF,
						fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6,
						fb_pixel_t color_background = 0,
						fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0,
						int transparent = CFrameBuffer::TM_BLACK)
						: CComponentsChannelLogo(x_pos, y_pos, 0, 0, channelName, channelId, parent, shadow_mode, color_frame, color_background, color_shadow, transparent){};
};

#endif
