#pragma once
#include "file_delta.hpp"
#include <winsqlite/winsqlite3.h>
namespace fsizes {
// One shared, disk-backed B-tree. No complete per-file map is resident in RAM.
class DiskIndex {
 sqlite3* db=nullptr; sqlite3_stmt* insert=nullptr; bool healthy=true;
 bool run(const char* sql,const std::vector<std::wstring>& args={},sqlite3_int64 bytes=0,bool number=false){
  sqlite3_stmt* s=nullptr;int rc=sqlite3_prepare_v2(db,sql,-1,&s,nullptr);if(rc!=SQLITE_OK)return false;
  for(int i=0;i<int(args.size());++i)sqlite3_bind_text16(s,i+1,args[i].c_str(),-1,SQLITE_TRANSIENT);
  if(number)sqlite3_bind_int64(s,int(args.size())+1,bytes);rc=sqlite3_step(s);sqlite3_finalize(s);return rc==SQLITE_DONE;
 }
 uint64_t sum(const std::wstring& root,const std::wstring& path,bool& ok){
  sqlite3_stmt* s=nullptr;auto prefix=path+L"\\",end=path+L"]";
  ok=sqlite3_prepare_v2(db,"SELECT coalesce(sum(bytes),0) FROM files WHERE root=? AND path>=? AND path<? AND (path=? OR path>=?)",-1,&s,nullptr)==SQLITE_OK;
  if(!ok)return 0;const std::wstring* a[]={&root,&path,&end,&path,&prefix};for(int i=0;i<5;i++)sqlite3_bind_text16(s,i+1,a[i]->c_str(),-1,SQLITE_TRANSIENT);
  ok=sqlite3_step(s)==SQLITE_ROW;uint64_t n=ok?sqlite3_column_int64(s,0):0;sqlite3_finalize(s);return n;
 }
public:
 explicit DiskIndex(const std::wstring& path){
  if(sqlite3_open16(path.c_str(),&db)!=SQLITE_OK){healthy=false;return;}
  sqlite3_busy_timeout(db,1000);
  healthy=sqlite3_exec(db,"PRAGMA journal_mode=WAL;PRAGMA synchronous=NORMAL;PRAGMA cache_size=-2048;PRAGMA mmap_size=0;PRAGMA temp_store=FILE;PRAGMA wal_autocheckpoint=256;PRAGMA journal_size_limit=4194304;CREATE TABLE IF NOT EXISTS files(root TEXT,path TEXT,bytes INTEGER,PRIMARY KEY(root,path)) WITHOUT ROWID;CREATE TABLE IF NOT EXISTS staging(root TEXT,path TEXT,bytes INTEGER,PRIMARY KEY(root,path)) WITHOUT ROWID;",nullptr,nullptr,nullptr)==SQLITE_OK;
  if(healthy)healthy=sqlite3_prepare_v2(db,"INSERT OR REPLACE INTO staging VALUES(?,?,?)",-1,&insert,nullptr)==SQLITE_OK;
 }
 ~DiskIndex(){if(insert)sqlite3_finalize(insert);if(db)sqlite3_close(db);}
 bool good()const{return healthy;}
 bool begin(){return healthy&&sqlite3_exec(db,"BEGIN IMMEDIATE",nullptr,nullptr,nullptr)==SQLITE_OK;}
 bool commit(){bool ok=healthy&&sqlite3_exec(db,"COMMIT",nullptr,nullptr,nullptr)==SQLITE_OK;if(!ok)sqlite3_exec(db,"ROLLBACK",nullptr,nullptr,nullptr);return ok;}
 void rollback(){sqlite3_exec(db,"ROLLBACK",nullptr,nullptr,nullptr);healthy=true;}
 bool startScan(const std::wstring& root){return run("DELETE FROM staging WHERE root=?",{root});}
 void observe(const std::wstring& root,const std::wstring& path,uint64_t bytes){
  sqlite3_reset(insert);sqlite3_clear_bindings(insert);sqlite3_bind_text16(insert,1,root.c_str(),-1,SQLITE_TRANSIENT);sqlite3_bind_text16(insert,2,path.c_str(),-1,SQLITE_TRANSIENT);sqlite3_bind_int64(insert,3,bytes);if(sqlite3_step(insert)!=SQLITE_DONE)healthy=false;
 }
 bool finishScan(const std::wstring& root){if(!begin())return false;bool ok=run("DELETE FROM files WHERE root=?",{root})&&run("INSERT INTO files SELECT root,path,bytes FROM staging WHERE root=?",{root})&&run("DELETE FROM staging WHERE root=?",{root});if(!ok){rollback();return false;}return commit();}
 bool remove(const std::wstring& root,const std::wstring& path,uint64_t& total){bool ok;auto old=sum(root,path,ok);if(!ok||!run("DELETE FROM files WHERE root=? AND path>=? AND path<? AND (path=? OR path>=?)",{root,path,path+L"]",path,path+L"\\"}))return false;total-=old;return true;}
 bool put(const std::wstring& root,const std::wstring& path,uint64_t bytes,uint64_t& total){if(!remove(root,path,total))return false;if(!run("INSERT INTO files VALUES(?,?,?)",{root,path},bytes,true))return false;total+=bytes;return true;}
 bool move(const std::wstring& root,const std::wstring& from,const std::wstring& to,uint64_t& total,bool& found){bool ok;auto n=sum(root,from,ok);if(!ok)return false;found=n!=0;if(!found)return true;if(!remove(root,to,total))return false;return run("UPDATE files SET path=? || substr(path,length(?)+1) WHERE root=? AND path>=? AND path<? AND (path=? OR path>=?)",{to,from,root,from,from+L"]",from,from+L"\\"});}
 bool delta(const std::wstring& root,const std::wstring& path,DWORD action,uint64_t& total,const std::atomic<bool>& stop,bool& approximate){
  WIN32_FILE_ATTRIBUTE_DATA d{};if(!GetFileAttributesExW(longPath(path).c_str(),GetFileExInfoStandard,&d)){auto e=GetLastError();return (e==ERROR_FILE_NOT_FOUND||e==ERROR_PATH_NOT_FOUND)&&remove(root,path,total);}
  if(!(d.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY))return put(root,path,(uint64_t(d.nFileSizeHigh)<<32)|d.nFileSizeLow,total);
  if(action!=FILE_ACTION_ADDED&&action!=FILE_ACTION_RENAMED_NEW_NAME)return true;
  if(!remove(root,path,total))return false;if(d.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)return true;bool ok=true;
  ScanJob scan(path,stop,{},[&](const std::wstring& p,uint64_t bytes){if(ok)ok=put(root,fileKey(p),bytes,total);});while(!scan.step(256,4)){if(stop)return false;Sleep(4);}if(scan.value().approximate)approximate=true;return ok&&scan.value().complete;
 }
 uint64_t total(const std::wstring& root,bool& ok){return sum(root,root,ok);}
 sqlite3_int64 cacheBytes()const{int used=0,high=0;sqlite3_db_status(db,SQLITE_DBSTATUS_CACHE_USED,&used,&high,0);return used;}
};
}


