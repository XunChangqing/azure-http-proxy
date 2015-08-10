#include "misc.hpp"
#include "registry.h"

#include <tchar.h>
#include "Windows.h"
#include "Winreg.h"
#include "Wininet.h"
//[HKEY_CURRENT_USER\Software\Policies\Microsoft\Internet Explorer\Control Panel]
namespace troll_control{
void RegistryAccess::DisableProxySettings(){
	//The connection settings for other instances of Internet Explorer. 
	//DWORD disposition;
	//HKEY key_policy_ie_control_panel;
	//RegCreateKeyEx(HKEY_CURRENT_USER, L"Software\\Policies\\Microsoft\\Internet Explorer\\Control Panel", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key_policy_ie_control_panel, &disposition);
	////if (ERROR_SUCCESS != RegCreateKeyEx(HKEY_CURRENT_USER, L"Software\\Policies\\Microsoft\\Internet Explorer\\Control Panel", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key_policy_ie_control_panel, &disposition)){
	////	std::cout << "Failed to open control panel key!\n";
	////	std::cout << GetLastError() << std::endl;
	////}
	////DWORD connections_tab = 0;
	////RegSetValueEx(key_policy_ie_control_panel, L"ConnectionsTab", 0, REG_DWORD, (const BYTE*)&connections_tab, sizeof(DWORD));
	////if (ERROR_SUCCESS != RegSetValueEx(key_policy_ie_control_panel, L"ConnectionsTab", 0, REG_DWORD, (const BYTE*)&connections_tab, sizeof(DWORD))){
	////	std::cout << "Failed to set value!\n";
	////	std::cout << GetLastError() << std::endl;
	////}

	//DWORD proxy_panel_enable = 1;
	//RegSetValueEx(key_policy_ie_control_panel, L"Proxy", 0, REG_DWORD, (const BYTE*)&proxy_panel_enable, sizeof(DWORD));
	////if (ERROR_SUCCESS != RegSetValueEx(key_policy_ie_control_panel, L"Proxy", 0, REG_DWORD, (const BYTE*)&proxy_panel_enable, sizeof(DWORD))){
	////	std::cout << "Failed to set value!\n";
	////	std::cout << GetLastError() << std::endl;
	////}
	//RegFlushKey(key_policy_ie_control_panel);
}
void RegistryAccess::RestoreProxySettings(){
	//DWORD disposition;
	//HKEY key_policy_ie_control_panel;
	//RegCreateKeyEx(HKEY_CURRENT_USER, L"Software\\Policies\\Microsoft\\Internet Explorer\\Control Panel", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key_policy_ie_control_panel, &disposition);
	//DWORD proxy_panel_enable = 0;
	//RegSetValueEx(key_policy_ie_control_panel, L"Proxy", 0, REG_DWORD, (const BYTE*)&proxy_panel_enable, sizeof(DWORD));
	//RegFlushKey(key_policy_ie_control_panel);
}
void RegistryAccess::SetProxyServer(){
	TCHAR buff[256] = _T("http=127.0.0.1:8090");
	// To include server for FTP, HTTPS, and so on, use the string
	// (ftp=http://<ProxyServerName>:80; https=https://<ProxyServerName>:80) 
	INTERNET_PER_CONN_OPTION_LIST    List;
	INTERNET_PER_CONN_OPTION         Option[3];
	unsigned long                    nSize = sizeof(INTERNET_PER_CONN_OPTION_LIST);

	Option[0].dwOption = INTERNET_PER_CONN_PROXY_SERVER;
	Option[0].Value.pszValue = buff;

	Option[1].dwOption = INTERNET_PER_CONN_FLAGS;
	Option[1].Value.dwValue = PROXY_TYPE_PROXY;
	//Option[1].Value.dwValue |= PROXY_TYPE_DIRECT;
	// This option sets all the possible connection types for the client. 
	// This case specifies that the proxy can be used or direct connection is possible.

	Option[2].dwOption = INTERNET_PER_CONN_PROXY_BYPASS;
	Option[2].Value.pszValue = _T("<local>;www.shanyaows.com");

	List.dwSize = sizeof(INTERNET_PER_CONN_OPTION_LIST);
	List.pszConnection = NULL;
	List.dwOptionCount = 3;
	List.dwOptionError = 0;
	List.pOptions = Option;

	if (!InternetSetOption(NULL, INTERNET_OPTION_PER_CONNECTION_OPTION, &List, nSize))
		BOOST_LOG_TRIVIAL(warning) << "InternetSetOption failed!";
		//printf("InternetSetOption failed! (%d)\n", GetLastError());

	InternetSetOption(NULL, INTERNET_OPTION_REFRESH, NULL, NULL);

	//registry
	//DWORD disposition;
	////[HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Internet Settings]
	//HKEY key_internet_settings;
	//RegCreateKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key_internet_settings, &disposition);
	////if (ERROR_SUCCESS != RegCreateKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key_internet_settings, &disposition)){
	////	std::cout << "Failed to open internet settings key!\n";
	////	std::cout << GetLastError() << std::endl;
	////}

	//DWORD proxy_enable = 1;
	//RegSetValueEx(key_internet_settings, L"ProxyEnable", 0, REG_DWORD, (const BYTE*)&proxy_enable, sizeof(DWORD));
	////if (ERROR_SUCCESS != RegSetValueEx(key_internet_settings, L"ProxyEnable", 0, REG_DWORD, (const BYTE*)&proxy_enable, sizeof(DWORD))){
	////	std::cout << "Failed to set value!\n";
	////	std::cout << GetLastError() << std::endl;
	////}

	//WCHAR proxy_server[255] = L"http=127.0.0.1:8090";
	//RegSetValueEx(key_internet_settings, L"ProxyServer", 0, REG_SZ, (BYTE*)proxy_server, 255);
	////if (ERROR_SUCCESS != RegSetValueEx(key_internet_settings, L"ProxyServer", 0, REG_SZ, (BYTE*)proxy_server, 255)){
	////	std::cout << "Failed to set value!\n";
	////	std::cout << GetLastError() << std::endl;
	////}

	//WCHAR proxy_override[255] = L"<local>;www.shanyaows.com;www.trollwiz.com;";
	//RegSetValueEx(key_internet_settings, L"ProxyOverride", 0, REG_SZ, (BYTE*)proxy_override, 255);
	////if (ERROR_SUCCESS != RegSetValueEx(key_internet_settings, L"ProxyOverride", 0, REG_SZ, (BYTE*)proxy_override, 255)){
	////	std::cout << "Failed to set value!\n";
	////	std::cout << GetLastError() << std::endl;
	////}

	//RegFlushKey(key_internet_settings);
}
void RegistryAccess::RestoreProxyServer(){
	INTERNET_PER_CONN_OPTION_LIST    List;
	INTERNET_PER_CONN_OPTION         Option[1];
	unsigned long                    nSize = sizeof(INTERNET_PER_CONN_OPTION_LIST);

	Option[0].dwOption = INTERNET_PER_CONN_FLAGS;
	Option[0].Value.dwValue = PROXY_TYPE_DIRECT;
	// This option sets all the possible connection types for the client. 
	// This case specifies that the proxy can be used or direct connection is possible.
	List.dwSize = sizeof(INTERNET_PER_CONN_OPTION_LIST);
	List.pszConnection = NULL;
	List.dwOptionCount = 1;
	List.dwOptionError = 0;
	List.pOptions = Option;

	if (!InternetSetOption(NULL, INTERNET_OPTION_PER_CONNECTION_OPTION, &List, nSize))
		BOOST_LOG_TRIVIAL(warning) << "InternetSetOption failed!";

	InternetSetOption(NULL, INTERNET_OPTION_REFRESH, NULL, NULL);
	
	////registry
	//DWORD disposition;
	////[HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Internet Settings]
	//HKEY key_internet_settings;
	//RegCreateKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key_internet_settings, &disposition);
	//DWORD proxy_enable = 0;
	//RegSetValueEx(key_internet_settings, L"ProxyEnable", 0, REG_DWORD, (const BYTE*)&proxy_enable, sizeof(DWORD));
	//RegFlushKey(key_internet_settings);
}
}
