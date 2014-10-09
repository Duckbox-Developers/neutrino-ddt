/*
	Neutrino-GUI  -   DBoxII-Project

	FSMount/Umount by Zwen

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <system/fsmounter.h>
#include <system/helpers.h>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>

#include <global.h>

#include <errno.h>
#include <pthread.h>

#include <sys/mount.h>
#include <unistd.h>

pthread_mutex_t g_mut;
pthread_cond_t g_cond;
pthread_t g_mnt;
int g_mntstatus;

void *mount_thread(void* cmd)
{
	int ret;
	ret=system((const char *) cmd);
	pthread_mutex_lock(&g_mut);
	g_mntstatus=ret;
	pthread_cond_broadcast(&g_cond);
	pthread_mutex_unlock(&g_mut);
	pthread_exit(NULL);
}

CFSMounter::CFSMounter()
{
}

bool in_proc_filesystems(const char * const fsname)
{
	std::string s;
	std::string t;
	std::ifstream in("/proc/filesystems", std::ifstream::in);

	t = fsname;
	
	while (in >> s)
	{
		if (s == t)
	  	{
			in.close();
			return true;
		}
	}
	in.close();
	return false;
}

bool insert_modules(const CFSMounter::FSType fstype)
{
	if (fstype == CFSMounter::NFS)
	{
#ifdef HAVE_MODPROBE
		return (system("modprobe nfs") == 0);
#else
		return ((system("insmod sunrpc") == 0) && (system("insmod lockd") == 0) && (system("insmod nfs") == 0));
#endif
	}
	else if (fstype == CFSMounter::CIFS)
		return (system("insmod cifs") == 0);
	else if (fstype == CFSMounter::LUFS)
		return (system("insmod lufs") == 0);
	return false;
}

bool nfs_mounted_once = false;

bool remove_modules(const CFSMounter::FSType fstype)
{
	if (fstype == CFSMounter::NFS)
	{
		return ((system("rmmod nfs") == 0) && (system("rmmod lockd") == 0) && (system("rmmod sunrpc") == 0));
	}
	else if (fstype == CFSMounter::CIFS)
		return (system("rmmod cifs") == 0);
	else if (fstype == CFSMounter::LUFS)
		return (system("rmmod lufs") == 0);
	return false;
}

CFSMounter::FS_Support CFSMounter::fsSupported(const CFSMounter::FSType fstype, const bool keep_modules)
{
	const char * fsname = NULL;

	if (fstype == CFSMounter::NFS)
		fsname = "nfs";
	else if (fstype == CFSMounter::CIFS)
		fsname = "cifs";
	else if (fstype == CFSMounter::LUFS)
		fsname = "lufs";

	if (in_proc_filesystems(fsname))
		return CFSMounter::FS_READY;

	if (insert_modules(fstype))
	{
		if (in_proc_filesystems(fsname))
		{
			if (keep_modules)
			{
				if (fstype == CFSMounter::NFS)
					nfs_mounted_once = true;
			}
			else
			{
				remove_modules(fstype);
			}

			return CFSMounter::FS_NEEDS_MODULES;
		}
	}
	remove_modules(fstype);
	return CFSMounter::FS_UNSUPPORTED;
}

bool CFSMounter::isMounted(const std::string &local_dir)
{
	std::ifstream in;
	if (local_dir.empty())
		return false;
	
#ifdef PATH_MAX
	char mount_point[PATH_MAX];
#else
	char mount_point[4096];
#endif
	if (realpath(local_dir.c_str(), mount_point) == NULL) {
		printf("[CFSMounter] could not resolve dir: %s: %s\n",local_dir.c_str(), strerror(errno));
		return false;
	}
	in.open("/proc/mounts", std::ifstream::in);
	while(in.good())
	{
		MountInfo mi;
		in >> mi.device >> mi.mountPoint >> mi.type;
		if (strcmp(mi.mountPoint.c_str(),mount_point) == 0)
		{   
			return true;
		}
		in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
	}
	return false;
}

CFSMounter::MountRes CFSMounter::mount(const std::string &ip, const std::string &dir, const std::string &local_dir,
				       const FSType fstype, const std::string &username, const std::string &password,
				       std::string options1, std::string options2)
{
	std::string cmd;
	pthread_mutex_init(&g_mut, NULL);
	pthread_cond_init(&g_cond, NULL);
	g_mntstatus=-1;

	FS_Support sup = fsSupported(fstype, true); /* keep modules if necessary */

	if (sup == CFSMounter::FS_UNSUPPORTED)
	{
		printf("[CFSMounter] FS type %d not supported\n", (int) fstype);
		return MRES_FS_NOT_SUPPORTED;
	}

	printf("[CFSMounter] Mount(%d) %s:%s -> %s\n", (int) fstype, ip.c_str(), dir.c_str(), local_dir.c_str());

	CFileHelpers fh;
	fh.createDir(local_dir.c_str(), 0755);
	
	if (isMounted(local_dir))
	{
		printf("[CFSMounter] FS mount error %s already mounted\n", local_dir.c_str());
		return MRES_FS_ALREADY_MOUNTED;
	}

	if(options1.empty())
	{
		options1 = options2;
		options2 = "";
	}
	
	if(options1.empty() && options2.empty())
	{
		if(fstype == NFS)
		{
			options1 = "ro,soft,udp";
			options2 = "nolock,rsize=8192,wsize=8192";
		}
		else if(fstype == CIFS)
		{
			options1 = "ro";
			options2 = "";
		}
		else if(fstype == LUFS)
		{
			options1 = "";
			options2 = "";
		}
	}
	
	if(fstype == NFS)
	{
		cmd = "mount -t nfs ";
		cmd += ip;
		cmd += ':';
		cmd += dir;
		cmd += ' ';
		cmd += local_dir;
		cmd += " -o ";
		cmd += options1;
	}
	else if(fstype == CIFS)
	{
		cmd = "mount -t cifs //";
		cmd += ip;
		cmd += '/';
		cmd += dir;
		cmd += ' ';
		cmd += local_dir;
		cmd += " -o username=";
		cmd += username;
		cmd += ",password=";
		cmd += password;
		//cmd += ",unc=//"; for whats needed?
		//cmd += ip;
		//cmd += '/';
		//cmd += dir;
		//cmd += ',';
		//cmd += options1;
	}
	else
	{
		cmd = "lufsd none ";
		cmd += local_dir;
		cmd += " -o fs=ftpfs,username=";
		cmd += username;
		cmd += ",password=";
		cmd += password;
		cmd += ",host=";
		cmd += ip;
		cmd += ",root=/";
		cmd += dir;
		cmd += ',';
		cmd += options1;
	}
	
	if (options2[0] !='\0')
	{
		cmd += ',';
		cmd += options2;
	}
	
	pthread_create(&g_mnt, 0, mount_thread, (void *) cmd.c_str());
	
	struct timespec timeout;
	int retcode;

	pthread_mutex_lock(&g_mut);
	timeout.tv_sec = time(NULL) + 5;
	timeout.tv_nsec = 0;
	retcode = pthread_cond_timedwait(&g_cond, &g_mut, &timeout);
	if (retcode == ETIMEDOUT) 
	{  // timeout occurred
		pthread_cancel(g_mnt);
	}
	pthread_mutex_unlock(&g_mut);
	pthread_join(g_mnt, NULL);
	if ( g_mntstatus != 0 )
	{
		printf("[CFSMounter] FS mount error: \"%s\"\n", cmd.c_str());
		return (retcode == ETIMEDOUT) ? MRES_TIMEOUT : MRES_UNKNOWN;
	}
	return MRES_OK;

}

bool CFSMounter::automount()
{
	bool res = true; 
	for(int i = 0; i < NETWORK_NFS_NR_OF_ENTRIES; i++)
	{
		if(g_settings.network_nfs[i].automount)
		{
			res = (MRES_OK == mount(g_settings.network_nfs[i].ip, g_settings.network_nfs[i].dir, g_settings.network_nfs[i].local_dir,
						       (FSType) g_settings.network_nfs[i].type, g_settings.network_nfs[i].username,
						       g_settings.network_nfs[i].password, g_settings.network_nfs[i].mount_options1,
						       g_settings.network_nfs[i].mount_options2)) && res;
		}
	}
	return res;
}

CFSMounter::UMountRes CFSMounter::umount(const char * const dir)
{
	UMountRes res = UMRES_OK;
	if (dir != NULL)
	{
		if (umount2(dir, MNT_FORCE) != 0)
		{
			return UMRES_ERR;
		}
	}
	else
	{
		MountInfo mi;
		std::ifstream in("/proc/mounts", std::ifstream::in);
		while(in.good())
		{
			in >> mi.device >> mi.mountPoint >> mi.type;
			in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

			if(strcmp(mi.type.c_str(),"nfs")==0 && strcmp(mi.mountPoint.c_str(),"/")==0)
			{
				if (umount2(mi.mountPoint.c_str(),MNT_FORCE) != 0)
				{
					printf("[CFSMounter] Error umounting %s\n",mi.device.c_str());
					res = UMRES_ERR;
				}
			}
		}
	}
	if (nfs_mounted_once)
		remove_modules(CFSMounter::NFS);
	return res;
}

void CFSMounter::getMountedFS(MountInfos& info)
{
	std::ifstream in("/proc/mounts", std::ifstream::in);

	while(in.good())
	{
		MountInfo mi;
		in >> mi.device >> mi.mountPoint >> mi.type;
		in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
		if (mi.type == "nfs" ||
		    mi.type == "cifs" ||
		    mi.type == "lufs")
		{
			info.push_back(mi);
			printf("[CFSMounter] mounted fs: dev: %s, mp: %s, type: %s\n",
			       mi.device.c_str(),mi.mountPoint.c_str(),mi.type.c_str());
		}
	}
}
