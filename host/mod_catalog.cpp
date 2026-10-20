#include "mod_catalog.hpp"
#include "psprecomp/sha256.hpp"
#include <algorithm>
#include <charconv>
#include <sstream>
#include <chrono>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>
#include <string_view>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#endif
namespace portablekit::fu::catalog {
namespace {
namespace fs=std::filesystem;
constexpr std::uint64_t max_zip=2ull*1024*1024*1024;
constexpr std::uint64_t max_index=2*1024*1024;
struct Json {
 enum Kind {Object,Array,String,Number,Other} kind{Other};
 std::map<std::string,Json> object; std::vector<Json> array; std::string text;
 const Json &at(const char *key) const {auto i=object.find(key);if(kind!=Object||i==object.end())throw std::runtime_error("Missing catalogue field: "+std::string(key));return i->second;}
 std::string str() const {if(kind!=String)throw std::runtime_error("Invalid catalogue string");return text;}
 std::uint64_t integer() const {std::uint64_t n{};auto r=std::from_chars(text.data(),text.data()+text.size(),n);if(kind!=Number||r.ec!=std::errc{}||r.ptr!=text.data()+text.size())throw std::runtime_error("Invalid catalogue size");return n;}
};
// Small JSON reader, shared by the bounded catalogue and release-asset metadata.
class Reader {
 std::string_view s; std::size_t p{};
 [[noreturn]] void bad() const {throw std::runtime_error("The mod catalogue contains invalid JSON.");}
 void ws(){while(p<s.size()&&(s[p]==' '||s[p]=='\n'||s[p]=='\r'||s[p]=='\t'))++p;}
 bool take(char c){ws();if(p<s.size()&&s[p]==c){++p;return true;}return false;}
 unsigned hex(){unsigned v=0;for(int i=0;i<4;++i){if(p==s.size())bad();char c=s[p++];v*=16;if(c>='0'&&c<='9')v+=c-'0';else if(c>='a'&&c<='f')v+=c-'a'+10;else if(c>='A'&&c<='F')v+=c-'A'+10;else bad();}return v;}
 void utf8(std::string &o,unsigned n){if(n<128)o+=char(n);else if(n<2048){o+=char(192|(n>>6));o+=char(128|(n&63));}else if(n<65536){o+=char(224|(n>>12));o+=char(128|((n>>6)&63));o+=char(128|(n&63));}else{o+=char(240|(n>>18));o+=char(128|((n>>12)&63));o+=char(128|((n>>6)&63));o+=char(128|(n&63));}}
 std::string string(){if(!take('"'))bad();std::string o;while(p<s.size()){unsigned char c=s[p++];if(c=='"')return o;if(c<32)bad();if(c!='\\'){o+=char(c);continue;}if(p==s.size())bad();switch(c=s[p++]){case '"':case '\\':case '/':o+=char(c);break;case 'b':o+='\b';break;case 'f':o+='\f';break;case 'n':o+='\n';break;case 'r':o+='\r';break;case 't':o+='\t';break;case 'u':{unsigned n=hex();if(n>=0xd800&&n<=0xdbff){if(p+2>s.size()||s[p++]!='\\'||s[p++]!='u')bad();unsigned lo=hex();if(lo<0xdc00||lo>0xdfff)bad();n=65536+((n-0xd800)<<10)+(lo-0xdc00);}else if(n>=0xdc00&&n<=0xdfff)bad();utf8(o,n);break;}default:bad();}}bad();}
 Json value(unsigned depth){ws();if(depth>16||p==s.size())bad();Json j;if(s[p]=='{'){++p;j.kind=Json::Object;if(take('}'))return j;do{std::string k=string();if(!take(':'))bad();if(!j.object.emplace(k,value(depth+1)).second)bad();}while(take(','));if(!take('}'))bad();return j;}if(s[p]=='['){++p;j.kind=Json::Array;if(take(']'))return j;do{if(j.array.size()>=2048)bad();j.array.push_back(value(depth+1));}while(take(','));if(!take(']'))bad();return j;}if(s[p]=='"'){j.kind=Json::String;j.text=string();return j;}std::size_t begin=p;if(s[p]=='-'||(s[p]>='0'&&s[p]<='9')){j.kind=Json::Number;if(s[p]=='-')++p;if(p==s.size())bad();if(s[p]=='0')++p;else{if(s[p]<'1'||s[p]>'9')bad();while(p<s.size()&&s[p]>='0'&&s[p]<='9')++p;}if(p<s.size()&&s[p]=='.'){++p;std::size_t at=p;while(p<s.size()&&s[p]>='0'&&s[p]<='9')++p;if(at==p)bad();}if(p<s.size()&&(s[p]=='e'||s[p]=='E')){++p;if(p<s.size()&&(s[p]=='+'||s[p]=='-'))++p;std::size_t at=p;while(p<s.size()&&s[p]>='0'&&s[p]<='9')++p;if(at==p)bad();}j.text=std::string(s.substr(begin,p-begin));return j;}for(auto literal:{"true","false","null"})if(s.substr(p,std::char_traits<char>::length(literal))==literal){p+=std::char_traits<char>::length(literal);return j;}bad();}
public:explicit Reader(std::string_view input):s(input){}Json read(){Json j=value(0);ws();if(p!=s.size())bad();return j;}
};
bool token(const std::string &s){return !s.empty()&&s.size()<=128&&s!="."&&s!=".."&&std::all_of(s.begin(),s.end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.';});}
std::string load(const fs::path &p){std::ifstream f(p,std::ios::binary|std::ios::ate);if(!f||f.tellg()>max_index)throw std::runtime_error("No cached mod catalogue is available.");auto n=f.tellg();if(n<0)throw std::runtime_error("Cannot read cached catalogue.");std::string s(static_cast<std::size_t>(n),'\0');f.seekg(0);if(!f.read(s.data(),s.size()))throw std::runtime_error("Cannot read cached catalogue.");return s;}
#ifdef _WIN32
std::string checksum(const fs::path &file, const Progress *progress = nullptr) {
 struct Algorithm { BCRYPT_ALG_HANDLE h{}; ~Algorithm(){if(h)BCryptCloseAlgorithmProvider(h,0);} } alg;
 struct Hash { BCRYPT_HASH_HANDLE h{}; ~Hash(){if(h)BCryptDestroyHash(h);} } hash;
 if(BCryptOpenAlgorithmProvider(&alg.h,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0 ||
    BCryptCreateHash(alg.h,&hash.h,nullptr,0,nullptr,0,0)<0)
  throw std::runtime_error("Cannot initialize download checksum verification.");
 std::ifstream in(file,std::ios::binary);if(!in)throw std::runtime_error("Cannot read the downloaded mod ZIP.");
 unsigned char buffer[64*1024];
 while(in){
  if(progress && progress->cancel)throw std::runtime_error("Download cancelled.");
  in.read(reinterpret_cast<char *>(buffer),sizeof(buffer));const auto n=in.gcount();
  if(n && BCryptHashData(hash.h,buffer,static_cast<ULONG>(n),0)<0)
   throw std::runtime_error("Cannot verify the downloaded mod ZIP.");
 }
 if(!in.eof())throw std::runtime_error("Cannot finish reading the downloaded mod ZIP.");
 unsigned char digest[32];if(BCryptFinishHash(hash.h,digest,sizeof(digest),0)<0)
  throw std::runtime_error("Cannot finish download checksum verification.");
 constexpr char hex[]="0123456789abcdef";std::string out;out.reserve(64);
 for(auto b:digest){out+=hex[b>>4];out+=hex[b&15];}return out;
}
struct Handle {HINTERNET h{};~Handle(){if(h)WinHttpCloseHandle(h);}operator HINTERNET()const{return h;}};
std::wstring wide(const std::string &s){int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),int(s.size()),nullptr,0);if(!n)throw std::runtime_error("Invalid download address.");std::wstring w(n,0);MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),int(s.size()),w.data(),n);return w;}
void get(std::string url,const fs::path &target,std::uint64_t limit,Progress &progress){
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::minutes(15);
 Handle session{WinHttpOpen(L"FreedomUnbound-Mods",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0)};
 if(!session.h)throw std::runtime_error("Cannot start the mod download connection.");
 WinHttpSetTimeouts(session,10000,10000,15000,15000);
 for(int redirect=0;redirect<=5;++redirect){
  if(progress.cancel)throw std::runtime_error("Download cancelled.");
  std::wstring address=wide(url);URL_COMPONENTS parts{};parts.dwStructSize=sizeof(parts);parts.dwHostNameLength=parts.dwUrlPathLength=parts.dwExtraInfoLength=DWORD(-1);
  if(!WinHttpCrackUrl(address.c_str(),0,0,&parts)||parts.nScheme!=INTERNET_SCHEME_HTTPS||parts.nPort!=443||parts.dwUserNameLength||parts.dwPasswordLength)throw std::runtime_error("Only HTTPS downloads are supported.");
  std::wstring host(parts.lpszHostName,parts.dwHostNameLength),path(parts.lpszUrlPath,parts.dwUrlPathLength);if(parts.dwExtraInfoLength)path.append(parts.lpszExtraInfo,parts.dwExtraInfoLength);
  // Release assets redirect to GitHub's signed asset host. Never use arbitrary servers.
  if(host!=L"github.com"&&host!=L"raw.githubusercontent.com"&&host!=L"release-assets.githubusercontent.com"&&host!=L"objects.githubusercontent.com")throw std::runtime_error("The mod download redirected outside GitHub.");
  Handle connection{WinHttpConnect(session,host.c_str(),443,0)};
  Handle request{connection.h?WinHttpOpenRequest(connection,L"GET",path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE):nullptr};
  if(!request.h)throw std::runtime_error("Cannot connect to GitHub.");
  DWORD policy=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;WinHttpSetOption(request,WINHTTP_OPTION_REDIRECT_POLICY,&policy,sizeof(policy));
  if(!WinHttpSendRequest(request,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0)||!WinHttpReceiveResponse(request,nullptr))throw std::runtime_error("GitHub could not be reached. Check your connection and try again.");
  DWORD status{},length=sizeof(status);if(!WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,nullptr,&status,&length,nullptr))throw std::runtime_error("GitHub returned an unreadable response.");
  if(status>=300&&status<400){wchar_t next[8192];length=sizeof(next);if(!WinHttpQueryHeaders(request,WINHTTP_QUERY_LOCATION,nullptr,next,&length,nullptr))throw std::runtime_error("GitHub returned an invalid redirect.");int n=WideCharToMultiByte(CP_UTF8,0,next,-1,nullptr,0,nullptr,nullptr);std::string next_url(n,'\0');WideCharToMultiByte(CP_UTF8,0,next,-1,next_url.data(),n,nullptr,nullptr);next_url.pop_back();url=std::move(next_url);continue;}
  if(status!=200)throw std::runtime_error(status==404?"The mod index or ZIP is unavailable. The repository must be public.":"GitHub returned HTTP "+std::to_string(status)+". Try again later.");
  std::ofstream out(target,std::ios::binary|std::ios::trunc);if(!out)throw std::runtime_error("Cannot write the mod download. Check free space and permissions.");
  std::uint64_t received=0;char buffer[64*1024];
  for(;;){if(progress.cancel)throw std::runtime_error("Download cancelled.");if(std::chrono::steady_clock::now()>deadline)throw std::runtime_error("The mod download timed out.");DWORD read{};if(!WinHttpReadData(request,buffer,sizeof(buffer),&read))throw std::runtime_error("The download was interrupted. Please retry.");if(!read)break;if(read>limit-received)throw std::runtime_error("The download exceeds its published size.");received+=read;out.write(buffer,read);if(!out)throw std::runtime_error("Cannot save the download. Check free space.");progress.bytes=received;}
  out.flush();if(!out)throw std::runtime_error("Cannot finish saving the download.");return;
 }
 throw std::runtime_error("GitHub returned too many download redirects.");
}
#else
std::string checksum(const fs::path &file, const Progress * = nullptr){return psprecomp::sha256_file(file);}
void get(std::string,const fs::path &,std::uint64_t,Progress &){throw std::runtime_error("Mod downloads are currently supported on Windows.");}
#endif
}
namespace {
std::string lower(std::string s){for(char &c:s)if(c>='A'&&c<='Z')c=char(c-'A'+'a');return s;}
bool download_url(const Entry &e){
 const std::string suffix="/releases/download/"+e.release+"/"+e.file;
 if(!token(e.release)||e.url.size()<suffix.size())return false;
 const auto origin=e.url.substr(0,e.url.size()-suffix.size());
 return lower(origin)==lower(e.origin_repository)&&e.url.ends_with(suffix);
}
void replace_file(const fs::path &from,const fs::path &to){
#ifdef _WIN32
 if(!MoveFileExW(from.c_str(),to.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
  throw std::runtime_error("Cannot save the mod index settings. Check permissions and free space.");
#else
 fs::rename(from,to);
#endif
}
}
Source source_from_url(std::string url){
 const auto first=url.find_first_not_of(" \t\r\n"),last=url.find_last_not_of(" \t\r\n");
 if(first==std::string::npos)throw std::runtime_error("Enter a public GitHub repository or index JSON address.");
 url=url.substr(first,last-first+1);while(url.ends_with('/'))url.pop_back();
 if(url.size()>2048||url.find_first_of("?#\\\r\n\t ")!=std::string::npos)
  throw std::runtime_error("Use a GitHub HTTPS address without a query or fragment.");
 constexpr std::string_view github="https://github.com/",raw="https://raw.githubusercontent.com/";
 bool is_raw=url.starts_with(raw);std::size_t offset=is_raw?raw.size():github.size();
 if(!is_raw&&!url.starts_with(github))throw std::runtime_error("Mod indexes must use public GitHub HTTPS addresses.");
 std::vector<std::string> parts;std::stringstream path(url.substr(offset));std::string segment;
 while(std::getline(path,segment,'/')){if(!token(segment))throw std::runtime_error("Invalid GitHub index path.");parts.push_back(segment);}
 if(parts.size()<2)throw std::runtime_error("Include the GitHub owner and repository name.");
 if(!is_raw&&parts.size()==2&&parts[1].ends_with(".git"))parts[1].resize(parts[1].size()-4);
 if(!token(parts[1]))throw std::runtime_error("Invalid GitHub repository name.");
 Source out;out.name=parts[0]+"/"+parts[1];out.repository=std::string(github)+lower(parts[0])+"/"+lower(parts[1]);
 out.url=std::string(raw)+lower(parts[0])+"/"+lower(parts[1]);
 if(!is_raw&&parts.size()==2){out.url+="/main/releases.json";return out;}
 std::size_t begin=2;if(!is_raw){if(parts.size()<5||parts[2]!="blob")throw std::runtime_error("Use a repository URL or the address of its index JSON file.");begin=3;}
 if(parts.size()-begin<2||!parts.back().ends_with(".json"))throw std::runtime_error("The index address must point to a JSON file.");
 for(std::size_t i=begin;i<parts.size();++i)out.url+="/"+parts[i];
 return out;
}
Source default_source(){auto source=source_from_url(index_url);source.name="Freedom Unbound Official Mod Page";return source;}
std::vector<Source> load_sources(const fs::path &file,std::string &error){
 std::vector<Source> out{default_source()};error.clear();std::error_code ec;
 if(!fs::exists(file,ec))return out;
 if(fs::file_size(file,ec)>64*1024||ec){error="The saved mod index list cannot be read.";return out;}
 std::ifstream in(file);if(!in){error="The saved mod index list cannot be read.";return out;}
 std::set<std::string> urls{out.front().url};std::string line;
 while(std::getline(in,line)){
  if(line.find_first_not_of(" \t\r")==std::string::npos)continue;
  try{auto source=source_from_url(line);if(!urls.insert(source.url).second)continue;
   if(out.size()>=32)throw std::runtime_error("Too many saved indexes");out.push_back(std::move(source));
  }catch(...){error="Some saved mod index addresses could not be loaded.";}
 }
 return out;
}
void save_sources(const fs::path &file,const std::vector<Source> &sources){
 if(sources.size()>32)throw std::runtime_error("Up to 32 mod indexes can be saved.");
 std::set<std::string> urls;std::string text;
 for(const auto &source:sources){auto checked=source_from_url(source.url);if(!urls.insert(checked.url).second)throw std::runtime_error("This mod index is already added.");if(checked.url!=default_source().url)text+=checked.url+"\n";}
 fs::create_directories(file.parent_path());auto temp=file;temp+=".part";
 try{{std::ofstream out(temp,std::ios::binary|std::ios::trunc);if(!out||!out.write(text.data(),text.size())||!out.flush())throw std::runtime_error("Cannot save the mod index list.");}replace_file(temp,file);}
 catch(...){std::error_code ec;fs::remove(temp,ec);throw;}
}
fs::path cache_directory(const fs::path &root,const Source &source){
 if(source.url==default_source().url)return root;
 const auto &url=source.url;return root/"indexes"/psprecomp::sha256_bytes(std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t *>(url.data()),url.size())).substr(0,32);
}
Index parse(const std::string &text,const Source &source){
 const auto origin=source_from_url(source.url).repository;
 if(text.size()>max_index)throw std::runtime_error("The mod catalogue is too large.");
 const Json root=Reader(text).read();Index out;out.release=root.at("release").str();if(!token(out.release))throw std::runtime_error("Invalid catalogue release.");
 const auto &mods=root.at("mods");if(mods.kind!=Json::Array||mods.array.size()>512)throw std::runtime_error("Invalid mod list.");std::set<std::string> ids,files;
 for(const auto &j:mods.array){Entry e;e.id=j.at("id").str();e.name=j.at("name").str();e.version=j.at("version").str();e.file=j.at("file").str();e.bytes=j.at("bytes").integer();e.sha256=j.at("sha256").str();e.url=j.at("url").str();e.guide=j.at("guide").str();e.origin_repository=origin;e.release=out.release;
  if(!token(e.id)||!token(e.version)||!token(e.file)||!e.file.ends_with(".zip")||e.name.empty()||e.name.size()>256||e.name.find_first_of("\r\n")!=std::string::npos||!e.bytes||e.bytes>max_zip||e.sha256.size()!=64||!std::all_of(e.sha256.begin(),e.sha256.end(),[](char c){return(c>='0'&&c<='9')||(c>='a'&&c<='f');})||!ids.insert(e.id).second||!files.insert(e.file).second||!download_url(e)||!e.guide.starts_with("guides/")||e.guide.find("..")!=std::string::npos||e.guide.find_first_of("\\?#\r\n")!=std::string::npos)throw std::runtime_error("The mod catalogue contains an invalid or duplicate entry.");
  out.mods.push_back(std::move(e));
 }return out;
}
namespace {
bool verified(const Entry &e,const fs::path &file,const Progress *progress=nullptr){std::error_code ec;return fs::is_regular_file(file,ec)&&fs::file_size(file,ec)==e.bytes&&!ec&&checksum(file,progress)==e.sha256;}
}
bool matches(const Entry &e,const fs::path &file){return verified(e,file);}
Listing fetch(const fs::path &cache,Progress &progress,const Source &source){Listing r;const auto temp=cache/"index.part",saved=cache/"releases.json";
 try{fs::create_directories(cache);progress.bytes=0;progress.total=0;get(source_from_url(source.url).url,temp,max_index,progress);r.index=parse(load(temp),source);replace_file(temp,saved);}
 catch(const std::exception &e){r.error=e.what();if(!progress.cancel)try{r.index=parse(load(saved),source);r.cached=true;}catch(...) {}}
 std::error_code ec;fs::remove(temp,ec);return r;
}
Download download(const Entry &e,const fs::path &cache,Progress &progress){Download out;fs::path temp;
 try{ // Validate direct callers too, before using a filename or address.
  if(!token(e.file)||!token(e.id)||!e.file.ends_with(".zip")||!e.bytes||e.bytes>max_zip||!download_url(e)||source_from_url(e.origin_repository).repository!=lower(e.origin_repository))throw std::runtime_error("Invalid mod download entry.");
  fs::create_directories(cache);const auto saved=cache/e.file;temp=cache/(e.file+".part");progress.bytes=0;progress.total=e.bytes;
  if(progress.cancel)throw std::runtime_error("Download cancelled.");
  if(verified(e,saved,&progress)){progress.bytes=e.bytes;out.file=saved;return out;}
  get(e.url,temp,e.bytes,progress);if(progress.cancel)throw std::runtime_error("Download cancelled.");if(!verified(e,temp,&progress))throw std::runtime_error("The mod ZIP does not match its published size or SHA-256 checksum. It was not installed.");
  std::error_code ec;fs::remove(saved,ec);fs::rename(temp,saved);out.file=saved;
 }catch(const std::exception &e){out.error=e.what();}
 if(!temp.empty()){std::error_code ec;fs::remove(temp,ec);}return out;
}
}
