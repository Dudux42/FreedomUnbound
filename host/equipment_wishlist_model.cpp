#include "equipment_wishlist_model.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <istream>
#include <limits>
#include <ostream>
#include <utility>
namespace portablekit::fu::wishlist {
namespace {
bool ready(const Recipe &r,const Inventory &owned) {
    return std::all_of(r.materials.begin(),r.materials.end(),[&](auto m){return owned[m.item]>=m.required;});
}
bool normalize(Recipe &r) {
    if(r.name.empty() || r.name.size()>256 || r.materials.empty() || r.materials.size()>4 ||
       r.key.operation>1 || r.key.category>1 || r.key.type>5 || !r.key.equipment ||
       r.key.equipment>4095 || r.key.recipe>8191)return false;
    for(unsigned char c:r.name)if(c<32 || c==127)return false;
    std::sort(r.materials.begin(),r.materials.end(),[](auto a,auto b){return a.item<b.item;});
    std::vector<Material> merged;
    for(auto m:r.materials) {
        if(!m.item || m.item>=item_limit || !m.required || m.required>3996)return false;
        if(!merged.empty() && merged.back().item==m.item)merged.back().required+=m.required;
        else merged.push_back(m);
    }
    for(auto m:merged)if(m.required>3996)return false;
    r.materials=std::move(merged);return true;
}
}
bool List::contains(const Key &key) const {
    return std::any_of(entries_.begin(),entries_.end(),[&](const auto &e){return e.recipe.key==key;});
}
bool List::contains_name(const std::string &name) const {
    return !name.empty() && std::any_of(entries_.begin(),entries_.end(),[&](const auto &e){return e.recipe.name==name;});
}
std::vector<Recipe> List::recipes() const {
    std::vector<Recipe> result;result.reserve(entries_.size());
    for(const auto &entry:entries_)result.push_back(entry.recipe);
    return result;
}
bool List::pin(Recipe recipe) {
    if(!normalize(recipe) || contains(recipe.key))return false;
    const bool complete=ready(recipe,previous_);
    entries_.push_back({std::move(recipe),complete});return true;
}
bool List::unpin(const Key &key) {
    const auto removed=std::erase_if(entries_,[&](const auto &e){return e.recipe.key==key;});
    return removed!=0;
}
std::vector<Requirement> List::checklist(const Key &key) const {
    std::vector<Requirement> result;
    for(const auto &e:entries_)if(e.recipe.key==key)for(auto m:e.recipe.materials) {
        const auto owned=previous_[m.item];
        result.push_back({m.item,m.required,owned,owned>=m.required ? 0 : m.required-owned});
    }
    return result;
}
void List::observe(const Inventory &owned,bool quest) {
    const bool announce=initialized_ && quest_ && quest;
    std::array<bool,item_limit> gained{};
    if(announce)for(unsigned id=1;id<item_limit;id++)if(owned[id]>previous_[id]) {
        const bool wanted=std::any_of(entries_.begin(),entries_.end(),[&](const auto &e){
            return std::any_of(e.recipe.materials.begin(),e.recipe.materials.end(),[&](auto m){return m.item==id;});
        });
        if(wanted){gained[id]=true;pending_.push_back({false,id,owned[id]-previous_[id],{}});}
    }
    for(auto &e:entries_) {
        const bool complete=ready(e.recipe,owned);
        const bool relevant=std::any_of(e.recipe.materials.begin(),e.recipe.materials.end(),[&](auto m){return gained[m.item];});
        if(announce && relevant && complete && !e.ready)pending_.push_back({true,0,0,e.recipe.name});
        e.ready=complete;
    }
    previous_=owned;initialized_=true;quest_=quest;
}
void List::announce_ready() {
    for(const auto &e:entries_)if(e.ready)pending_.push_back({true,0,0,e.recipe.name});
}
std::vector<Notice> List::take_notices() {return std::exchange(pending_,{});}
void List::write(std::ostream &out) const {
    out<<"FUWISHLIST 1\n"<<entries_.size()<<'\n';
    for(const auto &e:entries_) {
        const auto &r=e.recipe;const auto &k=r.key;
        out<<k.operation<<' '<<k.category<<' '<<k.recipe<<' '<<k.type<<' '<<k.equipment<<' '<<std::quoted(r.name)<<' '<<r.materials.size();
        for(auto m:r.materials)out<<' '<<m.item<<' '<<m.required;
        out<<'\n';
    }
}
bool List::read(std::istream &in) {
    std::string magic;unsigned version{},count{};
    if(!(in>>magic>>version>>count) || magic!="FUWISHLIST" || version!=1 || count>10000)return false;
    List parsed;
    for(unsigned i=0;i<count;i++) {
        Recipe r;auto &k=r.key;unsigned materials{};
        if(!(in>>k.operation>>k.category>>k.recipe>>k.type>>k.equipment>>std::quoted(r.name)>>materials) || materials>4)return false;
        for(unsigned j=0;j<materials;j++){Material m;if(!(in>>m.item>>m.required))return false;r.materials.push_back(m);}
        if(!parsed.pin(std::move(r)))return false;
    }
    in>>std::ws;if(!in.eof())return false;
    *this=std::move(parsed);return true;
}
Motion motion(double age) {
    const auto smooth=[](double t){t=std::clamp(t,0.,1.);return t*t*(3-2*t);};
    const float enter=static_cast<float>(smooth(age/.3)),leave=static_cast<float>(smooth((age-4.7)/.3));
    return {enter*(1-leave),14*(1-enter)-14*leave};
}
void Notifications::push(Notice notice) {waiting_.push_back(std::move(notice));}
void Notifications::clear() {waiting_.clear();visible_.clear();peak_=0;since_start_=1;}
void Notifications::advance(double seconds) {
    const auto dt=std::isfinite(seconds) ? std::max(seconds,0.) : 0.;
    since_start_+=dt;
    for(auto &card:visible_)card.age+=dt;
    std::erase_if(visible_,[](const auto &c){return c.age>=5.;});
    if(visible_.empty())peak_=0;
    if(!waiting_.empty() && visible_.size()<3 && since_start_>=.4) {
        visible_.push_back({std::move(waiting_.front()),0,0,++serial_});
        waiting_.pop_front();since_start_=0;
        peak_=std::max(peak_,static_cast<unsigned>(visible_.size()));
    }
    for(unsigned i=0;i<visible_.size();i++) {
        const float target=static_cast<float>(i)-static_cast<float>(peak_)+1;
        auto &card=visible_[i];
        if(card.age==0)card.y=target;
        else card.y+=(target-card.y)*static_cast<float>(1-std::exp(-14*dt));
    }
}
}
