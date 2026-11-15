#include "mission_images.h"
#include "../vendor/miniz/miniz.h"
#include <cstdio>
#include <limits>

namespace mw2 {
namespace {
std::uint16_t u16(const std::vector<std::uint8_t>& b,std::size_t at){return static_cast<std::uint16_t>(b[at]|(b[at+1]<<8));}
std::uint32_t u32(const std::vector<std::uint8_t>& b,std::size_t at){return static_cast<std::uint32_t>(b[at])|(static_cast<std::uint32_t>(b[at+1])<<8)|(static_cast<std::uint32_t>(b[at+2])<<16)|(static_cast<std::uint32_t>(b[at+3])<<24);}
struct Archive {
  FILE* file=nullptr; mz_zip_archive zip{}; bool initialized=false;
  ~Archive(){if(initialized)mz_zip_reader_end(&zip);if(file)std::fclose(file);}
};
}
MissionRaster decodeMissionIwi(const std::vector<std::uint8_t>& b){
  if(b.size()<32||b[0]!='I'||b[1]!='W'||b[2]!='i'||b[3]!=8||b[4]!=0x73||u16(b,8)!=2||u16(b,14)!=1)return {};
  const unsigned width=u16(b,10),height=u16(b,12);
  if(!width||!height||width>2048||height>2048)return {};
  const std::size_t pixels=static_cast<std::size_t>(width)*height;
  if(b.size()!=32+pixels*3)return {};
  for(std::size_t offset=16;offset<32;offset+=4)if(u32(b,offset)!=b.size())return {};
  MissionRaster result;result.width=static_cast<int>(width);result.height=static_cast<int>(height);result.bgra.resize(pixels*4);
  for(std::size_t i=0;i<pixels;i++){
    result.bgra[i*4]=b[32+i*3];result.bgra[i*4+1]=b[32+i*3+1];result.bgra[i*4+2]=b[32+i*3+2];result.bgra[i*4+3]=255;
  }
  return result;
}
std::vector<MissionRaster> readMissionImages(const std::filesystem::path& game,const std::vector<std::string>& assets){
  std::vector<MissionRaster> result(assets.size());
  Archive archive;
  if(_wfopen_s(&archive.file,(game/L"main/iw_02.iwd").c_str(),L"rb")!=0||!archive.file)return result;
  if(!mz_zip_reader_init_cfile(&archive.zip,archive.file,0,0))return result;
  archive.initialized=true;
  for(std::size_t i=0;i<assets.size();i++){
    if(assets[i].empty()||assets[i].find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_")!=std::string::npos)continue;
    const auto name="images/"+assets[i]+".iwi";
    const int index=mz_zip_reader_locate_file(&archive.zip,name.c_str(),nullptr,MZ_ZIP_FLAG_CASE_SENSITIVE);
    if(index<0)continue;
    mz_zip_archive_file_stat stat{};
    if(!mz_zip_reader_file_stat(&archive.zip,static_cast<mz_uint>(index),&stat)||stat.m_uncomp_size<32||stat.m_uncomp_size>4*1024*1024||stat.m_is_encrypted||stat.m_is_directory)continue;
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(stat.m_uncomp_size));
    // miniz validates decompression bounds and the archive entry's CRC.
    if(mz_zip_reader_extract_to_mem(&archive.zip,static_cast<mz_uint>(index),bytes.data(),bytes.size(),0))result[i]=decodeMissionIwi(bytes);
  }
  return result;
}
} // namespace mw2
