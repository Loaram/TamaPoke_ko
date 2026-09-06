#pragma once
#include <map>
#include <string>
#include <vector>
#include <cstring>
#define FILE_READ "r"
#define FILE_WRITE "w"
inline std::map<std::string,std::vector<uint8_t>> cardFiles;
inline bool cardMissing=false,cardShortWrite=false;
struct File {
  std::string path;bool good=false;
  operator bool()const{return good;}
  size_t size(){return good?cardFiles[path].size():0;}
  size_t read(uint8_t*out,size_t n){if(!good)return 0;n=std::min(n,size());memcpy(out,cardFiles[path].data(),n);return n;}
  size_t write(const uint8_t*in,size_t n){if(!good)return 0;if(cardShortWrite)n/=2;cardFiles[path].assign(in,in+n);return n;}
  void flush(){}void close(){}
};
struct FakeSD {
  File open(const char*path,const char*mode){if(cardMissing)return {};if(!strcmp(mode,FILE_WRITE)){cardFiles[path].clear();return {path,true};}return {path,cardFiles.count(path)!=0};}
};
inline FakeSD SD_MMC;
