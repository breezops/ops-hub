#include "mission_images.h"
#include "missions.h"
#include <iostream>
#include <stdexcept>

int wmain(int argc,wchar_t** argv){
  auto check=[](bool ok,const char* label){std::cout<<(ok?"PASS ":"FAIL ")<<label<<'\n';if(!ok)throw std::runtime_error(label);};
  try{
    std::vector<std::uint8_t> fixture(44,0);fixture[0]='I';fixture[1]='W';fixture[2]='i';fixture[3]=8;fixture[4]=0x73;fixture[8]=2;fixture[10]=2;fixture[12]=2;fixture[14]=1;
    for(int offset:{16,20,24,28})fixture[offset]=44;
    fixture[32]=11;fixture[33]=22;fixture[34]=33;
    auto image=mw2::decodeMissionIwi(fixture);
    check(image.available()&&image.bgra[0]==11&&image.bgra[1]==22&&image.bgra[2]==33&&image.bgra[3]==255,"bounded BGR24 to BGRA decode");
    auto broken=fixture;broken.resize(35);check(!mw2::decodeMissionIwi(broken).available(),"truncated payload rejected");
    broken=fixture;broken[10]=0;check(!mw2::decodeMissionIwi(broken).available(),"zero dimensions rejected");
    broken=fixture;broken[4]=0x0b;check(!mw2::decodeMissionIwi(broken).available(),"unsupported format rejected");
    broken=fixture;broken[16]=45;check(!mw2::decodeMissionIwi(broken).available(),"inconsistent mip lengths rejected");
    std::vector<std::string> assets;for(const auto& mission:mw2::MissionCatalog)assets.emplace_back(mission.imageAsset);
    auto missing=mw2::readMissionImages(L"Z:/missing-mw2-test-folder",assets);
    bool empty=true;for(const auto& item:missing)empty&=!item.available();check(empty,"missing installation handled without fallback mislabelling");
    if(argc==2){auto loaded=mw2::readMissionImages(argv[1],assets);std::size_t count=0;for(std::size_t i=0;i<loaded.size();i++){if(assets[i].empty())continue;check(loaded[i].available(),assets[i].c_str());++count;}check(count==23,"all 23 original mission images loaded from local IWD");}
    else std::cout<<"SKIP local game images: supply the installed game folder.\n";
    return 0;
  }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
