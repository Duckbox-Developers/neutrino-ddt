//=============================================================================
// YHTTPD
// Global System Configuration
//=============================================================================

//-----------------------------------------------------------------------------
#ifndef __yconfig_h__
#define __yconfig_h__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
// c++
#include <string>
#include <map>
#include <vector>

//=============================================================================
// Base Configuration
//=============================================================================
//-----------------------------------------------------------------------------
// System Choice <configure!> ONE choice
//-----------------------------------------------------------------------------
#ifndef CONFIG_SYSTEM_BY_COMPILER			// use Compiler directive to set CONFIG_SYSTEM
#define CONFIG_SYSTEM_TUXBOX	y			// Tuxbox project
#endif
//-----------------------------------------------------------------------------
// General central Definitions <configure!>
//-----------------------------------------------------------------------------
#define HTTPD_VERSION 			"3.4.1"		// Webserver version  (can be overloaded)
#define YHTTPD_VERSION			"1.3.2"		// Webserver version  (Version of yhttpd-core!)
#define IADDR_LOCAL			"127.0.0.1"	// local IP
#define HTTPD_NAME			"yhttpd"	// Webserver name (can be overloaded)
#define YHTTPD_NAME			"yhttpd_core"	// Webserver name (Name of yhttpd-core!)
#define AUTH_NAME_MSG			"yhttpd"	// Name in Authentication Dialogue
#define CONF_VERSION			4		// Version of yhttpd-conf file
#define HTTPD_KEEPALIVE_TIMEOUT		500000		// Timeout for Keep-Alive in mircoseconds
//=============================================================================
// Features wanted <configure!>
//=============================================================================
//-----------------------------------------------------------------------------
// modules
//-----------------------------------------------------------------------------
#define Y_CONFIG_USE_YPARSER y				// Add mod: "y-Parsing"
#define Y_CONFIG_USE_AUTHHOOK y				// Add mod: "Authentication"
#define Y_CONFIG_USE_WEBLOG y				// Add mod: "WebLogging"
#define Y_CONFIG_USE_CACHE y				// Add mod: Can cache production pages
#define Y_CONFIG_USE_SENDFILE y				// Add mod: can send static files (mandantory)
//-----------------------------------------------------------------------------
// Features & Build
//-----------------------------------------------------------------------------
#define Y_CONFIG_FEATURE_CHECK_PORT_AUTORITY y		// System: Port < 1024 need Admin-Privileges-Check
#define Y_CONFIG_HAVE_SENDFILE y			// System: Have *IX SendFile
#define Y_CONFIG_FEATURE_UPLOAD y			// Add Feature: File Upload POST Command
#define Y_CONFIG_USE_HOSTEDWEB y			// Add Feature: Use HOSTED Web
#define Y_CONFIG_FEATURE_SHOW_SERVER_CONFIG y		// Add Feature (in yParser): add /y/server-config
//#define Y_CONFIG_USE_OPEN_SSL y			// Add Feature: use openSSL
//#define Y_CONFIG_FEATURE_KEEP_ALIVE y			// Add Feature: Keep-alive //FIXME: does not work correctly now
#define Y_CONFIG_FEATUE_SENDFILE_CAN_ACCESS_ALL y	// Add Feature: every file can be accessed (use carefully: security!!)
//#define Y_CONFIG_FEATURE_CHROOT y			// Add Feature: Use Change Root for Security
//#define Y_CONFIG_FEATURE_HTTPD_USER y			// Add Feature: Set User for yhttpd-Process
#define Y_CONFIG_FEATURE_THREADING y			// Build with possibility for multi threading
//-----------------------------------------------------------------------------
// Define/Undefine Features forced by CONFIG_SYSTEM_xxx
// Dependencies
//-----------------------------------------------------------------------------
//#ifdef Y_CONFIG_USE_OPEN_SSL
//#undef Y_CONFIG_HAVE_SENDFILE				// Sendfile does not work for SSL, but we'll fallback to send
//#endif

#ifdef CONFIG_SYSTEM_TUXBOX
#define Y_CONFIG_FEATURE_UPLOAD y
#define Y_CONFIG_USE_YPARSER y
#define Y_CONFIG_USE_AUTHHOOK y
#endif

#ifdef Y_CONFIG_FEATURE_KEEP_ALIVE
#define HTTP_PROTOCOL "HTTP/1.1"
#else
#define HTTP_PROTOCOL "HTTP/1.0"
#endif
//=============================================================================
// Configurations for systems/OSs <configure!>
//=============================================================================
//-----------------------------------------------------------------------------
// Configurations for LINUX (Tuxbox)
//-----------------------------------------------------------------------------
#undef HTTPD_NAME
#define HTTPD_NAME			"nhttpd"
#define HTTPD_STANDARD_PORT		80
#define HTTPD_FALLBACK_PORT		8080
#define HTTPD_MAX_CONNECTIONS		50
#define HTTPD_REQUEST_LOG		"/tmp/httpd_log"
#define LOG_FILE			"/tmp/yhttpd.log"
#define LOG_FORMAT			""
#define UPLOAD_TMP_FILE			"/tmp/upload.tmp"
#define CACHE_DIR			"/tmp/.cache"
#define HTTPD_ERRORPAGE			"/Y_ErrorPage.yhtm"
#define HTTPD_SENDFILE_EXT		"htm:text/html,html:text/html,xml:text/xml,txt:text/plain,jpg:image/jpeg,jpeg:image/jpeg,gif:image/gif,png:image/png,bmp:image/bmp,css:text/css,js:text/plain,yjs:text/plain,img:application/octet-stream,ico:image/x-icon,m3u:application/octet-stream,tar:application/octet-stream,gz:text/x-gzip,ts:video/MP2T,mkv:video/x-matroska,avi:video/avi,mp3:audio/mpeg,ogg:audio/ogg"
#define HTTPD_SENDFILE_ALL		"true"
#define HTTPD_LANGUAGEDIR		"languages"
#define HTTPD_DEFAULT_LANGUAGE 		"English"

#define AUTHUSER			"root"
#define AUTHPASSWORD			"tuxbox"

#define HTTPD_CONFIGDIR			CONFIGDIR
#define HTTPD_CONFIGFILE		HTTPD_CONFIGDIR"/nhttpd.conf"
#define YWEB_CONFIGFILE			HTTPD_CONFIGDIR"/Y-Web.conf"
#define NEUTRINO_CONFIGFILE		CONFIGDIR"/neutrino.conf"
#define MOVIEBROWSER_CONFIGFILE		CONFIGDIR"/moviebrowser.conf"

#define PRIVATEDOCUMENTROOT		PRIVATE_HTTPDDIR
#define PUBLICDOCUMENTROOT		PUBLIC_HTTPDDIR
#define HOSTEDDOCUMENTROOT		HOSTED_HTTPDDIR
#define HOSTEDDOCUMENTURL		"/hosted/"
#define EXTRASDOCUMENTROOT		HOSTED_HTTPDDIR"/extras"
#define EXTRASDOCUMENTURL		"/hosted/extras"

#define ZAPITXMLPATH			CONFIGDIR"/zapit"
#define TUXBOX_LOGOS_URL		LOGODIR

#define SSL_PEMFILE			HTTPD_CONFIGDIR "/server.pem"
#define SSL_CA_FILE			HTTPD_CONFIGDIR "/cacert.pem"
//-----------------------------------------------------------------------------
// Aggregated definitions
//-----------------------------------------------------------------------------
#define WEBSERVERNAME HTTPD_NAME "/" HTTPD_VERSION " (" YHTTPD_NAME "/" YHTTPD_VERSION ")"

#endif // __yconfig_h__
