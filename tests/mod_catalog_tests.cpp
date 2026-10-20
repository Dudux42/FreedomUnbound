#include "mod_catalog.hpp"
#include "psprecomp/sha256.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <chrono>
using namespace portablekit::fu;
void check(bool value){if(!value)throw std::runtime_error("Catalogue fixture failed");}
std::string sample(){return R"({"release":"mods-2026-10-16","mods":[{"id":"example","name":"Example \u00e9 \ud83c\udfae","version":"1.0.0","file":"example.zip","bytes":3,"sha256":"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","url":"https://github.com/Dudux42/FreedomUnboundmods/releases/download/mods-2026-10-16/example.zip","guide":"guides/example.md"}]})";}
int main(int argc,char **argv){try{
 if(argc==4&&std::string(argv[1])=="--live"){
  catalog::Progress p;const auto listing=catalog::fetch(argv[2],p);if(!listing.error.empty())throw std::runtime_error(listing.error);check(listing.index.mods.size()>=1);
  for(const auto &e:listing.index.mods)if(e.id==argv[3]){auto result=catalog::download(e,argv[2],p);if(!result.error.empty())throw std::runtime_error(result.error);check(catalog::matches(e,result.file));std::cout<<"Live catalogue/download PASS: "<<listing.index.mods.size()<<" entries; "<<e.id<<" "<<e.version<<"\n";return 0;}
  throw std::runtime_error("Live mod not found");
 }
 const auto builtin=catalog::default_source();
 check(catalog::source_from_url("https://github.com/Dudux42/FreedomUnboundmods/").url==builtin.url);
 check(catalog::source_from_url("https://github.com/Dudux42/FreedomUnboundmods/blob/main/releases.json").url==builtin.url);
 check(catalog::source_from_url("https://github.com/Dudux42/FreedomUnboundmods.git").url==builtin.url);
 const auto third=catalog::source_from_url("https://github.com/Other/Mods");
 check(third.repository=="https://github.com/other/mods");
 for(const char *url:{"http://github.com/owner/repo","https://example.com/index.json","https://github.com/owner/../releases.json","https://github.com/owner/repo?token=x","https://raw.githubusercontent.com/owner/repo/main/not-json","https://github.com.evil/owner/repo","https://user@github.com/owner/repo","https://github.com/owner//repo"}){
  bool refused=false;try{catalog::source_from_url(url);}catch(...){refused=true;}check(refused);
 }
 const auto good=sample();auto index=catalog::parse(good);check(index.mods.size()==1&&index.mods[0].bytes==3);check(index.mods[0].name=="Example \xc3\xa9 \xf0\x9f\x8e\xae");
 auto other_json=good;auto origin_at=other_json.find(catalog::repository);
 other_json.replace(origin_at,std::string(catalog::repository).size(),third.repository);
 const auto other_index=catalog::parse(other_json,third);check(other_index.mods[0].origin_repository==third.repository);
 bool foreign_refused=false;try{catalog::parse(good,third);}catch(...){foreign_refused=true;}check(foreign_refused);
 auto refuses=[](const std::string &s){try{catalog::parse(s);}catch(...){return;}throw std::runtime_error("Malformed catalogue was accepted");};
 for(auto pair:{std::pair{"\"bytes\":3","\"bytes\":-1"},{"\"bytes\":3","\"bytes\":0"},{"\"bytes\":3","\"bytes\":2147483649"},{"\"bytes\":3","\"bytes\":3.5"},{"\"bytes\":3","\"bytes\":03"},{"\"file\":\"example.zip\"","\"file\":\"../example.zip\""},{"\"id\":\"example\"","\"id\":\"..\""},{"https://github.com/Dudux42/FreedomUnboundmods/","https://example.com/"},{"guides/example.md","guides/../../secret"},{"\\ud83c\\udfae","\\ud83cBAD"},{"\"bytes\":3","\"bytes\":3,\"bytes\":4"}}){auto s=good;auto at=s.find(pair.first);check(at!=std::string::npos);s.replace(at,std::string(pair.first).size(),pair.second);refuses(s);}
 refuses(good+"garbage");refuses(std::string(2*1024*1024+1,' '));refuses("{\"release\":\"x\",\"mods\":{}}");
 auto duplicate=good;duplicate.insert(duplicate.size()-2,","+good.substr(good.find("[{" )+1,good.size()-good.find("[{")-3));refuses(duplicate);
 const auto dir=std::filesystem::temp_directory_path()/("fu-catalog-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directories(dir);
 try{
  const auto settings=dir/"mod-indexes.txt";std::string error;
  check(catalog::load_sources(settings,error).size()==1&&error.empty());
  catalog::save_sources(settings,{builtin,third});
  auto sources=catalog::load_sources(settings,error);
  check(error.empty()&&sources.size()==2&&sources[0].url==builtin.url&&sources[1].url==third.url);
  bool duplicate_refused=false;try{catalog::save_sources(settings,{builtin,third,third});}catch(...){duplicate_refused=true;}check(duplicate_refused);
  check(catalog::load_sources(settings,error).size()==2);
  check(catalog::cache_directory(dir,builtin)==dir);
  check(catalog::cache_directory(dir,builtin)!=catalog::cache_directory(dir,third));
  catalog::save_sources(settings,{builtin});check(catalog::load_sources(settings,error).size()==1);
  {std::ofstream bad_settings(settings);bad_settings<<"https://example.com/bad.json\n"<<third.url<<"\n";}
  sources=catalog::load_sources(settings,error);check(sources.size()==2&&!error.empty());
  auto path=dir/"example.zip";{std::ofstream f(path,std::ios::binary);f<<"abc";}
  auto streamed=index.mods[0];std::vector<std::uint8_t> large(2*64*1024+17);
  for(std::size_t i=0;i<large.size();++i)large[i]=static_cast<std::uint8_t>(i*17);
  streamed.bytes=large.size();streamed.sha256=psprecomp::sha256_bytes(large);
  const auto large_file=dir/"large.zip";{std::ofstream f(large_file,std::ios::binary);f.write(reinterpret_cast<const char *>(large.data()),large.size());}
  check(catalog::matches(streamed,large_file));
  check(catalog::matches(index.mods[0],path));catalog::Progress progress;auto cached=catalog::download(index.mods[0],dir,progress);check(cached.file==path&&cached.error.empty());
  progress.cancel=true;auto cancelled=catalog::download(index.mods[0],dir,progress);check(cancelled.file.empty()&&!cancelled.error.empty());
  {std::ofstream f(path,std::ios::binary);f<<"abd";}check(!catalog::matches(index.mods[0],path));
  {std::ofstream f(path,std::ios::binary);f<<"abcd";}check(!catalog::matches(index.mods[0],path));
  auto invalid=index.mods[0];invalid.file="../escape.zip";progress.cancel=false;auto bad=catalog::download(invalid,dir,progress);check(bad.file.empty()&&!bad.error.empty());
 }catch(...){std::filesystem::remove_all(dir);throw;}std::filesystem::remove_all(dir);
 std::cout<<"Catalogue parsing, Unicode, custom index persistence/origin isolation, path bounds, cache/checksum and cancellation checks PASS\n";
 }catch(const std::exception &e){std::cerr<<e.what()<<"\n";return 1;}return 0;}
