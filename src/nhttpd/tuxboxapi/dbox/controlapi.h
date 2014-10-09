//=============================================================================
// NHTTPD
// Neutrino ControlAPI
//=============================================================================
#ifndef __nhttpd_neutrinocontrolapi_hpp__
#define __nhttpd_neutrinocontrolapi_hpp__
// C++
#include <string>
// yhttpd
#include "yhook.h"

// forward declaration
class CNeutrinoAPI;

//-----------------------------------------------------------------------------
class CControlAPI : public Cyhook
{
private:
	// Dispatcher Array
	typedef void (CControlAPI::*TyFunc)(CyhookHandler *hh);
	typedef struct
	{
		const char *func_name;
		TyFunc pfunc;
		const char *mime_type;
	} TyCgiCall;
	const static TyCgiCall yCgiCallList[];

	int rc_send(int ev, unsigned int code, unsigned int value);

	// send functions for ExecuteCGI (controld api)
	void SendEventList(CyhookHandler *hh,t_channel_id channel_id);
	void SendcurrentVAPid(CyhookHandler *hh);
	void SendAllCurrentVAPid(CyhookHandler *hh);
	void SendSettings(CyhookHandler *hh);
	void SendStreamInfo(CyhookHandler *hh);
	void SendBouquets(CyhookHandler *hh);
	void SendBouquet(CyhookHandler *hh,int BouquetNr);
	void SendChannelList(CyhookHandler *hh);
	void SendTimers(CyhookHandler *hh);

	// subs
	friend class CNeutrinoWebserver; // for timer /fb/ compatibility
	void doModifyTimer(CyhookHandler *hh);
	void doNewTimer(CyhookHandler *hh);

	//yweb
	void YWeb_SendVideoStreamingPids(CyhookHandler *hh, int apid_no);
	void YWeb_SendRadioStreamingPid(CyhookHandler *hh);
	void compatibility_Timer(CyhookHandler *hh);
	std::string YexecuteScript(CyhookHandler *hh, std::string cmd);

	// CGI functions for ExecuteCGI
	void TimerCGI(CyhookHandler *hh);
	void SetModeCGI(CyhookHandler *hh);
	void GetModeCGI(CyhookHandler *hh);
	void ExecCGI(CyhookHandler *hh);
	void SystemCGI(CyhookHandler *hh);
	void StandbyCGI(CyhookHandler *hh);
	void EsoundCGI(CyhookHandler *hh);
	void RCCGI(CyhookHandler *hh);
	void GetDateCGI(CyhookHandler *hh);
	void GetTimeCGI(CyhookHandler *hh);
	void SettingsCGI(CyhookHandler *hh);
	void GetServicesxmlCGI(CyhookHandler *hh);
	void GetBouquetsxmlCGI(CyhookHandler *hh);
	void GetChannel_IDCGI(CyhookHandler *hh);
	void MessageCGI(CyhookHandler *hh);
	void InfoCGI(CyhookHandler *hh);
	void ShutdownCGI(CyhookHandler *hh);
	void VolumeCGI(CyhookHandler *hh);
	void ChannellistCGI(CyhookHandler *hh);
	void GetBouquetCGI(CyhookHandler *hh);
	void GetBouquetsCGI(CyhookHandler *hh);
	void EpgCGI(CyhookHandler *hh);
	void VersionCGI(CyhookHandler *hh);
	void ZaptoCGI(CyhookHandler *hh);
	void StartPluginCGI(CyhookHandler *hh);
	void LCDAction(CyhookHandler *hh);
	void YWebCGI(CyhookHandler *hh);
	void RebootCGI(CyhookHandler *hh);
	void RCEmCGI(CyhookHandler *hh);
	void AspectRatioCGI(CyhookHandler *hh);
	void VideoFormatCGI(CyhookHandler *hh);
	void VideoOutputCGI(CyhookHandler *hh);
	void VCROutputCGI(CyhookHandler *hh);
	void ScartModeCGI(CyhookHandler *hh);
	void setBouquetCGI(CyhookHandler *hh);
	void saveBouquetCGI(CyhookHandler *hh);
	void moveBouquetCGI(CyhookHandler *hh);
	void deleteBouquetCGI(CyhookHandler *hh);
	void addBouquetCGI(CyhookHandler *hh);
	void renameBouquetCGI(CyhookHandler *hh);
	void changeBouquetCGI(CyhookHandler *hh);
	void updateBouquetCGI(CyhookHandler *hh);
	void build_live_url(CyhookHandler *hh);

protected:
	static const unsigned int PLUGIN_DIR_COUNT = 5;
	static std::string PLUGIN_DIRS[PLUGIN_DIR_COUNT];
	CNeutrinoAPI	*NeutrinoAPI;

	void init(CyhookHandler *hh);
	void Execute(CyhookHandler *hh);

public:
	// constructor & deconstructor
	CControlAPI(CNeutrinoAPI *_NeutrinoAPI);

	// virtual functions for HookHandler/Hook
	virtual std::string getHookName(void) {return std::string("mod_ControlAPI");}
	virtual std::string 	getHookVersion(void) {return std::string("$Revision$");}
	virtual THandleStatus Hook_SendResponse(CyhookHandler *hh);
	virtual THandleStatus Hook_PrepareResponse(CyhookHandler *hh);
};

#endif /* __nhttpd_neutrinocontrolapi_hpp__ */
