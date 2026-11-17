#pragma once
// Discos físicos para o monitor/HUD: taxas de leitura/escrita e tempo ativo pelos contadores
// PhysicalDisk do Windows; modelo e temperatura por IOCTL_STORAGE_QUERY_PROPERTY, que não exige
// administrador (acesso 0 ao \\.\PhysicalDriveN). Temperatura ausente = unidade não expõe o sensor.
#include <windows.h>
#include <winioctl.h>
#include <pdh.h>
#include <algorithm>
#include <chrono>
#include <map>
#include <sstream>
#include <string>
#include <vector>
namespace WinDisks {
struct Info{std::string model;double temperature=-1000;};
inline std::string trim(std::string s){while(!s.empty()&&(s.back()==' '||s.back()=='\0'))s.pop_back();size_t i=0;while(i<s.size()&&s[i]==' ')i++;return s.substr(i);}
inline Info query(int index){
 Info info;wchar_t path[64];swprintf_s(path,L"\\\\.\\PhysicalDrive%d",index);
 HANDLE h=CreateFileW(path,0,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);if(h==INVALID_HANDLE_VALUE)return info;
 STORAGE_PROPERTY_QUERY q{};q.PropertyId=StorageDeviceProperty;q.QueryType=PropertyStandardQuery;std::vector<unsigned char> buffer(4096);DWORD bytes=0;
 if(DeviceIoControl(h,IOCTL_STORAGE_QUERY_PROPERTY,&q,sizeof q,buffer.data(),DWORD(buffer.size()),&bytes,nullptr)&&bytes>=sizeof(STORAGE_DEVICE_DESCRIPTOR)){
  auto d=reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(buffer.data());if(d->ProductIdOffset&&d->ProductIdOffset<bytes)info.model=trim(reinterpret_cast<char*>(buffer.data()+d->ProductIdOffset));
 }
 q.PropertyId=StorageDeviceTemperatureProperty;bytes=0;std::fill(buffer.begin(),buffer.end(),0);
 if(DeviceIoControl(h,IOCTL_STORAGE_QUERY_PROPERTY,&q,sizeof q,buffer.data(),DWORD(buffer.size()),&bytes,nullptr)&&bytes>=sizeof(STORAGE_TEMPERATURE_DATA_DESCRIPTOR)){
  auto t=reinterpret_cast<STORAGE_TEMPERATURE_DATA_DESCRIPTOR*>(buffer.data());
  for(unsigned i=0;i<t->InfoCount&&i<8;i++){auto c=t->TemperatureInfo[i].Temperature;if(c>-40&&c<150){info.temperature=c;break;}}
 }
 CloseHandle(h);return info;
}
class Monitor {
 PDH_HCOUNTER read{},write{},idle{};std::map<int,Info> cache;std::chrono::steady_clock::time_point refreshed{};
public:
 void add(PDH_HQUERY q){PdhAddEnglishCounterW(q,L"\\PhysicalDisk(*)\\Disk Read Bytes/sec",0,&read);PdhAddEnglishCounterW(q,L"\\PhysicalDisk(*)\\Disk Write Bytes/sec",0,&write);PdhAddEnglishCounterW(q,L"\\PhysicalDisk(*)\\% Idle Time",0,&idle);}
 template<class Values> std::string json(Values values){
  struct Row{std::string volumes;double read=-1,write=-1,active=-1;};std::map<int,Row> rows;
  auto collect=[&](PDH_HCOUNTER c,int field){if(!c)return;for(auto &[name,value]:values(c)){if(name.find(L"_total")!=std::wstring::npos)continue;int index=_wtoi(name.c_str());if(name.empty()||name[0]<L'0'||name[0]>L'9')continue;auto &r=rows[index];auto space=name.find(L' ');if(space!=std::wstring::npos){std::string v;for(auto ch:name.substr(space+1))v+=char(ch<128?towupper(ch):'?');r.volumes=v;}if(field==0)r.read=value;else if(field==1)r.write=value;else r.active=std::clamp(100.-value,0.,100.);}};
  collect(read,0);collect(write,1);collect(idle,2);
  auto now=std::chrono::steady_clock::now();if(now-refreshed>std::chrono::seconds(5)){refreshed=now;for(auto &[index,row]:rows)cache[index]=query(index);}
  std::ostringstream out;out.precision(10);out<<'[';bool first=true;
  for(auto &[index,row]:rows){if(!first)out<<',';first=false;auto &info=cache[index];out<<"{\"index\":"<<index<<",\"volumes\":\""<<row.volumes<<"\"";
   std::string model;for(unsigned char ch:info.model)if(ch>=32&&ch!='"'&&ch!='\\')model+=char(ch);if(!model.empty())out<<",\"model\":\""<<model<<"\"";
   if(row.read>=0)out<<",\"readBytes\":"<<row.read;if(row.write>=0)out<<",\"writeBytes\":"<<row.write;if(row.active>=0)out<<",\"active\":"<<row.active;if(info.temperature>-40)out<<",\"temp\":"<<info.temperature;out<<'}';}
  out<<']';return out.str();
 }
};
}
