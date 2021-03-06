//

// NikonAdapter.cpp
//

// DISCLAIMER:
// This software was developed by U.S. Government employees as part of
// their official duties and is not subject to copyright. No warranty implied
// or intended.

#include "stdafx.h"
#define BOOST_ALL_NO_LIB

#include "NikonAdapter.h"
#include "NikonAgent.h"

#include "config.hpp"
#include "Device.hpp"
#include "Agent.hpp"
#include "ATLComTime.h"
#include <fstream> // ifstream
#include <algorithm>

#include <stdio.h>
#include <stdlib.h>
#include <share.h>

#include "StdStringFcn.h"
#include "Globals.h"
#include "Logger.h"
#include "WinTricks.h"
#include "Logger.h"

using namespace crp;

HRESULT AdapterT::ErrorMessage (std::string errmsg)
{
  AtlTrace(errmsg.c_str( ) );
  GLogger.LogMessage(errmsg, LOWERROR);

  if ( Globals.Debug > 0 )
  {
//		EventLogger.LogEvent(errmsg);
  }
  return E_FAIL;
}
HRESULT AdapterT::DebugMessage (std::string errmsg)
{
  AtlTrace(errmsg.c_str( ) );
  GLogger.LogMessage(errmsg, WARNING);

  if ( Globals.Debug > 3 )
  {
//		EventLogger.LogEvent(errmsg);
  }
  return E_FAIL;
}
// ///////////////////////////////////////////////////////////////////
static void trans_func (unsigned int u, EXCEPTION_POINTERS *pExp)
{
  std::string errmsg = StdStringFormat("COpcAdapter In trans_func - Code = 0x%x\n", pExp->ExceptionRecord->ExceptionCode);

  OutputDebugString(errmsg.c_str( ) );
  throw std::exception(errmsg.c_str( ), pExp->ExceptionRecord->ExceptionCode);
}
AdapterT::AdapterT(AgentConfigurationEx *mtcagent, // mtconnect agent
  crp::Config &                          config,
  std::string                            machine,  // ip address or pc name
  std::string                            device) :

  _agentconfig(mtcagent),
  _device(device),
  _config(config)
{
  inifile             = Globals.inifile.c_str( );
  _nServerRate        = 1000;
  _nQueryServerPeriod = 10000;
}
AdapterT::~AdapterT(void)
{ }
void AdapterT::SetMTCTagValue (std::string tag, std::string value)
{
#if 1
  items.SetTag(tag, value);

  Agent *agent = _agentconfig->getAgent( );

  if ( agent == NULL )
  {
    ErrorMessage(StdStringFormat("AdapterT::SetMTCTagValue for %s NULL Agent Pointer\n", _device.c_str( ) ) );
    return;
  }

  Device *pDev = agent->getDeviceByName(_device);

  if ( pDev == NULL )
  {
    ErrorMessage(StdStringFormat("AdapterT::SetMTCTagValue for %s NULL Device Pointer\n", _device.c_str( ) ) );
    return;
  }
  DataItem *di = pDev->getDeviceDataItem(tag);

  if ( di != NULL )
  {
    std::string time = getCurrentTime(GMT_UV_SEC);
    agent->addToBuffer(di, value, time);
  }
  else
  {
    LOGONCE GLogger.LogMessage(StdStringFormat(" (%s) Could not find data item: %s  \n", _device.c_str( ), tag.c_str( ) ) );
  }
#endif
}
void AdapterT::Off ( )
{
  for ( int i = 0; i < items.size( ); i++ )
  {
    items[i]->_value     = "UNAVAILABLE";
    items[i]->_lastvalue = "UNAVAILABLE";
    SetMTCTagValue(items[i]->_tagname, "UNAVAILABLE");
  }

  // changed from unavailable. Causes agent to make everything unavailable.
  SetMTCTagValue("avail", "UNAVAILABLE");
  SetMTCTagValue("power", "OFF");
}
void AdapterT::On ( )
{
  SetMTCTagValue("avail", "AVAILABLE");
  SetMTCTagValue("power", "ON");
}
void NikonAdapter::CreateItem (std::string tag, std::string type)
{
  Item *item = new Item( );

  item->_type    = _T("Event");
  item->_tagname = tag;
  items.push_back(item);
}
void NikonAdapter::Dump ( )
{
  std::stringstream str;

  str << _device << ":ProductionLog= " << unc << std::endl;
  str << _device << ":QueryServer= " << _nQueryServerPeriod << std::endl;
  str << _device << ":ServerRate= " << _nServerRate << std::endl;
  str << _device << ":Simulation= " << _simulation << std::endl;
  str << _device << ":LastLogDate= " << LastLogDate.Format("%Y/%m/%d") << std::endl;

  OutputDebugString(str.str( ).c_str( ) );
}
void NikonAdapter::Config ( )
{
  // GLogger.LogMessage(StdStringFormat("COpcAdapter::Config() for IP = %s\n",_device.c_str()));
  std::string cfgfile = Globals.inifile;
  std::string sLastLogDate;

  try
  {
    if ( GetFileAttributesA(cfgfile.c_str( ) ) == INVALID_FILE_ATTRIBUTES )
    {
      throw std::exception("Could not find ini file \n");
    }

    unc = config.GetSymbolValue(_device + ".ProductionLog", File.ExeDirectory( ) + "ProductionLog.csv").c_str( );
    std::vector<std::string> logday = TrimmedTokenize(config.GetSymbolValue(_device + ".LastLogDate", "0/0/0").c_str( ), "/");

    if ( logday.size( ) < 2 )
    {
      throw std::exception("Bad log date");
    }
    LastLogDate         = COleDateTime(ConvertString(logday[2], 0), ConvertString(logday[0], 0), ConvertString(logday[1], 0), 0, 0, 0); // .ParseDateTime(sLastLogDate.c_str(), VAR_DATEVALUEONLY);
    _nServerRate        = config.GetSymbolValue(_device + ".ServerRate", Globals.ServerRate).toNumber<int>( );
    _nQueryServerPeriod = config.GetSymbolValue(_device + ".QueryServer", Globals.QueryServer).toNumber<int>( );
    _simulation         = config.GetSymbolValue(_device + ".Simulation", 0).toNumber<int>( );

    // Network share addition - in case user SYSTEM cant access remote file
    _User         = config.GetSymbolValue(_device + ".User", "").c_str( );
    _Password     = config.GetSymbolValue(_device + ".Pw", "").c_str( );
    _LocalShare   = config.GetSymbolValue(_device + ".LocalShare", "").c_str( );
    _NetworkShare = config.GetSymbolValue(_device + ".NetworkShare", "").c_str( );
  }
  catch ( std::exception errmsg )
  {
    _agentconfig->AbortMsg(StdStringFormat("Could not find ini file for device %s\n", _device.c_str( ) ) );
  }
  catch ( ... )
  {
    _agentconfig->AbortMsg(StdStringFormat("Could not find ini file for device %s\n", _device.c_str( ) ) );
  }
  Dump( );
}
void NikonAdapter::Cycle ( )
{
  Config( );

  _set_se_translator(trans_func);      // correct thread?
  SetPriorityClass(GetCurrentProcess( ), ABOVE_NORMAL_PRIORITY_CLASS);

  int nHeartbeat = 0;
  CreateItem("heartbeat");
  CreateItem("program");
  CreateItem("controllermode");
  CreateItem("execution");
  CreateItem("power");
  CreateItem("avail");
  CreateItem("error");
  CreateItem("path_feedrateovr");
  CreateItem("operator");
  CreateItem("Srpm");
  CreateItem("Xabs");
  CreateItem("Yabs");
  CreateItem("Zabs");
  CreateItem("last_update");

  Off( );

  // GLogger.LogMessage(StdStringFormat("COpcAdapter::Cycle() Enter Loop for IP = %s\n",_device.c_str()));
  _mRunning = true;
  Off( );

  while ( _mRunning )
  {
    try
    {
      // PING IP?
      SetMTCTagValue("heartbeat", StdStringFormat("%d", nHeartbeat++) );
      SetMTCTagValue("avail", "AVAILABLE");

      if ( FAILED(GatherDeviceData( ) ) )
      {
        ::Sleep(this->_nQueryServerPeriod);
        continue;
      }
      ::Sleep(_nServerRate);
    }
    catch ( std::exception e )
    {
      Off( );
    }
    catch ( ... )
    {
      Off( );
    }
  }
}
COleDateTime GetDateTime (std::string s)
{
  std::string::size_type delimPos = s.find_first_of("\t", 0);

  if ( std::string::npos != delimPos )
  {
    s = s.substr(0, delimPos);
  }

  // parse 2012/11/03 Skip time
  int Year, Month, Day;

  if ( sscanf(s.c_str( ), "%4d-%02d-%02d", &Year, &Month, &Day) == 3 )
  { }
  else
  {
    throw std::exception("Unrecognized date-time format\n");
  }

  return COleDateTime(Year, Month, Day, 0, 0, 0);
}
// Format: 2015-03-11 17:04:39.998
boolean GetLogTimestamp (std::string s, COleDateTime & ts)
{
  std::string::size_type delimPos = s.find_first_of("\t", 0);

  if ( std::string::npos != delimPos )
  {
    s = s.substr(0, delimPos);
  }

  // parse 2012/11/03 Skip time
  int Year, Month, Day, Hour, Min, Sec;

  if ( sscanf(s.c_str( ), "%4d-%02d-%02d %02d:%02d:%02d", &Year, &Month, &Day, &Hour, &Min, &Sec) == 6 )
  {
    ts = COleDateTime(Year, Month, Day, Hour, Min, Sec);
    return true;
  }
  return false;
}
static void Nop ( ) { }
HRESULT NikonAdapter::FailWithMsg (HRESULT hr, std::string errmsg)
{
  this->DebugMessage(errmsg);
  return hr;
}
HRESULT NikonAdapter::WarnWithMsg (HRESULT hr, std::string errmsg)
{
  this->DebugMessage(errmsg);
  return S_OK;
}
static bool getline (size_t & n, std::string & data, std::string & line)
{
  line.clear( );
  size_t N = data.size( );

  if ( n >= N )
  {
    return false;
  }

  size_t start = n;
  size_t end   = n;

  while ( end < N && data[end] != '\n' )
  {
    end++;
  }

  while ( end < N && data[end] == '\n' )
  {
    end++;
  }

  line = data.substr(start, end - start);
  n    = end; // skip over \n
  return true;

  // if((n=data.find_first_of('\n')) != std::string::npos)
  // {
  //	line=data.substr(0,n);
  //	data=data.substr(n+1);
  //	return true;
  // }
}
static bool getlinebackwards (long & n, std::string & data, std::string & line)
{
  line.clear( );

  if ( n < 0 )
  {
    return false;
  }

  long start = n;
  long end   = n;

  while ( end >= 0 && data[end] == '\n' )
  {
    end--;
  }

  while ( end >= 0 && data[end] != '\n' )
  {
    end--;
  }

  line = data.substr(end + 1, start - end);

  // OutputDebugString(line.c_str());
  n = end;
  return true;
}
HRESULT NikonAdapter::GatherDeviceData ( )
{
  USES_CONVERSION;
  HRESULT      hr = S_OK;
  DWORD        filesize;
  COleDateTime modtime;
  try
  {
    std::string  filename = unc;
    COleDateTime today;

    if ( ( items.GetTag("execution", "PAUSED") == "EXECUTING" ) && ( items.GetTag("controllermode", "MANUAL") == "AUTOMATIC" ) )
    {
      double X = fmod(ConvertString<double>(items.GetTag("Xabs", "0.0"), 0.0) + 0.1, 100.0);
      double Y = fmod(ConvertString<double>(items.GetTag("Yabs", "0.0"), 0.0) + 0.1, 100.0);
      double Z = fmod(ConvertString<double>(items.GetTag("Zabs", "0.0"), 0.0) + 0.1, 100.0);
      SetMTCTagValue("Xabs", ConvertToString(X) );
      SetMTCTagValue("Yabs", ConvertToString(Y) );
      SetMTCTagValue("Zabs", ConvertToString(Z) );
    }
    else
    {
      SetMTCTagValue("Xabs", "0");
      SetMTCTagValue("Yabs", "10");
      SetMTCTagValue("Zabs", "20");
    }

    if ( !_User.empty( ) )
    {
      if ( !WinTricks::DriveExists(_LocalShare + "\\") )  // "Z:\\"))
      {
        WarnWithMsg(E_FAIL, StdStringFormat("Network share: %s doesnt exist\n", _LocalShare.c_str( ) ).c_str( ) );
        WinTricks::MapNetworkDrive(_NetworkShare, _LocalShare, _User, _Password);
      }
    }

    //
    // Check if connected - this could take too long if
    if ( !File.Exists(filename) )
    {
      // DebugMessage(StdStringFormat("UNC File not found for device %s\n", _device.c_str()));
      return WarnWithMsg(E_FAIL, StdStringFormat("UNC File %s not found for device %s\n", filename.c_str( ), _device.c_str( ) ) );
    }

    // Check mod time, if same, returns
    modtime = File.GetFileModTime(filename);

    if ( lastmodtime == modtime )
    {
      return WarnWithMsg(E_PENDING, StdStringFormat("File %s Device %s modtime unchanged %s\n", filename.c_str( ), _device.c_str( ), modtime.Format("%c") ) );
    }

    // Check file size, if same, returns
    if ( FAILED(File.Size(filename, filesize) ) )
    {
      return WarnWithMsg(E_FAIL, StdStringFormat("File %s Device %s filesize  Failed\n", filename.c_str( ), _device.c_str( ) ) );
    }

    if ( filesize == _lastfilesize )
    {
      return WarnWithMsg(E_PENDING, StdStringFormat("File %s Device %s filesize:%d = lastfilesize:%d\n", filename.c_str( ), _device.c_str( ), filesize, _lastfilesize) );
    }

    if ( filesize == 0 )
    {
      return WarnWithMsg(E_PENDING, StdStringFormat("File %s Device %s filesize:%d = 0\n", filename.c_str( ), _device.c_str( ), filesize) );
    }
    COleDateTime now = COleDateTime::GetCurrentTime( );   // -COleDateTimeSpan(/* days, hours, mins, secs*/  0,0,0,0);

    if ( !_simulation )
    {
      today = COleDateTime(now.GetYear( ), now.GetMonth( ), now.GetDay( ), 0, 0, 0);
    }
    else
    {
      today = LastLogDate;
    }
    long        ulFileSize = filesize;
    std::string data;
    data.resize(ulFileSize);

    // data.resize(ulFileSize);

    // Posix file utilies dont work with UNC file names?

    HANDLE hFile = CreateFile(filename.c_str( ),
      GENERIC_READ,                                           // access (read) mode
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, // share mode
      NULL,                                                   // pointer to security attributes
      OPEN_EXISTING,                                          // how to create
      FILE_ATTRIBUTE_NORMAL,                                  // file attributes
      NULL);                                                  // handle to file with attributes to copy

    if ( hFile == INVALID_HANDLE_VALUE )                      // did we succeed?
    {
      return WarnWithMsg(E_FAIL, StdStringFormat("INVALID_HANDLE_VALUE File \"%s\" for device %s\n", filename.c_str( ), _device.c_str( ) ) );
    }

    GLogger.Warning(StdStringFormat("Check Filesize %d vs Last Filesize %d\n", filesize, _lastfilesize) );

    if ( ( filesize > _lastfilesize ) && ( _lastfilesize != 0 ) )
    {
      // fseek(stream,_lastfilesize,SEEK_SET);
      DWORD dwPtr = SetFilePointer(hFile, _lastfilesize, NULL, FILE_BEGIN);

      if ( dwPtr == INVALID_SET_FILE_POINTER )     // Test for failure
      {
        return WarnWithMsg(E_FAIL, StdStringFormat("INVALID_SET_FILE_POINTER File \"%s\" Error = %x for device %s\n", filename.c_str( ), GetLastError( ), _device.c_str( ) ) );
      }

      ulFileSize = filesize - _lastfilesize;
    }

    GLogger.Warning(StdStringFormat("Read File Size %d\n", ulFileSize) );

    // read the original file byte in the buffer
    DWORD dwNewSize;
    void *pAddr = &data[0];
    ReadFile(hFile, pAddr, ulFileSize, &dwNewSize, NULL);
    CloseHandle(hFile);

    // Now parse buffer full of data
    std::string line;

    long         m           = (long) data.size( ) - 1;
    COleDateTime beginningts = now - COleDateTimeSpan(7, 0, 0, 0); // 1 week ago

    // OutputDebugString("Beginning:");
    // OutputDebugString(beginningts.Format("%A, %B %d, %Y"));
    while ( getlinebackwards(m, data, line) )
    {
      size_t it;

      if ( ( it = line.find_first_of("\t") ) != std::string::npos )
      {
        COleDateTime ts     = now;
        std::string  tstamp = line.substr(0, it);

        if ( GetLogTimestamp(tstamp, ts) )
        {
          // OutputDebugString(ts.Format("%A, %B %d, %Y"));
        }

        if ( ts <= beginningts )
        {
          data = data.substr(m);
          break;
        }
      }
    }

    size_t n = 0;

    while ( getline(n, data, line) )
    {
      size_t it;

      if ( ( it = line.find_first_of("\t") ) != std::string::npos )
      {
        items.SetTag("last_update", line.substr(0, it) );
        this->SetMTCTagValue("last_update", line.substr(0, it) );
      }

      if ( line.find("CAMIO Studio stopped") != std::string::npos )
      {
        for ( int k = 0; k < items.size( ); k++ )
        {
          if ( items[k]->_tagname == "last_update" )
          {
            continue;
          }
          items.SetTag(items[k]->_tagname, "UNAVAILABLE");
        }

        // changed from unavailable. Causes agent to make everything unavailable.
        items.SetTag("avail", "AVAILABLE");
        items.SetTag("power", "OFF");
      }
      else if ( line.find("CAMIO Studio started") != std::string::npos )
      {
        items.SetTag("power", "ON");
        items.SetTag("controllermode", "MANUAL");
        items.SetTag("execution", "IDLE");
        items.SetTag("error", "");
      }
      else if ( ( line.find("DMIS Program started") != std::string::npos ) ||
                ( line.find("DMIS Program stepped") != std::string::npos ) ) // step for 50 milliseconds!
      {
        items.SetTag("power", "ON");
        items.SetTag("controllermode", "AUTOMATIC");
        items.SetTag("execution", "EXECUTING");
        items.SetTag("error", "");
      }
      else if ( line.find("DMIS Program stopped") != std::string::npos )
      {
        items.SetTag("power", "ON");
        items.SetTag("controllermode", "MANUAL");
        items.SetTag("execution", "IDLE");
      }
      else if ( line.find("DMIS command error") != std::string::npos )
      {
        items.SetTag("power", "ON");
        items.SetTag("controllermode", "MANUAL");
        items.SetTag("execution", "IDLE");

        // parse error
        std::vector<std::string> tokens = TrimmedTokenize(line, "\t");
        std::string              err;

        if ( tokens.size( ) > 4 )
        {
          std::vector<std::string> errs = TrimmedTokenize(tokens[4], "-");

          if ( items.size( ) > 1 )
          {
            err = errs[1];
          }
        }
        items.SetTag("error", err);
      }
      else if ( line.find("DMIS Program opened") != std::string::npos )
      {
        items.SetTag("power", "ON");
        items.SetTag("controllermode", "MANUAL");
        items.SetTag("execution", "IDLE");
        std::string              prg;
        std::vector<std::string> tokens = TrimmedTokenize(line, "\t");

        if ( tokens.size( ) > 4 )
        {
          std::vector<std::string> items = TrimmedTokenize(tokens[4], "-");

          if ( items.size( ) > 1 )
          {
            prg = items[1];
          }
          prg = ExtractFilename(prg);
        }
        items.SetTag("program", prg);
        items.SetTag("error", "");
      }
#if 1

      // else if(tokens.size()>4 && (tokens[4].find("\\\\")) == 0)
      else if ( ( it = line.find("\t\\\\") ) != std::string::npos )
      {
        std::string userlogon = line.substr(it + 3);

        // log on
        items.SetTag("power", "ON");
        items.SetTag("controllermode", "MANUAL");
        items.SetTag("execution", "IDLE");
        std::vector<std::string> parts = TrimmedTokenize(userlogon, "\\");
        std::string              logon;

        if ( parts.size( ) > 1 )
        {
          logon = parts[1];
        }

        items.SetTag("operator", logon);
        items.SetTag("error", "");
      }
#endif
    }

    // Guess RPM
    if ( ( items.GetTag("execution", "PAUSED") == "EXECUTING" ) && ( items.GetTag("controllermode", "MANUAL") == "AUTOMATIC" ) )
    {
      items.SetTag("Srpm", "99");
    }
    else
    {
      items.SetTag("Srpm", "0");
    }

    // FIXME: should guess xyz motion if auto/exec and timestamp and now > threshold of time
    for ( int i = 0; i < items.size( ); i++ )
    {
      if ( ( items[i]->_type == _T("Event") ) || ( items[i]->_type == _T("Sample") ) )
      {
        if ( items[i]->_value != items[i]->_lastvalue )
        {
          this->SetMTCTagValue(items[i]->_tagname, items[i]->_value);
          items[i]->_lastvalue = items[i]->_value;
        }
      }
    }
  }
  catch ( std::exception e )
  {
    GLogger.LogMessage(StdStringFormat("COpcAdapter Exception in %s - COpcAdapter::GatherDeviceData() %s\n", _device.c_str( ), (LPCSTR) e.what( ) ), LOWERROR);
    Off( );
    Disconnect( );
    hr = E_FAIL;
  }
  catch ( ... )
  {
    GLogger.LogMessage(StdStringFormat("COpcAdapter Exception in %s - COpcAdapter::GatherDeviceData()\n", _device.c_str( ) ), LOWERROR);
    Off( );
    Disconnect( );
    hr = E_FAIL;
  }
  _lastfilesize = filesize;
  lastmodtime   = modtime;
  return hr;
}
